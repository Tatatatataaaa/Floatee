#!/usr/bin/env python3
"""
render_skin.py - Rasterize the quads exported by the extracted Tee pipeline
(example/main.cpp) into a PNG, sampling the real protocol-7 skin atlas.

Pipeline:
  1. Build & run tee_render_example  ->  quads.txt + skin_render.svg
  2. python render_skin.py           ->  skin_render.png

Quad file format (one per line):
  tex_id cx cy w h rot r g b a u0 v0 u1 v1 flip

Texture ids:
  1 = skin atlas (hollowknight.png, the full protocol-7 4K template).
  2 = emoticons atlas (emoticons.png, the over-head emoticon bubbles).

Dependencies: Pillow only.
"""

import math
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)  # tee_render (where the C++ exe writes quads.txt)
SKIN_ATLAS = os.path.join(HERE, "hollowknight.png")
EMOTICON_ATLAS = os.path.join(HERE, "emoticons.png")
QUADS = os.path.join(ROOT, "quads.txt")
OUT_PNG = os.path.join(ROOT, "skin_render.png")

CANVAS_W, CANVAS_H = 3840, 2160


def load_textures():
    textures = {}
    atlas = Image.open(SKIN_ATLAS).convert("RGBA")
    textures[1] = atlas
    emoticons = Image.open(EMOTICON_ATLAS).convert("RGBA")
    textures[2] = emoticons
    return textures


def sample_region(tex, u0, v0, u1, v1, w, h):
    """Crop the UV region from the texture, resized to w x h."""
    tw, th = tex.size
    x0, y0 = u0 * tw, v0 * th
    x1, y1 = u1 * tw, v1 * th
    region = tex.crop((int(round(x0)), int(round(y0)), int(round(x1)), int(round(y1))))
    if region.size != (w, h):
        region = region.resize((int(round(w)), int(round(h))), Image.LANCZOS)
    return region


def render(canvas):
    textures = load_textures()
    quads = []
    with open(QUADS) as fh:
        for line in fh:
            parts = line.split()
            if len(parts) < 14:
                continue
            quads.append({
                "tex": int(parts[0]),
                "cx": float(parts[1]),
                "cy": float(parts[2]),
                "w": float(parts[3]),
                "h": float(parts[4]),
                "rot": float(parts[5]),
                "r": float(parts[6]),
                "g": float(parts[7]),
                "b": float(parts[8]),
                "a": float(parts[9]),
                "u0": float(parts[10]),
                "v0": float(parts[11]),
                "u1": float(parts[12]),
                "v1": float(parts[13]),
                "flip": int(parts[14]) if len(parts) > 14 else 0,
            })

    for q in quads:
        tex = textures.get(q["tex"])
        if tex is None:
            continue
        w = int(round(q["w"]))
        h = int(round(q["h"]))
        if w <= 0 or h <= 0:
            continue
        # Sample the UV region at the quad's own size (this is what the GPU would
        # sample); for crispness sample a bit larger.
        region = sample_region(tex, q["u0"], q["v0"], q["u1"], q["v1"], w, h)
        if q["flip"]:
            region = region.transpose(Image.FLIP_LEFT_RIGHT)

        # Alpha from the pipeline color multiplies the texture alpha.
        alpha = max(0.0, min(1.0, q["a"]))
        if alpha < 1.0:
            r, g, b, a = region.split()
            a = a.point(lambda v: int(v * alpha))
            region = Image.merge("RGBA", (r, g, b, a))

        rot = q["rot"]
        # Rotate the sampled region about its center.
        rot_img = region.rotate(-math.degrees(rot), resample=Image.BICUBIC, expand=False)

        cx, cy = q["cx"], q["cy"]
        canvas.alpha_composite(rot_img, (int(round(cx - w / 2)), int(round(cy - h / 2))))

    return canvas


def main():
    canvas = Image.new("RGBA", (CANVAS_W, CANVAS_H), (27, 30, 43, 255))
    render(canvas)
    canvas.save(OUT_PNG)
    print(f"Wrote {OUT_PNG} ({CANVAS_W}x{CANVAS_H})")


if __name__ == "__main__":
    main()
