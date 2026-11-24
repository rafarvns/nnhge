"""Generates the pathfinding example's tileset into ../assets/art (original, made for NNHGE, zlib like the rest):
run `python -I make_art.py`.

tiles.png  128x16: eight 16x16 tiles, left to right (tile ids in the level are index + 1):
    0 floor (dark flagstones), 1 wall (grey bricks), 2 water (shallow, light waves), 3 swamp (green-brown mud),
    4 road (sand with ruts), 5 open door (a floor with a doorframe), 6 closed door (wooden planks), 7 deep water.
The hero, the chasers and the goal marker are drawn as shapes.

Pure Python (zlib + struct), no image library needed; deterministic, so the PNG is reproducible.
"""

import struct
import zlib
from pathlib import Path

SIZE = 16


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


def floor(x, y):
    if x % 8 == 0 or y % 8 == 0:
        return (40, 42, 52, 255)
    return (58, 60, 72, 255)


def wall(x, y):
    row = y // 4
    offset = 0 if row % 2 == 0 else 4
    if y % 4 == 3 or (x + offset) % 8 == 7:
        return (52, 50, 58, 255)
    shade = 118 + ((x + offset) // 8 + row) % 2 * 16
    return (shade, shade - 6, shade + 4, 255)


def water(x, y):
    if y in (4, 11) and (x + y) % 6 < 3:
        return (150, 205, 250, 255)
    return (50, 110, 190, 255)


def swamp(x, y):
    if (x * 7 + y * 3) % 11 == 0:
        return (120, 150, 60, 255)
    return (70, 84, 44, 255)


def road(x, y):
    if x in (4, 11):
        return (168, 140, 92, 255)
    return (196, 170, 118, 255)


def door_open(x, y):
    if x in (0, 15):
        return (110, 70, 38, 255)
    return floor(x, y)


def door_closed(x, y):
    if x % 5 == 0 or y in (2, 13):
        return (90, 56, 30, 255)
    return (160, 108, 58, 255)


def deep(x, y):
    if y in (6,) and (x + y) % 7 < 2:
        return (70, 110, 170, 255)
    return (18, 44, 110, 255)


def main():
    tiles = [floor, wall, water, swamp, road, door_open, door_closed, deep]
    pixels = [[tiles[x // SIZE](x % SIZE, y) for x in range(SIZE * len(tiles))] for y in range(SIZE)]
    out = Path(__file__).resolve().parent.parent / "assets" / "art"
    out.mkdir(parents=True, exist_ok=True)
    write_png(out / "tiles.png", SIZE * len(tiles), SIZE, pixels)


if __name__ == "__main__":
    main()
