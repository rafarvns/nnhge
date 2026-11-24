"""Generates the transitions example's tileset into ../assets/art (original, made for NNHGE, zlib like the rest):
run `python -I make_art.py`.

tiles.png  64x64: a 16x16 tileset, 4 tiles per row (tile ids in the maps are index + 1).
    row 0: 0 stone floor (room 1), 1 tiled floor (room 2), 2 wall, 3 plant
    row 1: 4 rug, 5 crate, 6 curtain cloth (red velvet with a fold), 7 curtain hem (gold fringe at the bottom)
    row 2: 8 loading bar frame (left end), 9 frame (middle), 10 frame (right end), 11 the player's square
    row 3: empty

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


def stone(x, y):
    if x == 0 or y == 0:
        return (70, 64, 58, 255)
    return (128, 116, 100, 255) if (x * 5 + y * 3) % 7 else (112, 100, 88, 255)


def tiled(x, y):
    if x in (0, 8) or y in (0, 8):
        return (40, 70, 90, 255)
    return (90, 150, 170, 255) if ((x // 8) + (y // 8)) % 2 == 0 else (70, 120, 150, 255)


def wall(x, y):
    mortar = (50, 44, 60, 255)
    if y % 8 == 0 or (x + (8 if (y // 8) % 2 else 0)) % 16 == 0:
        return mortar
    return (120, 96, 140, 255) if (x * 7 + y * 3) % 11 else (100, 80, 120, 255)


def plant(x, y):
    base = stone(x, y)
    if 11 <= y <= 14 and 5 <= x <= 10:
        return (160, 90, 50, 255)  # pot
    if y <= 10 and abs(x - 7.5) + abs(y - 6) * 0.6 <= 5:
        return (60, 160, 70, 255) if (x + y) % 3 else (40, 120, 50, 255)
    return base


def rug(x, y):
    if x in (0, 15) or y in (0, 15):
        return (230, 200, 120, 255)
    return (170, 50, 60, 255) if (x + y) % 4 else (200, 80, 80, 255)


def crate(x, y):
    edge = x in (0, 15) or y in (0, 15) or x == y or x == 15 - y
    return (220, 170, 90, 255) if edge else (160, 110, 50, 255)


def cloth(x, y):
    fold = (x % 8) / 8.0
    shade = int(120 + 80 * (1 - abs(fold - 0.5) * 2))
    return (shade, 20, 40, 255)


def hem(x, y):
    if y >= 12:
        return (240, 200, 70, 255) if (x + y) % 2 == 0 else (190, 150, 40, 255)
    return cloth(x, y)


def frame_piece(left, right):
    def pixel(x, y):
        if y in (3, 12) or (left and x == 2 and 3 <= y <= 12) or (right and x == 13 and 3 <= y <= 12):
            return (240, 240, 250, 255)
        if 4 <= y <= 11 and (not left or x > 2) and (not right or x < 13):
            return (20, 20, 40, 255)
        return CLEAR

    return pixel


def square(x, y):
    if x in (0, 15) or y in (0, 15):
        return (20, 60, 80, 255)
    if 4 <= x <= 6 and 4 <= y <= 6:
        return (240, 255, 255, 255)
    return (60, 210, 230, 255)


TILES = [stone, tiled, wall, plant, rug, crate, cloth, hem,
         frame_piece(True, False), frame_piece(False, False), frame_piece(False, True), square]


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
