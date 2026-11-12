from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = TOOLS_ROOT.parent
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from hmi_project.manifest import (  # noqa: E402
    PROFILE_ALLOWED_KEYS,
    PROFILE_REQUIRED_KEYS,
    REGISTRY_ALLOWED_KEYS,
    REGISTRY_REQUIRED_KEYS,
    ManifestError,
    load_registry,
    SUPPORTED_MODULES,
)


def make_profile(profile_id: str = "main", namespace: str = "hmi_main") -> dict:
    return {
        "schema_version": 1,
        "id": profile_id,
        "kind": "application",
        "description": f"Fixture profile {profile_id}.",
        "entry": {"header": "app.hpp", "type": f"fixture::{profile_id.replace('-', '_')}::App"},
        "sources": {"common": ["app.cpp"], "windows": [], "hmi": []},
        "include_dirs": ["."],
        "modules": [],
        "hosts": {
            "windows": {"enabled": True},
            "hmi": {"enabled": True, "board": "jc1060p470c"},
        },
        "assets": [],
        "config": [],
        "data": {
            "windows_root": profile_id,
            "hmi_sd_root": f"hmi/{profile_id}",
            "nvs_namespace": namespace,
        },
        "verification": {"tests": [f"{profile_id.replace('-', '_')}_smoke"]},
    }


class FixtureRepository:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.registry_path = root / "profiles" / "registry.json"
        self.registry = {
            "schema_version": 1,
            "default_profile": "main",
            "profiles": ["../apps/main/profile.json"],
        }
        self.profiles: dict[str, dict] = {"main": make_profile()}

    def add_profile(
        self, directory: str, profile: dict, registry_path: str | None = None
    ) -> None:
        self.profiles[directory] = profile
        self.registry["profiles"].append(
            registry_path or f"../apps/{directory}/profile.json"
        )

    def write(self) -> None:
        self.registry_path.parent.mkdir(parents=True, exist_ok=True)
        (self.root / "boards" / "jc1060p470c").mkdir(parents=True, exist_ok=True)
        self.registry_path.write_text(
            json.dumps(self.registry, indent=2), encoding="utf-8"
        )
        for directory, profile in self.profiles.items():
            profile_root = self.root / "apps" / directory
            profile_root.mkdir(parents=True, exist_ok=True)
            (profile_root / "profile.json").write_text(
                json.dumps(profile, indent=2), encoding="utf-8"
            )
            (profile_root / "app.hpp").write_text("#pragma once\n", encoding="utf-8")
            (profile_root / "app.cpp").write_text("// fixture\n", encoding="utf-8")


class ManifestValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.root = Path(self.temporary_directory.name)
        self.fixture = FixtureRepository(self.root)

    def load(self):
        self.fixture.write()
        return load_registry(self.root)

    def assert_manifest_error(
        self, code: str, path_fragment: str, message_fragment: str = ""
    ) -> ManifestError:
        with self.assertRaises(ManifestError) as captured:
            self.load()
        error = captured.exception
        self.assertEqual(code, error.code)
        self.assertIn(path_fragment, error.path)
        self.assertIn(message_fragment, error.message)
        self.assertIn(code, str(error))
        self.assertIn(path_fragment, str(error))
        return error

    def test_real_registry_and_main_profile_are_valid(self) -> None:
        registry = load_registry(REPOSITORY_ROOT)
        self.assertEqual("main", registry.default_profile)
        self.assertEqual(
            ["main", "sample-suite"],
            [profile.id for profile in registry.profiles],
        )
        self.assertEqual(("windows", "hmi"), registry.get("main").enabled_hosts)
        self.assertIn("interfaces", SUPPORTED_MODULES)
        self.assertIn("interfaces", registry.get("sample-suite").data["modules"])
        self.assertIn("update", SUPPORTED_MODULES)
        self.assertIn("update", registry.get("sample-suite").data["modules"])

    def test_loader_does_not_write_repository_files(self) -> None:
        self.fixture.write()
        before = {
            path.relative_to(self.root): path.read_bytes()
            for path in self.root.rglob("*")
            if path.is_file()
        }
        load_registry(self.root)
        after = {
            path.relative_to(self.root): path.read_bytes()
            for path in self.root.rglob("*")
            if path.is_file()
        }
        self.assertEqual(before, after)

    def test_rejects_unsupported_profile_version(self) -> None:
        self.fixture.profiles["main"]["schema_version"] = 2
        self.assert_manifest_error(
            "unsupported-version", "schema_version", "expected schema version 1"
        )

    def test_rejects_unsupported_registry_version(self) -> None:
        self.fixture.registry["schema_version"] = 2
        self.assert_manifest_error(
            "unsupported-version", "schema_version", "expected schema version 1"
        )

    def test_rejects_duplicate_profile_id(self) -> None:
        self.fixture.add_profile("other", make_profile("main", "hmi_other"))
        self.assert_manifest_error("duplicate-profile-id", ".id", "already declared")

    def test_rejects_duplicate_nvs_namespace(self) -> None:
        self.fixture.add_profile("other", make_profile("other", "hmi_main"))
        self.assert_manifest_error(
            "duplicate-nvs-namespace", "data.nvs_namespace", "already declared"
        )

    def test_rejects_missing_source_file(self) -> None:
        self.fixture.profiles["main"]["sources"]["common"] = ["missing.cpp"]
        self.assert_manifest_error(
            "missing-file", "sources.common[0]", "does not exist"
        )

    def test_rejects_repository_path_escape(self) -> None:
        self.fixture.profiles["main"]["sources"]["common"] = [
            "../../../outside.cpp"
        ]
        self.assert_manifest_error(
            "repository-path-escape", "sources.common[0]", "outside the repository"
        )

    def test_rejects_cross_profile_source_reference(self) -> None:
        self.fixture.add_profile("other", make_profile("other", "hmi_other"))
        self.fixture.profiles["main"]["sources"]["common"] = ["../other/app.cpp"]
        self.assert_manifest_error(
            "cross-profile-path", "sources.common[0]", "another profile"
        )

    def test_rejects_wildcard_source(self) -> None:
        self.fixture.profiles["main"]["sources"]["common"] = ["*.cpp"]
        self.assert_manifest_error(
            "wildcard-path", "sources.common[0]", "wildcards are not allowed"
        )

    def test_rejects_absolute_source(self) -> None:
        absolute_source = (self.root / "apps" / "main" / "app.cpp").resolve()
        self.fixture.profiles["main"]["sources"]["common"] = [
            str(absolute_source)
        ]
        self.assert_manifest_error(
            "absolute-path", "sources.common[0]", "absolute paths are not allowed"
        )

    def test_rejects_unknown_module(self) -> None:
        self.fixture.profiles["main"]["modules"] = ["not-registered"]
        self.assert_manifest_error(
            "unknown-module", "modules[0]", "is not registered"
        )

    def test_rejects_unknown_profile_field(self) -> None:
        self.fixture.profiles["main"]["unexpected"] = True
        self.assert_manifest_error(
            "unknown-field", "profile.json", "unknown field 'unexpected'"
        )


class SchemaContractTest(unittest.TestCase):
    def read_schema(self, name: str) -> dict:
        path = REPOSITORY_ROOT / "profiles" / "schema" / name
        return json.loads(path.read_text(encoding="utf-8"))

    def test_registry_schema_and_loader_have_the_same_top_level_fields(self) -> None:
        schema = self.read_schema("registry.schema.json")
        self.assertEqual(REGISTRY_REQUIRED_KEYS, frozenset(schema["required"]))
        self.assertEqual(REGISTRY_ALLOWED_KEYS, frozenset(schema["properties"]))
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(1, schema["properties"]["schema_version"]["const"])

    def test_profile_schema_and_loader_have_the_same_top_level_fields(self) -> None:
        schema = self.read_schema("profile.schema.json")
        self.assertEqual(PROFILE_REQUIRED_KEYS, frozenset(schema["required"]))
        self.assertEqual(PROFILE_ALLOWED_KEYS, frozenset(schema["properties"]))
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(1, schema["properties"]["schema_version"]["const"])


if __name__ == "__main__":
    unittest.main()
