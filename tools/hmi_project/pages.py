"""Sample page registry validation used before generating compile-time inputs."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .manifest import ManifestError, Profile
from .paths import PathRuleError, resolve_contained_path

_ID = re.compile(r"^[a-z][a-z0-9-]{0,31}$")
_DATA_KEY = re.compile(r"^[a-z][a-z0-9_]{0,31}$")
_CPP_TYPE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)+$")
_TEST_ID = re.compile(r"^[a-z][a-z0-9_-]{0,63}$")
_CATALOG_KEY = re.compile(r"^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+$")
_PAGE_KEYS = frozenset(
    {"id", "title_key", "description_key", "order", "entry", "sources",
     "capabilities", "verification", "data_key"}
)


@dataclass(frozen=True)
class PageDefinition:
    data: dict[str, Any]

    @property
    def id(self) -> str:
        return self.data["id"]


@dataclass(frozen=True)
class PageRegistry:
    path: Path
    pages: tuple[PageDefinition, ...]
    data: dict[str, Any]


def _fail(code: str, path: str, message: str) -> None:
    raise ManifestError(code, path, message)


def _read(path: Path, label: str) -> dict[str, Any]:
    def unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                _fail("duplicate-key", label, f"duplicate key '{key}'")
            result[key] = value
        return result

    try:
        value = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)
    except ManifestError:
        raise
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        _fail("invalid-page-registry", label, str(error))
    if not isinstance(value, dict):
        _fail("invalid-type", label, "root value must be an object")
    return value


def _exact(value: dict[str, Any], keys: frozenset[str], path: str) -> None:
    missing = sorted(keys - value.keys())
    if missing:
        _fail("missing-field", path, f"missing required field '{missing[0]}'")
    unknown = sorted(value.keys() - keys)
    if unknown:
        _fail("unknown-field", path, f"unknown field '{unknown[0]}'")


def _text(value: Any, path: str, pattern: re.Pattern[str] | None = None) -> str:
    if not isinstance(value, str) or not value:
        _fail("invalid-type", path, "must be a non-empty string")
    if pattern is not None and pattern.fullmatch(value) is None:
        _fail("invalid-value", path, f"invalid value '{value}'")
    return value


def _strings(value: Any, path: str, pattern: re.Pattern[str]) -> list[str]:
    if not isinstance(value, list):
        _fail("invalid-type", path, "must be an array")
    result: list[str] = []
    for index, item in enumerate(value):
        text = _text(item, f"{path}[{index}]", pattern)
        if text in result:
            _fail("duplicate-value", f"{path}[{index}]", f"duplicate value '{text}'")
        result.append(text)
    return result


def _file(
    profile: Profile,
    repository_root: Path,
    value: Any,
    path: str,
    *,
    containment_root: Path | None = None,
) -> Path:
    try:
        resolved = resolve_contained_path(
            value,
            base=profile.root,
            repository_root=repository_root,
            containment_root=containment_root or profile.root,
        )
    except PathRuleError as error:
        raise ManifestError(error.code, path, str(error)) from error
    if not resolved.is_file():
        _fail("missing-file", path, f"file '{value}' does not exist")
    return resolved


def load_page_registry(repository_root: Path, profile: Profile) -> PageRegistry | None:
    if profile.kind != "sample":
        return None
    if "page_registry.json" not in profile.data["config"]:
        _fail(
            "missing-page-registry",
            f"{profile.manifest_path}.config",
            "sample profiles must register page_registry.json",
        )

    registry_path = profile.root / "page_registry.json"
    label = registry_path.relative_to(repository_root).as_posix()
    data = _read(registry_path, label)
    _exact(data, frozenset({"schema_version", "pages"}), label)
    if type(data["schema_version"]) is not int or data["schema_version"] != 2:
        _fail("unsupported-version", f"{label}.schema_version", "expected schema version 2")
    if not isinstance(data["pages"], list) or not data["pages"]:
        _fail("missing-page", f"{label}.pages", "must contain at least one page")

    pages: list[PageDefinition] = []
    ids: set[str] = set()
    orders: set[int] = set()
    data_keys: set[str] = set()
    for index, raw in enumerate(data["pages"]):
        page_path = f"{label}.pages[{index}]"
        if not isinstance(raw, dict):
            _fail("invalid-type", page_path, "must be an object")
        _exact(raw, _PAGE_KEYS, page_path)
        page_id = _text(raw["id"], f"{page_path}.id", _ID)
        _text(raw["title_key"], f"{page_path}.title_key", _CATALOG_KEY)
        _text(raw["description_key"], f"{page_path}.description_key", _CATALOG_KEY)
        if type(raw["order"]) is not int or raw["order"] < 0:
            _fail("invalid-value", f"{page_path}.order", "must be a non-negative integer")
        data_key = _text(raw["data_key"], f"{page_path}.data_key", _DATA_KEY)
        if page_id in ids:
            _fail("duplicate-page-id", f"{page_path}.id", f"duplicate page ID '{page_id}'")
        if raw["order"] in orders:
            _fail("duplicate-page-order", f"{page_path}.order", "page order must be unique")
        if data_key in data_keys:
            _fail("duplicate-page-data-key", f"{page_path}.data_key", "data key must be unique")
        ids.add(page_id)
        orders.add(raw["order"])
        data_keys.add(data_key)
        page_root = profile.root / "pages" / page_id.replace("-", "_")
        if not page_root.is_dir():
            _fail(
                "missing-page-directory",
                page_path,
                f"page directory '{page_root.relative_to(profile.root).as_posix()}' does not exist",
            )

        entry = raw["entry"]
        if not isinstance(entry, dict):
            _fail("invalid-type", f"{page_path}.entry", "must be an object")
        _exact(entry, frozenset({"header", "type"}), f"{page_path}.entry")
        _file(
            profile,
            repository_root,
            entry["header"],
            f"{page_path}.entry.header",
            containment_root=page_root,
        )
        _text(entry["type"], f"{page_path}.entry.type", _CPP_TYPE)

        sources = raw["sources"]
        if not isinstance(sources, dict):
            _fail("invalid-type", f"{page_path}.sources", "must be an object")
        _exact(sources, frozenset({"common", "windows", "hmi"}), f"{page_path}.sources")
        for host in ("common", "windows", "hmi"):
            source_values = _strings(sources[host], f"{page_path}.sources.{host}", re.compile(r".+"))
            for source_index, source in enumerate(source_values):
                source_path = f"{page_path}.sources.{host}[{source_index}]"
                resolved = _file(
                    profile,
                    repository_root,
                    source,
                    source_path,
                    containment_root=page_root,
                )
                if resolved.suffix.lower() not in (".c", ".cc", ".cpp", ".cxx"):
                    _fail("invalid-source-extension", source_path, "source must be a C/C++ file")

        _strings(raw["capabilities"], f"{page_path}.capabilities", _ID)
        _strings(raw["verification"], f"{page_path}.verification", _TEST_ID)
        pages.append(PageDefinition(raw))

    catalog_manifests = [
        profile.root / value
        for value in profile.data["config"]
        if Path(value).name == "catalog_manifest.json"
    ]
    if len(catalog_manifests) != 1:
        _fail(
            "missing-page-catalog",
            f"{profile.manifest_path}.config",
            "sample page registry requires exactly one catalog_manifest.json",
        )
    catalog_label = catalog_manifests[0].relative_to(repository_root).as_posix()
    catalog_data = _read(catalog_manifests[0], catalog_label)
    entries = catalog_data.get("entries")
    if not isinstance(entries, list):
        _fail("invalid-page-catalog", f"{catalog_label}.entries", "must be an array")
    catalog_keys = {
        entry.get("key") for entry in entries if isinstance(entry, dict)
    }
    for page in pages:
        for field in ("title_key", "description_key"):
            key = page.data[field]
            if key not in catalog_keys:
                _fail(
                    "unknown-page-catalog-key",
                    f"{label}.pages[{data['pages'].index(page.data)}].{field}",
                    f"catalog key '{key}' does not exist",
                )

    pages.sort(key=lambda page: (page.data["order"], page.id))
    return PageRegistry(registry_path, tuple(pages), data)


def check_page_source_isolation(repository_root: Path, profile: Profile) -> None:
    """Reject platform/main coupling and direct page knowledge in suite composition."""
    registry = load_page_registry(repository_root, profile)
    if registry is None:
        return

    forbidden = (
        "Arduino", "freertos", "esp_", "driver/", "platform/windows",
        "platform/hmi", "boards/", "apps/main", "MainApp",
    )
    generic_files: list[Path] = [profile.root / profile.data["entry"]["header"]]
    for group in ("common", "windows", "hmi"):
        generic_files.extend(profile.root / value for value in profile.data["sources"][group])
    page_tokens = tuple(
        token
        for page in registry.pages
        for token in (page.id, page.id.replace("-", "_"))
    )
    for path in generic_files:
        text = path.read_text(encoding="utf-8")
        for token in forbidden:
            if token in text:
                _fail(
                    "sample-composition-coupling",
                    path.relative_to(repository_root).as_posix(),
                    f"generic sample composition directly references '{token}'",
                )
        for token in page_tokens:
            # 짧은 page ID가 wifi_registration 같은 공용 service 식별자의
            # 접두어일 수 있다. C/C++ identifier 경계 밖의 독립 token만 page
            # 직접 결합으로 본다.
            pattern = rf"(?<![A-Za-z0-9_]){re.escape(token)}(?![A-Za-z0-9_])"
            if re.search(pattern, text):
                _fail(
                    "sample-composition-coupling",
                    path.relative_to(repository_root).as_posix(),
                    f"generic sample composition directly references '{token}'",
                )

    for page in registry.pages:
        files = [profile.root / page.data["entry"]["header"]]
        for group in ("common", "windows", "hmi"):
            files.extend(profile.root / value for value in page.data["sources"][group])
        other_page_tokens = tuple(
            token
            for other in registry.pages
            if other.id != page.id
            for token in (other.id, other.id.replace("-", "_"))
        )
        for path in files:
            text = path.read_text(encoding="utf-8")
            for token in forbidden:
                if token in text:
                    _fail(
                        "sample-page-coupling",
                        path.relative_to(repository_root).as_posix(),
                        f"page '{page.id}' references forbidden boundary '{token}'",
                    )
            for token in other_page_tokens:
                pattern = rf"(?<![A-Za-z0-9_]){re.escape(token)}(?![A-Za-z0-9_])"
                if re.search(pattern, text):
                    _fail(
                        "sample-page-coupling",
                        path.relative_to(repository_root).as_posix(),
                        f"page '{page.id}' references forbidden boundary '{token}'",
                    )
