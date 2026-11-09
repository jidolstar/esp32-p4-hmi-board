from __future__ import annotations

import base64
import hashlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from assets.push import (  # noqa: E402
    AssetPushError,
    _cancel_preparation,
    _handshake,
    _set_preparation,
    _upload_stream,
    load_bundle,
)


class FakeConnection:
    def __init__(self, expected_size: int) -> None:
        self.expected_size = expected_size
        self.received = bytearray()
        self.responses: list[bytes] = []
        self.window_chunks = 0

    def write(self, data: bytes) -> int:
        line = data.decode("ascii").strip()
        if line.startswith("ASSET_FILE "):
            self.responses.append(b"ASSET_FILE_READY\n")
        elif line.startswith("ASSET_DATA "):
            _, offset_text, encoded = line.split()
            self.assert_offset(int(offset_text))
            chunk = base64.b64decode(encoded, validate=True)
            self.received.extend(chunk)
            self.window_chunks += 1
            if self.window_chunks == 16 or len(self.received) == self.expected_size:
                self.responses.append(f"ASSET_ACK {len(self.received)}\n".encode("ascii"))
                self.window_chunks = 0
        elif line == "ASSET_END_FILE":
            self.responses.append(b"ASSET_FILE_OK\n")
        return len(data)

    def assert_offset(self, offset: int) -> None:
        if offset != len(self.received):
            raise AssertionError(f"offset {offset} != {len(self.received)}")

    def flush(self) -> None:
        pass

    def readline(self) -> bytes:
        return self.responses.pop(0) if self.responses else b""


class HandshakeConnection:
    def __init__(self, *responses: bytes) -> None:
        self.responses = list(responses)
        self.sent: list[bytes] = []

    def reset_input_buffer(self) -> None:
        pass

    def write(self, data: bytes) -> int:
        self.sent.append(data)
        return len(data)

    def flush(self) -> None:
        pass

    def readline(self) -> bytes:
        return self.responses.pop(0) if self.responses else b""


class AssetPushTest(unittest.TestCase):
    def make_bundle(self, root: Path) -> bytes:
        bundle_root = root / "dist" / "sd" / "hmi" / "sample-suite"
        asset = bundle_root / "assets" / "fonts" / "font.bin"
        asset.parent.mkdir(parents=True)
        contents = b"font-data"
        asset.write_bytes(contents)
        manifest = {
            "schema_version": 1,
            "profile_id": "sample-suite",
            "destination_root": "hmi/sample-suite",
            "assets": [
                {
                    "relative_path": "assets/fonts/font.bin",
                    "size": len(contents),
                    "sha256": hashlib.sha256(contents).hexdigest(),
                    "source": "fixture/font.bin",
                }
            ],
        }
        data = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")
        (bundle_root / "asset_manifest.json").write_bytes(data)
        return data

    def test_load_bundle_verifies_profile_files_and_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = self.make_bundle(root)
            bundle = load_bundle(root, "sample-suite")
            self.assertEqual("sample-suite", bundle.profile_id)
            self.assertEqual(1, len(bundle.files))
            self.assertEqual(manifest, bundle.manifest_bytes)
            self.assertEqual(len(manifest) + len(b"font-data"), bundle.total_bytes)

    def test_load_bundle_rejects_changed_file(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_bundle(root)
            asset = (
                root
                / "dist/sd/hmi/sample-suite/assets/fonts/font.bin"
            )
            asset.write_bytes(b"changed")
            with self.assertRaises(AssetPushError):
                load_bundle(root, "sample-suite")

    def test_load_bundle_accepts_fragmented_user_font_set(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bundle_root = root / "dist" / "sd" / "hmi" / "sample-suite"
            font_root = bundle_root / "assets" / "fonts"
            font_root.mkdir(parents=True)
            assets = []
            for index in range(176):
                relative = f"assets/fonts/user_{index:03d}.bin"
                contents = bytes((index & 0xFF,))
                (bundle_root / relative).write_bytes(contents)
                assets.append(
                    {
                        "relative_path": relative,
                        "size": len(contents),
                        "sha256": hashlib.sha256(contents).hexdigest(),
                        "source": f"fixture/user_{index:03d}.bin",
                    }
                )
            manifest = {
                "schema_version": 1,
                "profile_id": "sample-suite",
                "destination_root": "hmi/sample-suite",
                "assets": assets,
            }
            (bundle_root / "asset_manifest.json").write_text(
                json.dumps(manifest), encoding="utf-8"
            )

            bundle = load_bundle(root, "sample-suite")
            self.assertEqual(176, len(bundle.files))

    def test_upload_stream_uses_bounded_windows_and_exact_offsets(self) -> None:
        contents = bytes(index % 251 for index in range(2049))
        connection = FakeConnection(len(contents))
        _upload_stream(
            connection,  # type: ignore[arg-type]
            "assets/fonts/font.bin",
            io.BytesIO(contents),
            len(contents),
            hashlib.sha256(contents).hexdigest(),
        )
        self.assertEqual(contents, bytes(connection.received))

    def test_handshake_reports_hmi_storage_error(self) -> None:
        connection = HandshakeConnection(b"ASSET_ERROR storage_unavailable\n")
        with self.assertRaisesRegex(AssetPushError, "storage_unavailable"):
            _handshake(connection, "sample-suite")  # type: ignore[arg-type]
        self.assertEqual(
            [b"ASSET_HELLO 2 sample-suite\n"],
            connection.sent,
        )

    def test_preparation_stage_is_sent_after_handshake(self) -> None:
        connection = HandshakeConnection(
            b"ASSET_V2 sample-suite\n", b"ASSET_PREPARING fonts\n"
        )
        _set_preparation(connection, "sample-suite", "fonts")  # type: ignore[arg-type]
        self.assertEqual(
            [
                b"ASSET_HELLO 2 sample-suite\n",
                b"ASSET_PREPARE fonts\n",
            ],
            connection.sent,
        )

    def test_preparation_cancel_is_sent_after_handshake(self) -> None:
        connection = HandshakeConnection(
            b"ASSET_V2 sample-suite\n", b"ASSET_CANCELLED\n"
        )
        _cancel_preparation(connection, "sample-suite")  # type: ignore[arg-type]
        self.assertEqual(
            [
                b"ASSET_HELLO 2 sample-suite\n",
                b"ASSET_CANCEL\n",
            ],
            connection.sent,
        )


if __name__ == "__main__":
    unittest.main()
