"""Generates the example's art (original, zlib like the rest of NNHGE): run `python -I make_art.py`.

hero.png  64x16: a round critter, 4 walking frames of 16x16, facing right.
tiles.png 64x32: two 32x32 tiles, grass (left) and dirt (right).

Pure Python (zlib + struct), no image library needed.
"""

import struct
import sys
import zlib
from pathlib import Path


def write_png(path, width, height, pixels):
    """pixels: rows of (r, g, b, a) tuples."""
    raw = b"".join(b"\x00" + b"".join(bytes(p) for p in row) for row in pixels)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    Path(path).write_bytes(png)


CLEAR = (0, 0, 0, 0)
HERO_COLORS = {
    ".": CLEAR,
    "K": (24, 32, 48, 255),     # outline
    "B": (64, 178, 168, 255),   # body
    "L": (140, 226, 204, 255),  # highlight
    "W": (250, 250, 245, 255),  # eye
    "P": (24, 24, 36, 255),     # pupil
    "F": (240, 160, 64, 255),   # feet
}

HERO_BODY = [
    ".....KKKKKK.....",
    "...KKLLLLLLKK...",
    "..KLLLBBBBBBBK..",
    "..KLBBBBBBBBBK..",
    ".KLBBBWWBBBWWBK.",
    ".KBBBBWPBBBWPBK.",
    ".KBBBBWPBBBWPBK.",
    ".KBBBBBBBBBBBBK.",
    ".KBBBBBBBBKKBBK.",
    ".KBBBBBBBBBBBBK.",
    "..KBBBBBBBBBBK..",
    "...KKBBBBBBKK...",
    ".....KKKKKK.....",
]

# Feet per frame (two rows) and how far the body rises: a 4-step walk cycle.
HERO_FRAMES = [
    (["....FF....FF....", "....FF....FF...."], 0),
    (["...FF......FF...", "................"], 1),
    (["....FF....FF....", "....FF....FF...."], 0),
    (["......FFFF......", "......FFFF......"], 1),
]


def hero():
    width, height = 64, 16
    pixels = [[CLEAR] * width for _ in range(height)]
    for f, (feet, rise) in enumerate(HERO_FRAMES):
        rows = ["................"] * (1 - rise) + HERO_BODY + feet + ["................"] * rise
        assert len(rows) == 16, len(rows)
        for y, row in enumerate(rows):
            assert len(row) == 16, row
            for x, c in enumerate(row):
                pixels[y][f * 16 + x] = HERO_COLORS[c]
    return width, height, pixels


def noise(x, y, seed):
    """Deterministic 0..255 noise, so the art is reproducible."""
    n = (x * 374761393 + y * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return (n ^ (n >> 16)) & 0xFF


def dirt(x, y, seed):
    n = noise(x, y, seed)
    if n < 18:
        return (150, 110, 72, 255)  # light speck
    if n > 236:
        return (78, 52, 34, 255)  # dark speck
    return (116, 80, 52, 255)


def tiles():
    width, height = 64, 32
    pixels = [[CLEAR] * width for _ in range(height)]
    for y in range(32):
        for x in range(32):
            # Grass tile: grass on the top rows with blades hanging into the dirt.
            blade = 6 + (noise(x, 0, 7) % 4)
            if y < 5:
                g = (84, 176, 72, 255) if noise(x, y, 3) > 40 else (120, 204, 96, 255)
            elif y < blade:
                g = (60, 140, 56, 255)
            else:
                g = dirt(x, y, 1)
            pixels[y][x] = g
            # Dirt tile, with a few stones.
            stone = noise(x // 4, y // 4, 11) > 244
            pixels[y][32 + x] = (128, 120, 112, 255) if stone else dirt(x, y, 2)
    return width, height, pixels


def main():
    here = Path(__file__).resolve().parent
    for name, make in (("hero.png", hero), ("tiles.png", tiles)):
        w, h, px = make()
        write_png(here / name, w, h, px)
        print(f"wrote {name} {w}x{h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
