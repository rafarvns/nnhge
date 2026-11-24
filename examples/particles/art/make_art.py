"""Generates the particles example's sheet into ../assets/art (original, made for NNHGE, zlib like the rest):
run `python -I make_art.py`.

particles.png 64x64:
    row y 0..7:   spark (0), smoke puff (8), ember (16), star (24), rain streak (32, 2x8)
    row y 8..15:  four flame frames (0, 8, 16, 24), big to small
    y 32..47:     the ship (0, 16x16, pointing right)
    y 48..63:     a brick tile (48, 16x16: tile 15 of a 16x16 tileset)
Particles are tinted by their colour over life, so the sprites are mostly white or light.

Pure Python (zlib + struct), no image library needed; deterministic, so the PNG is reproducible.
"""

import math
import struct
import zlib
from pathlib import Path

W = H = 64
CLEAR = (0, 0, 0, 0)


def write_png(path, width, height, pixels):
    raw = b"".join(b"\x00" + b"".join(bytes(p) for p in row) for row in pixels)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    Path(path).write_bytes(png)


def disc(px, ox, oy, r, color, soft=False):
    for y in range(8):
        for x in range(8):
            d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
            if d <= r:
                a = color[3] if not soft else int(color[3] * max(0.25, 1 - d / (r + 0.5)))
                px[oy + y][ox + x] = (color[0], color[1], color[2], a)


def main():
    px = [[CLEAR for _ in range(W)] for _ in range(H)]
    # spark: a plus with a bright centre
    for i in range(8):
        px[3][i] = px[4][i] = (255, 255, 255, 120)
        px[i][3] = px[i][4] = (255, 255, 255, 120)
    for y in (3, 4):
        for x in (2, 3, 4, 5):
            px[y][x] = (255, 255, 255, 255)
    disc(px, 8, 0, 3.6, (235, 235, 235, 255), soft=True)  # smoke puff
    disc(px, 16, 0, 1.8, (255, 255, 255, 255))  # ember
    for i in range(8):  # star: a diagonal cross
        px[i][24 + i] = px[i][31 - i] = (255, 255, 255, 200)
    px[3][27] = px[3][28] = px[4][27] = px[4][28] = (255, 255, 255, 255)
    for y in range(8):  # rain streak
        px[y][32] = (200, 220, 255, 90 + y * 20)
    for f, r in enumerate((3.8, 3.0, 2.2, 1.4)):  # flame frames
        for y in range(8):
            for x in range(8):
                dx, dy = x + 0.5 - 4, (y + 0.5 - 5) * 0.8
                if math.hypot(dx, dy) <= r:
                    px[8 + y][f * 8 + x] = (255, 255 - int(40 * (y / 8)), 255 - int(120 * (1 - y / 8)), 255)
    # ship: a dart pointing right
    for y in range(16):
        for x in range(16):
            dy = abs(y + 0.5 - 8)
            if x < 14 and dy < (14 - x) * 0.55 and not (x < 4 and dy < 2):
                edge = dy > (14 - x) * 0.55 - 1.2
                px[32 + y][x] = (60, 90, 160, 255) if edge else (170, 200, 240, 255)
    px[39][9] = px[40][9] = px[39][10] = px[40][10] = (255, 240, 120, 255)  # cockpit
    for y in range(16):  # brick tile
        for x in range(16):
            mortar = y % 8 == 7 or (x + (4 if (y // 8) % 2 else 0)) % 8 == 7
            px[48 + y][48 + x] = (90, 70, 60, 255) if mortar else (170, 90 + (x * 3 + y) % 10, 70, 255)
    out = Path(__file__).resolve().parent.parent / "assets" / "art" / "particles.png"
    write_png(out, W, H, px)
    print("wrote", out)


if __name__ == "__main__":
    main()
