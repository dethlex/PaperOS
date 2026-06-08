#!/usr/bin/env python3
"""Prepare images for the M5Paper screensaver.

Takes any JPG/PNG/HEIC/etc. that Pillow can open and writes a 540x960
baseline JPEG into the output directory (default /Volumes/<SD>/paperos/wallpapers/).

Why:
  * M5Paper screen is 540x960 portrait, 16-grey e-ink. Anything larger wastes
    SD bandwidth and on-device decode time.
  * TJpgDec (the JPEG path in screensaver) only supports baseline (non-
    progressive) JPEG. iPhone exports are usually progressive.
  * PNGdec works but is heavier and Adam7-interlaced or 16-bit-depth PNGs
    don't decode; resizing+converting sidesteps all of that.

Usage:
    python3 tools/prepare_wallpapers.py SOURCE_DIR /Volumes/SD/paperos/wallpapers/
    python3 tools/prepare_wallpapers.py photo1.jpg photo2.png  --out ./wallpapers/
    python3 tools/prepare_wallpapers.py photo.heic --beside   # -> photo-540x960.jpg next to it

The script always emits baseline JPEG, quality 85, fitted-to-screen with
cover-crop so the whole 540x960 frame is filled.
"""

import argparse
import os
import sys
from pathlib import Path

from PIL import Image, ImageOps

# HEIC (iPhone exports): auto-enabled if pillow-heif is installed
# (`pip install pillow-heif`). Silently skipped otherwise.
try:
    import pillow_heif
    pillow_heif.register_heif_opener()
except ImportError:
    pass

W, H = 540, 960


def prepare(src: Path, dst: Path) -> Path:
    img = Image.open(src)
    # Apply EXIF orientation so portrait photos stay portrait.
    img = ImageOps.exif_transpose(img)
    img = img.convert("L")  # 8-bit grayscale — e-ink is grey anyway
    # Cover-fit: scale to fully cover 540x960, then centre-crop.
    sw, sh = img.size
    scale = max(W / sw, H / sh)
    nw, nh = int(round(sw * scale)), int(round(sh * scale))
    img = img.resize((nw, nh), Image.LANCZOS)
    left = (nw - W) // 2
    top = (nh - H) // 2
    img = img.crop((left, top, left + W, top + H))

    dst.parent.mkdir(parents=True, exist_ok=True)
    img.save(dst, format="JPEG", quality=85, progressive=False, optimize=True)
    return dst


def expand(inputs):
    files = []
    for s in inputs:
        p = Path(s)
        if p.is_dir():
            for ext in ("*.jpg", "*.jpeg", "*.png", "*.JPG", "*.JPEG", "*.PNG"):
                files.extend(sorted(p.glob(ext)))
        elif p.is_file():
            files.append(p)
        else:
            print(f"skip: {s} (not a file or dir)", file=sys.stderr)
    # de-dup (case-insensitive on macOS)
    seen = set()
    out = []
    for f in files:
        key = str(f.resolve()).lower()
        if key in seen:
            continue
        seen.add(key)
        out.append(f)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Resize/convert wallpapers for PaperOS screensaver.")
    ap.add_argument("inputs", nargs="+",
                    help="image files or directories to process")
    ap.add_argument("--out", default=None,
                    help="output directory (defaults to last positional arg if it's a dir)")
    ap.add_argument("--beside", action="store_true",
                    help="write <name>-540x960.jpg next to each source file (ignores --out, "
                         "original untouched)")
    args = ap.parse_args()

    # --beside: result lands right next to each source as <stem>-540x960.jpg; the
    # original file is never overwritten. No SD card / --out needed.
    if args.beside:
        files = expand(args.inputs)
        if not files:
            print("no input files found", file=sys.stderr)
            return 1
        print("writing 540x960 baseline JPG beside each source")
        rc = 0
        for src in files:
            if src.stem.endswith("-540x960"):
                continue  # skip our own previously-generated outputs (re-run safety)
            try:
                dst = prepare(src, src.with_name(src.stem + "-540x960.jpg"))
                print(f"  {src.name} -> {dst.name}")
            except Exception as e:
                print(f"  {src.name}: FAILED ({e})", file=sys.stderr)
                rc = 1
        return rc

    # Convention: if --out absent, the LAST positional that's an existing dir is
    # the target, rest are sources. Otherwise --out is the target.
    out_dir = Path(args.out) if args.out else None
    sources = list(args.inputs)
    if out_dir is None and len(sources) >= 2:
        last = Path(sources[-1])
        if last.is_dir() or sources[-1].endswith("/"):
            out_dir = last
            sources = sources[:-1]
    if out_dir is None:
        out_dir = Path("./wallpapers")

    files = expand(sources)
    if not files:
        print("no input files found", file=sys.stderr)
        return 1

    print(f"writing to {out_dir}")
    rc = 0
    for src in files:
        try:
            dst = prepare(src, out_dir / (src.stem + ".jpg"))
            print(f"  {src.name} -> {dst.name}")
        except Exception as e:
            print(f"  {src.name}: FAILED ({e})", file=sys.stderr)
            rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
