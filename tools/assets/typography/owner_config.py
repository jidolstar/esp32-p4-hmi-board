"""Load a bounded owner configuration for reusable typography generators."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any


SUPPORTED_LOCALES = ("ko-KR", "en-US")
_OWNER_ID = re.compile(r"^[a-z][a-z0-9-]{0,63}$")
_CPP_IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


class OwnerConfigError(ValueError):
    """A deterministic typography owner configuration failure."""

    def __init__(self, code: str, path: str, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.path = path
        self.message = message

    def __str__(self) -> str:
        return f"[{self.code}] {self.path}: {self.message}"


@dataclass(frozen=True)
class TypographyOwnerConfig:
    path: Path
    owner_id: str
    owner_root: Path
    catalogs: dict[str, Path]
    typography: Path
    fonts: Path
    generated_output: Path
    user_output: Path
    cpp_namespace: str
    symbol_prefix: str
    user_asset_prefix: str


def _fail(code: str, path: str, message: str) -> None:
    raise OwnerConfigError(code, path, message)


def _mapping(value: Any, path: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        _fail("invalid-type", path, "expected an object")
    return value


def _string(value: Any, path: str) -> str:
    if not isinstance(value, str) or not value:
        _fail("invalid-type", path, "expected a non-empty string")
    return value


def _exact(value: dict[str, Any], expected: set[str], path: str) -> None:
    missing = sorted(expected - value.keys())
    extra = sorted(value.keys() - expected)
    if missing or extra:
        _fail("invalid-fields", path, f"missing={missing}, extra={extra}")


def _contained_path(
    value: Any,
    *,
    base: Path,
    containment_root: Path,
    path: str,
    require_file: bool = False,
) -> Path:
    raw = _string(value, path)
    candidate = Path(raw)
    if candidate.is_absolute():
        _fail("absolute-path", path, raw)
    resolved = (base / candidate).resolve()
    try:
        resolved.relative_to(containment_root.resolve())
    except ValueError:
        _fail("path-escape", path, raw)
    if require_file and not resolved.is_file():
        _fail("missing-input", path, raw)
    return resolved


def _runtime_prefix(value: Any, path: str) -> str:
    raw = _string(value, path).replace("\\", "/")
    parsed = PurePosixPath(raw)
    if parsed.is_absolute() or any(part in ("", ".", "..") for part in parsed.parts):
        _fail("invalid-runtime-prefix", path, raw)
    return parsed.as_posix().rstrip("/")


def load_owner_config(path: Path) -> TypographyOwnerConfig:
    config_path = path.resolve()
    label = config_path.as_posix()
    try:
        data = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        _fail("read-error", label, str(error))
    root = _mapping(data, label)
    _exact(
        root,
        {
            "schema_version",
            "owner_id",
            "cpp_namespace",
            "symbol_prefix",
            "paths",
        },
        label,
    )
    if root["schema_version"] != 1:
        _fail("unsupported-version", label, "expected schema version 1")

    owner_id = _string(root["owner_id"], f"{label}.owner_id")
    if _OWNER_ID.fullmatch(owner_id) is None:
        _fail("invalid-owner-id", f"{label}.owner_id", owner_id)

    cpp_namespace = _string(root["cpp_namespace"], f"{label}.cpp_namespace")
    if any(_CPP_IDENTIFIER.fullmatch(part) is None for part in cpp_namespace.split("::")):
        _fail("invalid-cpp-namespace", f"{label}.cpp_namespace", cpp_namespace)
    symbol_prefix = _string(root["symbol_prefix"], f"{label}.symbol_prefix")
    if _CPP_IDENTIFIER.fullmatch(symbol_prefix) is None:
        _fail("invalid-symbol-prefix", f"{label}.symbol_prefix", symbol_prefix)

    paths = _mapping(root["paths"], f"{label}.paths")
    _exact(
        paths,
        {
            "owner_root",
            "catalogs",
            "typography",
            "fonts",
            "generated_output",
            "user_output",
            "user_asset_prefix",
        },
        f"{label}.paths",
    )
    config_directory = config_path.parent
    owner_root = _contained_path(
        paths["owner_root"],
        base=config_directory,
        containment_root=config_directory.parent,
        path=f"{label}.paths.owner_root",
    )
    catalogs_value = _mapping(paths["catalogs"], f"{label}.paths.catalogs")
    if set(catalogs_value) != set(SUPPORTED_LOCALES):
        _fail(
            "invalid-locales",
            f"{label}.paths.catalogs",
            f"expected={list(SUPPORTED_LOCALES)}, actual={sorted(catalogs_value)}",
        )
    catalogs = {
        locale: _contained_path(
            catalogs_value[locale],
            base=config_directory,
            containment_root=owner_root,
            path=f"{label}.paths.catalogs.{locale}",
            require_file=True,
        )
        for locale in SUPPORTED_LOCALES
    }
    typography = _contained_path(
        paths["typography"],
        base=config_directory,
        containment_root=owner_root,
        path=f"{label}.paths.typography",
        require_file=True,
    )
    fonts = _contained_path(
        paths["fonts"],
        base=config_directory,
        containment_root=owner_root,
        path=f"{label}.paths.fonts",
        require_file=True,
    )
    generated_output = _contained_path(
        paths["generated_output"],
        base=config_directory,
        containment_root=owner_root,
        path=f"{label}.paths.generated_output",
    )
    user_output = _contained_path(
        paths["user_output"],
        base=config_directory,
        containment_root=owner_root,
        path=f"{label}.paths.user_output",
    )
    if generated_output == user_output:
        _fail("output-collision", f"{label}.paths", "generated and user outputs match")
    input_paths = {*catalogs.values(), typography, fonts, config_path}
    for output_name, output in (
        ("generated_output", generated_output),
        ("user_output", user_output),
    ):
        if output in input_paths or any(
            input_path.is_relative_to(output) for input_path in input_paths
        ):
            _fail("output-input-collision", f"{label}.paths.{output_name}", output.as_posix())

    return TypographyOwnerConfig(
        path=config_path,
        owner_id=owner_id,
        owner_root=owner_root,
        catalogs=catalogs,
        typography=typography,
        fonts=fonts,
        generated_output=generated_output,
        user_output=user_output,
        cpp_namespace=cpp_namespace,
        symbol_prefix=symbol_prefix,
        user_asset_prefix=_runtime_prefix(
            paths["user_asset_prefix"], f"{label}.paths.user_asset_prefix"
        ),
    )
