"""Draws the hierarchy example's art (spec 022 AC-5): assets/art/yard.png, a 128x32 RGBA sheet of original shapes —
ship, gun, walker, hat, cart, rider, crate, block, bullet, hand. Regions are listed in yard.hpp. Standard library only;
the output is committed: python -I examples/hierarchy/art/make_art.py"""

import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets", "art", "yard.png")
W, H = 128, 32
img = [[(0, 0, 0, 0)] * W for _ in range(H)]


def rect(x, y, w, h, c, edge=None):
    for j in range(h):
        for i in range(w):
            on_edge = edge is not None and (i in (0, w - 1) or j in (0, h - 1))
            img[y + j][x + i] = edge if on_edge else c


def disc(cx, cy, r, c):
    for j in range(H):
        for i in range(W):
            if (i - cx) ** 2 + (j - cy) ** 2 <= r * r:
                img[j][i] = c


DARK = (30, 34, 48, 255)
rect(0, 0, 24, 16, (90, 150, 220, 255), DARK)      # ship hull
rect(16, 5, 6, 6, (230, 240, 255, 255))            # its cockpit, at the nose (+x)
rect(24, 0, 14, 4, (150, 156, 170, 255), DARK)     # gun barrel (+x)
rect(40, 0, 12, 16, (220, 120, 70, 255), DARK)     # walker body
rect(47, 3, 3, 3, (255, 255, 255, 255))            # its eye, facing +x
rect(52, 0, 10, 6, (90, 60, 140, 255), DARK)       # hat
rect(64, 0, 24, 12, (150, 100, 60, 255), DARK)     # cart
rect(88, 0, 10, 14, (90, 190, 110, 255), DARK)     # rider
rect(100, 0, 12, 12, (190, 150, 80, 255), (110, 80, 40, 255))  # crate
rect(104, 4, 4, 4, (110, 80, 40, 255))
rect(112, 0, 16, 16, (200, 90, 160, 255), DARK)    # block
rect(124, 6, 4, 4, (255, 230, 120, 255))           # its +x mark
disc(2, 18, 1.6, (255, 230, 90, 255))              # bullet (0, 16, 4, 4)
rect(4, 16, 4, 4, (240, 200, 170, 255), DARK)      # hand


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def main():
    rows = b"".join(b"\0" + b"".join(bytes(p) for p in row) for row in img)
    header = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(data)


if __name__ == "__main__":
    main()
