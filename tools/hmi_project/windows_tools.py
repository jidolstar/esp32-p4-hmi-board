"""Resolve and provision the Windows simulator's build tools."""

from __future__ import annotations

import hashlib
import os
import platform
import shutil
import sys
import tempfile
import time
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ToolArchive:
    name: str
    directory: str
    release: str
    sha256: str
    bin_directory: str
    commands: tuple[str, ...]

    def available(self, path: str) -> bool:
        return all(shutil.which(command, path=path) for command in self.commands)

    def installed(self, directory: Path) -> bool:
        return all((directory / self.bin_directory / command).is_file() for command in self.commands)


CMAKE = ToolArchive(
    "CMake",
    "cmake-3.31.10",
    "Kitware/CMake/releases/download/v3.31.10/cmake-3.31.10-windows-x86_64.zip",
    "13d1a463d7130df5339baedd63d8ae990aaf385062b2f42f372796143ae94086",
    "cmake-3.31.10-windows-x86_64/bin",
    ("cmake.exe", "ctest.exe"),
)
NINJA = ToolArchive(
    "Ninja",
    "ninja-1.13.2",
    "ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip",
    "07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65",
    ".",
    ("ninja.exe",),
)
GIT = ToolArchive(
    "Git",
    "mingit-2.56.1",
    "git-for-windows/git/releases/download/v2.56.1.windows.1/MinGit-2.56.1-64-bit.zip",
    "4a92850b6e917da4fd010669b614b55dccf5b0ec333a534cc4be4700dfefdafd",
    "cmd",
    ("git.exe",),
)
GCC = ToolArchive(
    "GCC (WinLibs)",
    "winlibs-16.1.0-r3",
    "brechtsanders/winlibs_mingw/releases/download/16.1.0posix-14.0.0-ucrt-r3/"
    "winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r3.zip",
    "4273565109cd8ab8ecef1b0dc2a56fd7f5c7ee0885840a1c011b9325160ec0c3",
    "mingw64/bin",
    ("gcc.exe", "g++.exe"),
)


class WindowsToolError(RuntimeError):
    """The Windows build environment could not be provisioned."""


def _install(tool: ToolArchive, cache: Path) -> None:
    destination = cache / tool.directory
    if destination.exists():
        if tool.installed(destination):
            return
        raise WindowsToolError(
            f"Incomplete {tool.name} cache at {destination}. Remove that directory and retry."
        )
    print(f"Downloading {tool.name} for the Windows simulator...", flush=True)
    try:
        cache.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="setup-", dir=cache) as temporary:
            staging = Path(temporary)
            archive = staging / "tool.zip"
            with urllib.request.urlopen(f"https://github.com/{tool.release}", timeout=60) as response:
                with archive.open("wb") as target:
                    shutil.copyfileobj(response, target, length=1024 * 1024)
            with archive.open("rb") as source:
                if hashlib.file_digest(source, "sha256").hexdigest() != tool.sha256:
                    raise WindowsToolError(f"SHA-256 mismatch for {tool.name} download")
            print(f"Extracting {tool.name}...", flush=True)
            payload = staging / "files"
            with zipfile.ZipFile(archive) as package:
                package.extractall(payload)
            if not tool.installed(payload):
                raise WindowsToolError(f"Expected executables are missing from the {tool.name} archive")
            _publish(tool, payload, destination)
    except (OSError, zipfile.BadZipFile) as error:
        raise WindowsToolError(f"Could not prepare {tool.name}: {error}") from error
    print(f"Ready: {tool.name}", flush=True)


def _publish(tool: ToolArchive, payload: Path, destination: Path) -> None:
    for delay in (0.25, 0.5, 1, 2, 4, None):
        try:
            payload.rename(destination)
            return
        except (FileExistsError, PermissionError):
            if tool.installed(destination):
                return
            if delay is None:
                raise
            # Windows scanners can briefly hold executables after extraction.
            time.sleep(delay)


def compiler_available(environment: dict[str, str]) -> bool:
    path = environment.get("PATH", "")
    return bool(
        environment.get("CC")
        or environment.get("CXX")
        or shutil.which("cl", path=path)
        or all(shutil.which(command, path=path) for command in ("gcc", "g++"))
        or all(shutil.which(command, path=path) for command in ("clang", "clang++"))
    )


def _tool_cache(repository_root: Path) -> Path:
    return repository_root / ".local" / "windows-tools"


def uses_bundled_gcc(repository_root: Path, environment: dict[str, str]) -> bool:
    compiler = _tool_cache(repository_root) / GCC.directory / GCC.bin_directory / "gcc.exe"
    return environment.get("CC") == str(compiler)


def windows_environment(repository_root: Path, *, download: bool = False) -> dict[str, str]:
    environment = os.environ.copy()
    if sys.platform != "win32":
        return environment

    cache = _tool_cache(repository_root)
    path = environment.get("PATH", "")
    system_compiler = compiler_available(environment)
    for tool in (CMAKE, NINJA, GIT, GCC):
        directory = cache / tool.directory
        available = system_compiler if tool is GCC else tool.available(path)
        if download and not available and not tool.installed(directory):
            if platform.machine().upper() not in {"AMD64", "X86_64"}:
                raise WindowsToolError(f"Install {tool.name} manually on this Windows architecture")
            _install(tool, cache)
        if tool.installed(directory):
            binaries = directory / tool.bin_directory
            path = os.pathsep.join((path, str(binaries)))
            if tool is GCC and not system_compiler:
                # Select the same toolchain for both languages on the first CMake configure.
                environment["CC"] = str(binaries / "gcc.exe")
                environment["CXX"] = str(binaries / "g++.exe")
    environment["PATH"] = path
    return environment
