import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("validate_level", ROOT / "tools/validate_level.py")
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class ValidateLevelTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.data = json.loads((ROOT / "assets/levels/gotham_central.json").read_text())
        self.rows = (ROOT / "assets/levels/gotham_central.map").read_text().splitlines()

    def validate(self):
        path = self.root / "gotham_central.json"
        path.write_text(json.dumps(self.data))
        (self.root / self.data["map_file"]).write_text("\n".join(self.rows) + "\n")
        return validator.validate_level(path)

    def test_shipped_level_and_documentation_copy_pass(self):
        self.assertEqual(self.validate(), [])
        self.assertEqual(validator.validate_level(ROOT / "docs/levels/gotham_central.json"), [])

    def test_wall_waypoint_and_out_of_bounds_camera_are_reported(self):
        self.data["guards"][0]["wp"][0] = [0, 0]
        self.data["cameras"][0]["pos"] = [80, 2]
        errors = self.validate()
        self.assertTrue(any("is a wall" in error for error in errors))
        self.assertTrue(any("out of bounds" in error for error in errors))

    def test_short_row_and_missing_item_fail(self):
        self.rows[0] = self.rows[0][:-1]
        self.assertTrue(any("56 rows" in error for error in self.validate()))
        self.rows[0] += "#"
        self.rows = [row.replace("k", ".") for row in self.rows]
        self.assertTrue(any("'k'" in error for error in self.validate()))

    def test_disconnected_keycard_is_rejected(self):
        x, y = next((x, y) for y, row in enumerate(self.rows)
                    for x, tile in enumerate(row) if tile == "k")
        for nx, ny in ((x-1, y), (x+1, y), (x, y-1), (x, y+1)):
            self.rows[ny] = self.rows[ny][:nx] + "#" + self.rows[ny][nx+1:]
        self.assertTrue(any("unreachable" in error for error in self.validate()))

    def test_bad_json_and_missing_file_return_errors(self):
        path = self.root / "bad.json"
        path.write_text("{invalid")
        self.assertTrue(validator.validate_level(path))
        self.assertTrue(validator.validate_level(self.root / "missing.json"))


if __name__ == "__main__":
    unittest.main()
