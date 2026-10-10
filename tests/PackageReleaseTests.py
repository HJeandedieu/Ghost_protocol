"""Submission packaging checks: complete inputs, exclusions and failure cases."""

from pathlib import Path
import importlib.util
import json
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("package_release", ROOT / "tools/package_release.py")
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class PackageReleaseTests(unittest.TestCase):
    def setUp(self):
        temporary_root = ROOT / "build/package-tests"
        temporary_root.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=temporary_root)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / "build"
        self.build.mkdir()
        for name in ("README.md", "ASSETS.md", "tools/release_licenses/raylib-LICENSE.txt",
                     "tools/release_licenses/nlohmann-json-LICENSE.txt"):
            self.write(self.root / name)
        for name in PACKAGE.WEB_FILES:
            self.write(self.build / name)

    @staticmethod
    def write(path, content="test"):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def native_fixture(self):
        self.write(self.build / "ghost_game.exe")
        manifest = []
        register = []
        for i in range(1, 26):
            file = f"audio/voice/V{i:02}.ogg"
            self.write(self.build / "assets" / file)
            register.append(f"`assets/{file}`")
            manifest.append({"id": f"V{i:02}", "file": file})
        self.write(self.root / "ASSETS.md", "\n".join(register))
        self.write(self.build / "assets/config/voice_lines.json", json.dumps(manifest))
        for name in ("config/tuning.json", "config/weapons.json", "config/enemies.json",
                     "levels/gotham_central.map", "levels/gotham_central.json",
                     "shaders/glsl330/post.fs", "shaders/glsl100/post.fs",
                     "shaders/glsl330/bank.vs", "shaders/glsl330/bank.fs",
                     "shaders/glsl100/bank.vs", "shaders/glsl100/bank.fs",
                     "shaders/glsl330/tactical.vs", "shaders/glsl330/tactical.fs",
                     "shaders/glsl100/tactical.vs", "shaders/glsl100/tactical.fs"):
            self.write(self.build / "assets" / name)

    def test_web_archive_is_reproducible_and_excludes_development_inputs(self):
        for name in (".env", "save/settings.json", "reference/logo.png", "ghost_tests.exe"):
            self.write(self.build / name, "must not ship")
        files = PACKAGE.web_files(self.build, self.root)
        first, second = self.root / "a.zip", self.root / "b.zip"
        PACKAGE.archive(first, files)
        PACKAGE.archive(second, files)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        with zipfile.ZipFile(first) as archive:
            self.assertEqual(set(archive.namelist()), set(PACKAGE.WEB_FILES) | {
                "README.md", "ASSETS.md", "licenses/raylib-LICENSE.txt",
                "licenses/nlohmann-json-LICENSE.txt"})

    def test_web_rejects_oversize_and_missing_payloads(self):
        with self.assertRaisesRegex(ValueError, "exceeds"):
            PACKAGE.web_files(self.build, self.root, limit=1)
        (self.build / "index.data").unlink()
        with self.assertRaisesRegex(ValueError, "release input"):
            PACKAGE.web_files(self.build, self.root)

    def test_archive_rejects_oversize_zip(self):
        with self.assertRaisesRegex(ValueError, "exceeds"):
            PACKAGE.archive(self.root / "large.zip", PACKAGE.web_files(self.build, self.root), 1)

    def test_native_includes_all_recordings_and_only_runtime_executable(self):
        self.native_fixture()
        self.write(self.build / "ghost_tests.exe")
        self.write(self.build / "logs/ghost.log")
        names = {name for _, name in PACKAGE.native_files(self.build, self.root)}
        self.assertIn("ghost_game.exe", names)
        self.assertIn("assets/audio/voice/V25.ogg", names)
        self.assertIn("assets/shaders/glsl330/bank.vs", names)
        self.assertIn("assets/shaders/glsl100/bank.fs", names)
        self.assertNotIn("ghost_tests.exe", names)
        self.assertNotIn("logs/ghost.log", names)

    def test_native_rejects_each_missing_tactical_shader(self):
        self.native_fixture()
        for version in ("glsl330", "glsl100"):
            for extension in ("vs", "fs"):
                path = self.build / f"assets/shaders/{version}/tactical.{extension}"
                content = path.read_bytes()
                path.unlink()
                with self.subTest(shader=str(path)):
                    with self.assertRaisesRegex(ValueError, "release input"):
                        PACKAGE.native_files(self.build, self.root)
                path.write_bytes(content)

    def test_native_rejects_missing_recording(self):
        self.native_fixture()
        (self.build / "assets/audio/voice/V12.ogg").unlink()
        with self.assertRaisesRegex(ValueError, "release input"):
            PACKAGE.native_files(self.build, self.root)

    def test_native_rejects_missing_perspective_shader(self):
        self.native_fixture()
        (self.build / "assets/shaders/glsl330/bank.vs").unlink()
        with self.assertRaisesRegex(ValueError, "release input"):
            PACKAGE.native_files(self.build, self.root)

    def test_native_rejects_unregistered_art_and_unknown_dll(self):
        self.native_fixture()
        self.write(self.build / "assets/ui/new.png")
        with self.assertRaisesRegex(ValueError, "Unregistered"):
            PACKAGE.native_files(self.build, self.root)
        (self.build / "assets/ui/new.png").unlink()
        self.write(self.build / "unexpected.dll")
        with self.assertRaisesRegex(ValueError, "Unrecognized"):
            PACKAGE.native_files(self.build, self.root)

    def test_native_rejects_secrets_in_assets(self):
        self.native_fixture()
        self.write(self.build / "assets/.env")
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            PACKAGE.native_files(self.build, self.root)


if __name__ == "__main__":
    unittest.main()
