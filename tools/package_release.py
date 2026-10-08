"""Build verified submission archives using only the Python standard library."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile


ROOT = Path(__file__).resolve().parents[1]
WEB_FILES = ("index.html", "index.js", "index.wasm", "index.data")
WEB_LIMIT = 50_000_000
RUNTIME_DLL = re.compile(r"(?:libgcc_s_[a-z0-9]+-1|libstdc\+\+-6|libwinpthread-1)\.dll")
ASSET_EXTENSIONS = {".json", ".map", ".ogg", ".png", ".ttf", ".txt", ".fs"}


def checked(path):
    if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
        raise ValueError(f"Missing, empty or linked release input: {path}")
    return path


def common_files(root):
    files = [(checked(root / name), name) for name in ("README.md", "ASSETS.md")]
    notices = root / "tools/release_licenses"
    for name in ("raylib-LICENSE.txt", "nlohmann-json-LICENSE.txt"):
        checked(notices / name)
    for path in sorted(notices.glob("*.txt")):
        files.append((checked(path), "licenses/" + path.name))
    return files


def native_files(build, root):
    files = [(checked(build / "ghost_game.exe"), "ghost_game.exe")]
    for path in sorted(build.glob("*.dll")):
        if not RUNTIME_DLL.fullmatch(path.name):
            raise ValueError(f"Unrecognized runtime DLL: {path.name}")
        files.append((checked(path), path.name))
    if any(name.endswith(".dll") for _, name in files):
        for name in ("GCC-GPL-3.0.txt", "GCC-RUNTIME-EXCEPTION.txt",
                     "Winpthreads-COPYING.txt", "MinGW-w64-COPYING.txt"):
            checked(root / "tools/release_licenses" / name)
    # Package copied build assets, so the archive contains exactly what was tested.
    assets = build / "assets"
    if assets.is_symlink() or not assets.is_dir():
        raise ValueError("Native build has no regular assets directory")
    register = (root / "ASSETS.md").read_text(encoding="utf-8-sig")
    for path in sorted(assets.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Linked asset: {path}")
        if not path.is_file():
            continue
        name = "assets/" + path.relative_to(assets).as_posix()
        if path.suffix not in ASSET_EXTENSIONS or any(
            part.startswith(".") or part in {"save", "logs", "reference", "models"}
            for part in path.relative_to(assets).parts
        ):
            raise ValueError(f"Unexpected runtime asset: {name}")
        if path.suffix in {".ogg", ".png", ".ttf"} and f"`{name}`" not in register:
            raise ValueError(f"Unregistered runtime asset: {name}")
        files.append((checked(path), name))
    manifest = json.loads(checked(assets / "config/voice_lines.json").read_text())
    if not isinstance(manifest, list) or len(manifest) != 25 or {
        entry["id"] for entry in manifest
    } != {f"V{i:02}" for i in range(1, 26)}:
        raise ValueError("Expected the complete V01–V25 voice manifest")
    for entry in manifest:
        relative = Path(entry["file"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("Voice paths must stay within assets")
        checked(assets / relative)
    for name in ("config/tuning.json", "config/weapons.json", "config/enemies.json",
                 "levels/gotham_central.map", "levels/gotham_central.json",
                 "shaders/glsl330/post.fs", "shaders/glsl100/post.fs"):
        checked(assets / name)
    return files + common_files(root)


def web_files(build, root, limit=WEB_LIMIT):
    files = [(checked(build / name), name) for name in WEB_FILES]
    payload = sum(path.stat().st_size for path, _ in files)
    if payload > limit:
        raise ValueError(f"Web payload {payload:,} bytes exceeds {limit:,} byte limit")
    return files + common_files(root)


def archive(path, files, limit=None):
    # Fixed timestamps, ordering and permissions make identical inputs reproducible.
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as out:
        for source, name in sorted(files, key=lambda item: item[1]):
            info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            out.writestr(info, source.read_bytes(), compresslevel=9)
    with zipfile.ZipFile(path) as out:
        if out.testzip() is not None:
            raise ValueError(f"Archive integrity check failed: {path}")
    if limit is not None and path.stat().st_size > limit:
        raise ValueError(f"Archive {path.name} exceeds {limit:,} byte limit")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path)
    parser.add_argument("--web", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "build/submission")
    args = parser.parse_args()
    if not args.native and not args.web:
        parser.error("Supply --native, --web, or both")
    # Validate all inputs before writing any archive.
    packages = []
    if args.native:
        packages.append(("ghost-protocol-win.zip", native_files(args.native, ROOT)))
    if args.web:
        packages.append(("ghost-protocol-web.zip", web_files(args.web, ROOT)))
    args.output.mkdir(parents=True, exist_ok=True)
    checksums = []
    for name, files in packages:
        target = args.output / name
        archive(target, files, 100_000_000 if name.endswith("win.zip") else WEB_LIMIT)
        checksums.append(f"{hashlib.sha256(target.read_bytes()).hexdigest()}  {name}")
        print(f"{target}: {target.stat().st_size:,} bytes, {len(files)} entries")
    (args.output / "SHA256SUMS.txt").write_text("\n".join(checksums) + "\n", encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError) as error:
        raise SystemExit(f"Release validation failed: {error}") from error
