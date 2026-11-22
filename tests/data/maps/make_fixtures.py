"""Generates the tileset images of the map converter fixtures (spec 012 T5.7, T5.9). The PNGs are committed;
rerun only to change them:  python -I tests/data/maps/make_fixtures.py   (standard library only).

The Tiled files (*.tmj, *.tsj) next to them are written by hand. The converter reads only a PNG's size; the
images are real PNGs anyway, so the fixtures open in Tiled.
"""

import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def png(name, width, height, gray=False):
    """A plain image: RGBA stripes, or all-black grayscale for large ones (compresses to almost nothing)."""
    if gray:
        rows = b"".join(b"\0" + bytes(width) for _ in range(height))
        header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    else:
        rows = b""
        for y in range(height):
            row = bytearray()
            for x in range(width):
                shade = 60 + 40 * ((x // 8 + y // 8) % 4)
                row += bytes((shade, 120, 200 - shade // 2, 255))
            rows += b"\0" + bytes(row)
        header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    path = os.path.join(HERE, "tiles", name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def main():
    png("good.png", 64, 32)       # 8 tiles of 16x16 (tiles/good.tsj)
    png("second.png", 64, 64)     # 9 tiles of 16x16 with margin 1 and spacing 4 (embedded in good.tmj)
    png("npot.png", 100, 60)      # not a power of two
    png("wide.png", 64, 16)       # 2 tiles of 32x16 (non-square)
    png("big.png", 1024, 1024, gray=True)  # 16384 tiles of 8x8: over 8191 ids


if __name__ == "__main__":
    main()
