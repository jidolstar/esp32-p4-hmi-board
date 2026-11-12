from __future__ import annotations

import shutil
import json
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = TOOLS_ROOT.parent
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from assets.typography.generate import (  # noqa: E402
    CatalogError,
    load_inputs,
    render_outputs,
)
from assets.typography.owner_config import (  # noqa: E402
    OwnerConfigError,
    load_owner_config,
)


class TypographyGenerationTest(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.input_directory = Path(temporary.name) / "i18n"
        source = REPOSITORY_ROOT / "samples" / "suite" / "i18n"
        shutil.copytree(source, self.input_directory)
        self.owner = load_owner_config(self.input_directory / "catalog.json")

    def replace(self, filename: str, old: str, new: str) -> None:
        path = self.input_directory / filename
        text = path.read_text(encoding="utf-8")
        path.write_text(text.replace(old, new), encoding="utf-8", newline="\n")

    def assert_error(self, code: str) -> None:
        with self.assertRaises(CatalogError) as captured:
            load_inputs(self.owner.catalogs, self.owner.typography)
        self.assertEqual(code, captured.exception.code)

    def test_valid_catalogs_have_key_placeholder_and_role_parity(self) -> None:
        inputs = load_inputs(self.owner.catalogs, self.owner.typography)
        self.assertEqual(set(inputs.catalogs["ko-KR"]), set(inputs.catalogs["en-US"]))
        self.assertEqual(set(inputs.catalogs["ko-KR"]), set(inputs.roles))
        self.assertEqual(404, len(inputs.roles))

    def test_same_input_renders_identical_bytes(self) -> None:
        self.assertEqual(render_outputs(self.owner), render_outputs(self.owner))

    def test_rejects_missing_locale_key(self) -> None:
        self.replace("en.yaml", '  common.open: "Open"\n', "")
        self.assert_error("catalog-key-mismatch")

    def test_rejects_placeholder_type_mismatch(self) -> None:
        self.replace("en.yaml", "{count:u32}", "{count:string}")
        self.assert_error("placeholder-mismatch")

    def test_rejects_untyped_placeholder(self) -> None:
        self.replace("ko.yaml", "{count:u32}", "{count}")
        self.assert_error("invalid-placeholder")

    def test_rejects_missing_typography_role(self) -> None:
        self.replace("typography.yaml", "  common.open: control\n", "")
        self.assert_error("typography-key-mismatch")

    def test_rejects_unknown_typography_role(self) -> None:
        self.replace("typography.yaml", "  common.open: control", "  common.open: unknown")
        self.assert_error("unknown-typography-role")

    def test_generated_header_keeps_catalogs_non_owning_and_typed(self) -> None:
        header = render_outputs(self.owner)["text_catalog.hpp"]
        self.assertIn("enum class TextId : std::uint16_t", header)
        self.assertIn("hmi::i18n::CatalogView", header)
        self.assertIn("TypographyRole::key_action", header)
        self.assertIn("TextId text_id(std::string_view key)", header)
        self.assertIn("text(hmi::i18n::Locale locale,\n                                               std::string_view key)", header)
        self.assertNotIn("std::string ", header)

    def test_two_owners_keep_namespace_symbol_and_outputs_isolated(self) -> None:
        roots = []
        rendered = []
        for suffix in ("alpha", "beta"):
            root = self.input_directory.parent / suffix
            shutil.copytree(self.input_directory, root / "i18n")
            config_path = root / "i18n" / "catalog.json"
            config = json.loads(config_path.read_text(encoding="utf-8"))
            config["owner_id"] = suffix
            config["cpp_namespace"] = f"fixture::{suffix}::generated"
            config["symbol_prefix"] = f"fixture_{suffix}"
            config_path.write_text(
                json.dumps(config, indent=2) + "\n", encoding="utf-8", newline="\n"
            )
            owner = load_owner_config(config_path)
            roots.append(owner.generated_output)
            rendered.append(render_outputs(owner))

        self.assertNotEqual(roots[0], roots[1])
        self.assertIn("namespace fixture::alpha::generated", rendered[0]["text_catalog.hpp"])
        self.assertIn("namespace fixture::beta::generated", rendered[1]["text_catalog.hpp"])
        self.assertIn('"symbol_prefix": "fixture_alpha"', rendered[0]["catalog_manifest.json"])
        self.assertIn('"symbol_prefix": "fixture_beta"', rendered[1]["catalog_manifest.json"])
        self.assertNotIn("fixture::beta", rendered[0]["text_catalog.hpp"])

    def test_owner_config_rejects_output_escape(self) -> None:
        config_path = self.input_directory / "catalog.json"
        config = json.loads(config_path.read_text(encoding="utf-8"))
        config["paths"]["generated_output"] = "../../escaped"
        config_path.write_text(
            json.dumps(config, indent=2) + "\n", encoding="utf-8", newline="\n"
        )
        with self.assertRaises(OwnerConfigError) as captured:
            load_owner_config(config_path)
        self.assertEqual("path-escape", captured.exception.code)

    def test_owner_config_rejects_invalid_namespace_and_output_collision(self) -> None:
        config_path = self.input_directory / "catalog.json"
        config = json.loads(config_path.read_text(encoding="utf-8"))
        config["cpp_namespace"] = "fixture::bad-name"
        config_path.write_text(
            json.dumps(config, indent=2) + "\n", encoding="utf-8", newline="\n"
        )
        with self.assertRaises(OwnerConfigError) as captured:
            load_owner_config(config_path)
        self.assertEqual("invalid-cpp-namespace", captured.exception.code)

        config["cpp_namespace"] = "fixture::generated"
        config["paths"]["user_output"] = config["paths"]["generated_output"]
        config_path.write_text(
            json.dumps(config, indent=2) + "\n", encoding="utf-8", newline="\n"
        )
        with self.assertRaises(OwnerConfigError) as captured:
            load_owner_config(config_path)
        self.assertEqual("output-collision", captured.exception.code)


if __name__ == "__main__":
    unittest.main()
