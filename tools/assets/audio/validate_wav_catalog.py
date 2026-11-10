"""고정 WAV catalog의 경로, RIFF/PCM header, 크기와 hash를 검증한다."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath


SUPPORTED_SAMPLE_RATES = {8000, 16000, 22050, 32000, 44100, 48000}
SUPPORTED_BITS = {8, 16, 24}
ID_PATTERN = re.compile(r"^[a-z][a-z0-9_]{0,47}$")
TEXT_KEY_PATTERN = re.compile(r"^[a-z][a-z0-9_.]{0,95}$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")


class WavCatalogError(RuntimeError):
    """catalog 또는 WAV가 등록 계약을 위반한다."""


@dataclass(frozen=True)
class WavInfo:
    file_bytes: int
    data_bytes: int
    channels: int
    sample_rate_hz: int
    bits_per_sample: int
    duration_ms: int


@dataclass(frozen=True)
class CatalogEntry:
    identifier: str
    asset_path: str
    title_key: str
    description_key: str
    source_path: str
    source_authorization: str
    sha256: str
    info: WavInfo


@dataclass(frozen=True)
class Catalog:
    maximum: int
    generated_header: Path
    entries: tuple[CatalogEntry, ...]


def _fail(path: str, message: str) -> None:
    raise WavCatalogError(f"{path}: {message}")


def _exact_keys(value: dict[str, object], expected: set[str], path: str) -> None:
    actual = set(value)
    if actual != expected:
        _fail(path, f"expected keys {sorted(expected)}, got {sorted(actual)}")


def _integer(value: object, path: str, minimum: int = 0) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < minimum:
        _fail(path, f"expected integer >= {minimum}")
    return value


def _string(value: object, path: str) -> str:
    if not isinstance(value, str) or not value:
        _fail(path, "expected non-empty string")
    return value


def _relative_path(value: object, path: str) -> str:
    raw = _string(value, path)
    if "\\" in raw:
        _fail(path, "use '/' separators")
    parsed = PurePosixPath(raw)
    if parsed.is_absolute() or any(part in ("", ".", "..") for part in parsed.parts):
        _fail(path, "must be a normalized relative path without '..'")
    return parsed.as_posix()


def _resolve_under(root: Path, relative: str, path: str) -> Path:
    resolved = root.joinpath(*PurePosixPath(relative).parts).resolve()
    try:
        resolved.relative_to(root)
    except ValueError:
        _fail(path, "resolved path escapes its owner")
    return resolved


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(64 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inspect_wav(path: Path) -> WavInfo:
    """HMI reader와 같은 PCM allowlist로 WAV chunk 범위를 검사한다."""
    try:
        data = path.read_bytes()
    except OSError as error:
        raise WavCatalogError(f"{path}: cannot read WAV: {error}") from error
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise WavCatalogError(f"{path}: invalid RIFF/WAVE header")
    riff_end = struct.unpack_from("<I", data, 4)[0] + 8
    if riff_end < 12 or riff_end > len(data):
        raise WavCatalogError(f"{path}: truncated RIFF payload")

    audio_format: tuple[int, int, int, int] | None = None
    data_bytes: int | None = None
    offset = 12
    while offset < riff_end:
        if riff_end - offset < 8:
            raise WavCatalogError(f"{path}: truncated chunk header")
        tag = data[offset : offset + 4]
        length = struct.unpack_from("<I", data, offset + 4)[0]
        body = offset + 8
        end = body + length
        padded = end + (length & 1)
        if end > riff_end or padded > riff_end:
            raise WavCatalogError(f"{path}: truncated chunk body")
        if tag == b"fmt " and audio_format is None:
            if length < 16:
                raise WavCatalogError(f"{path}: truncated fmt chunk")
            encoding, channels, rate, byte_rate, alignment, bits = struct.unpack_from(
                "<HHIIHH", data, body
            )
            frame_bytes = channels * (bits // 8)
            if encoding != 1:
                raise WavCatalogError(f"{path}: unsupported encoding {encoding}")
            if channels not in (1, 2):
                raise WavCatalogError(f"{path}: unsupported channels {channels}")
            if rate not in SUPPORTED_SAMPLE_RATES:
                raise WavCatalogError(f"{path}: unsupported sample rate {rate}")
            if bits not in SUPPORTED_BITS:
                raise WavCatalogError(f"{path}: unsupported bit depth {bits}")
            if alignment != frame_bytes or byte_rate != rate * alignment:
                raise WavCatalogError(f"{path}: inconsistent PCM layout")
            audio_format = (channels, rate, bits, frame_bytes)
        elif tag == b"data":
            if data_bytes is not None:
                raise WavCatalogError(f"{path}: multiple data chunks are unsupported")
            data_bytes = length
        offset = padded

    if audio_format is None or data_bytes is None or data_bytes == 0:
        raise WavCatalogError(f"{path}: missing fmt or non-empty data chunk")
    channels, rate, bits, frame_bytes = audio_format
    if data_bytes % frame_bytes != 0:
        raise WavCatalogError(f"{path}: data is not frame aligned")
    duration_ms = data_bytes * 1000 // (rate * frame_bytes)
    return WavInfo(len(data), data_bytes, channels, rate, bits, duration_ms)


def load_catalog(repository_root: Path, config_path: Path) -> Catalog:
    root = repository_root.resolve()
    config = config_path.resolve()
    try:
        config.relative_to(root)
    except ValueError:
        raise WavCatalogError("config must be inside the repository") from None
    try:
        raw = json.loads(config.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise WavCatalogError(f"{config}: cannot load catalog: {error}") from error
    if not isinstance(raw, dict):
        _fail("catalog", "expected object")
    _exact_keys(
        raw,
        {"schema_version", "owner_root", "generated_header", "max_entries", "entries"},
        "catalog",
    )
    if raw["schema_version"] != 1:
        _fail("catalog.schema_version", "expected 1")
    owner_relative = _relative_path(raw["owner_root"], "catalog.owner_root")
    header_relative = _relative_path(raw["generated_header"], "catalog.generated_header")
    owner_root = _resolve_under(root, owner_relative, "catalog.owner_root")
    generated_header = _resolve_under(root, header_relative, "catalog.generated_header")
    maximum = _integer(raw["max_entries"], "catalog.max_entries", 1)
    if maximum > 32:
        _fail("catalog.max_entries", "must be <= 32")
    raw_entries = raw["entries"]
    if not isinstance(raw_entries, list) or not 0 < len(raw_entries) <= maximum:
        _fail("catalog.entries", f"expected 1..{maximum} entries")

    entries: list[CatalogEntry] = []
    identifiers: set[str] = set()
    asset_paths: set[str] = set()
    for index, value in enumerate(raw_entries):
        entry_path = f"catalog.entries[{index}]"
        if not isinstance(value, dict):
            _fail(entry_path, "expected object")
        _exact_keys(
            value,
            {
                "id",
                "asset_path",
                "title_key",
                "description_key",
                "source_path",
                "source_authorization",
                "file_bytes",
                "sha256",
                "format",
            },
            entry_path,
        )
        identifier = _string(value["id"], f"{entry_path}.id")
        if ID_PATTERN.fullmatch(identifier) is None or identifier in identifiers:
            _fail(f"{entry_path}.id", "invalid or duplicate identifier")
        identifiers.add(identifier)
        asset_path = _relative_path(value["asset_path"], f"{entry_path}.asset_path")
        if not asset_path.lower().endswith(".wav") or asset_path in asset_paths:
            _fail(f"{entry_path}.asset_path", "expected unique .wav path")
        asset_paths.add(asset_path)
        title_key = _string(value["title_key"], f"{entry_path}.title_key")
        description_key = _string(value["description_key"], f"{entry_path}.description_key")
        if TEXT_KEY_PATTERN.fullmatch(title_key) is None:
            _fail(f"{entry_path}.title_key", "invalid catalog text key")
        if TEXT_KEY_PATTERN.fullmatch(description_key) is None:
            _fail(f"{entry_path}.description_key", "invalid catalog text key")
        source_path = _string(value["source_path"], f"{entry_path}.source_path")
        authorization = _string(
            value["source_authorization"], f"{entry_path}.source_authorization"
        )
        expected_bytes = _integer(value["file_bytes"], f"{entry_path}.file_bytes", 1)
        expected_hash = _string(value["sha256"], f"{entry_path}.sha256").lower()
        if SHA256_PATTERN.fullmatch(expected_hash) is None:
            _fail(f"{entry_path}.sha256", "expected lowercase SHA-256")
        raw_format = value["format"]
        if not isinstance(raw_format, dict):
            _fail(f"{entry_path}.format", "expected object")
        _exact_keys(
            raw_format,
            {"encoding", "channels", "sample_rate_hz", "bits_per_sample", "duration_ms"},
            f"{entry_path}.format",
        )
        if raw_format["encoding"] != "pcm":
            _fail(f"{entry_path}.format.encoding", "expected pcm")
        asset = _resolve_under(owner_root, asset_path, f"{entry_path}.asset_path")
        info = inspect_wav(asset)
        expected_info = WavInfo(
            expected_bytes,
            info.data_bytes,
            _integer(raw_format["channels"], f"{entry_path}.format.channels", 1),
            _integer(raw_format["sample_rate_hz"], f"{entry_path}.format.sample_rate_hz", 1),
            _integer(raw_format["bits_per_sample"], f"{entry_path}.format.bits_per_sample", 1),
            _integer(raw_format["duration_ms"], f"{entry_path}.format.duration_ms", 1),
        )
        if info.file_bytes != expected_info.file_bytes:
            _fail(f"{entry_path}.file_bytes", f"expected {info.file_bytes}")
        if (
            info.channels != expected_info.channels
            or info.sample_rate_hz != expected_info.sample_rate_hz
            or info.bits_per_sample != expected_info.bits_per_sample
            or info.duration_ms != expected_info.duration_ms
        ):
            _fail(f"{entry_path}.format", f"actual format is {info}")
        actual_hash = _sha256(asset)
        if actual_hash != expected_hash:
            _fail(f"{entry_path}.sha256", f"actual hash is {actual_hash}")
        entries.append(
            CatalogEntry(
                identifier,
                asset_path,
                title_key,
                description_key,
                source_path,
                authorization,
                expected_hash,
                info,
            )
        )
    return Catalog(maximum, generated_header, tuple(entries))


def render_header(catalog: Catalog) -> str:
    rows = []
    for entry in catalog.entries:
        rows.append(
            "    {"
            + ", ".join(
                [
                    json.dumps(entry.identifier),
                    json.dumps(entry.asset_path),
                    json.dumps(entry.title_key),
                    json.dumps(entry.description_key),
                    f"{entry.info.file_bytes}U",
                    json.dumps(entry.sha256),
                    f"{entry.info.channels}U",
                    f"{entry.info.sample_rate_hz}U",
                    f"{entry.info.bits_per_sample}U",
                    f"{entry.info.duration_ms}U",
                ]
            )
            + "},"
        )
    return "\n".join(
        [
            "// Generated by tools/assets/audio/validate_wav_catalog.py. Do not edit.",
            "#pragma once",
            "",
            "#include <array>",
            "#include <cstddef>",
            "#include <cstdint>",
            "",
            "namespace hmi::samples::suite::generated {",
            "",
            "struct WavCatalogEntry {",
            "  const char* id;",
            "  const char* asset_path;",
            "  const char* title_key;",
            "  const char* description_key;",
            "  std::uint32_t file_bytes;",
            "  const char* sha256;",
            "  std::uint16_t channels;",
            "  std::uint32_t sample_rate_hz;",
            "  std::uint16_t bits_per_sample;",
            "  std::uint32_t duration_ms;",
            "};",
            "",
            f"inline constexpr std::size_t kMaxRegisteredSounds = {catalog.maximum}U;",
            f"inline constexpr std::array<WavCatalogEntry, {len(catalog.entries)}U> kWavCatalog = {{{{",
            *rows,
            "}};",
            "static_assert(kWavCatalog.size() <= kMaxRegisteredSounds);",
            "",
            "}  // namespace hmi::samples::suite::generated",
            "",
        ]
    )


def generate(repository_root: Path, config_path: Path, check: bool) -> Catalog:
    catalog = load_catalog(repository_root, config_path)
    rendered = render_header(catalog)
    if check:
        try:
            current = catalog.generated_header.read_text(encoding="utf-8")
        except OSError as error:
            raise WavCatalogError(
                f"generated header is missing: {catalog.generated_header}"
            ) from error
        if current != rendered:
            raise WavCatalogError(
                f"generated header is stale: {catalog.generated_header}"
            )
    else:
        catalog.generated_header.parent.mkdir(parents=True, exist_ok=True)
        catalog.generated_header.write_text(rendered, encoding="utf-8", newline="\n")
    return catalog


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository-root", type=Path, default=Path.cwd())
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    try:
        catalog = generate(
            args.repository_root.resolve(),
            (args.repository_root / args.config).resolve()
            if not args.config.is_absolute()
            else args.config,
            args.check,
        )
        verb = "checked" if args.check else "generated"
        print(f"{verb} {len(catalog.entries)} WAV entries: {catalog.generated_header}")
        return 0
    except WavCatalogError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

