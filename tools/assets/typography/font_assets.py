"""Deterministic LVGL font asset generation shared by fixed and user fonts."""

from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


class FontAssetError(ValueError):
    """A deterministic font configuration or generation failure."""

    def __init__(self, code: str, path: str, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.path = path
        self.message = message

    def __str__(self) -> str:
        return f"[{self.code}] {self.path}: {self.message}"


@dataclass(frozen=True)
class SourceFont:
    weight: str
    path: Path
    repository_path: str
    sha256: str


@dataclass(frozen=True)
class FixedStyle:
    id: str
    size: int
    weight: str
    roles: tuple[str, ...]
    extra_characters: str


@dataclass(frozen=True)
class UserAsset:
    id: str
    ranges: tuple[tuple[int, int], ...]
    range_arguments: tuple[str, ...]


@dataclass(frozen=True)
class FontConfig:
    path: Path
    sources: dict[str, SourceFont]
    fixed_bpp: int
    fixed_styles: tuple[FixedStyle, ...]
    user_size: int
    user_bpp: int
    user_cache_capacity: int
    replacement_codepoint: int
    user_assets: tuple[UserAsset, ...]


def _fail(code: str, path: str, message: str) -> None:
    raise FontAssetError(code, path, message)


def _exact_keys(value: dict[str, Any], expected: set[str], path: str) -> None:
    missing = sorted(expected - value.keys())
    extra = sorted(value.keys() - expected)
    if missing or extra:
        _fail("invalid-fields", path, f"missing={missing}, extra={extra}")


def _mapping(value: Any, path: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        _fail("invalid-type", path, "expected an object")
    return value


def _string(value: Any, path: str) -> str:
    if not isinstance(value, str) or not value:
        _fail("invalid-type", path, "expected a non-empty string")
    return value


def _integer(value: Any, path: str, minimum: int, maximum: int) -> int:
    if type(value) is not int or value < minimum or value > maximum:
        _fail("invalid-value", path, f"expected {minimum}..{maximum}")
    return value


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _parse_codepoint(value: str, path: str) -> int:
    try:
        result = int(value, 16) if value.lower().startswith("0x") else int(value)
    except ValueError:
        _fail("invalid-codepoint", path, f"invalid codepoint '{value}'")
    if result < 0 or result > 0x10FFFF or 0xD800 <= result <= 0xDFFF:
        _fail("invalid-codepoint", path, f"invalid Unicode scalar '{value}'")
    return result


def _parse_range(value: str, path: str) -> tuple[int, int]:
    parts = value.split("-", 1)
    first = _parse_codepoint(parts[0], path)
    last = _parse_codepoint(parts[1], path) if len(parts) == 2 else first
    if first > last:
        _fail("invalid-range", path, f"range start exceeds end in '{value}'")
    return first, last


def load_font_config(repository_root: Path, path: Path) -> FontConfig:
    path = path.resolve()
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        _fail("read-error", path.as_posix(), str(error))
    root = _mapping(data, path.as_posix())
    _exact_keys(root, {"schema_version", "sources", "fixed", "user"}, path.as_posix())
    if root["schema_version"] != 1:
        _fail("unsupported-version", path.as_posix(), "expected schema version 1")

    sources_value = _mapping(root["sources"], f"{path.as_posix()}.sources")
    _exact_keys(sources_value, {"regular", "bold"}, f"{path.as_posix()}.sources")
    sources: dict[str, SourceFont] = {}
    for weight, raw_source in sources_value.items():
        source_path = f"{path.as_posix()}.sources.{weight}"
        source = _mapping(raw_source, source_path)
        _exact_keys(source, {"path", "sha256"}, source_path)
        repository_path = _string(source["path"], f"{source_path}.path")
        resolved = (repository_root / repository_path).resolve()
        try:
            resolved.relative_to(repository_root.resolve())
        except ValueError:
            _fail("path-escape", f"{source_path}.path", repository_path)
        if not resolved.is_file():
            _fail("missing-source", f"{source_path}.path", repository_path)
        expected_hash = _string(source["sha256"], f"{source_path}.sha256").lower()
        actual_hash = _sha256(resolved)
        if actual_hash != expected_hash:
            _fail("source-hash-mismatch", f"{source_path}.sha256", actual_hash)
        sources[weight] = SourceFont(weight, resolved, repository_path, actual_hash)

    fixed_value = _mapping(root["fixed"], f"{path.as_posix()}.fixed")
    _exact_keys(fixed_value, {"bpp", "styles"}, f"{path.as_posix()}.fixed")
    fixed_bpp = _integer(fixed_value["bpp"], f"{path.as_posix()}.fixed.bpp", 1, 8)
    if fixed_bpp not in (1, 2, 3, 4, 8):
        _fail("invalid-bpp", f"{path.as_posix()}.fixed.bpp", str(fixed_bpp))
    raw_styles = fixed_value["styles"]
    if not isinstance(raw_styles, list) or not raw_styles:
        _fail("invalid-type", f"{path.as_posix()}.fixed.styles", "expected a non-empty array")
    styles: list[FixedStyle] = []
    style_ids: set[str] = set()
    assigned_roles: set[str] = set()
    for index, raw_style in enumerate(raw_styles):
        style_path = f"{path.as_posix()}.fixed.styles[{index}]"
        style = _mapping(raw_style, style_path)
        _exact_keys(style, {"id", "size", "weight", "roles", "extra_characters"}, style_path)
        style_id = _string(style["id"], f"{style_path}.id")
        if not style_id.replace("_", "").isalnum() or style_id in style_ids:
            _fail("invalid-style-id", f"{style_path}.id", style_id)
        style_ids.add(style_id)
        weight = _string(style["weight"], f"{style_path}.weight")
        if weight not in sources:
            _fail("unknown-weight", f"{style_path}.weight", weight)
        roles_value = style["roles"]
        if not isinstance(roles_value, list) or not roles_value:
            _fail("invalid-type", f"{style_path}.roles", "expected a non-empty array")
        roles: list[str] = []
        for role_index, raw_role in enumerate(roles_value):
            role = _string(raw_role, f"{style_path}.roles[{role_index}]")
            if role in assigned_roles:
                _fail("duplicate-role-style", f"{style_path}.roles[{role_index}]", role)
            assigned_roles.add(role)
            roles.append(role)
        extra = style["extra_characters"]
        if not isinstance(extra, str):
            _fail("invalid-type", f"{style_path}.extra_characters", "expected a string")
        styles.append(
            FixedStyle(
                style_id,
                _integer(style["size"], f"{style_path}.size", 8, 72),
                weight,
                tuple(roles),
                extra,
            )
        )

    user_value = _mapping(root["user"], f"{path.as_posix()}.user")
    _exact_keys(
        user_value,
        {"size", "bpp", "cache_capacity", "replacement_codepoint", "assets"},
        f"{path.as_posix()}.user",
    )
    user_bpp = _integer(user_value["bpp"], f"{path.as_posix()}.user.bpp", 1, 8)
    if user_bpp not in (1, 2, 3, 4, 8):
        _fail("invalid-bpp", f"{path.as_posix()}.user.bpp", str(user_bpp))
    raw_assets = user_value["assets"]
    if not isinstance(raw_assets, list) or not raw_assets:
        _fail("invalid-type", f"{path.as_posix()}.user.assets", "expected a non-empty array")
    assets: list[UserAsset] = []
    asset_ids: set[str] = set()
    all_ranges: list[tuple[int, int, str]] = []
    for index, raw_asset in enumerate(raw_assets):
        asset_path = f"{path.as_posix()}.user.assets[{index}]"
        asset = _mapping(raw_asset, asset_path)
        allowed_keys = {"id", "ranges"}
        if "block_size" in asset:
            allowed_keys.add("block_size")
        _exact_keys(asset, allowed_keys, asset_path)
        asset_id = _string(asset["id"], f"{asset_path}.id")
        if not asset_id.replace("_", "").isalnum() or asset_id in asset_ids:
            _fail("invalid-asset-id", f"{asset_path}.id", asset_id)
        asset_ids.add(asset_id)
        ranges_value = asset["ranges"]
        if not isinstance(ranges_value, list) or not ranges_value:
            _fail("invalid-type", f"{asset_path}.ranges", "expected a non-empty array")
        ranges: list[tuple[int, int]] = []
        arguments: list[str] = []
        for range_index, raw_range in enumerate(ranges_value):
            label = f"{asset_path}.ranges[{range_index}]"
            argument = _string(raw_range, label)
            first, last = _parse_range(argument, label)
            for other_first, other_last, other_label in all_ranges:
                if first <= other_last and other_first <= last:
                    _fail("overlapping-range", label, f"overlaps {other_label}")
            all_ranges.append((first, last, label))
            ranges.append((first, last))
            arguments.append(argument)
        if "block_size" not in asset:
            assets.append(UserAsset(asset_id, tuple(ranges), tuple(arguments)))
            continue
        if len(ranges) != 1:
            _fail("invalid-block-range", asset_path, "blocked asset requires one range")
        block_size = _integer(asset["block_size"], f"{asset_path}.block_size", 1, 256)
        if block_size & (block_size - 1):
            _fail("invalid-block-size", f"{asset_path}.block_size", "expected a power of two")
        first, last = ranges[0]
        block_first = first
        while block_first <= last:
            block_last = min(last, block_first + block_size - 1)
            block_id = f"{asset_id}_{block_first:04x}"
            if block_id in asset_ids:
                _fail("invalid-asset-id", f"{asset_path}.id", block_id)
            asset_ids.add(block_id)
            argument = f"0x{block_first:04X}-0x{block_last:04X}"
            assets.append(UserAsset(block_id, ((block_first, block_last),), (argument,)))
            block_first = block_last + 1

    capacity = _integer(
        user_value["cache_capacity"], f"{path.as_posix()}.user.cache_capacity", 1, 255
    )
    if len(assets) > capacity:
        _fail(
            "cache-too-small",
            f"{path.as_posix()}.user.cache_capacity",
            f"{capacity} slots cannot retain {len(assets)} assets",
        )
    replacement = _parse_codepoint(
        _string(user_value["replacement_codepoint"], f"{path.as_posix()}.user.replacement_codepoint"),
        f"{path.as_posix()}.user.replacement_codepoint",
    )
    if not any(first <= replacement <= last for first, last, _ in all_ranges):
        _fail("missing-replacement", f"{path.as_posix()}.user.replacement_codepoint", hex(replacement))

    return FontConfig(
        path=path,
        sources=sources,
        fixed_bpp=fixed_bpp,
        fixed_styles=tuple(styles),
        user_size=_integer(user_value["size"], f"{path.as_posix()}.user.size", 8, 72),
        user_bpp=user_bpp,
        user_cache_capacity=capacity,
        replacement_codepoint=replacement,
        user_assets=tuple(assets),
    )


def converter_path(repository_root: Path) -> Path:
    path = repository_root / "tools/assets/typography/node_modules/lv_font_conv/lv_font_conv.js"
    if not path.is_file():
        _fail(
            "missing-converter",
            path.relative_to(repository_root).as_posix(),
            "run npm ci --prefix tools/assets/typography",
        )
    return path


def _run_converter(repository_root: Path, arguments: list[str], output: Path) -> None:
    command = ["node", converter_path(repository_root).relative_to(repository_root).as_posix(), *arguments, "-o", str(output)]
    completed = subprocess.run(
        command,
        cwd=repository_root,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout).strip()
        _fail("converter-failed", output.name, detail)
    if not output.is_file() or output.stat().st_size == 0:
        _fail("missing-converter-output", output.name, "converter produced no data")


def _canonical_json(payload: dict[str, Any]) -> bytes:
    return json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True).encode("utf-8") + b"\n"


def _content_hash(payload: dict[str, Any]) -> str:
    canonical = json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def _fixed_header(
    config: FontConfig,
    role_order: Iterable[str],
    cpp_namespace: str,
    symbol_prefix: str,
) -> bytes:
    style_for_role = {
        role: style for style in config.fixed_styles for role in style.roles
    }
    cases = "\n".join(
        f"    case TypographyRole::{role}: return &{symbol_prefix}_{style_for_role[role].id};"
        for role in role_order
    )
    declarations = "\n".join(
        f"LV_FONT_DECLARE({symbol_prefix}_{style.id});" for style in config.fixed_styles
    )
    return f'''// Generated by tools/assets/typography/generate.py. Do not edit.
#pragma once

#include <lvgl.h>

#include "text_catalog.hpp"

{declarations}

namespace {cpp_namespace} {{

[[nodiscard]] inline const lv_font_t *fixed_font(TypographyRole role) noexcept {{
  switch (role) {{
{cases}
  }}
  return nullptr;
}}

}}  // namespace {cpp_namespace}
'''.encode("utf-8")


def build_fixed_outputs(
    repository_root: Path,
    config: FontConfig,
    catalogs: dict[str, dict[str, str]],
    typography_roles: dict[str, str],
    role_order: Iterable[str],
    cpp_namespace: str,
    symbol_prefix: str,
) -> dict[str, bytes]:
    configured_roles = {role for style in config.fixed_styles for role in style.roles}
    used_roles = set(typography_roles.values())
    if configured_roles != used_roles:
        _fail(
            "fixed-role-mismatch",
            config.path.as_posix(),
            f"missing={sorted(used_roles - configured_roles)}, extra={sorted(configured_roles - used_roles)}",
        )
    outputs: dict[str, bytes] = {}
    style_entries: list[dict[str, Any]] = []
    with tempfile.TemporaryDirectory() as temporary:
        temporary_root = Path(temporary)
        for style in config.fixed_styles:
            characters = set(style.extra_characters)
            for locale_catalog in catalogs.values():
                for key, role in typography_roles.items():
                    if role in style.roles:
                        characters.update(locale_catalog[key])
            symbols = "".join(sorted(characters, key=ord))
            if not symbols:
                _fail("empty-fixed-charset", config.path.as_posix(), style.id)
            source = config.sources[style.weight]
            temporary_output = temporary_root / f"{style.id}.c"
            _run_converter(
                repository_root,
                [
                    "--size", str(style.size),
                    "--bpp", str(config.fixed_bpp),
                    "--format", "lvgl",
                    "--font", source.repository_path,
                    "--symbols", symbols,
                    "--lv-font-name", f"{symbol_prefix}_{style.id}",
                ],
                temporary_output,
            )
            text = temporary_output.read_text(encoding="utf-8")
            normalized_lines: list[str] = []
            inside_converter_options = False
            for line in text.splitlines():
                if line.startswith(" * Opts:"):
                    normalized_lines.append(
                        " * Generated deterministically from owner config input "
                        f"{config.path.name}"
                    )
                    inside_converter_options = True
                    continue
                if inside_converter_options:
                    # 최신 converter는 긴 --symbols/--output 인자를 여러 줄로
                    # 감싼다. 임시 output 경로까지 모두 버려야 byte가 재현된다.
                    if line.endswith("******/"):
                        normalized_lines.append(line)
                        inside_converter_options = False
                    continue
                normalized_lines.append(line)
            content = ("\n".join(normalized_lines).rstrip("\n") + "\n").encode("utf-8")
            relative = f"fonts/fixed/{style.id}.c"
            outputs[relative] = content
            style_entries.append(
                {
                    "id": style.id,
                    "size": style.size,
                    "bpp": config.fixed_bpp,
                    "weight": style.weight,
                    "roles": list(style.roles),
                    "glyph_count": len(characters),
                    "source": source.repository_path,
                    "output": relative,
                    "bytes": len(content),
                    "sha256": hashlib.sha256(content).hexdigest(),
                }
            )
    header = _fixed_header(
        config,
        [role for role in role_order if role in used_roles],
        cpp_namespace,
        symbol_prefix,
    )
    outputs["fixed_fonts.hpp"] = header
    manifest: dict[str, Any] = {
        "schema_version": 1,
        "generator": "tools/assets/typography/generate.py",
        "styles": style_entries,
        "header": {
            "path": "fixed_fonts.hpp",
            "bytes": len(header),
            "sha256": hashlib.sha256(header).hexdigest(),
        },
    }
    manifest["content_hash"] = _content_hash(manifest)
    outputs["fixed_font_manifest.json"] = _canonical_json(manifest)
    return outputs


def _user_header(
    config: FontConfig,
    asset_metadata: list[tuple[int, int]],
    cpp_namespace: str,
    user_asset_prefix: str,
) -> bytes:
    asset_lines = []
    range_lines = []
    for asset_index, asset in enumerate(config.user_assets):
        size_bytes, crc32 = asset_metadata[asset_index]
        asset_lines.append(
            f'    {{"{user_asset_prefix}/user_{config.user_size}_{asset.id}.bin", '
            f'{size_bytes}U, 0x{crc32:08X}U}}'
        )
        for first, last in asset.ranges:
            range_lines.append(
                f"    {{0x{first:04X}U, 0x{last:04X}U, {asset_index}U}}"
            )
    return f'''// Generated by tools/assets/typography/generate_user.py. Do not edit.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "hmi/ui/font_repository.hpp"

namespace {cpp_namespace} {{

inline constexpr std::array<hmi::ui::FontAsset, {len(asset_lines)}U> kUserFontAssets = {{{{
{",\n".join(asset_lines)}
}}}};

inline constexpr std::array<hmi::ui::FontRange, {len(range_lines)}U> kUserFontRanges = {{{{
{",\n".join(range_lines)}
}}}};

inline constexpr hmi::ui::FontManifestView kUserFontManifest{{
    kUserFontAssets.data(), kUserFontAssets.size(),
    kUserFontRanges.data(), kUserFontRanges.size(),
    0x{config.replacement_codepoint:04X}U}};

inline constexpr std::size_t kUserFontCacheCapacity = {config.user_cache_capacity}U;

}}  // namespace {cpp_namespace}
'''.encode("utf-8")


def build_user_outputs(
    repository_root: Path,
    config: FontConfig,
    cpp_namespace: str,
    user_asset_prefix: str,
) -> dict[str, bytes]:
    outputs: dict[str, bytes] = {}
    entries: list[dict[str, Any]] = []
    asset_metadata: list[tuple[int, int]] = []
    source = config.sources["regular"]
    with tempfile.TemporaryDirectory() as temporary:
        temporary_root = Path(temporary)
        for asset in config.user_assets:
            temporary_output = temporary_root / f"user_{config.user_size}_{asset.id}.bin"
            range_argument = ",".join(asset.range_arguments)
            _run_converter(
                repository_root,
                [
                    "--size", str(config.user_size),
                    "--bpp", str(config.user_bpp),
                    "--format", "bin",
                    "--font", source.repository_path,
                    "--range", range_argument,
                ],
                temporary_output,
            )
            content = temporary_output.read_bytes()
            crc32 = zlib.crc32(content) & 0xFFFFFFFF
            relative = f"user_{config.user_size}_{asset.id}.bin"
            outputs[relative] = content
            asset_metadata.append((len(content), crc32))
            entries.append(
                {
                    "id": asset.id,
                    "ranges": [
                        {"first": f"0x{first:04X}", "last": f"0x{last:04X}"}
                        for first, last in asset.ranges
                    ],
                    "path": f"{user_asset_prefix}/{relative}",
                    "bytes": len(content),
                    "crc32": f"0x{crc32:08X}",
                    "sha256": hashlib.sha256(content).hexdigest(),
                }
            )
    header = _user_header(
        config, asset_metadata, cpp_namespace, user_asset_prefix
    )
    manifest: dict[str, Any] = {
        "schema_version": 1,
        "generator": "tools/assets/typography/generate_user.py",
        "source": {"path": source.repository_path, "sha256": source.sha256},
        "size": config.user_size,
        "bpp": config.user_bpp,
        "cache_capacity": config.user_cache_capacity,
        "replacement_codepoint": f"0x{config.replacement_codepoint:04X}",
        "assets": entries,
        "header": {
            "path": "user_font_manifest.hpp",
            "bytes": len(header),
            "sha256": hashlib.sha256(header).hexdigest(),
        },
    }
    manifest["content_hash"] = _content_hash(manifest)
    outputs["user_font_manifest.hpp"] = header
    outputs["user_font_manifest.json"] = _canonical_json(manifest)
    return outputs


def write_outputs(root: Path, outputs: dict[str, bytes]) -> None:
    for relative, content in outputs.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_name(f".{path.name}.tmp")
        temporary.write_bytes(content)
        temporary.replace(path)


def check_outputs(root: Path, outputs: dict[str, bytes]) -> None:
    for relative, expected in outputs.items():
        path = root / relative
        try:
            actual = path.read_bytes()
        except OSError as error:
            _fail("missing-generated-output", path.as_posix(), str(error))
        if actual != expected:
            _fail("stale-generated-output", path.as_posix(), "run the generator")


def remove_extra_outputs(root: Path, outputs: dict[str, bytes], pattern: str) -> None:
    """생성기가 소유한 pattern 중 새 manifest에 없는 stale 파일만 제거한다."""
    expected = set(outputs)
    for path in root.glob(pattern):
        if path.name not in expected:
            path.unlink()


def check_no_extra_outputs(root: Path, outputs: dict[str, bytes], pattern: str) -> None:
    expected = set(outputs)
    extras = sorted(path.name for path in root.glob(pattern) if path.name not in expected)
    if extras:
        _fail("extra-generated-output", root.as_posix(), f"unexpected={extras}")

