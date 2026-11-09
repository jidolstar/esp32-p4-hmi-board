"""Versioned profile registry loader and deterministic validator."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

from .paths import PathRuleError, resolve_contained_path, validate_logical_path

SCHEMA_VERSION = 1
SUPPORTED_HOSTS = ("windows", "hmi")
SUPPORTED_KINDS = ("application", "sample")
SUPPORTED_MODULES = frozenset(
    {"connectivity", "i18n", "settings", "text", "ui"}
)

REGISTRY_REQUIRED_KEYS = frozenset(
    {"schema_version", "default_profile", "profiles"}
)
REGISTRY_ALLOWED_KEYS = REGISTRY_REQUIRED_KEYS
PROFILE_REQUIRED_KEYS = frozenset(
    {
        "schema_version",
        "id",
        "kind",
        "description",
        "entry",
        "sources",
        "include_dirs",
        "modules",
        "hosts",
        "assets",
        "config",
        "data",
        "verification",
    }
)
PROFILE_ALLOWED_KEYS = PROFILE_REQUIRED_KEYS

_PROFILE_ID_PATTERN = re.compile(r"^[a-z][a-z0-9-]{0,31}$")
_CPP_TYPE_PATTERN = re.compile(
    r"^[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)+$"
)
_NVS_PATTERN = re.compile(r"^[a-z][a-z0-9_]*$")
_TEST_ID_PATTERN = re.compile(r"^[a-z][a-z0-9_-]{0,63}$")


class ManifestError(ValueError):
    """A stable validation error containing a code and JSON-style location."""

    def __init__(self, code: str, path: str, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.path = path
        self.message = message

    def __str__(self) -> str:
        return f"[{self.code}] {self.path}: {self.message}"


@dataclass(frozen=True)
class Profile:
    id: str
    kind: str
    description: str
    manifest_path: Path
    root: Path
    data: dict[str, Any]

    @property
    def enabled_hosts(self) -> tuple[str, ...]:
        return tuple(
            host
            for host in SUPPORTED_HOSTS
            if self.data["hosts"].get(host, {}).get("enabled") is True
        )


@dataclass(frozen=True)
class ProfileRegistry:
    path: Path
    default_profile: str
    profiles: tuple[Profile, ...]

    def get(self, profile_id: str) -> Profile:
        for profile in self.profiles:
            if profile.id == profile_id:
                return profile
        raise ManifestError(
            "unknown-profile", "--profile", f"profile '{profile_id}' is not registered"
        )


def _object_without_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ManifestError(
                "duplicate-key", "$", f"JSON object contains duplicate key '{key}'"
            )
        result[key] = value
    return result


def _read_json(path: Path, display_path: str | None = None) -> dict[str, Any]:
    label = display_path or str(path)
    try:
        with path.open("r", encoding="utf-8") as stream:
            value = json.load(stream, object_pairs_hook=_object_without_duplicate_keys)
    except ManifestError as error:
        raise ManifestError(error.code, label, error.message) from error
    except FileNotFoundError as error:
        raise ManifestError("missing-file", label, "file does not exist") from error
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ManifestError("invalid-json", label, str(error)) from error
    if not isinstance(value, dict):
        raise ManifestError("invalid-type", label, "root value must be an object")
    return value


def _expect_object(value: Any, path: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ManifestError("invalid-type", path, "must be an object")
    return value


def _expect_list(value: Any, path: str) -> list[Any]:
    if not isinstance(value, list):
        raise ManifestError("invalid-type", path, "must be an array")
    return value


def _expect_string(value: Any, path: str) -> str:
    if not isinstance(value, str) or not value:
        raise ManifestError("invalid-type", path, "must be a non-empty string")
    return value


def _expect_exact_keys(
    value: dict[str, Any],
    *,
    path: str,
    required: Iterable[str],
    allowed: Iterable[str],
) -> None:
    required_set = set(required)
    allowed_set = set(allowed)
    missing = sorted(required_set - value.keys())
    if missing:
        raise ManifestError(
            "missing-field", path, f"missing required field '{missing[0]}'"
        )
    unknown = sorted(value.keys() - allowed_set)
    if unknown:
        raise ManifestError("unknown-field", path, f"unknown field '{unknown[0]}'")


def _expect_schema_version(value: Any, path: str) -> None:
    if type(value) is not int or value != SCHEMA_VERSION:
        raise ManifestError(
            "unsupported-version",
            path,
            f"expected schema version {SCHEMA_VERSION}, got {value!r}",
        )


def _expect_unique_strings(
    value: Any, path: str, *, pattern: re.Pattern[str] | None = None
) -> list[str]:
    entries = _expect_list(value, path)
    seen: set[str] = set()
    result: list[str] = []
    for index, entry in enumerate(entries):
        entry_path = f"{path}[{index}]"
        text = _expect_string(entry, entry_path)
        if pattern is not None and pattern.fullmatch(text) is None:
            raise ManifestError("invalid-value", entry_path, f"invalid value '{text}'")
        if text in seen:
            raise ManifestError("duplicate-value", entry_path, f"duplicate value '{text}'")
        seen.add(text)
        result.append(text)
    return result


def _path_error(error: PathRuleError, path: str) -> ManifestError:
    return ManifestError(error.code, path, str(error))


def _resolve_profile_path(
    value: Any,
    *,
    field_path: str,
    profile_root: Path,
    repository_root: Path,
    other_profile_roots: tuple[Path, ...],
    expected: str,
) -> Path:
    try:
        resolved = resolve_contained_path(
            value,
            base=profile_root,
            repository_root=repository_root,
            containment_root=profile_root,
            other_profile_roots=other_profile_roots,
        )
    except PathRuleError as error:
        raise _path_error(error, field_path) from error

    if expected == "file" and not resolved.is_file():
        raise ManifestError("missing-file", field_path, f"file '{value}' does not exist")
    if expected == "directory" and not resolved.is_dir():
        raise ManifestError(
            "missing-directory", field_path, f"directory '{value}' does not exist"
        )
    return resolved


def _validate_profile(
    data: dict[str, Any],
    *,
    manifest_path: Path,
    repository_root: Path,
    other_profile_roots: tuple[Path, ...],
) -> Profile:
    label = manifest_path.relative_to(repository_root).as_posix()
    _expect_exact_keys(
        data,
        path=label,
        required=PROFILE_REQUIRED_KEYS,
        allowed=PROFILE_ALLOWED_KEYS,
    )
    _expect_schema_version(data["schema_version"], f"{label}.schema_version")

    profile_id = _expect_string(data["id"], f"{label}.id")
    if _PROFILE_ID_PATTERN.fullmatch(profile_id) is None:
        raise ManifestError("invalid-id", f"{label}.id", f"invalid profile ID '{profile_id}'")
    kind = _expect_string(data["kind"], f"{label}.kind")
    if kind not in SUPPORTED_KINDS:
        raise ManifestError("invalid-kind", f"{label}.kind", f"unsupported kind '{kind}'")
    description = _expect_string(data["description"], f"{label}.description")

    profile_root = manifest_path.parent.resolve()
    entry = _expect_object(data["entry"], f"{label}.entry")
    _expect_exact_keys(
        entry,
        path=f"{label}.entry",
        required=("header", "type"),
        allowed=("header", "type"),
    )
    _resolve_profile_path(
        entry["header"],
        field_path=f"{label}.entry.header",
        profile_root=profile_root,
        repository_root=repository_root,
        other_profile_roots=other_profile_roots,
        expected="file",
    )
    entry_type = _expect_string(entry["type"], f"{label}.entry.type")
    if _CPP_TYPE_PATTERN.fullmatch(entry_type) is None:
        raise ManifestError(
            "invalid-entry-type", f"{label}.entry.type", "must be a qualified C++ type"
        )

    sources = _expect_object(data["sources"], f"{label}.sources")
    _expect_exact_keys(
        sources,
        path=f"{label}.sources",
        required=("common", "windows", "hmi"),
        allowed=("common", "windows", "hmi"),
    )
    for source_kind in ("common", "windows", "hmi"):
        source_values = _expect_unique_strings(
            sources[source_kind], f"{label}.sources.{source_kind}"
        )
        for index, source in enumerate(source_values):
            source_path = f"{label}.sources.{source_kind}[{index}]"
            resolved = _resolve_profile_path(
                source,
                field_path=source_path,
                profile_root=profile_root,
                repository_root=repository_root,
                other_profile_roots=other_profile_roots,
                expected="file",
            )
            if resolved.suffix.lower() not in (".c", ".cc", ".cpp", ".cxx"):
                raise ManifestError(
                    "invalid-source-extension", source_path, "source must be a C/C++ file"
                )

    include_dirs = _expect_unique_strings(data["include_dirs"], f"{label}.include_dirs")
    for index, include_dir in enumerate(include_dirs):
        _resolve_profile_path(
            include_dir,
            field_path=f"{label}.include_dirs[{index}]",
            profile_root=profile_root,
            repository_root=repository_root,
            other_profile_roots=other_profile_roots,
            expected="directory",
        )

    modules = _expect_unique_strings(
        data["modules"], f"{label}.modules", pattern=_PROFILE_ID_PATTERN
    )
    for index, module in enumerate(modules):
        if module not in SUPPORTED_MODULES:
            raise ManifestError(
                "unknown-module",
                f"{label}.modules[{index}]",
                f"module '{module}' is not registered",
            )
        if not (repository_root / "modules" / module).is_dir():
            raise ManifestError(
                "missing-module",
                f"{label}.modules[{index}]",
                f"registered module directory 'modules/{module}' does not exist",
            )

    hosts = _expect_object(data["hosts"], f"{label}.hosts")
    unknown_hosts = sorted(hosts.keys() - set(SUPPORTED_HOSTS))
    if unknown_hosts:
        raise ManifestError(
            "unknown-host", f"{label}.hosts", f"unsupported host '{unknown_hosts[0]}'"
        )
    if not hosts:
        raise ManifestError("missing-host", f"{label}.hosts", "at least one host is required")
    enabled_count = 0
    for host, host_data_value in hosts.items():
        host_path = f"{label}.hosts.{host}"
        host_data = _expect_object(host_data_value, host_path)
        required = ("enabled", "board") if host == "hmi" else ("enabled",)
        _expect_exact_keys(host_data, path=host_path, required=required, allowed=required)
        if type(host_data["enabled"]) is not bool:
            raise ManifestError("invalid-type", f"{host_path}.enabled", "must be a boolean")
        if host_data["enabled"]:
            enabled_count += 1
        if host == "hmi":
            board = _expect_string(host_data["board"], f"{host_path}.board")
            if _PROFILE_ID_PATTERN.fullmatch(board) is None:
                raise ManifestError("invalid-board", f"{host_path}.board", "invalid board ID")
            if host_data["enabled"] and not (repository_root / "boards" / board).is_dir():
                raise ManifestError(
                    "missing-board",
                    f"{host_path}.board",
                    f"board directory 'boards/{board}' does not exist",
                )
    if enabled_count == 0:
        raise ManifestError("missing-host", f"{label}.hosts", "at least one host must be enabled")

    asset_paths = _expect_unique_strings(data["assets"], f"{label}.assets")
    for index, value in enumerate(asset_paths):
        resolved = _resolve_profile_path(
            value,
            field_path=f"{label}.assets[{index}]",
            profile_root=profile_root,
            repository_root=repository_root,
            other_profile_roots=other_profile_roots,
            expected="existing",
        )
        if not resolved.is_file() and not resolved.is_dir():
            raise ManifestError(
                "missing-asset",
                f"{label}.assets[{index}]",
                f"asset file or directory '{value}' does not exist",
            )

    config_paths = _expect_unique_strings(data["config"], f"{label}.config")
    for index, value in enumerate(config_paths):
        _resolve_profile_path(
            value,
            field_path=f"{label}.config[{index}]",
            profile_root=profile_root,
            repository_root=repository_root,
            other_profile_roots=other_profile_roots,
            expected="file",
        )

    data_config = _expect_object(data["data"], f"{label}.data")
    _expect_exact_keys(
        data_config,
        path=f"{label}.data",
        required=("windows_root", "hmi_sd_root", "nvs_namespace"),
        allowed=("windows_root", "hmi_sd_root", "nvs_namespace"),
    )
    for field in ("windows_root", "hmi_sd_root"):
        try:
            validate_logical_path(data_config[field])
        except PathRuleError as error:
            raise _path_error(error, f"{label}.data.{field}") from error
    namespace = _expect_string(data_config["nvs_namespace"], f"{label}.data.nvs_namespace")
    if len(namespace) > 15 or _NVS_PATTERN.fullmatch(namespace) is None:
        raise ManifestError(
            "invalid-namespace",
            f"{label}.data.nvs_namespace",
            "must match ^[a-z][a-z0-9_]*$ and be at most 15 characters",
        )

    verification = _expect_object(data["verification"], f"{label}.verification")
    _expect_exact_keys(
        verification,
        path=f"{label}.verification",
        required=("tests",),
        allowed=("tests",),
    )
    _expect_unique_strings(
        verification["tests"], f"{label}.verification.tests", pattern=_TEST_ID_PATTERN
    )

    return Profile(
        id=profile_id,
        kind=kind,
        description=description,
        manifest_path=manifest_path,
        root=profile_root,
        data=data,
    )


def load_registry(repository_root: Path, registry_path: Path | None = None) -> ProfileRegistry:
    repository_root = repository_root.resolve()
    if registry_path is None:
        registry_path = repository_root / "profiles" / "registry.json"
    registry_path = registry_path.resolve()
    try:
        label = registry_path.relative_to(repository_root).as_posix()
    except ValueError as error:
        raise ManifestError(
            "repository-path-escape",
            "registry",
            "registry path resolves outside the repository",
        ) from error
    registry = _read_json(registry_path, label)
    _expect_exact_keys(
        registry,
        path=label,
        required=REGISTRY_REQUIRED_KEYS,
        allowed=REGISTRY_ALLOWED_KEYS,
    )
    _expect_schema_version(registry["schema_version"], f"{label}.schema_version")
    default_profile = _expect_string(
        registry["default_profile"], f"{label}.default_profile"
    )
    if _PROFILE_ID_PATTERN.fullmatch(default_profile) is None:
        raise ManifestError(
            "invalid-id", f"{label}.default_profile", "invalid default profile ID"
        )

    manifest_values = _expect_unique_strings(registry["profiles"], f"{label}.profiles")
    if not manifest_values:
        raise ManifestError("missing-profile", f"{label}.profiles", "must not be empty")

    manifest_paths: list[Path] = []
    for index, value in enumerate(manifest_values):
        try:
            resolved = resolve_contained_path(
                value,
                base=registry_path.parent,
                repository_root=repository_root,
                containment_root=repository_root,
            )
        except PathRuleError as error:
            raise _path_error(error, f"{label}.profiles[{index}]") from error
        if not resolved.is_file():
            raise ManifestError(
                "missing-file",
                f"{label}.profiles[{index}]",
                f"profile manifest '{value}' does not exist",
            )
        manifest_paths.append(resolved)

    profile_roots = tuple(path.parent.resolve() for path in manifest_paths)
    profiles: list[Profile] = []
    ids: dict[str, Path] = {}
    namespaces: dict[str, Path] = {}
    for manifest_path in manifest_paths:
        other_roots = tuple(root for root in profile_roots if root != manifest_path.parent.resolve())
        profile = _validate_profile(
            _read_json(
                manifest_path,
                manifest_path.relative_to(repository_root).as_posix(),
            ),
            manifest_path=manifest_path,
            repository_root=repository_root,
            other_profile_roots=other_roots,
        )
        if profile.id in ids:
            raise ManifestError(
                "duplicate-profile-id",
                f"{manifest_path}.id",
                f"profile ID '{profile.id}' is already declared by '{ids[profile.id].name}'",
            )
        ids[profile.id] = manifest_path
        namespace = profile.data["data"]["nvs_namespace"]
        if namespace in namespaces:
            raise ManifestError(
                "duplicate-nvs-namespace",
                f"{manifest_path}.data.nvs_namespace",
                f"NVS namespace '{namespace}' is already declared by '{namespaces[namespace].name}'",
            )
        namespaces[namespace] = manifest_path
        profiles.append(profile)

    if default_profile not in ids:
        raise ManifestError(
            "unknown-default-profile",
            f"{label}.default_profile",
            f"default profile '{default_profile}' is not registered",
        )
    result = ProfileRegistry(
        path=registry_path,
        default_profile=default_profile,
        profiles=tuple(profiles),
    )
    # sample profile의 page 계약도 doctor/list 전에 함께 검증한다.
    from .pages import load_page_registry

    for profile in result.profiles:
        load_page_registry(repository_root, profile)
    return result
