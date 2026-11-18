"""Draws NNHGE's default package art into cmake/3ds/ (spec 009): the icon games get without ICON, and the
banner and banner sound of a .cia without BANNER/BANNER_AUDIO. All original: a 5x7 pixel font defined
below, gradients and a synthesized jingle. The outputs are committed; rerun only to change them:
  python scripts/make_package_art.py   (needs: pip install --user pillow soundfile numpy; not `python -I`)
"""

import math
import os

import numpy as np
import soundfile as sf
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "cmake", "3ds")

# 5x7 glyphs, one string per row, '#' = pixel.
GLYPHS = {
    "N": ["#...#", "##..#", "#.#.#", "#.#.#", "#..##", "#...#", "#...#"],
    "H": ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "G": [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    "M": ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
    "W": ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"],
    "I": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    " ": [".....", ".....", ".....", ".....", ".....", ".....", "....."],
}

YELLOW = (255, 210, 80, 255)
SHADOW = (10, 14, 30, 255)
WHITE = (235, 240, 255, 255)


def text_width(text, scale):
    return len(text) * 6 * scale - scale


def draw_text(draw, x, y, text, scale, color, shadow=True):
    for layer, (dx, dy, c) in enumerate([(scale, scale, SHADOW), (0, 0, color)]):
        if layer == 0 and not shadow:
            continue
        cx = x
        for ch in text:
            for row, bits in enumerate(GLYPHS[ch]):
                for col, bit in enumerate(bits):
                    if bit == "#":
                        px, py = cx + col * scale + dx, y + row * scale + dy
                        draw.rectangle([px, py, px + scale - 1, py + scale - 1], fill=c)
            cx += 6 * scale


def gradient(width, height, top, bottom):
    image = Image.new("RGBA", (width, height))
    draw = ImageDraw.Draw(image)
    for y in range(height):
        t = y / max(1, height - 1)
        color = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)
        draw.line([(0, y), (width - 1, y)], fill=color)
    return image


def icon():
    image = gradient(48, 48, (40, 70, 140), (18, 24, 52))
    mask = Image.new("L", (48, 48), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, 47, 47], radius=8, fill=255)
    out = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    out.paste(image, (0, 0), mask)
    draw = ImageDraw.Draw(out)
    draw.rounded_rectangle([0, 0, 47, 47], radius=8, outline=(90, 130, 220, 255))
    draw_text(draw, (48 - text_width("NNH", 2)) // 2, 7, "NNH", 2, YELLOW)
    draw_text(draw, (48 - text_width("GE", 2)) // 2, 25, "GE", 2, WHITE)
    out.save(os.path.join(OUT, "default_icon.png"))


def banner():
    image = gradient(256, 128, (40, 70, 140), (14, 18, 40))
    grid = Image.new("RGBA", (256, 128), (0, 0, 0, 0))
    grid_draw = ImageDraw.Draw(grid)
    for i in range(0, 256, 16):  # a faint grid, like a level editor (blended, not painted over)
        grid_draw.line([(i, 0), (i, 127)], fill=(255, 255, 255, 22))
    for i in range(0, 128, 16):
        grid_draw.line([(0, i), (255, i)], fill=(255, 255, 255, 22))
    image = Image.alpha_composite(image, grid)
    draw = ImageDraw.Draw(image)
    draw_text(draw, (256 - text_width("NNHGE", 6)) // 2, 30, "NNHGE", 6, YELLOW)
    draw_text(draw, (256 - text_width("MADE WITH NNHGE", 1)) // 2, 92, "MADE WITH NNHGE", 1, WHITE, shadow=False)
    image.save(os.path.join(OUT, "default_banner.png"))


def jingle():
    rate = 22050
    notes = [72, 76, 79, 84]  # C5 E5 G5 C6
    step = int(0.12 * rate)
    total = step * 3 + int(0.9 * rate)
    data = np.zeros(total, dtype=np.float32)
    for i, midi in enumerate(notes):
        freq = 440.0 * 2 ** ((midi - 69) / 12)
        n = total - i * step
        t = np.arange(n) / rate
        phase = (freq * t) % 1.0
        wave = np.where(phase < 0.25, 1.0, -1.0) * 0.6 + np.sin(2 * math.pi * freq * t) * 0.4
        data[i * step:] += (wave * np.exp(-t / (0.12 if i < 3 else 0.35))).astype(np.float32)
    data *= 0.8 / np.max(np.abs(data))
    sf.write(os.path.join(OUT, "default_banner.wav"), data, rate, subtype="PCM_16")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    icon()
    banner()
    jingle()
