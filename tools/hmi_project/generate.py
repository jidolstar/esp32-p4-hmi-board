"""Deterministic host/profile build input generation."""

from __future__ import annotations

import hashlib
import json
import shutil
import uuid
from pathlib import Path

from .manifest import Profile, ProfileRegistry, SUPPORTED_HOSTS
from .pages import check_page_source_isolation, load_page_registry

GENERATOR_VERSION = 23

MODULE_INPUTS = {
    "audio": {
        "sources": ("modules/audio/src/playback.cpp",),
        "include_dirs": ("modules/audio/include",),
    },
    "connectivity": {
        "sources": (
            "modules/connectivity/src/wifi_ap_runtime.cpp",
            "modules/connectivity/src/wifi_ap_web_controller.cpp",
            "modules/connectivity/src/wifi_ap_web_protocol.cpp",
            "modules/connectivity/src/wifi_scan_service.cpp",
            "modules/connectivity/src/wifi_sta_registration.cpp",
            "modules/connectivity/src/wifi_station_settings.cpp",
        ),
        "include_dirs": ("modules/connectivity/include",),
    },
    "i18n": {
        "sources": ("modules/i18n/src/locale.cpp", "modules/i18n/src/format.cpp"),
        "include_dirs": ("modules/i18n/include",),
    },
    "settings": {
        "sources": ("modules/settings/src/level_settings.cpp",),
        "include_dirs": ("modules/settings/include",),
    },
    "storage": {
        "sources": ("modules/storage/src/storage_access_controller.cpp",),
        "include_dirs": ("modules/storage/include",),
    },
    "text": {
        "sources": (
            "modules/text/src/utf8.cpp",
            "modules/text/src/editor.cpp",
            "modules/text/src/hangul_composer.cpp",
            "modules/text/src/input_model.cpp",
            "modules/text/src/keyboard_layout.cpp",
        ),
        "include_dirs": ("modules/text/include",),
    },
    "time": {
        "sources": (
            "modules/time/src/date_time.cpp",
            "modules/time/src/date_time_model.cpp",
            "modules/time/src/time_sync_controller.cpp",
        ),
        "include_dirs": ("modules/time/include",),
    },
    "ui": {
        "sources": (
            "modules/ui/src/asset_update_overlay.cpp",
            "modules/ui/src/theme.cpp",
            "modules/ui/src/components.cpp",
            "modules/ui/src/font_repository.cpp",
            "modules/ui/src/user_text_label.cpp",
            "modules/ui/src/user_text_input_line.cpp",
            "modules/ui/src/virtual_keyboard.cpp",
        ),
        "include_dirs": ("modules/ui/include",),
    },
}

WINDOWS_INPUTS = {
    "sources": (
        "hosts/windows/main.cpp",
        "platform/windows/lvgl_mem.cpp",
        "platform/windows/sdl_lvgl_port.cpp",
        "platform/windows/settings/memory_level_settings.cpp",
    ),
    "include_dirs": ("include", "platform/windows"),
}

HMI_INPUTS = {
    "sources": ("hosts/hmi/main.cpp", "platform/hmi/lvgl_mem.cpp"),
    "include_dirs": ("include", "platform/hmi"),
}

BOARD_INPUTS = {
    "jc1060p470c": {
        "sources": (
            "boards/jc1060p470c/board.cpp",
            "boards/jc1060p470c/storage.cpp",
            "boards/jc1060p470c/internal/i2c_transaction.cpp",
            "boards/jc1060p470c/internal/lvgl_port.cpp",
            "boards/jc1060p470c/internal/mipi_dsi_display.cpp",
            "boards/jc1060p470c/internal/sd_storage.cpp",
            "boards/jc1060p470c/internal/touch_controller.cpp",
        ),
        "include_dirs": ("boards/jc1060p470c",),
    }
}


class GenerationError(RuntimeError):
    """Generated input is invalid, stale, or could not be committed safely."""

    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code

    def __str__(self) -> str:
        return f"[{self.code}] {super().__str__()}"


def _unique(values: list[str]) -> list[str]:
    return list(dict.fromkeys(values))


def _repository_path(path: Path, repository_root: Path) -> str:
    try:
        return path.resolve().relative_to(repository_root.resolve()).as_posix()
    except ValueError as error:
        raise GenerationError(
            "generator-path-escape", f"path '{path}' is outside the repository"
        ) from error


def _require_files(repository_root: Path, values: list[str], group: str) -> None:
    for value in values:
        if not (repository_root / value).is_file():
            raise GenerationError(
                "missing-generator-input", f"{group} file '{value}' does not exist"
            )


def _require_directories(repository_root: Path, values: list[str], group: str) -> None:
    for value in values:
        if not (repository_root / value).is_dir():
            raise GenerationError(
                "missing-generator-input", f"{group} directory '{value}' does not exist"
            )


def resolve_profile_inputs(
    repository_root: Path, profile: Profile, host: str
) -> dict:
    if host not in SUPPORTED_HOSTS:
        raise GenerationError("unsupported-host", f"unsupported host '{host}'")
    if host not in profile.enabled_hosts:
        raise GenerationError(
            "unsupported-profile-host",
            f"profile '{profile.id}' does not enable host '{host}'",
        )

    repository_root = repository_root.resolve()
    profile_sources: list[str] = []
    for source_group in ("common", host):
        for source in profile.data["sources"][source_group]:
            profile_sources.append(
                _repository_path(profile.root / source, repository_root)
            )

    page_sources: list[str] = []
    page_descriptors: list[dict] = []
    page_registry = load_page_registry(repository_root, profile)
    if page_registry is not None:
        for page in page_registry.pages:
            for source_group in ("common", host):
                for source in page.data["sources"][source_group]:
                    page_sources.append(
                        _repository_path(profile.root / source, repository_root)
                    )
            page_descriptors.append(
                {
                    "id": page.data["id"],
                    "title_key": page.data["title_key"],
                    "description_key": page.data["description_key"],
                    "order": page.data["order"],
                    "data_key": page.data["data_key"],
                    "entry": {
                        "header": _repository_path(
                            profile.root / page.data["entry"]["header"],
                            repository_root,
                        ),
                        "type": page.data["entry"]["type"],
                    },
                    "capabilities": page.data["capabilities"],
                    "verification": page.data["verification"],
                }
            )

    profile_include_dirs = [
        _repository_path(profile.root / include_dir, repository_root)
        for include_dir in profile.data["include_dirs"]
    ]
    module_sources: list[str] = []
    module_include_dirs: list[str] = []
    for module in profile.data["modules"]:
        module_input = MODULE_INPUTS[module]
        module_sources.extend(module_input["sources"])
        module_include_dirs.extend(module_input["include_dirs"])

    host_input = WINDOWS_INPUTS if host == "windows" else HMI_INPUTS
    host_sources = list(host_input["sources"])
    host_include_dirs = list(host_input["include_dirs"])
    if "connectivity" in profile.data["modules"]:
        if host == "windows":
            module_sources.extend(
                (
                    "platform/windows/connectivity/virtual_wifi_ap.cpp",
                    "platform/windows/connectivity/virtual_wifi_sta.cpp",
                )
            )
        else:
            host_sources.extend(
                (
                    "platform/hmi/connectivity/esp_hosted_wifi_ap.cpp",
                    "platform/hmi/connectivity/esp_hosted_wifi_sta.cpp",
                    "platform/hmi/connectivity/preferences_wifi_station_settings_store.cpp",
                )
            )
    if "storage" in profile.data["modules"] and host == "windows":
        module_sources.append(
            "platform/windows/storage/virtual_storage_access.cpp"
        )
    if "time" in profile.data["modules"]:
        if host == "windows":
            module_sources.append(
                "platform/windows/time/virtual_time_sync_ports.cpp"
            )
        else:
            host_sources.append("platform/hmi/time/esp_network_time.cpp")
    if "audio" in profile.data["modules"]:
        if host == "windows":
            host_sources.append("platform/windows/audio/sdl_wav_playback.cpp")
        else:
            host_sources.append("platform/hmi/audio/board_wav_playback.cpp")
    board_sources: list[str] = []
    board_include_dirs: list[str] = []
    if host == "hmi":
        board_id = profile.data["hosts"]["hmi"]["board"]
        if board_id not in BOARD_INPUTS:
            raise GenerationError(
                "unsupported-board", f"generator has no input map for board '{board_id}'"
            )
        board_sources.extend(BOARD_INPUTS[board_id]["sources"])
        if "settings" in profile.data["modules"]:
            host_sources.append(
                "platform/hmi/settings/preferences_level_settings.cpp"
            )
        if "settings" in profile.data["modules"] or "audio" in profile.data["modules"]:
            board_sources.extend(
                (
                    "boards/jc1060p470c/audio.cpp",
                    "boards/jc1060p470c/internal/audio_output.cpp",
                    "boards/jc1060p470c/internal/audio_service.cpp",
                    "boards/jc1060p470c/internal/wav_reader.cpp",
                )
            )
        if profile.data["assets"]:
            board_sources.append(
                "boards/jc1060p470c/internal/asset_usb_updater.cpp"
            )
        if "storage" in profile.data["modules"]:
            host_sources.append(
                "platform/hmi/storage/board_storage_access.cpp"
            )
            board_sources.append(
                "boards/jc1060p470c/internal/usb_msc_driver.cpp"
            )
        board_include_dirs.extend(BOARD_INPUTS[board_id]["include_dirs"])

    all_sources = _unique(
        profile_sources + page_sources + module_sources + host_sources + board_sources
    )
    all_include_dirs = _unique(
        ["."]
        + profile_include_dirs
        + module_include_dirs
        + host_include_dirs
        + board_include_dirs
    )
    _require_files(repository_root, all_sources, "source")
    _require_directories(repository_root, all_include_dirs, "include")

    entry_header = _repository_path(
        profile.root / profile.data["entry"]["header"], repository_root
    )
    asset_files: list[Path] = []
    for asset_entry in profile.data["assets"]:
        source_path = profile.root / asset_entry
        if source_path.is_file():
            asset_files.append(source_path)
            continue
        if source_path.is_dir():
            asset_files.extend(path for path in source_path.rglob("*") if path.is_file())

    assets = []
    for source_path in sorted(asset_files, key=lambda path: path.as_posix()):
        relative_asset = source_path.relative_to(profile.root).as_posix()
        assets.append(
            {
                "source": _repository_path(source_path, repository_root),
                "relative_path": relative_asset,
                "size": source_path.stat().st_size,
                "sha256": hashlib.sha256(source_path.read_bytes()).hexdigest(),
            }
        )
    asset_root = (
        f"A:{_repository_path(profile.root, repository_root)}/"
        if host == "windows"
        else f"A:/sdcard/{profile.data['data']['hmi_sd_root']}/"
    )
    return {
        "schema_version": 1,
        "generator_version": GENERATOR_VERSION,
        "host": host,
        "profile": {
            "id": profile.id,
            "kind": profile.kind,
            "manifest": _repository_path(profile.manifest_path, repository_root),
            "root": _repository_path(profile.root, repository_root),
            "modules": list(profile.data["modules"]),
            "data": dict(profile.data["data"]),
        },
        "entry": {
            "header": entry_header,
            "type": profile.data["entry"]["type"],
        },
        "asset_root": asset_root,
        "asset_destination_root": profile.data["data"]["hmi_sd_root"],
        "assets": assets,
        "source_groups": {
            "profile": profile_sources,
            "pages": page_sources,
            "modules": module_sources,
            "host": host_sources,
            "board": board_sources,
        },
        "include_groups": {
            "profile": profile_include_dirs,
            "modules": module_include_dirs,
            "host": host_include_dirs,
            "board": board_include_dirs,
        },
        "all_sources": all_sources,
        "all_include_dirs": all_include_dirs,
        "pages": page_descriptors,
        "verification": profile.data["verification"],
    }


def input_hash(profile: Profile, host: str, resolved: dict) -> str:
    payload = {
        "generator_version": GENERATOR_VERSION,
        "host": host,
        "manifest": profile.data,
        "resolved": resolved,
    }
    canonical = json.dumps(
        payload, ensure_ascii=True, separators=(",", ":"), sort_keys=True
    ).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


def _json_text(value: dict) -> str:
    return json.dumps(value, indent=2, sort_keys=True) + "\n"


def _selected_header(resolved: dict) -> str:
    profile = resolved["profile"]
    entry = resolved["entry"]
    return (
        "// Generated by tools/project.py. Do not edit.\n"
        "#pragma once\n\n"
        '#include "profiles/runtime/app_contract.hpp"\n'
        f'#include "{entry["header"]}"\n\n'
        "namespace hmi::profiles::generated {\n\n"
        f"using SelectedProfileApp = {entry['type']};\n"
        f'inline constexpr char kSelectedProfileId[] = "{profile["id"]}";\n'
        f'inline constexpr char kSelectedHostId[] = "{resolved["host"]}";\n\n'
        f"inline constexpr bool kSelectedHasAssets = {str(bool(resolved['assets'])).lower()};\n"
        f"inline constexpr bool kSelectedHasConnectivity = "
        f"{str('connectivity' in profile['modules']).lower()};\n"
        f"inline constexpr bool kSelectedHasStorage = "
        f"{str('storage' in profile['modules']).lower()};\n"
        f'inline constexpr char kSelectedAssetRoot[] = "{resolved["asset_root"]}";\n\n'
        f'inline constexpr char kSelectedAssetStorageRoot[] = "/{resolved["asset_destination_root"]}";\n\n'
        f'inline constexpr char kSelectedNvsNamespace[] = "{resolved["profile"]["data"]["nvs_namespace"]}";\n'
        f"inline constexpr bool kSelectedHasSettings = "
        f"{str('settings' in resolved['profile']['modules']).lower()};\n\n"
        f"inline constexpr bool kSelectedHasAudio = "
        f"{str('audio' in resolved['profile']['modules']).lower()};\n\n"
        f"inline constexpr bool kSelectedHasTime = "
        f"{str('time' in resolved['profile']['modules']).lower()};\n\n"
        "}  // namespace hmi::profiles::generated\n"
    )


def _cmake_list(name: str, values: list[str]) -> str:
    lines = [f"set({name}"]
    lines.extend(f'  "${{HMI_REPOSITORY_ROOT}}/{value}"' for value in values)
    lines.append(")")
    return "\n".join(lines)


def _cmake_input(resolved: dict) -> str:
    groups = resolved["source_groups"]
    include_dirs = resolved["all_include_dirs"]
    profile_root = Path(resolved["profile"]["manifest"]).parent.as_posix()
    test_ids = resolved["verification"]["tests"]
    profile_sources = groups["profile"] + groups["pages"]
    profile_source_input = _cmake_list("HMI_PROFILE_SOURCES", profile_sources)
    if resolved["pages"]:
        profile_source_input += (
            '\nlist(APPEND HMI_PROFILE_SOURCES '
            '"${CMAKE_CURRENT_LIST_DIR}/page_registry.cpp")'
        )
    return "\n\n".join(
        [
            "# Generated by tools/project.py. Do not edit.\n"
            'get_filename_component(HMI_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)',
            f'set(HMI_PROFILE_ID "{resolved["profile"]["id"]}")',
            f'set(HMI_PROFILE_ROOT "${{HMI_REPOSITORY_ROOT}}/{profile_root}")',
            "set(HMI_PROFILE_HAS_AUDIO "
            + ("1" if "audio" in resolved["profile"]["modules"] else "0")
            + ")",
            "set(HMI_PROFILE_HAS_TIME "
            + ("1" if "time" in resolved["profile"]["modules"] else "0")
            + ")",
            "set(HMI_PROFILE_TEST_IDS\n"
            + "\n".join(f'  "{test_id}"' for test_id in test_ids)
            + "\n)",
            profile_source_input,
            _cmake_list("HMI_MODULE_SOURCES", groups["modules"]),
            _cmake_list("HMI_HOST_SOURCES", groups["host"]),
            _cmake_list("HMI_BOARD_SOURCES", groups["board"]),
            _cmake_list("HMI_INCLUDE_DIRS", include_dirs),
            'set(HMI_SELECTED_PROFILE_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}")',
        ]
    ) + "\n"


def _platformio_profile_input(
    resolved: dict,
    *,
    output_relative: str,
) -> str:
    sources = list(resolved["all_sources"])
    if resolved["pages"]:
        sources.append(f"{output_relative}/page_registry.cpp")
    source_filter = ["  -<*>"] + [
        f"  +<{source}>" for source in sources
    ]
    build_flags = [f"  -I {value}" for value in resolved["all_include_dirs"]]
    build_flags.append(f"  -I {output_relative}")
    build_flags.append(
        f"  -D HMI_PROFILE_HAS_ASSETS={1 if resolved['assets'] else 0}"
    )
    build_flags.append(
        "  -D HMI_PROFILE_HAS_SETTINGS="
        f"{1 if 'settings' in resolved['profile']['modules'] else 0}"
    )
    build_flags.append(
        "  -D HMI_PROFILE_HAS_AUDIO="
        f"{1 if 'audio' in resolved['profile']['modules'] else 0}"
    )
    build_flags.append(
        "  -D HMI_PROFILE_HAS_CONNECTIVITY="
        f"{1 if 'connectivity' in resolved['profile']['modules'] else 0}"
    )
    build_flags.append(
        "  -D HMI_PROFILE_HAS_STORAGE="
        f"{1 if 'storage' in resolved['profile']['modules'] else 0}"
    )
    build_flags.append(
        "  -D HMI_PROFILE_HAS_TIME="
        f"{1 if 'time' in resolved['profile']['modules'] else 0}"
    )
    environment = f"hmi-{resolved['profile']['id']}"
    return (
        "; Generated by tools/project.py. Do not edit.\n"
        f"[env:{environment}]\n"
        "build_src_filter =\n"
        + "\n".join(source_filter)
        + "\n"
        "build_flags =\n"
        "  ${env.build_flags}\n"
        + "\n".join(build_flags)
        + "\n"
    )


def _platformio_selection_input(
    resolved: dict,
    *,
    build_relative: str,
) -> str:
    environment = f"hmi-{resolved['profile']['id']}"
    return (
        "; Generated by tools/project.py. Do not edit.\n"
        "[platformio]\n"
        f"default_envs = {environment}\n"
        f"build_dir = {build_relative}\n"
    )


def _page_registry_header() -> str:
    return (
        "// Generated by tools/project.py. Do not edit.\n"
        "#pragma once\n\n"
        "#include <cstddef>\n\n"
        '#include "page_contract.hpp"\n\n'
        "namespace hmi::samples::suite::generated {\n\n"
        "const PageDescriptor *page_descriptors() noexcept;\n"
        "std::size_t page_descriptor_count() noexcept;\n\n"
        "}  // namespace hmi::samples::suite::generated\n"
    )


def _cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def _page_registry_source(resolved: dict) -> str:
    pages = resolved["pages"]
    includes = "\n".join(
        f'#include "{page["entry"]["header"]}"' for page in pages
    )
    factories = []
    descriptors = []
    for index, page in enumerate(pages):
        factories.append(
            f"SamplePage *create_page_{index}() noexcept {{\n"
            f"  return new (std::nothrow) {page['entry']['type']}();\n"
            "}"
        )
        descriptors.append(
            "    {"
            + ", ".join(
                (
                    _cpp_string(page["id"]),
                    _cpp_string(page["title_key"]),
                    _cpp_string(page["description_key"]),
                    str(page["order"]),
                    _cpp_string(page["data_key"]),
                    f"&create_page_{index}",
                )
            )
            + "},"
        )
    return (
        "// Generated by tools/project.py. Do not edit.\n"
        '#include "page_registry.hpp"\n\n'
        "#include <array>\n"
        "#include <new>\n\n"
        + includes
        + "\n\nnamespace hmi::samples::suite::generated {\n"
        "namespace {\n\n"
        + "\n\n".join(factories)
        + "\n\nconstexpr std::array<PageDescriptor, "
        + str(len(pages))
        + "> kPageDescriptors = {{\n"
        + "\n".join(descriptors)
        + "\n}};\n\n"
        "}  // namespace\n\n"
        "const PageDescriptor *page_descriptors() noexcept {\n"
        "  return kPageDescriptors.data();\n"
        "}\n\n"
        "std::size_t page_descriptor_count() noexcept {\n"
        "  return kPageDescriptors.size();\n"
        "}\n\n"
        "}  // namespace hmi::samples::suite::generated\n"
    )


def _platformio_wrapper(
    output_relative: str,
) -> str:
    return (
        "; Generated by tools/project.py. Do not edit.\n"
        "[platformio]\n"
        "src_dir = .\n"
        "extra_configs =\n"
        "  platformio.ini\n"
        f"  {output_relative}/selection.ini\n"
    )


def render_generated_files(
    repository_root: Path,
    profile: Profile,
    host: str,
    output_root: Path,
) -> dict[str, str]:
    resolved = resolve_profile_inputs(repository_root, profile, host)
    digest = input_hash(profile, host, resolved)
    resolved_with_hash = dict(resolved)
    resolved_with_hash["input_hash"] = digest
    metadata = {
        "schema_version": 1,
        "generator_version": GENERATOR_VERSION,
        "host": host,
        "profile_id": profile.id,
        "input_hash": digest,
    }
    output_directory = output_root / host / profile.id
    try:
        output_relative = output_directory.resolve().relative_to(
            repository_root.resolve()
        ).as_posix()
    except ValueError:
        output_relative = output_directory.as_posix()

    files = {
        "selected_profile.hpp": _selected_header(resolved),
        "resolved_profile.json": _json_text(resolved_with_hash),
        "metadata.json": _json_text(metadata),
        "asset_manifest.json": _json_text(
            {
                "schema_version": 1,
                "profile_id": profile.id,
                "destination_root": resolved["asset_destination_root"],
                "assets": resolved["assets"],
            }
        ),
    }
    if resolved["pages"]:
        files["page_registry.hpp"] = _page_registry_header()
        files["page_registry.cpp"] = _page_registry_source(resolved)
    if host == "windows":
        files["profile.cmake"] = _cmake_input(resolved)
    else:
        build_relative = f".pio/build/{host}/{profile.id}"
        files["profile.ini"] = _platformio_profile_input(
            resolved,
            output_relative=output_relative,
        )
        files["selection.ini"] = _platformio_selection_input(
            resolved,
            build_relative=build_relative,
        )
        files["platformio.ini"] = _platformio_wrapper(
            output_relative=output_relative,
        )
    return files


def _write_staging_file(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8", newline="\n")


def _replace_directory(target: Path, files: dict[str, str]) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    token = uuid.uuid4().hex
    staging = target.parent / f".{target.name}.staging-{token}"
    backup = target.parent / f".{target.name}.backup-{token}"
    staging.mkdir()
    try:
        for relative_path, content in sorted(files.items()):
            destination = staging / relative_path
            destination.parent.mkdir(parents=True, exist_ok=True)
            _write_staging_file(destination, content)

        had_previous = target.exists()
        if had_previous:
            target.replace(backup)
        try:
            staging.replace(target)
        except BaseException:
            if had_previous and backup.exists() and not target.exists():
                backup.replace(target)
            raise
        if backup.exists():
            shutil.rmtree(backup)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
        if backup.exists() and target.exists():
            shutil.rmtree(backup)


def prepare_profile(
    repository_root: Path,
    registry: ProfileRegistry,
    host: str,
    profile_id: str,
    *,
    output_root: Path | None = None,
) -> Path:
    repository_root = repository_root.resolve()
    profile = registry.get(profile_id)
    output_root = output_root or repository_root / ".generated"
    target = output_root / host / profile.id
    files = render_generated_files(
        repository_root, profile, host, output_root
    )
    _replace_directory(target, files)
    return target


def check_generated_isolation(
    repository_root: Path,
    registry: ProfileRegistry,
    *,
    output_root: Path | None = None,
) -> list[str]:
    repository_root = repository_root.resolve()
    output_root = output_root or repository_root / ".generated"
    registered_ids = {profile.id for profile in registry.profiles}
    checked: list[str] = []

    for profile in registry.profiles:
        check_page_source_isolation(repository_root, profile)

    if output_root.exists():
        for host_directory in output_root.iterdir():
            if not host_directory.is_dir() or host_directory.name not in SUPPORTED_HOSTS:
                continue
            for profile_directory in host_directory.iterdir():
                if profile_directory.is_dir() and profile_directory.name not in registered_ids:
                    raise GenerationError(
                        "stale-generated-profile",
                        f"generated profile '{profile_directory.name}' is no longer registered",
                    )

    forbidden_by_host = {
        "windows": ("hosts/hmi/", "platform/hmi/", "boards/"),
        "hmi": ("hosts/windows/", "platform/windows/"),
    }
    for host in SUPPORTED_HOSTS:
        for profile in registry.profiles:
            target = output_root / host / profile.id
            if not target.is_dir():
                continue
            metadata_path = target / "metadata.json"
            resolved_path = target / "resolved_profile.json"
            try:
                metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
                resolved = json.loads(resolved_path.read_text(encoding="utf-8"))
            except (OSError, UnicodeError, json.JSONDecodeError) as error:
                raise GenerationError(
                    "invalid-generated-output", f"cannot read '{target}': {error}"
                ) from error

            expected_resolved = resolve_profile_inputs(repository_root, profile, host)
            expected_hash = input_hash(profile, host, expected_resolved)
            if metadata.get("input_hash") != expected_hash:
                raise GenerationError(
                    "stale-generated-output",
                    f"generated input for {host}/{profile.id} does not match its manifest",
                )
            if resolved.get("input_hash") != expected_hash:
                raise GenerationError(
                    "stale-generated-output",
                    f"resolved input for {host}/{profile.id} does not match its manifest",
                )
            if resolved.get("all_sources") != expected_resolved["all_sources"]:
                raise GenerationError(
                    "modified-generated-output",
                    f"source list for {host}/{profile.id} was modified",
                )
            for source in resolved["all_sources"]:
                if any(source.startswith(prefix) for prefix in forbidden_by_host[host]):
                    raise GenerationError(
                        "host-source-leak",
                        f"{host}/{profile.id} contains forbidden source '{source}'",
                    )
            checked.append(f"{host}/{profile.id}")
    return checked
