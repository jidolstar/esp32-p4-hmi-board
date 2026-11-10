from __future__ import annotations

import hashlib
import json
import struct
import sys
import tempfile
import unittest
import wave
from pathlib import Path


TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from assets.audio.validate_wav_catalog import (  # noqa: E402
    WavCatalogError,
    generate,
    inspect_wav,
    load_catalog,
)


class WavCatalogTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.root = Path(self.temporary_directory.name)
        self.owner = self.root / "samples/suite"
        self.sound = self.owner / "assets/sounds/notice.wav"
        self.sound.parent.mkdir(parents=True)
        self._write_wav(self.sound)
        self.config = self.owner / "pages/wav_sounds/wav_catalog.json"
        self.config.parent.mkdir(parents=True)
        self._write_catalog()

    @staticmethod
    def _write_wav(path: Path, rate: int = 48000) -> None:
        with wave.open(str(path), "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(rate)
            output.writeframes(b"\x00\x00" * (rate // 10))

    def _payload(self) -> dict[str, object]:
        info = inspect_wav(self.sound)
        return {
            "schema_version": 1,
            "owner_root": "samples/suite",
            "generated_header": "samples/suite/generated/wav_catalog.hpp",
            "max_entries": 8,
            "entries": [
                {
                    "id": "notice",
                    "asset_path": "assets/sounds/notice.wav",
                    "title_key": "wav.notice.title",
                    "description_key": "wav.notice.description",
                    "source_path": "../source/notice.wav",
                    "source_authorization": "fixture",
                    "file_bytes": info.file_bytes,
                    "sha256": hashlib.sha256(self.sound.read_bytes()).hexdigest(),
                    "format": {
                        "encoding": "pcm",
                        "channels": info.channels,
                        "sample_rate_hz": info.sample_rate_hz,
                        "bits_per_sample": info.bits_per_sample,
                        "duration_ms": info.duration_ms,
                    },
                }
            ],
        }

    def _write_catalog(self, payload: dict[str, object] | None = None) -> None:
        self.config.write_text(
            json.dumps(payload or self._payload(), indent=2) + "\n",
            encoding="utf-8",
        )

    def test_generate_and_stale_check(self) -> None:
        catalog = generate(self.root, self.config, check=False)
        self.assertEqual(1, len(catalog.entries))
        self.assertIn("kMaxRegisteredSounds = 8U", catalog.generated_header.read_text())
        generate(self.root, self.config, check=True)
        catalog.generated_header.write_text("stale\n", encoding="utf-8")
        with self.assertRaisesRegex(WavCatalogError, "stale"):
            generate(self.root, self.config, check=True)

    def test_rejects_unsupported_24_khz(self) -> None:
        self._write_wav(self.sound, 24000)
        with self.assertRaisesRegex(WavCatalogError, "unsupported sample rate 24000"):
            load_catalog(self.root, self.config)

    def test_rejects_duplicate_identifier(self) -> None:
        payload = self._payload()
        first = payload["entries"][0]
        payload["entries"] = [first, dict(first)]
        self._write_catalog(payload)
        with self.assertRaisesRegex(WavCatalogError, "duplicate identifier"):
            load_catalog(self.root, self.config)

    def test_rejects_path_traversal_and_hash_drift(self) -> None:
        payload = self._payload()
        payload["entries"][0]["asset_path"] = "../notice.wav"
        self._write_catalog(payload)
        with self.assertRaisesRegex(WavCatalogError, "without '..'"):
            load_catalog(self.root, self.config)

        payload = self._payload()
        payload["entries"][0]["sha256"] = "0" * 64
        self._write_catalog(payload)
        with self.assertRaisesRegex(WavCatalogError, "actual hash"):
            load_catalog(self.root, self.config)

    def test_rejects_truncated_data_chunk(self) -> None:
        data = bytearray(self.sound.read_bytes())
        at = data.index(b"data")
        struct.pack_into("<I", data, at + 4, len(data) * 2)
        self.sound.write_bytes(data)
        with self.assertRaisesRegex(WavCatalogError, "truncated chunk body"):
            inspect_wav(self.sound)


if __name__ == "__main__":
    unittest.main()

