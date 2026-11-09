"""Read-only P1 command implementations."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

from .generate import check_generated_isolation, prepare_profile
from .manifest import ManifestError, Profile, ProfileRegistry


def list_profiles(registry: ProfileRegistry) -> int:
    print("ID           KIND         HOSTS        DEFAULT  DESCRIPTION")
    for profile in registry.profiles:
        hosts = ",".join(profile.enabled_hosts)
        marker = "yes" if profile.id == registry.default_profile else "-"
        print(
            f"{profile.id:<12} {profile.kind:<12} {hosts:<12} "
            f"{marker:<8} {profile.description}"
        )
    return 0


def _platformio_command() -> str | None:
    for command in ("pio", "platformio"):
        found = shutil.which(command)
        if found:
            return found
    user_profile = Path.home()
    for name in ("pio.exe", "platformio.exe", "pio", "platformio"):
        candidate = user_profile / ".platformio" / "penv" / "Scripts" / name
        if candidate.is_file():
            return str(candidate)
    return None


def _run(command: list[str], *, working_directory: Path) -> int:
    completed = subprocess.run(command, cwd=working_directory, check=False)
    return completed.returncode


def doctor(profile: Profile, host: str) -> int:
    if host not in profile.enabled_hosts:
        raise ManifestError(
            "unsupported-profile-host",
            f"{profile.manifest_path}.hosts.{host}",
            f"profile '{profile.id}' does not enable host '{host}'",
        )

    checks: list[tuple[str, bool, str]] = [
        ("Python 3.11+", sys.version_info >= (3, 11), f"{sys.version_info.major}.{sys.version_info.minor}"),
        ("profile manifest", True, profile.manifest_path.name),
    ]
    if host == "windows":
        checks.extend(
            [
                ("CMake", shutil.which("cmake") is not None, "command"),
                ("Ninja", shutil.which("ninja") is not None, "command"),
            ]
        )
    elif host == "hmi":
        checks.append(("PlatformIO", _platformio_command() is not None, "command"))

    print(f"Doctor: profile={profile.id} host={host}")
    failed = False
    for name, passed, detail in checks:
        status = "ok" if passed else "missing"
        print(f"  [{status}] {name}: {detail}")
        failed = failed or not passed
    if failed:
        print("Result: failed (install or expose the missing tool, then run doctor again)")
        return 3
    print("Result: ready")
    return 0


def prepare(
    repository_root: Path,
    registry: ProfileRegistry,
    host: str,
    profile_id: str,
) -> int:
    target = prepare_profile(repository_root, registry, host, profile_id)
    relative_target = target.relative_to(repository_root).as_posix()
    print(f"Prepared: {host}/{profile_id}")
    print(f"Output: {relative_target}")
    return 0


def bundle_assets(
    repository_root: Path,
    registry: ProfileRegistry,
    profile_id: str,
) -> int:
    profile = registry.get(profile_id)
    generated = prepare_profile(repository_root, registry, "hmi", profile_id)
    destination = (
        repository_root / "dist" / "sd" / profile.data["data"]["hmi_sd_root"]
    )
    manifest = json.loads((generated / "asset_manifest.json").read_text(encoding="utf-8"))
    assets = manifest["assets"]
    desired_files = {
        Path(asset["relative_path"]).as_posix() for asset in assets
    } | {"asset_manifest.json"}
    destination.mkdir(parents=True, exist_ok=True)
    for existing in sorted(destination.rglob("*"), reverse=True):
        if existing.is_file():
            relative_existing = existing.relative_to(destination).as_posix()
            if relative_existing not in desired_files:
                existing.unlink()
        elif existing.is_dir() and not any(existing.iterdir()):
            existing.rmdir()
    for asset in assets:
        source = repository_root / asset["source"]
        target = destination / asset["relative_path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    shutil.copy2(generated / "asset_manifest.json", destination / "asset_manifest.json")
    print(f"Bundled assets: {profile_id}")
    print(f"Output: {destination.relative_to(repository_root).as_posix()}")
    print(f"Files: {len(assets)}")
    return 0


def push_assets(
    repository_root: Path,
    registry: ProfileRegistry,
    profile_id: str,
    port: str | None,
    *,
    dry_run: bool,
    list_ports: bool,
) -> int:
    """profile 자산을 준비하고 USB CDC updater로 전송한다."""
    from assets.push import (
        AssetPushError,
        available_ports,
        describe_bundle,
        load_bundle,
        push_bundle,
    )

    if list_ports:
        ports = available_ports()
        if not ports:
            print("No serial ports found.")
        for device, description in ports:
            print(f"{device}: {description}")
        return 0
    result = bundle_assets(repository_root, registry, profile_id)
    if result != 0:
        return result
    try:
        bundle = load_bundle(repository_root, profile_id)
        describe_bundle(bundle)
        if dry_run:
            print("Dry run complete; HMI was not changed.")
            return 0
        if port is None:
            raise AssetPushError("--port is required unless --dry-run or --list-ports is used")
        push_bundle(bundle, port)
    except AssetPushError as error:
        print(f"error: {error}", file=sys.stderr)
        return 5
    print("Asset update complete. Reopen Text Input to load the new fonts.")
    return 0


def check_isolation(repository_root: Path, registry: ProfileRegistry) -> int:
    checked = check_generated_isolation(repository_root, registry)
    print("Isolation: ready")
    if checked:
        print(f"Checked: {', '.join(checked)}")
    else:
        print("Checked: no generated profile inputs")
    return 0


def build_profile(
    repository_root: Path,
    registry: ProfileRegistry,
    host: str,
    profile_id: str,
) -> int:
    profile = registry.get(profile_id)
    if host not in profile.enabled_hosts:
        raise ManifestError(
            "unsupported-profile-host",
            f"{profile.manifest_path}.hosts.{host}",
            f"profile '{profile.id}' does not enable host '{host}'",
        )
    generated = prepare_profile(repository_root, registry, host, profile_id)

    if host == "windows":
        build_directory = repository_root / "build" / f"windows-{profile.id}"
        configure_command = [
            "cmake",
            "-S",
            str(repository_root),
            "-B",
            str(build_directory),
            "-G",
            "Ninja",
            "-DBUILD_TESTING=ON",
            f"-DHMI_GENERATED_DIR={generated}",
        ]
        if _run(configure_command, working_directory=repository_root) != 0:
            return 4
        return _run(
            ["cmake", "--build", str(build_directory)],
            working_directory=repository_root,
        )

    platformio = _platformio_command()
    if platformio is None:
        print("error: PlatformIO command is not available", file=sys.stderr)
        return 3
    configuration = generated / "platformio.ini"
    environment = f"hmi-{profile.id}"
    return _run(
        [
            platformio,
            "run",
            "-d",
            str(repository_root),
            "-c",
            str(configuration),
            "-e",
            environment,
        ],
        working_directory=repository_root,
    )


def check_profile(
    repository_root: Path,
    registry: ProfileRegistry,
    host: str,
    profile_id: str,
) -> int:
    profile = registry.get(profile_id)
    if host != "windows":
        raise ManifestError(
            "unsupported-check-host",
            "--host",
            "profile test execution is currently available only for windows",
        )
    build_directory = repository_root / "build" / f"windows-{profile.id}"
    if not (build_directory / "CTestTestfile.cmake").is_file():
        build_result = build_profile(repository_root, registry, host, profile_id)
        if build_result != 0:
            return build_result
    tests = profile.data["verification"]["tests"]
    if not tests:
        print(f"Check: profile={profile.id} host={host} (no tests declared)")
        return 0
    expression = "^(" + "|".join(tests) + ")$"
    return _run(
        [
            "ctest",
            "--test-dir",
            str(build_directory),
            "--tests-regex",
            expression,
            "--output-on-failure",
        ],
        working_directory=repository_root,
    )


def run_profile(
    repository_root: Path,
    registry: ProfileRegistry,
    host: str,
    profile_id: str,
) -> int:
    if host != "windows":
        raise ManifestError(
            "unsupported-run-host",
            "--host",
            "HMI run/upload is intentionally manual; build the firmware and upload it yourself",
        )
    build_result = build_profile(repository_root, registry, host, profile_id)
    if build_result != 0:
        return build_result
    executable_id = profile_id.replace("-", "_")
    executable = (
        repository_root
        / "build"
        / f"windows-{profile_id}"
        / "hosts"
        / "windows"
        / f"hmi_{executable_id}_windows.exe"
    )
    if not executable.is_file():
        print(f"error: Windows executable is missing: {executable.name}", file=sys.stderr)
        return 4
    return _run([str(executable)], working_directory=repository_root)
