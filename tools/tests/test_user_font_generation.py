from __future__ import annotations

import sys
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
REPOSITORY_ROOT = TOOLS_ROOT.parent
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from assets.typography.font_assets import load_font_config  # noqa: E402
from assets.typography.owner_config import load_owner_config  # noqa: E402


class UserFontGenerationTest(unittest.TestCase):
    def test_hangul_is_expanded_to_64_codepoint_blocks(self) -> None:
        owner = load_owner_config(
            REPOSITORY_ROOT
            / "samples/suite/i18n/catalog.json"
        )
        config = load_font_config(REPOSITORY_ROOT, owner.fonts)
        self.assertEqual(2, config.user_bpp)
        self.assertEqual(176, config.user_cache_capacity)
        self.assertEqual(176, len(config.user_assets))
        self.assertEqual("common", config.user_assets[0].id)

        hangul = config.user_assets[1:]
        self.assertEqual(175, len(hangul))
        self.assertEqual((0xAC00, 0xAC3F), hangul[0].ranges[0])
        self.assertEqual((0xD780, 0xD7A3), hangul[-1].ranges[0])
        for asset in hangul:
            first, last = asset.ranges[0]
            self.assertLessEqual(last - first + 1, 64)


if __name__ == "__main__":
    unittest.main()
