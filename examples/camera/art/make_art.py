"""Generates the camera example's tileset into ../assets/art (original, made for NNHGE, zlib like the rest):
run `python -I make_art.py`.

tiles.png  64x64: a 16x16 tileset, 4 tiles per row (tile ids in the game are index + 1). Every tile has a
one-pixel frame in a contrasting colour, so a seam (a gap or an overlap between zoomed tiles) shows as a doubled
or a missing line.
    row 0: 0 floor A (blue), 1 floor B (teal), 2 wall, 3 door
    row 1: 4 floor C (green), 5 floor D (purple), 6 crate, 7 arrow (points right: shows flips)
    row 2-3: empty

Pure Python (zlib + struct), no image library needed; deterministic, so the PNG is reproducible.
"""

import struct
import zlib
from pathlib import Path

SIZE = 16
CLEAR = (0, 0, 0, 0)


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


def framed(fill, frame, dot):
    """A floor tile: a frame on the top and left edges (so neighbours don't double it) and a dot in the middle."""

    def pixel(x, y):
        if x == 0 or y == 0:
            return frame
        if 6 <= x <= 9 and 6 <= y <= 9:
            return dot
        return fill

    return pixel


def wall(x, y):
    mortar = (60, 52, 48, 255)
    if y % 8 == 0 or (x + (8 if (y // 8) % 2 else 0)) % 16 == 0:
        return mortar
    return (176, 96, 72, 255) if (x * 7 + y * 3) % 11 else (150, 82, 60, 255)


def door(x, y):
    if x == 0 or y == 0 or x == 15:
        return (240, 220, 120, 255)
    return (110, 70, 40, 255) if x != 11 or y != 8 else (240, 220, 120, 255)


def crate(x, y):
    edge = x in (0, 15) or y in (0, 15) or x == y or x == 15 - y
    return (220, 170, 90, 255) if edge else (160, 110, 50, 255)


def arrow(x, y):
    base = framed((40, 40, 60, 255), (250, 250, 250, 255), (40, 40, 60, 255))(x, y)
    if 7 <= y <= 8 and 3 <= x <= 11:
        return (250, 80, 80, 255)
    if 3 <= x <= 12 and abs(y - 7.5) <= (12 - x) * 0.6 and x >= 9:
        return (250, 80, 80, 255)
    return base


TILES = [
    framed((44, 66, 120, 255), (230, 230, 255, 255), (90, 120, 200, 255)),
    framed((36, 104, 104, 255), (220, 255, 250, 255), (80, 170, 160, 255)),
    wall,
    door,
    framed((50, 110, 56, 255), (230, 255, 220, 255), (100, 180, 100, 255)),
    framed((90, 56, 112, 255), (250, 230, 255, 255), (160, 110, 190, 255)),
    crate,
    arrow,
]


def main():
    width, height = 64, 64
    pixels = [[CLEAR] * width for _ in range(height)]
    for index, tile in enumerate(TILES):
        ox, oy = (index % 4) * SIZE, (index // 4) * SIZE
        for y in range(SIZE):
            for x in range(SIZE):
                pixels[oy + y][ox + x] = tile(x, y)
    out = Path(__file__).resolve().parent.parent / "assets" / "art"
    out.mkdir(parents=True, exist_ok=True)
    write_png(out / "tiles.png", width, height, pixels)


if __name__ == "__main__":
    main()
