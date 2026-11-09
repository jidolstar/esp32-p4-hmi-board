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

    def test_public_storage_facade_owns_lease_boundary(self) -> None:
        board_root = REPOSITORY_ROOT / "boards" / "jc1060p470c"
        public_header = (board_root / "storage.hpp").read_text(encoding="utf-8")
        implementation = (board_root / "storage.cpp").read_text(encoding="utf-8")
        updater = (board_root / "internal" / "asset_usb_updater.cpp").read_text(
            encoding="utf-8"
        )
        audio = (board_root / "internal" / "audio_service.cpp").read_text(
            encoding="utf-8"
        )

        self.assertNotIn('#include "internal/', public_header)
        self.assertIn("class ExclusiveLease", public_header)
        self.assertIn("ScopedFileLease", implementation)
        self.assertIn("storage_lease_ = std::move(transaction_lease)", updater)
        self.assertIn("storage_lease_ = Storage::ExclusiveLease{}", updater)
        self.assertIn("storage_.acquire_exclusive_lease()", audio)

    def test_raw_owner_and_file_leases_share_one_atomic_state(self) -> None:
        guard = (
            REPOSITORY_ROOT
            / "boards"
            / "jc1060p470c"
            / "internal"
            / "storage_access_guard.hpp"
        ).read_text(encoding="utf-8")
        self.assertIn("std::atomic<std::uint16_t> state_", guard)
        self.assertIn("compare_exchange_strong(expected, kRawOwner", guard)
        self.assertIn("try_acquire_file_lease", guard)


if __name__ == "__main__":
    unittest.main()
