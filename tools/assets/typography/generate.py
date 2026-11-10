"""Generate a typed bilingual catalog and fixed fonts from an owner config."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any

try:
    from .font_assets import (
        FontAssetError,
        build_fixed_outputs,
        check_outputs as check_font_outputs,
        converter_path,
        load_font_config,
        write_outputs as write_font_outputs,
    )
    from .owner_config import (
        OwnerConfigError,
        SUPPORTED_LOCALES,
        TypographyOwnerConfig,
        load_owner_config,
    )
except ImportError:  # Direct script execution from the repository root.
    from font_assets import (  # type: ignore[no-redef]
        FontAssetError,
        build_fixed_outputs,
        check_outputs as check_font_outputs,
        converter_path,
        load_font_config,
        write_outputs as write_font_outputs,
    )
    from owner_config import (  # type: ignore[no-redef]
        OwnerConfigError,
        SUPPORTED_LOCALES,
        TypographyOwnerConfig,
        load_owner_config,
    )

GENERATOR_VERSION = 2
ROLE_ORDER = (
    "page_title",
    "body",
    "field_label",
    "placeholder",
    "control",
    "key_action",
    "status",
    "error",
)
KEY_PATTERN = re.compile(r"^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+$")
PLACEHOLDER_PATTERN = re.compile(r"\{([a-z][a-z0-9_]*):(u32|string)\}")


class CatalogError(ValueError):
    """A deterministic source validation failure."""

    def __init__(self, code: str, path: str, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.path = path
        self.message = message

    def __str__(self) -> str:
        return f"[{self.code}] {self.path}: {self.message}"


@dataclass(frozen=True)
class CatalogInputs:
    catalogs: dict[str, dict[str, str]]
    roles: dict[str, str]
    source_paths: tuple[Path, ...]


def _fail(code: str, path: str, message: str) -> None:
    raise CatalogError(code, path, message)


def _parse_scalar(raw: str, path: str) -> Any:
    value = raw.strip()
    if not value:
        _fail("missing-value", path, "value must not be empty")
    if value.startswith('"'):
        try:
            parsed = json.loads(value)
        except json.JSONDecodeError as error:
            _fail("invalid-string", path, str(error))
        if not isinstance(parsed, str):
            _fail("invalid-type", path, "quoted value must be a string")
        return parsed
    if value.isdecimal():
        return int(value)
    return value


def _read_simple_yaml(path: Path, section_name: str) -> dict[str, Any]:
    """Read the intentionally small mapping-only YAML subset used by this project."""
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as error:
        _fail("read-error", path.as_posix(), str(error))

    result: dict[str, Any] = {}
    section: dict[str, Any] | None = None
    for line_number, original in enumerate(lines, start=1):
        if not original.strip() or original.lstrip().startswith("#"):
            continue
        label = f"{path.as_posix()}:{line_number}"
        if original.startswith((" ", "\t")):
            if not original.startswith("  ") or original.startswith("   ") or section is None:
                _fail("invalid-indentation", label, "section entries require exactly two spaces")
            content = original[2:]
            if ":" not in content:
                _fail("invalid-line", label, "expected 'key: value'")
            key, raw_value = content.split(":", 1)
            key = key.strip()
            if KEY_PATTERN.fullmatch(key) is None:
                _fail("invalid-key", label, f"invalid text key '{key}'")
            if key in section:
                _fail("duplicate-key", label, f"duplicate key '{key}'")
            section[key] = _parse_scalar(raw_value, label)
            continue

        if ":" not in original:
            _fail("invalid-line", label, "expected 'key: value'")
        key, raw_value = original.split(":", 1)
        key = key.strip()
        if key in result:
            _fail("duplicate-key", label, f"duplicate key '{key}'")
        if key == section_name:
            if raw_value.strip():
                _fail("invalid-section", label, f"'{section_name}' must be a mapping")
            section = {}
            result[key] = section
        else:
            section = None
            result[key] = _parse_scalar(raw_value, label)
    return result


def _placeholder_contract(text: str, path: str) -> tuple[tuple[str, str], ...]:
    matches = list(PLACEHOLDER_PATTERN.finditer(text))
    remainder = PLACEHOLDER_PATTERN.sub("", text)
    if "{" in remainder or "}" in remainder:
        _fail(
            "invalid-placeholder",
            path,
            "placeholders must use {name:u32} or {name:string}",
        )
    types_by_name: dict[str, str] = {}
    for match in matches:
        name, value_type = match.groups()
        previous = types_by_name.get(name)
        if previous is not None and previous != value_type:
            _fail(
                "placeholder-type-conflict",
                path,
                f"placeholder '{name}' uses both '{previous}' and '{value_type}'",
            )
        types_by_name[name] = value_type
    counts = Counter(match.groups() for match in matches)
    return tuple(
        item
        for placeholder, count in sorted(counts.items())
        for item in (placeholder,) * count
    )


def load_inputs(
    catalog_paths: dict[str, Path], typography_path: Path
) -> CatalogInputs:
    catalogs: dict[str, dict[str, str]] = {}
    contracts: dict[str, dict[str, tuple[tuple[str, str], ...]]] = {}
    for expected_locale, path in catalog_paths.items():
        data = _read_simple_yaml(path, "texts")
        if set(data) != {"schema_version", "locale", "texts"}:
            _fail("invalid-fields", path.as_posix(), "expected schema_version, locale, texts")
        if data["schema_version"] != 1:
            _fail("unsupported-version", path.as_posix(), "expected schema version 1")
        if data["locale"] != expected_locale:
            _fail(
                "locale-mismatch",
                path.as_posix(),
                f"expected locale '{expected_locale}', got '{data['locale']}'",
            )
        texts = data["texts"]
        if not isinstance(texts, dict) or not texts:
            _fail("missing-texts", path.as_posix(), "texts must not be empty")
        for key, value in texts.items():
            if not isinstance(value, str) or not value.strip():
                _fail("empty-text", f"{path.as_posix()}.{key}", "text must not be empty")
        catalogs[expected_locale] = texts
        contracts[expected_locale] = {
            key: _placeholder_contract(value, f"{path.as_posix()}.{key}")
            for key, value in texts.items()
        }

    reference_keys = set(catalogs[SUPPORTED_LOCALES[0]])
    for locale in SUPPORTED_LOCALES[1:]:
        keys = set(catalogs[locale])
        if keys != reference_keys:
            missing = sorted(reference_keys - keys)
            extra = sorted(keys - reference_keys)
            detail = f"missing={missing}, extra={extra}"
            _fail("catalog-key-mismatch", catalog_paths[locale].as_posix(), detail)
        for key in sorted(reference_keys):
            if contracts[locale][key] != contracts[SUPPORTED_LOCALES[0]][key]:
                _fail(
                    "placeholder-mismatch",
                    f"{catalog_paths[locale].as_posix()}.{key}",
                    f"expected {contracts[SUPPORTED_LOCALES[0]][key]}, got {contracts[locale][key]}",
                )

    typography = _read_simple_yaml(typography_path, "roles")
    if set(typography) != {"schema_version", "roles"}:
        _fail("invalid-fields", typography_path.as_posix(), "expected schema_version, roles")
    if typography["schema_version"] != 1:
        _fail("unsupported-version", typography_path.as_posix(), "expected schema version 1")
    roles = typography["roles"]
    if not isinstance(roles, dict):
        _fail("invalid-roles", typography_path.as_posix(), "roles must be a mapping")
    if set(roles) != reference_keys:
        missing = sorted(reference_keys - set(roles))
        extra = sorted(set(roles) - reference_keys)
        _fail(
            "typography-key-mismatch",
            typography_path.as_posix(),
            f"missing={missing}, extra={extra}",
        )
    for key, role in roles.items():
        if role not in ROLE_ORDER:
            _fail(
                "unknown-typography-role",
                f"{typography_path.as_posix()}.{key}",
                f"unknown role '{role}'",
            )

    return CatalogInputs(
        catalogs=catalogs,
        roles=roles,
        source_paths=tuple(catalog_paths.values()) + (typography_path,),
    )


def _identifier(key: str) -> str:
    return key.replace(".", "_").replace("-", "_")


def _cpp_string(value: str) -> str:
    # LVGL과 CatalogView는 UTF-8 char buffer를 사용한다. C++20의 char8_t로
    # 분리되는 u8 literal 대신 UTF-8 source encoding의 char literal을 생성한다.
    return json.dumps(value, ensure_ascii=False)


def render_header(inputs: CatalogInputs, cpp_namespace: str) -> str:
    keys = sorted(inputs.roles)
    used_roles = [role for role in ROLE_ORDER if role in set(inputs.roles.values())]
    contracts = {
        key: _placeholder_contract(inputs.catalogs["ko-KR"][key], key) for key in keys
    }

    text_ids = "\n".join(f"  {_identifier(key)}," for key in keys)
    typography_roles = "\n".join(f"  {role}," for role in used_roles)
    key_names = ",\n".join(f"    {_cpp_string(key)}" for key in keys)
    ko_values = ",\n".join(
        f"    {_cpp_string(inputs.catalogs['ko-KR'][key])}" for key in keys
    )
    en_values = ",\n".join(
        f"    {_cpp_string(inputs.catalogs['en-US'][key])}" for key in keys
    )
    role_values = ",\n".join(
        f"    TypographyRole::{inputs.roles[key]}" for key in keys
    )
    contract_values = ",\n".join(
        "    "
        + _cpp_string(",".join(f"{name}:{kind}" for name, kind in contracts[key]))
        for key in keys
    )

    return f'''// Generated by tools/assets/typography/generate.py. Do not edit.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "hmi/i18n/catalog.hpp"
#include "hmi/i18n/locale.hpp"

namespace {cpp_namespace} {{

enum class TextId : std::uint16_t {{
{text_ids}
  count,
}};

enum class TypographyRole : std::uint8_t {{
{typography_roles}
}};

inline constexpr std::size_t kTextCount = static_cast<std::size_t>(TextId::count);

inline constexpr std::array<std::string_view, kTextCount> kKeyNames = {{{{
{key_names}
}}}};

inline constexpr std::array<std::string_view, kTextCount> kKoKrText = {{{{
{ko_values}
}}}};

inline constexpr std::array<std::string_view, kTextCount> kEnUsText = {{{{
{en_values}
}}}};

inline constexpr std::array<TypographyRole, kTextCount> kTypographyRoles = {{{{
{role_values}
}}}};

inline constexpr std::array<std::string_view, kTextCount> kPlaceholderContracts = {{{{
{contract_values}
}}}};

[[nodiscard]] constexpr std::size_t index_of(TextId id) noexcept {{
  return static_cast<std::size_t>(id);
}}

[[nodiscard]] constexpr TextId text_id(std::string_view key) noexcept {{
  for (std::size_t index = 0U; index < kKeyNames.size(); ++index) {{
    if (kKeyNames[index] == key) {{
      return static_cast<TextId>(index);
    }}
  }}
  return TextId::count;
}}

[[nodiscard]] constexpr hmi::i18n::CatalogView catalog(hmi::i18n::Locale locale) noexcept {{
  switch (locale) {{
    case hmi::i18n::Locale::ko_kr:
      return {{kKoKrText.data(), kKoKrText.size()}};
    case hmi::i18n::Locale::en_us:
      return {{kEnUsText.data(), kEnUsText.size()}};
  }}
  return {{}};
}}

[[nodiscard]] constexpr std::string_view text(hmi::i18n::Locale locale,
                                               TextId id) noexcept {{
  return catalog(locale).get(index_of(id));
}}

[[nodiscard]] constexpr std::string_view text(hmi::i18n::Locale locale,
                                               std::string_view key) noexcept {{
  return text(locale, text_id(key));
}}

[[nodiscard]] constexpr TypographyRole typography(TextId id) noexcept {{
  const auto index = index_of(id);
  return index < kTypographyRoles.size() ? kTypographyRoles[index]
                                         : TypographyRole::{used_roles[0]};
}}

[[nodiscard]] constexpr TypographyRole typography(std::string_view key) noexcept {{
  return typography(text_id(key));
}}

[[nodiscard]] constexpr std::string_view placeholder_contract(TextId id) noexcept {{
  const auto index = index_of(id);
  return index < kPlaceholderContracts.size() ? kPlaceholderContracts[index]
                                               : std::string_view{{}};
}}

static_assert(kTextCount <= 65535U, "TextId must fit in two bytes");

}}  // namespace {cpp_namespace}
'''


def render_manifest(
    inputs: CatalogInputs, owner: TypographyOwnerConfig
) -> str:
    keys = sorted(inputs.roles)
    sources = {
        path.relative_to(owner.owner_root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in inputs.source_paths
    }
    sources[owner.path.relative_to(owner.owner_root).as_posix()] = hashlib.sha256(
        owner.path.read_bytes()
    ).hexdigest()
    entries = [
        {
            "key": key,
            "role": inputs.roles[key],
            "placeholders": [
                {"name": name, "type": kind}
                for name, kind in _placeholder_contract(inputs.catalogs["ko-KR"][key], key)
            ],
        }
        for key in keys
    ]
    payload = {
        "schema_version": 1,
        "generator_version": GENERATOR_VERSION,
        "owner_id": owner.owner_id,
        "cpp_namespace": owner.cpp_namespace,
        "symbol_prefix": owner.symbol_prefix,
        "locales": list(SUPPORTED_LOCALES),
        "text_count": len(keys),
        "sources": sources,
        "entries": entries,
    }
    canonical = json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    payload["content_hash"] = hashlib.sha256(canonical.encode("utf-8")).hexdigest()
    return json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def render_outputs(owner: TypographyOwnerConfig) -> dict[str, str]:
    inputs = load_inputs(owner.catalogs, owner.typography)
    return {
        "text_catalog.hpp": render_header(inputs, owner.cpp_namespace),
        "catalog_manifest.json": render_manifest(inputs, owner),
    }


def _write_outputs(output_directory: Path, outputs: dict[str, str]) -> None:
    output_directory.mkdir(parents=True, exist_ok=True)
    for name, content in outputs.items():
        temporary = output_directory / f".{name}.tmp"
        temporary.write_text(content, encoding="utf-8", newline="\n")
        temporary.replace(output_directory / name)


def _check_outputs(output_directory: Path, outputs: dict[str, str]) -> None:
    for name, expected in outputs.items():
        path = output_directory / name
        try:
            actual = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            _fail("missing-generated-output", path.as_posix(), str(error))
        if actual != expected:
            _fail("stale-generated-output", path.as_posix(), "run the generator")


def main(argv: list[str] | None = None) -> int:
    repository_root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="validate without writing")
    mode.add_argument("--check", action="store_true", help="verify generated output bytes")
    parser.add_argument("--repository-root", type=Path, default=repository_root)
    parser.add_argument("--config", type=Path, required=True, help="owner catalog config")
    args = parser.parse_args(argv)

    try:
        owner = load_owner_config(args.config)
        inputs = load_inputs(owner.catalogs, owner.typography)
        outputs = {
            "text_catalog.hpp": render_header(inputs, owner.cpp_namespace),
            "catalog_manifest.json": render_manifest(inputs, owner),
        }
        font_config = load_font_config(args.repository_root, owner.fonts)
        converter_path(args.repository_root)
        if args.check:
            _check_outputs(owner.generated_output, outputs)
            fixed_outputs = build_fixed_outputs(
                args.repository_root,
                font_config,
                inputs.catalogs,
                inputs.roles,
                ROLE_ORDER,
                owner.cpp_namespace,
                owner.symbol_prefix,
            )
            check_font_outputs(owner.generated_output, fixed_outputs)
            action = "checked"
        elif args.dry_run:
            action = "validated"
        else:
            _write_outputs(owner.generated_output, outputs)
            fixed_outputs = build_fixed_outputs(
                args.repository_root,
                font_config,
                inputs.catalogs,
                inputs.roles,
                ROLE_ORDER,
                owner.cpp_namespace,
                owner.symbol_prefix,
            )
            write_font_outputs(owner.generated_output, fixed_outputs)
            action = "generated"
    except (CatalogError, FontAssetError, OwnerConfigError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    manifest = json.loads(outputs["catalog_manifest.json"])
    print(
        f"Catalog {action}: locales={len(manifest['locales'])} "
        f"texts={manifest['text_count']} hash={manifest['content_hash']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
