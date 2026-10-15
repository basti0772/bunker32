"""Exercise the publication validator against valid and broken installation packages."""
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
SHELL = shutil.which("powershell") or shutil.which("pwsh")


class WebFirmwareTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(SHELL, "PowerShell is required")
        BUILD.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="web-test-", dir=BUILD)
        self.folder = Path(self.temporary.name).resolve()
        self.assertTrue(self.folder.is_relative_to(BUILD.resolve()))
        for name in ("docs", "firmware", "tools"):
            shutil.copytree(ROOT / name, self.folder / name)

    def tearDown(self):
        # The only recursively deleted directory is our verified disposable fixture.
        if self.folder.is_relative_to(BUILD.resolve()):
            self.temporary.cleanup()

    def check_package(self, success, message=None):
        result = subprocess.run(
            [SHELL, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
             str(ROOT / "tools/Test-Web-Firmware.ps1"), "-ProjectRoot", str(self.folder)],
            capture_output=True, text=True, errors="replace", timeout=60,
        )
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        if message:
            self.assertIn(message, result.stdout + result.stderr)

    def change_json(self, name, change):
        path = self.folder / name
        value = json.loads(path.read_text(encoding="utf-8"))
        change(value)
        path.write_text(json.dumps(value), encoding="utf-8")

    def test_valid_package_without_build_cache(self):
        self.check_package(True)

    def test_reject_wrong_family(self):
        self.change_json("docs/manifest.json", lambda x: x["builds"][0].update(chipFamily="ESP32-S3"))
        self.check_package(False, "Chipfamilie")

    def test_reject_wrong_merge_base(self):
        self.change_json("docs/manifest.json", lambda x: x["builds"][0]["parts"][0].update(offset=4096))
        self.check_package(False, "Basisadresse")

    def test_reject_source_offset_shift(self):
        self.change_json("docs/firmware/build-info.json", lambda x: x["inputs"][0].update(offset=x["inputs"][0]["offset"] + 1))
        self.check_package(False, "Image-Offset")

    def test_reject_pages_path_outside_docs(self):
        self.change_json("docs/manifest.json", lambda x: x["builds"][0]["parts"][0].update(path="../firmware/bunker32.bin"))
        self.check_package(False, "Pfad verlaesst")

    def test_reject_corrupt_binary(self):
        path = self.folder / "docs/firmware/bunker32.bin"
        contents = bytearray(path.read_bytes())
        contents[-1] ^= 1
        path.write_bytes(contents)
        self.check_package(False, "Pruefsumme")

    def test_reject_missing_binary(self):
        (self.folder / "docs/firmware/bunker32.bin").unlink()
        self.check_package(False, "Datei fehlt")

    def test_reject_stale_source(self):
        path = self.folder / "firmware/Bunker32/Game.cpp"
        with path.open("a", encoding="utf-8") as stream:
            stream.write("\n// Firmware source changed after the published build.\n")
        self.check_package(False, "veraltet")

    def test_reject_version_mismatch(self):
        self.change_json("docs/manifest.json", lambda x: x.update(version="99.0.0"))
        self.check_package(False, "Versionen")


if __name__ == "__main__":
    unittest.main(verbosity=2)
