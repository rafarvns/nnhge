"""Generates the prefabs example's tiles into ../assets/tiles (original, made for NNHGE, zlib like the rest):
run `python -I make_art.py`, then `python -I make_room.py` for the room.

room.png  64x16: a 16x16 tileset of 4 tiles (ids in the map are index + 1): 0 floor, 1 wall, 2 door, 3 rug.

Pure Python (zlib + struct), deterministic: the PNG is reproducible.
"""

import struct
import zlib
from pathlib import Path


def write_png(path, width, height, pixels):
    raw = b"".join(b"\x00" + b"".join(bytes(p) for p in row) for row in pixels)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def tile_pixel(tile, x, y):
    if tile == 0:  # floor: dark boards
        shade = 38 + (6 if (y // 4) % 2 == 0 else 0) + (4 if x in (0, 15) else 0)
        return (shade, shade + 6, shade + 16, 255)
    if tile == 1:  # wall: stone blocks
        mortar = y % 8 == 7 or (x + (4 if (y // 8) % 2 else 0)) % 16 == 15
        return (70, 74, 96, 255) if mortar else (112, 118, 146, 255)
    if tile == 2:  # door: wooden planks with a frame
        if x < 2 or x > 13 or y < 2:
            return (90, 60, 40, 255)
        return (150, 98, 52, 255) if x % 4 != 0 else (120, 76, 40, 255)
    rug = (x + y) % 6 < 3  # rug: a checker of two reds
    return (120, 40, 52, 255) if rug else (96, 30, 44, 255)


def main():
    here = Path(__file__).resolve().parent
    rows = [[tile_pixel(x // 16, x % 16, y) for x in range(64)] for y in range(16)]
    write_png(here.parent / "assets" / "tiles" / "room.png", 64, 16, rows)


if __name__ == "__main__":
    main()
