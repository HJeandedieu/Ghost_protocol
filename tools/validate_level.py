"""Validate the shipped ASCII/JSON level using Data Formats section 4.3."""

import argparse
from collections import Counter, deque
import json
from pathlib import Path
import sys


def validate_level(level_file):
    level_file = Path(level_file)
    errors = []
    try:
        data = json.loads(level_file.read_text(encoding="utf-8"))
        if not isinstance(data, dict):
            return ["Level JSON must be an object"]
        map_file = data.get("map_file")
        if not isinstance(map_file, str) or Path(map_file).name != map_file:
            return ["map_file must name a map in the level directory"]
        rows = (level_file.parent / map_file).read_text(encoding="utf-8").splitlines()
    except (OSError, ValueError) as error:
        return [f"Cannot read level: {error}"]

    width, height = 80, 56
    if data.get("width") != width or data.get("height") != height or data.get("tile_size") != 48:
        errors.append("Level metadata must specify width 80, height 56, tile_size 48")
    if len(rows) != height or any(len(row) != width for row in rows):
        return errors + ["Map must contain exactly 56 rows of 80 characters"]
    allowed = set("#.dSRGVFbZkPBNM@v")
    invalid = set("".join(rows)) - allowed
    if invalid:
        errors.append(f"Unknown map characters: {sorted(invalid)}")
    counts = Counter("".join(rows))
    for character, expected in {"@": 1, "k": 1, "P": 1, "B": 1, "N": 1, "M": 10}.items():
        if counts[character] != expected:
            errors.append(f"Expected {expected} '{character}' tiles, found {counts[character]}")

    def check_point(point, label):
        if not isinstance(point, list) or len(point) != 2 or any(type(n) is not int for n in point):
            errors.append(f"{label}: expected [integer x, integer y]")
            return
        x, y = point
        if not (0 <= x < width and 0 <= y < height):
            errors.append(f"{label}: tile {point} is out of bounds")
        elif rows[y][x] == "#":
            errors.append(f"{label}: tile {point} is a wall")

    for group in ("guards", "cameras"):
        entities = data.get(group)
        if not isinstance(entities, list):
            errors.append(f"{group} must be an array")
            continue
        for index, entity in enumerate(entities):
            if not isinstance(entity, dict):
                errors.append(f"{group}[{index}] must be an object")
                continue
            label = f"{group}[{entity.get('id', index)}]"
            if group == "guards":
                points = entity.get("wp")
                if not isinstance(points, list) or not points:
                    errors.append(f"{label}: wp must be a nonempty array")
                    continue
                for point in points:
                    check_point(point, label)
            else:
                check_point(entity.get("pos"), label)

    if counts["@"] == 1:
        start = next((x, y) for y, row in enumerate(rows) for x, tile in enumerate(row) if tile == "@")
        reached, queue = {start}, deque([start])
        # Closed doors are considered open for the documented reachability check.
        # Walls and bollards remain blockers; money/keycard/panels are floor.
        while queue:
            x, y = queue.popleft()
            for point in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                nx, ny = point
                if (0 <= nx < width and 0 <= ny < height and point not in reached
                        and rows[ny][nx] not in "#b"):
                    reached.add(point)
                    queue.append(point)
        for y, row in enumerate(rows):
            for x, tile in enumerate(row):
                if tile in "kPBNM" and (x, y) not in reached:
                    errors.append(f"Item '{tile}' at [{x}, {y}] is unreachable from @")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("level", nargs="?", type=Path,
                        default=Path(__file__).resolve().parents[1] / "assets/levels/gotham_central.json")
    args = parser.parse_args()
    errors = validate_level(args.level)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print(f"OK: {args.level}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
