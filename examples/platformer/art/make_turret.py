"""Draws the platformer's turret (spec 022 AC-4): assets/art/turret.png, 32x16 RGBA — the base at (0, 0, 16, 16) and the
barrel at (16, 0, 12, 4), whose pivot is its back end (0, 2). Original art; standard library only. The output is
committed; rerun only to change it: python -I examples/platformer/art/make_turret.py"""

import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "art", "turret.png")
W, H = 32, 16

CLEAR = (0, 0, 0, 0)
DARK = (52, 56, 70, 255)
BODY = (78, 84, 102, 255)
LIGHT = (128, 136, 156, 255)
RIM = (200, 70, 60, 255)
STEEL = (150, 156, 170, 255)
STEEL_DARK = (96, 100, 114, 255)
TIP = (40, 40, 46, 255)


def base(x, y):
    """A squat bunker: a dome on a block, a red rim where the barrel sits."""
    if y >= 9:  # the block
        if x in (0, 15) or y == 15:
            return DARK
        return LIGHT if y == 9 else BODY
    cx, cy, r = 7.5, 9.0, 6.5  # the dome
    d2 = (x - cx) ** 2 + (y - cy) ** 2
    if d2 > r * r:
        return CLEAR
    if d2 > (r - 1.2) ** 2:
        return DARK
    return RIM if abs(y - 6) <= 0 and 5 <= x <= 10 else BODY


def barrel(x, y):
    """A 12x4 barrel: steel with a dark band, the muzzle end darker."""
    if x >= 11:
        return TIP
    if y in (0, 3):
        return STEEL_DARK
    return STEEL if x != 3 else STEEL_DARK


def pixel(x, y):
    if x < 16:
        return base(x, y)
    if x < 28 and y < 4:
        return barrel(x - 16, y)
    return CLEAR


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def main():
    rows = b"".join(b"\0" + b"".join(bytes(pixel(x, y)) for x in range(W)) for y in range(H))
    header = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(data)


if __name__ == "__main__":
    main()
