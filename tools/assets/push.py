"""검증한 profile 자산을 실행 중인 HMI의 SD staging으로 전송한다."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import time
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import BinaryIO

import serial
from serial.tools import list_ports


CHUNK_BYTES = 96
WINDOW_CHUNKS = 16
MAX_FILES = 512
MAX_FILE_BYTES = 16 * 1024 * 1024
MAX_BUNDLE_BYTES = 64 * 1024 * 1024


class AssetPushError(RuntimeError):
    """로컬 묶음 또는 HMI 자산 전송 계약이 맞지 않는다."""


@dataclass(frozen=True)
class AssetFile:
    relative_path: str
    path: Path
    size: int
    sha256: str


@dataclass(frozen=True)
class AssetBundle:
    profile_id: str
    root: Path
    files: tuple[AssetFile, ...]
    manifest_path: Path
    manifest_bytes: bytes
    bundle_id: str

    @property
    def total_bytes(self) -> int:
        return sum(item.size for item in self.files) + len(self.manifest_bytes)


class RetryWindow(Exception):
    def __init__(self, offset: int) -> None:
        super().__init__(f"HMI requested retry from {offset}")
        self.offset = offset


def _checked_relative(value: object) -> str:
    if not isinstance(value, str) or not value or len(value) >= 120:
        raise AssetPushError(f"invalid asset path: {value!r}")
    path = PurePosixPath(value)
    if (
        path.is_absolute()
        or ".." in value
        or any(part in ("", ".", "..") for part in path.parts)
    ):
        raise AssetPushError(f"unsafe asset path: {value}")
    if path.suffix.lower() not in {".bin", ".json", ".png", ".svg", ".wav"}:
        raise AssetPushError(f"unsupported asset type: {value}")
    return path.as_posix()


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(64 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_bundle(repository_root: Path, profile_id: str) -> AssetBundle:
    root = repository_root / "dist" / "sd" / "hmi" / profile_id
    manifest_path = root / "asset_manifest.json"
    if not manifest_path.is_file():
        raise AssetPushError(
            f"asset bundle is missing: {manifest_path.relative_to(repository_root)}"
        )
    manifest_bytes = manifest_path.read_bytes()
    try:
        manifest = json.loads(manifest_bytes)
    except json.JSONDecodeError as error:
        raise AssetPushError(f"invalid asset manifest: {error}") from error
    if (
        manifest.get("schema_version") != 1
        or manifest.get("profile_id") != profile_id
        or manifest.get("destination_root") != f"hmi/{profile_id}"
        or not isinstance(manifest.get("assets"), list)
    ):
        raise AssetPushError("asset manifest profile or destination does not match")
    if not 0 < len(manifest["assets"]) <= MAX_FILES:
        raise AssetPushError("asset bundle file count is outside the supported range")

    files: list[AssetFile] = []
    total = len(manifest_bytes)
    seen: set[str] = set()
    for raw in manifest["assets"]:
        if not isinstance(raw, dict):
            raise AssetPushError("asset manifest contains a non-object entry")
        relative = _checked_relative(raw.get("relative_path"))
        if relative == "asset_manifest.json" or relative in seen:
            raise AssetPushError(f"duplicate or reserved asset path: {relative}")
        seen.add(relative)
        path = root.joinpath(*PurePosixPath(relative).parts)
        if not path.is_file() or path.is_symlink():
            raise AssetPushError(f"asset file is missing or linked: {relative}")
        size = path.stat().st_size
        digest = _sha256(path)
        if (
            not 0 < size <= MAX_FILE_BYTES
            or raw.get("size") != size
            or raw.get("sha256") != digest
        ):
            raise AssetPushError(f"asset size/hash mismatch: {relative}")
        total += size
        files.append(AssetFile(relative, path, size, digest))
    if total > MAX_BUNDLE_BYTES:
        raise AssetPushError("asset bundle exceeds the supported size")
    files.sort(key=lambda item: item.relative_path)
    return AssetBundle(
        profile_id=profile_id,
        root=root,
        files=tuple(files),
        manifest_path=manifest_path,
        manifest_bytes=manifest_bytes,
        bundle_id=hashlib.sha256(manifest_bytes).hexdigest(),
    )


def _send_line(connection: serial.Serial, value: str) -> None:
    connection.write((value + "\n").encode("ascii"))
    connection.flush()


def _response(
    connection: serial.Serial, expected: str, timeout: float = 10.0
) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = connection.readline().decode("ascii", errors="replace").strip()
        if not line:
            continue
        if line.startswith("ASSET_ERROR ") or line == "ASSET_BUSY":
            raise AssetPushError(f"HMI rejected asset transfer: {line}")
        if line.startswith("ASSET_RETRY "):
            try:
                raise RetryWindow(int(line.split()[1]))
            except (IndexError, ValueError) as error:
                raise AssetPushError(f"invalid retry response: {line}") from error
        if line == expected or line.startswith(expected + " "):
            return line
    raise AssetPushError(f"HMI did not answer {expected}")


def _handshake(connection: serial.Serial, profile_id: str) -> None:
    connection.reset_input_buffer()
    for _ in range(5):
        _send_line(connection, f"ASSET_HELLO 2 {profile_id}")
        try:
            response = _response(connection, "ASSET_V2", 2.0)
        except AssetPushError as error:
            if "did not answer" not in str(error):
                raise
            continue
        if response != f"ASSET_V2 {profile_id}":
            raise AssetPushError(f"unexpected HMI profile response: {response}")
        return
    raise AssetPushError(
        "HMI asset protocol did not answer. The selected port has no active asset "
        "receiver; verify that hmi-sample-suite was rebuilt after the updater change "
        "and close any serial monitor."
    )


def _set_preparation(
    connection: serial.Serial, profile_id: str, stage: str
) -> None:
    if stage not in {"fonts", "assets"}:
        raise AssetPushError(f"unsupported preparation stage: {stage}")
    _handshake(connection, profile_id)
    _send_line(connection, f"ASSET_PREPARE {stage}")
    response = _response(connection, "ASSET_PREPARING")
    if response != f"ASSET_PREPARING {stage}":
        raise AssetPushError(f"unexpected preparation response: {response}")


def _cancel_preparation(connection: serial.Serial, profile_id: str) -> None:
    _handshake(connection, profile_id)
    _send_line(connection, "ASSET_CANCEL")
    _response(connection, "ASSET_CANCELLED")


def set_preparation(port: str, profile_id: str, stage: str,
                    baud: int = 115200) -> None:
    """전송 전 PC 작업도 HMI 진행 화면에 나타나도록 준비 단계를 알린다."""
    try:
        connection = serial.Serial(
            port, baudrate=baud, timeout=0.25, write_timeout=10
        )
    except serial.SerialException as error:
        raise AssetPushError(f"cannot open {port}: {error}") from error
    with connection:
        time.sleep(0.5)
        _set_preparation(connection, profile_id, stage)


def cancel_preparation(port: str, profile_id: str,
                       baud: int = 115200) -> None:
    """PC 전처리 실패를 HMI에 알려 준비 화면이 무기한 남지 않게 한다."""
    try:
        connection = serial.Serial(
            port, baudrate=baud, timeout=0.25, write_timeout=10
        )
    except serial.SerialException as error:
        raise AssetPushError(f"cannot open {port}: {error}") from error
    with connection:
        time.sleep(0.5)
        _cancel_preparation(connection, profile_id)


def _upload_stream(
    connection: serial.Serial,
    relative: str,
    stream: BinaryIO,
    size: int,
    expected_sha: str,
) -> None:
    _send_line(connection, f"ASSET_FILE {relative} {size} {expected_sha}")
    _response(connection, "ASSET_FILE_READY")
    offset = 0
    while offset < size:
        window: list[bytes] = []
        next_offset = offset
        for _ in range(WINDOW_CHUNKS):
            data = stream.read(min(CHUNK_BYTES, size - next_offset))
            if not data:
                break
            window.append(data)
            next_offset += len(data)
            if next_offset == size:
                break
        if not window:
            raise AssetPushError(f"source ended early: {relative}")
        for attempt in range(3):
            current = offset
            for data in window:
                encoded = base64.b64encode(data).decode("ascii")
                _send_line(connection, f"ASSET_DATA {current} {encoded}")
                current += len(data)
            try:
                ack = _response(connection, "ASSET_ACK", 30.0)
                if int(ack.split()[1]) != next_offset:
                    raise AssetPushError(f"wrong HMI checkpoint: {ack}")
                break
            except (AssetPushError, RetryWindow) as error:
                if isinstance(error, AssetPushError) and "did not answer" not in str(error):
                    raise
                if isinstance(error, RetryWindow) and error.offset != offset:
                    raise AssetPushError(
                        f"HMI requested unexpected offset {error.offset}, expected {offset}"
                    ) from error
                if attempt == 2:
                    raise AssetPushError(
                        f"{relative} offset {offset}: three transfer attempts failed"
                    ) from error
                _send_line(connection, f"ASSET_REWIND {offset}")
                rewound = _response(connection, "ASSET_REWOUND")
                if int(rewound.split()[1]) != offset:
                    raise AssetPushError(f"wrong rewind checkpoint: {rewound}")
        offset = next_offset
    _send_line(connection, "ASSET_END_FILE")
    _response(connection, "ASSET_FILE_OK", 180.0)


def push_bundle(bundle: AssetBundle, port: str, baud: int = 115200) -> None:
    try:
        connection = serial.Serial(port, baudrate=baud, timeout=0.25, write_timeout=10)
    except serial.SerialException as error:
        raise AssetPushError(f"cannot open {port}: {error}") from error
    with connection:
        time.sleep(0.5)
        _handshake(connection, bundle.profile_id)
        _send_line(
            connection,
            f"ASSET_BEGIN {bundle.bundle_id} {len(bundle.files) + 1} "
            f"{bundle.total_bytes}",
        )
        _response(connection, "ASSET_READY")
        try:
            for index, item in enumerate(bundle.files, 1):
                with item.path.open("rb") as stream:
                    _upload_stream(
                        connection,
                        item.relative_path,
                        stream,
                        item.size,
                        item.sha256,
                    )
                print(f"[{index}/{len(bundle.files) + 1}] verified: {item.relative_path}")
            import io

            manifest_sha = hashlib.sha256(bundle.manifest_bytes).hexdigest()
            _upload_stream(
                connection,
                "asset_manifest.json",
                io.BytesIO(bundle.manifest_bytes),
                len(bundle.manifest_bytes),
                manifest_sha,
            )
            print(
                f"[{len(bundle.files) + 1}/{len(bundle.files) + 1}] "
                "verified: asset_manifest.json"
            )
            _send_line(connection, "ASSET_COMMIT")
            _response(connection, "ASSET_COMPLETE", 180.0)
        except BaseException:
            try:
                _send_line(connection, "ASSET_ABORT")
            except (OSError, serial.SerialException):
                pass
            raise


def available_ports() -> list[tuple[str, str]]:
    return [(item.device, item.description) for item in list_ports.comports()]


def describe_bundle(bundle: AssetBundle) -> None:
    print(f"Profile: {bundle.profile_id}")
    print(f"Bundle: dist/sd/hmi/{bundle.profile_id}")
    print(f"Files: {len(bundle.files) + 1}")
    print(f"Bytes: {bundle.total_bytes}")
    for item in bundle.files:
        print(f"  {item.relative_path} ({item.size} bytes)")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Push profile assets to HMI SD over USB CDC")
    parser.add_argument("--repository-root", type=Path, default=Path.cwd())
    parser.add_argument("--profile", default="sample-suite")
    parser.add_argument("--port")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args(argv)
    if args.list_ports:
        for device, description in available_ports():
            print(f"{device}: {description}")
        return 0
    try:
        bundle = load_bundle(args.repository_root.resolve(), args.profile)
        describe_bundle(bundle)
        if args.dry_run:
            print("Dry run complete; HMI was not changed.")
            return 0
        if not args.port:
            raise AssetPushError("--port is required unless --dry-run or --list-ports is used")
        push_bundle(bundle, args.port)
        print("Asset update complete. Reopen Text Input to load the new fonts.")
        return 0
    except AssetPushError as error:
        print(f"error: {error}")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
