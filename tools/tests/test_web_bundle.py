from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from tools.assets.web.embed_web_bundle import BundleError, load_config, render_bundle, run


class WebBundleTest(unittest.TestCase):
    def make_fixture(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name)
        (root / "web").mkdir()
        (root / "web/index.html").write_text(
            "<style>{{STYLE}}</style><script>{{SCRIPT}}</script>", encoding="utf-8"
        )
        (root / "web/app.css").write_text("body{color:#123}", encoding="utf-8")
        (root / "web/app.js").write_text("console.log('local')", encoding="utf-8")
        config = {
            "schema_version": 1,
            "owner_root": ".",
            "html": "web/index.html",
            "style": "web/app.css",
            "script": "web/app.js",
            "generated_header": "generated/web_bundle.hpp",
            "cpp_namespace": "fixture::generated",
            "max_source_bytes": 1024,
            "max_bundle_bytes": 4096,
        }
        path = root / "web_bundle.json"
        path.write_text(json.dumps(config), encoding="utf-8")
        return temporary, path

    def test_generate_and_check_are_deterministic(self) -> None:
        temporary, path = self.make_fixture()
        with temporary:
            output, digest = run(path, False)
            self.assertTrue(output.is_file())
            self.assertEqual(64, len(digest))
            run(path, True)
            header = output.read_text(encoding="utf-8")
            self.assertIn("body{color:#123}", header)
            self.assertIn("console.log('local')", header)

    def test_stale_output_is_rejected(self) -> None:
        temporary, path = self.make_fixture()
        with temporary:
            output, _ = run(path, False)
            output.write_text("stale", encoding="utf-8")
            with self.assertRaisesRegex(BundleError, "stale"):
                run(path, True)

    def test_external_resource_is_rejected(self) -> None:
        temporary, path = self.make_fixture()
        with temporary:
            root = path.parent
            (root / "web/app.js").write_text(
                "fetch('https://example.com/data')", encoding="utf-8"
            )
            with self.assertRaisesRegex(BundleError, "external"):
                render_bundle(load_config(path))

    def test_path_traversal_and_duplicate_source_are_rejected(self) -> None:
        temporary, path = self.make_fixture()
        with temporary:
            raw = json.loads(path.read_text(encoding="utf-8"))
            raw["script"] = "../outside.js"
            path.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaisesRegex(BundleError, "escapes"):
                load_config(path)

            raw["script"] = raw["style"]
            path.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaisesRegex(BundleError, "unique"):
                load_config(path)


if __name__ == "__main__":
    unittest.main()
