from PIL import Image, ImageDraw
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
FRAMES = os.path.join(ROOT, 'frames')
SKIN = Image.open(os.path.join(ROOT, 'hollowknight.png')).convert('RGBA')


def parse_quad(line):
    parts = line.strip().split()
    return {
        'tex': int(parts[0]),
        'x': float(parts[1]),
        'y': float(parts[2]),
        'w': float(parts[3]),
        'h': float(parts[4]),
        'rot': float(parts[5]),
        'r': float(parts[6]),
        'g': float(parts[7]),
        'b': float(parts[8]),
        'a': float(parts[9]),
        'u0': float(parts[10]),
        'v0': float(parts[11]),
        'u1': float(parts[12]),
        'v1': float(parts[13]),
        'flip': int(parts[14]) != 0,
    }


def render_quads(quads, width, height):
    canvas = Image.new('RGBA', (int(width), int(height)), (27, 30, 43, 255))
    for q in quads:
        if q['tex'] != 1:
            continue
        cw, ch = SKIN.size
        left = int(q['u0'] * cw)
        top = int(q['v0'] * ch)
        right = int(q['u1'] * cw)
        bottom = int(q['v1'] * ch)
        cell = SKIN.crop((left, top, right, bottom))
        cell = cell.resize((int(q['w']), int(q['h'])), Image.Resampling.LANCZOS)
        if q['flip']:
            cell = cell.transpose(Image.FLIP_LEFT_RIGHT)
        x = int(q['x'] - q['w'] / 2)
        y = int(q['y'] - q['h'] / 2)
        if abs(q['rot']) > 0.001:
            cell = cell.rotate(-q['rot'] * 180 / 3.14159265, expand=True)
            x = int(q['x'] - cell.width / 2)
            y = int(q['y'] - cell.height / 2)
        canvas.paste(cell, (x, y), cell)
    return canvas


def main():
    with open(os.path.join(FRAMES, 'meta.txt')) as f:
        width, height, count = map(float, f.read().strip().split())
    width, height, count = int(width), int(height), int(count)

    images = []
    for i in range(count):
        path = os.path.join(FRAMES, f'quads_{i:02d}.txt')
        with open(path) as f:
            quads = [parse_quad(line) for line in f if line.strip()]
        img = render_quads(quads, width, height)
        images.append(img)

    out_path = os.path.join(ROOT, 'walk_animation.gif')
    images[0].save(
        out_path,
        save_all=True,
        append_images=images[1:],
        duration=80,
        loop=0,
        optimize=False,
    )
    print(f'Wrote {out_path} ({len(images)} frames)')


if __name__ == '__main__':
    main()
