"""Generates the queries example's tileset into ../assets/art (original, made for NNHGE, zlib like the rest):
run `python -I make_art.py`.

tiles.png  64x16: four 16x16 tiles, left to right (tile ids in the game are index + 1):
    0 wall (grey bricks), 1 water (blue with light waves), 2 plank (a brown board on the top 5 rows, the rest
    clear), 3 floor (dark slate with a faint grid on its top and left edges).

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


def wall(x, y):
    mortar = (58, 56, 64, 255)
    row = y // 4
    offset = 0 if row % 2 == 0 else 4
    if y % 4 == 3 or (x + offset) % 8 == 7:
        return mortar
    shade = 120 + ((x + offset) // 8 + row) % 2 * 18
    return (shade, shade - 6, shade + 4, 255)


def water(x, y):
    if (y in (4, 11)) and (x + y) % 6 < 3:
        return (150, 200, 250, 255)
    return (40, 90, 170, 255)


def plank(x, y):
    if y >= 5:
        return CLEAR
    if y == 0 or y == 4:
        return (90, 56, 30, 255)
    if x % 8 == 0:
        return (110, 70, 38, 255)
    return (168, 116, 64, 255)


def floor(x, y):
    if x == 0 or y == 0:
        return (44, 48, 60, 255)
    return (30, 33, 42, 255)


def main():
    tiles = [wall, water, plank, floor]
    pixels = [[tiles[x // SIZE](x % SIZE, y) for x in range(SIZE * len(tiles))] for y in range(SIZE)]
    out = Path(__file__).resolve().parent.parent / "assets" / "art"
    out.mkdir(parents=True, exist_ok=True)
    write_png(out / "tiles.png", SIZE * len(tiles), SIZE, pixels)


if __name__ == "__main__":
    main()
