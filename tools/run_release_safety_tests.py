"""Verify that public-release signing rejects developer credentials."""
import tempfile
import unittest
from pathlib import Path
import release_firmware as release


class ReleaseSafetyTests(unittest.TestCase):
    def test_embedded_credentials_rejected_without_disclosing_value(self):
        with tempfile.TemporaryDirectory() as tmp:
            original = release.ROOT
            release.ROOT = Path(tmp)
            try:
                (release.ROOT / "include").mkdir()
                (release.ROOT / "include/secrets.h").write_text(
                    '#define BROOKLYN_WIFI_PASSWORD "synthetic-pass-123"\n'
                    '#define BROOKLYN_MQTT_BENCH_PASSWORD "synthetic-broker-456"\n'
                    '// #define BROOKLYN_WIFI_SSID "comment-only"\n'
                )
                release.check_developer_credentials(b"safe binary comment-only")
                for value in (b"synthetic-pass-123", b"synthetic-broker-456"):
                    with self.assertRaises(SystemExit) as raised:
                        release.check_developer_credentials(b"binary" + value)
                    self.assertNotIn(value.decode(), str(raised.exception))
                    self.assertIn("refusing release", str(raised.exception))
            finally:
                release.ROOT = original


if __name__ == "__main__":
    unittest.main()
