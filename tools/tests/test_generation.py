from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TOOLS_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = TOOLS_ROOT.parent
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from hmi_project.generate import (  # noqa: E402
    GenerationError,
    check_generated_isolation,
    prepare_profile,
    resolve_profile_inputs,
)
from hmi_project.manifest import ManifestError, load_registry  # noqa: E402


def file_snapshot(root: Path) -> dict[str, bytes]:
    return {
        path.relative_to(root).as_posix(): path.read_bytes()
        for path in root.rglob("*")
        if path.is_file()
    }


class GenerationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.output_root = Path(self.temporary_directory.name) / ".generated"
        self.registry = load_registry(REPOSITORY_ROOT)
        self.profile = self.registry.get("main")

    def prepare(self, host: str) -> Path:
        return prepare_profile(
            REPOSITORY_ROOT,
            self.registry,
            host,
            "main",
            output_root=self.output_root,
        )

    def test_hosts_share_profile_source_and_keep_platform_sources_isolated(self) -> None:
        windows = resolve_profile_inputs(REPOSITORY_ROOT, self.profile, "windows")
        hmi = resolve_profile_inputs(REPOSITORY_ROOT, self.profile, "hmi")

        self.assertEqual(
            ["apps/main/app.cpp"], windows["source_groups"]["profile"]
        )
        self.assertEqual(
            windows["source_groups"]["profile"], hmi["source_groups"]["profile"]
        )
        self.assertFalse(
            any(
                source.startswith(("hosts/hmi/", "platform/hmi/", "boards/"))
                for source in windows["all_sources"]
            )
        )
        self.assertFalse(
            any(
                source.startswith(("hosts/windows/", "platform/windows/"))
                for source in hmi["all_sources"]
            )
        )

    def test_windows_generation_has_alias_json_metadata_and_cmake_input(self) -> None:
        target = self.prepare("windows")
        self.assertEqual(
            {
                "asset_manifest.json",
                "metadata.json",
                "profile.cmake",
                "resolved_profile.json",
                "selected_profile.hpp",
            },
            {path.name for path in target.iterdir()},
        )
        selected = (target / "selected_profile.hpp").read_text(encoding="utf-8")
        self.assertIn('#include "apps/main/app.hpp"', selected)
        self.assertIn("using SelectedProfileApp = hmi::apps::main::MainApp;", selected)
        self.assertIn('kSelectedAssetRoot[] = "A:apps/main/"', selected)

    def test_sample_suite_generation_owns_page_factory_and_sources(self) -> None:
        target = prepare_profile(
            REPOSITORY_ROOT,
            self.registry,
            "windows",
            "sample-suite",
            output_root=self.output_root,
        )
        resolved = json.loads(
            (target / "resolved_profile.json").read_text(encoding="utf-8")
        )
        source = (target / "page_registry.cpp").read_text(encoding="utf-8")
        self.assertEqual(
            ["ui-basics", "text-input", "display-audio", "wifi"],
            [page["id"] for page in resolved["pages"]],
        )
        self.assertIn(
            "samples/suite/pages/ui_basics/ui_basics_page.cpp",
            resolved["source_groups"]["pages"],
        )
        self.assertIn(
            "samples/suite/pages/display_audio/display_audio_page.cpp",
            resolved["source_groups"]["pages"],
        )
        self.assertIn(
            "samples/suite/pages/wifi/wifi_settings_page.cpp",
            resolved["source_groups"]["pages"],
        )
        self.assertEqual(
            [
                "modules/connectivity/src/wifi_scan_service.cpp",
                "modules/connectivity/src/wifi_sta_registration.cpp",
                "modules/connectivity/src/wifi_station_settings.cpp",
                "modules/i18n/src/locale.cpp",
                "modules/i18n/src/format.cpp",
                "modules/settings/src/level_settings.cpp",
                "modules/text/src/utf8.cpp",
                "modules/text/src/editor.cpp",
                "modules/text/src/hangul_composer.cpp",
                "modules/text/src/input_model.cpp",
                "modules/text/src/keyboard_layout.cpp",
                "modules/ui/src/asset_update_overlay.cpp",
                "modules/ui/src/theme.cpp",
                "modules/ui/src/components.cpp",
                "modules/ui/src/font_repository.cpp",
                "modules/ui/src/user_text_label.cpp",
                "modules/ui/src/user_text_input_line.cpp",
                "modules/ui/src/virtual_keyboard.cpp",
                "platform/windows/connectivity/virtual_wifi_sta.cpp",
            ],
            resolved["source_groups"]["modules"],
        )
        self.assertIn("UiBasicsPage", source)
        self.assertIn('"ui-basics"', source)
        self.assertIn('"page.ui_basics.title"', source)
        self.assertIn('"display-audio"', source)
        self.assertIn('"wifi"', source)
        self.assertNotIn('"UI Basics"', source)
        self.assertEqual(177, len(resolved["assets"]))
        self.assertTrue(all(len(asset["sha256"]) == 64 for asset in resolved["assets"]))
        self.assertEqual(
            sorted(asset["relative_path"] for asset in resolved["assets"]),
            [asset["relative_path"] for asset in resolved["assets"]],
        )

        hmi_target = prepare_profile(
            REPOSITORY_ROOT,
            self.registry,
            "hmi",
            "sample-suite",
            output_root=self.output_root,
        )
        platformio_input = (hmi_target / "profile.ini").read_text(encoding="utf-8")
        self.assertIn("-D HMI_PROFILE_HAS_ASSETS=1", platformio_input)
        self.assertIn("-D HMI_PROFILE_HAS_SETTINGS=1", platformio_input)
        self.assertIn("-D HMI_PROFILE_HAS_CONNECTIVITY=1", platformio_input)
        self.assertIn(
            "+<platform/hmi/connectivity/esp_hosted_wifi_sta.cpp>",
            platformio_input,
        )
        self.assertIn(
            "+<platform/hmi/connectivity/preferences_wifi_station_settings_store.cpp>",
            platformio_input,
        )
        self.assertIn(
            "+<platform/hmi/settings/preferences_level_settings.cpp>",
            platformio_input,
        )
        self.assertIn(
            "+<boards/jc1060p470c/internal/audio_service.cpp>",
            platformio_input,
        )
        self.assertIn(
            "+<boards/jc1060p470c/internal/asset_usb_updater.cpp>",
            platformio_input,
        )

    def test_hmi_generation_has_explicit_platformio_allowlist_input(self) -> None:
        target = self.prepare("hmi")
        platformio_input = (target / "profile.ini").read_text(
            encoding="utf-8"
        )
        selection = (target / "selection.ini").read_text(encoding="utf-8")
        wrapper = (target / "platformio.ini").read_text(encoding="utf-8")
        positive_filters = [
            line.strip()
            for line in platformio_input.splitlines()
            if line.strip().startswith("+<")
        ]
        self.assertTrue(positive_filters)
        self.assertTrue(all("*" not in line for line in positive_filters))
        self.assertIn("+<apps/main/app.cpp>", positive_filters)
        self.assertNotIn("platform/windows", platformio_input)
        self.assertNotIn("[platformio]", platformio_input)
        self.assertIn("default_envs = hmi-main", selection)
        self.assertIn("build_dir = .pio/build/hmi/main", selection)
        self.assertNotIn("build_dir =", wrapper)
        self.assertIn(".generated/hmi/main/selection.ini", wrapper)
        selected = (target / "selected_profile.hpp").read_text(encoding="utf-8")
        self.assertIn('kSelectedAssetRoot[] = "A:/sdcard/hmi/main/"', selected)
        self.assertIn('kSelectedAssetStorageRoot[] = "/hmi/main"', selected)
        self.assertIn('kSelectedNvsNamespace[] = "hmi_main"', selected)
        self.assertIn("-D HMI_PROFILE_HAS_ASSETS=0", platformio_input)
        self.assertIn("-D HMI_PROFILE_HAS_SETTINGS=0", platformio_input)
        self.assertIn("-D HMI_PROFILE_HAS_CONNECTIVITY=0", platformio_input)
        self.assertNotIn("platform/hmi/connectivity", platformio_input)
        self.assertNotIn("asset_usb_updater.cpp", platformio_input)
        self.assertNotIn("audio_service.cpp", platformio_input)

    def test_root_platformio_selects_generated_main_not_common_base(self) -> None:
        root_config = (REPOSITORY_ROOT / "platformio.ini").read_text(encoding="utf-8")
        self.assertIn("default_envs = hmi-main", root_config)
        self.assertIn("build_dir = .pio/build/hmi", root_config)
        self.assertIn("extra_configs = .generated/hmi/*/profile.ini", root_config)
        self.assertIn("[env]", root_config)
        self.assertNotIn("[env:hmi-base]", root_config)

    def test_same_input_regenerates_identical_bytes_and_hash(self) -> None:
        target = self.prepare("windows")
        before = file_snapshot(target)
        target = self.prepare("windows")
        after = file_snapshot(target)
        self.assertEqual(before, after)

        metadata = json.loads((target / "metadata.json").read_text(encoding="utf-8"))
        resolved = json.loads(
            (target / "resolved_profile.json").read_text(encoding="utf-8")
        )
        self.assertEqual(metadata["input_hash"], resolved["input_hash"])
        self.assertEqual(64, len(metadata["input_hash"]))

    def test_staging_failure_preserves_previous_valid_output(self) -> None:
        target = self.prepare("windows")
        before = file_snapshot(target)
        with mock.patch(
            "hmi_project.generate._write_staging_file",
            side_effect=OSError("injected staging failure"),
        ):
            with self.assertRaises(OSError):
                self.prepare("windows")
        self.assertEqual(before, file_snapshot(target))
        self.assertFalse(
            any(".staging-" in path.name for path in target.parent.iterdir())
        )

    def test_isolation_check_accepts_current_windows_and_hmi_outputs(self) -> None:
        self.prepare("windows")
        self.prepare("hmi")
        checked = check_generated_isolation(
            REPOSITORY_ROOT, self.registry, output_root=self.output_root
        )
        self.assertEqual(["windows/main", "hmi/main"], checked)

    def test_isolation_check_rejects_stale_hash(self) -> None:
        target = self.prepare("windows")
        metadata_path = target / "metadata.json"
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        metadata["input_hash"] = "0" * 64
        metadata_path.write_text(json.dumps(metadata), encoding="utf-8")

        with self.assertRaises(GenerationError) as captured:
            check_generated_isolation(
                REPOSITORY_ROOT, self.registry, output_root=self.output_root
            )
        self.assertEqual("stale-generated-output", captured.exception.code)

    def test_isolation_check_rejects_removed_profile_output(self) -> None:
        stale = self.output_root / "windows" / "removed-profile"
        stale.mkdir(parents=True)
        with self.assertRaises(GenerationError) as captured:
            check_generated_isolation(
                REPOSITORY_ROOT, self.registry, output_root=self.output_root
            )
        self.assertEqual("stale-generated-profile", captured.exception.code)

    def test_generation_writes_only_below_requested_output_root(self) -> None:
        self.prepare("windows")
        self.prepare("hmi")
        generated_files = [path for path in self.output_root.rglob("*") if path.is_file()]
        self.assertTrue(generated_files)
        for path in generated_files:
            self.assertTrue(path.is_relative_to(self.output_root))

    def test_temporary_page_add_remove_keeps_production_files_unchanged(self) -> None:
        production_roots = (
            REPOSITORY_ROOT / "apps" / "main",
            REPOSITORY_ROOT / "samples" / "suite" / "app.cpp",
            REPOSITORY_ROOT / "samples" / "suite" / "app.hpp",
            REPOSITORY_ROOT / "samples" / "suite" / "pages",
            REPOSITORY_ROOT / "boards",
            REPOSITORY_ROOT / "platform",
            REPOSITORY_ROOT / "hosts",
        )

        def snapshot(paths: tuple[Path, ...]) -> dict[str, bytes]:
            result: dict[str, bytes] = {}
            for root in paths:
                files = root.rglob("*") if root.is_dir() else (root,)
                for path in files:
                    if path.is_file():
                        result[path.relative_to(REPOSITORY_ROOT).as_posix()] = path.read_bytes()
            return result

        def write_text(path: Path, content: str = "") -> None:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8", newline="\n")

        fixture_root = Path(self.temporary_directory.name) / "fixture-repository"
        fixture_profile = fixture_root / "samples" / "suite"
        fixture_output = fixture_root / ".generated"
        registry_path = fixture_root / "profiles" / "registry.json"
        page_registry_path = fixture_profile / "page_registry.json"
        base_page = {
            "id": "base",
            "title_key": "page.base.title",
            "description_key": "page.base.description",
            "order": 10,
            "entry": {
                "header": "pages/base/base_page.hpp",
                "type": "fixture::BasePage",
            },
            "sources": {
                "common": ["pages/base/base_page.cpp"],
                "windows": [],
                "hmi": [],
            },
            "capabilities": [],
            "verification": [],
            "data_key": "base",
        }
        noop_page = {
            "id": "noop-page",
            "title_key": "page.noop_page.title",
            "description_key": "page.noop_page.description",
            "order": 20,
            "entry": {
                "header": "pages/noop_page/noop_page.hpp",
                "type": "fixture::NoopPage",
            },
            "sources": {
                "common": ["pages/noop_page/noop_page.cpp"],
                "windows": [],
                "hmi": [],
            },
            "capabilities": [],
            "verification": [],
            "data_key": "noop_page",
        }
        profile = {
            "schema_version": 1,
            "id": "sample-suite",
            "kind": "sample",
            "description": "Temporary page registry fixture.",
            "entry": {"header": "app.hpp", "type": "fixture::App"},
            "sources": {"common": ["app.cpp"], "windows": [], "hmi": []},
            "include_dirs": ["."],
            "modules": [],
            "hosts": {"windows": {"enabled": True}},
            "assets": [],
            "config": ["page_registry.json", "generated/catalog_manifest.json"],
            "data": {
                "windows_root": "fixture",
                "hmi_sd_root": "hmi/fixture",
                "nvs_namespace": "hmi_fixture",
            },
            "verification": {"tests": []},
        }

        write_text(
            registry_path,
            json.dumps(
                {
                    "schema_version": 1,
                    "default_profile": "sample-suite",
                    "profiles": ["../samples/suite/profile.json"],
                },
                indent=2,
            )
            + "\n",
        )
        write_text(fixture_profile / "profile.json", json.dumps(profile, indent=2) + "\n")
        write_text(fixture_profile / "app.hpp")
        write_text(fixture_profile / "app.cpp")
        write_text(fixture_profile / "page_contract.hpp")
        write_text(fixture_profile / "pages/base/base_page.hpp")
        write_text(fixture_profile / "pages/base/base_page.cpp")
        write_text(
            fixture_profile / "generated/catalog_manifest.json",
            json.dumps(
                {
                    "schema_version": 1,
                    "entries": [
                        {"key": "page.base.title"},
                        {"key": "page.base.description"},
                        {"key": "page.noop_page.title"},
                        {"key": "page.noop_page.description"},
                    ],
                },
                indent=2,
            )
            + "\n",
        )
        write_text(fixture_root / "hosts/windows/main.cpp")
        write_text(fixture_root / "platform/windows/lvgl_mem.cpp")
        write_text(fixture_root / "platform/windows/sdl_lvgl_port.cpp")
        write_text(
            fixture_root / "platform/windows/settings/memory_level_settings.cpp"
        )
        (fixture_root / "include").mkdir(parents=True)
        write_text(
            page_registry_path,
            json.dumps({"schema_version": 2, "pages": [base_page]}, indent=2) + "\n",
        )

        invalid_page = dict(base_page)
        invalid_page["title_key"] = "page.base.missing"
        write_text(
            page_registry_path,
            json.dumps({"schema_version": 2, "pages": [invalid_page]}, indent=2)
            + "\n",
        )
        with self.assertRaises(ManifestError) as captured:
            load_registry(fixture_root, registry_path)
        self.assertEqual("unknown-page-catalog-key", captured.exception.code)
        write_text(
            page_registry_path,
            json.dumps({"schema_version": 2, "pages": [base_page]}, indent=2) + "\n",
        )

        production_before = snapshot(production_roots)
        existing_fixture_before = file_snapshot(fixture_profile)
        baseline_registry = load_registry(fixture_root, registry_path)
        baseline_target = prepare_profile(
            fixture_root,
            baseline_registry,
            "windows",
            "sample-suite",
            output_root=fixture_output,
        )
        baseline_generated = file_snapshot(baseline_target)

        write_text(fixture_profile / "pages/noop_page/noop_page.hpp")
        write_text(fixture_profile / "pages/noop_page/noop_page.cpp")
        write_text(
            page_registry_path,
            json.dumps(
                {"schema_version": 2, "pages": [base_page, noop_page]}, indent=2
            )
            + "\n",
        )
        expanded_registry = load_registry(fixture_root, registry_path)
        expanded_target = prepare_profile(
            fixture_root,
            expanded_registry,
            "windows",
            "sample-suite",
            output_root=fixture_output,
        )
        expanded_source = (expanded_target / "page_registry.cpp").read_text(
            encoding="utf-8"
        )
        expanded_resolved = json.loads(
            (expanded_target / "resolved_profile.json").read_text(encoding="utf-8")
        )
        self.assertIn('"noop-page"', expanded_source)
        self.assertIn(
            "samples/suite/pages/noop_page/noop_page.cpp",
            expanded_resolved["source_groups"]["pages"],
        )
        self.assertEqual(
            baseline_generated["selected_profile.hpp"],
            (expanded_target / "selected_profile.hpp").read_bytes(),
        )

        (fixture_profile / "pages/noop_page/noop_page.hpp").unlink()
        (fixture_profile / "pages/noop_page/noop_page.cpp").unlink()
        (fixture_profile / "pages/noop_page").rmdir()
        write_text(
            page_registry_path,
            json.dumps({"schema_version": 2, "pages": [base_page]}, indent=2) + "\n",
        )
        restored_registry = load_registry(fixture_root, registry_path)
        restored_target = prepare_profile(
            fixture_root,
            restored_registry,
            "windows",
            "sample-suite",
            output_root=fixture_output,
        )

        self.assertEqual(baseline_generated, file_snapshot(restored_target))
        self.assertEqual(existing_fixture_before, file_snapshot(fixture_profile))
        self.assertEqual(production_before, snapshot(production_roots))


if __name__ == "__main__":
    unittest.main()
