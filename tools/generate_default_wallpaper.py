#!/usr/bin/env python3
"""Render data/default_wallpaper.bin — 540x960 raw 4bpp grayscale.

Default mode (no args) generates a gradient with a "PaperOS" title baked in.
With --input <path> instead resizes/crops that image to 540x960 portrait and
quantises it to 16 grey levels, packing two pixels per byte.

The binary is INCBIN'd into firmware via board_build.embed_files; rebuild and
flash after regenerating.
"""

import argparse
import os
import sys

from PIL import Image, ImageDraw, ImageFont

W, H = 540, 960


def make_gradient() -> Image.Image:
    img = Image.new("L", (W, H), 0)
    draw = ImageDraw.Draw(img)
    for y in range(H):
        g = int(40 + (y / H) * 80)
        draw.line([(0, y), (W, y)], fill=g)
    try:
        f = ImageFont.truetype("/Library/Fonts/Arial.ttf", 56)
    except OSError:
        f = ImageFont.load_default()
    draw.text((W // 2 - 100, H // 2 - 30), "PaperOS", fill=240, font=f)
    return img


def fit_to_canvas(src_path: str) -> Image.Image:
    src = Image.open(src_path).convert("L")
    # Cover: scale so the image fully covers 540x960, then centre-crop.
    sw, sh = src.size
    scale = max(W / sw, H / sh)
    nw, nh = int(round(sw * scale)), int(round(sh * scale))
    src = src.resize((nw, nh), Image.LANCZOS)
    left = (nw - W) // 2
    top = (nh - H) // 2
    return src.crop((left, top, left + W, top + H))


def pack_4bpp(img: Image.Image) -> bytes:
    assert img.size == (W, H)
    px = img.load()
    raw = bytearray(W * H // 2)
    for y in range(H):
        for x in range(0, W, 2):
            a = px[x, y] >> 4
            b = px[x + 1, y] >> 4 if x + 1 < W else 0
            raw[(y * W + x) // 2] = (a << 4) | b
    return bytes(raw)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", help="source image (jpg/png); else built-in gradient")
    ap.add_argument(
        "--output",
        default="data/default_wallpaper.bin",
        help="destination raw 4bpp file",
    )
    args = ap.parse_args()

    img = fit_to_canvas(args.input) if args.input else make_gradient()
    raw = pack_4bpp(img)
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(raw)
    print(f"wrote {len(raw)} bytes to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
