"""Repository and profile path validation helpers."""

from __future__ import annotations

from pathlib import Path, PurePosixPath, PureWindowsPath

WILDCARD_CHARACTERS = "*?["


class PathRuleError(ValueError):
    """A path violates a manifest containment rule."""

    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code


def is_relative_to(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def validate_relative_text(value: object) -> str:
    if not isinstance(value, str) or not value:
        raise PathRuleError("invalid-path", "must be a non-empty string")
    if any(character in value for character in WILDCARD_CHARACTERS):
        raise PathRuleError("wildcard-path", "wildcards are not allowed")
    if PureWindowsPath(value).is_absolute() or PurePosixPath(value).is_absolute():
        raise PathRuleError("absolute-path", "absolute paths are not allowed")
    if PureWindowsPath(value).drive:
        raise PathRuleError("absolute-path", "drive-qualified paths are not allowed")
    return value


def resolve_contained_path(
    value: object,
    *,
    base: Path,
    repository_root: Path,
    containment_root: Path,
    other_profile_roots: tuple[Path, ...] = (),
) -> Path:
    relative = validate_relative_text(value)
    repository_root = repository_root.resolve()
    containment_root = containment_root.resolve()
    resolved = (base / relative).resolve()

    if not is_relative_to(resolved, repository_root):
        raise PathRuleError(
            "repository-path-escape", "path resolves outside the repository"
        )
    if not is_relative_to(resolved, containment_root):
        if any(is_relative_to(resolved, root.resolve()) for root in other_profile_roots):
            raise PathRuleError(
                "cross-profile-path", "path resolves inside another profile"
            )
        raise PathRuleError(
            "profile-path-escape", "path resolves outside the profile root"
        )
    return resolved


def validate_logical_path(value: object) -> str:
    relative = validate_relative_text(value)
    parts = PurePosixPath(relative.replace("\\", "/")).parts
    if any(part in ("", ".", "..") for part in parts):
        raise PathRuleError(
            "logical-path-escape", "logical paths may not contain '.' or '..'"
        )
    return relative
