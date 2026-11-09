from pathlib import Path
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


class BoardStorageContractTests(unittest.TestCase):
    def test_sdmmc_owns_sd_power_and_mount_has_frequency_fallbacks(self) -> None:
        implementation = (
            REPOSITORY_ROOT
            / "boards"
            / "jc1060p470c"
            / "internal"
            / "sd_storage.cpp"
        ).read_text(encoding="utf-8")
        header = (
            REPOSITORY_ROOT
            / "boards"
            / "jc1060p470c"
            / "internal"
            / "sd_storage.hpp"
        ).read_text(encoding="utf-8")

        # esp32p4 variant의 SD_MMC가 LDO4를 소유하므로 중복 취득을 금지한다.
        self.assertNotIn("esp_ldo_acquire_channel", implementation)
        self.assertNotIn("esp_ldo_channel_handle_t", header)
        self.assertIn("SD_MMC.begin", implementation)
        self.assertIn("20U * 1000U", implementation)
        self.assertIn("10U * 1000U", implementation)
        self.assertIn("4U * 1000U", implementation)


if __name__ == "__main__":
    unittest.main()
