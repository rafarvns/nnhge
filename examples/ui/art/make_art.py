"""Generates the UI example's sheet into ../assets/art (original, made for NNHGE, zlib like the rest):
run `python -I make_art.py`.

ui.png 128x128 (themes.cpp names every region):
    (0, 0)    24x24  panel frame (9-slice, insets 6): navy with a light rim and rounded corners
    (24, 0)   16x16  button frames (9-slice, insets 4): normal (24), focused (40), pressed (56), disabled (72)
    (88, 0)   16x6   slider track (insets 3); (104, 0) 6x12 knob; (112, 0) 16x6 slider fill (insets 3)
    (0, 24)   10x10  toggle off; (10, 24) 10x10 toggle on
    (20, 24)  8x10   choice arrow right; (28, 24) 8x10 left
    (36, 24)  12x10  hand cursor (pointing right)
    (48, 24)  12x12  heart
    (64, 24)  16x8   bar background (insets 2); (80, 24) 16x8 bar fill (insets 2)
    (96, 24)  16x12  list row, focused (insets 3)
    (0, 40)   16x16  x 16 item icons, 8 per row (rows y 40 and 56)
Pure Python (zlib + struct), no image library; deterministic, so the PNG is reproducible.
"""

import struct
import zlib
from pathlib import Path

W = H = 128
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


def rect(px, x, y, w, h, c):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            px[yy][xx] = c


def frame(px, x, y, w, h, fill, rim, shade, corner=2):
    """A rounded frame: rim colour, a darker inner line on the bottom/right, the fill inside."""
    rect(px, x, y, w, h, rim)
    rect(px, x + 1, y + 1, w - 2, h - 2, shade)
    rect(px, x + 1, y + 1, w - 3, h - 3, fill)
    for cx, cy in ((x, y), (x + w - 1, y), (x, y + h - 1), (x + w - 1, y + h - 1)):
        px[cy][cx] = CLEAR
    if corner > 1:
        for cx, cy in ((x + 1, y), (x, y + 1), (x + w - 2, y), (x + w - 1, y + 1), (x, y + h - 2), (x + 1, y + h - 1),
                       (x + w - 1, y + h - 2), (x + w - 2, y + h - 1)):
            px[cy][cx] = rim


def main():
    px = [[CLEAR for _ in range(W)] for _ in range(H)]
    frame(px, 0, 0, 24, 24, (22, 30, 62, 255), (150, 170, 230, 255), (12, 16, 36, 255))
    frame(px, 24, 0, 16, 16, (52, 64, 110, 255), (120, 140, 200, 255), (28, 34, 60, 255))  # normal
    frame(px, 40, 0, 16, 16, (250, 206, 70, 255), (255, 250, 210, 255), (170, 120, 20, 255))  # focused
    frame(px, 56, 0, 16, 16, (255, 242, 180, 255), (255, 255, 255, 255), (200, 170, 80, 255))  # pressed
    frame(px, 72, 0, 16, 16, (44, 46, 56, 255), (80, 82, 96, 255), (24, 26, 30, 255))  # disabled
    frame(px, 88, 0, 16, 6, (60, 70, 110, 255), (130, 140, 190, 255), (30, 34, 60, 255), corner=1)  # track
    frame(px, 104, 0, 6, 12, (240, 240, 250, 255), (255, 255, 255, 255), (150, 150, 170, 255), corner=1)  # knob
    frame(px, 112, 0, 16, 6, (110, 180, 255, 255), (190, 225, 255, 255), (50, 100, 180, 255), corner=1)  # fill
    # toggles: an empty box and a ticked one
    frame(px, 0, 24, 10, 10, (30, 36, 60, 255), (200, 210, 240, 255), (16, 20, 34, 255), corner=1)
    frame(px, 10, 24, 10, 10, (30, 36, 60, 255), (200, 210, 240, 255), (16, 20, 34, 255), corner=1)
    for i, (tx, ty) in enumerate(((12, 29), (13, 30), (14, 31), (15, 30), (16, 29), (17, 28), (18, 27))):
        px[ty][tx] = px[ty - 1][tx] = (120, 240, 140, 255)
    # choice arrows
    for i in range(5):
        for yy in range(5 - i, 5 + i):
            px[24 + yy][20 + 6 - i] = (240, 240, 255, 255)
            px[24 + yy][28 + 1 + i] = (240, 240, 255, 255)
    # hand cursor: a finger pointing right
    rect(px, 36, 27, 8, 4, (255, 236, 200, 255))
    rect(px, 36, 26, 5, 7, (255, 236, 200, 255))
    rect(px, 44, 28, 4, 2, (255, 236, 200, 255))
    for xx in range(36, 48):
        if px[26][xx] != CLEAR:
            px[25][xx] = (60, 40, 30, 255)
    # heart
    for yy in range(12):
        for xx in range(12):
            fx, fy = (xx - 5.5) / 6, (yy - 4.5) / 6
            if (fx * fx + fy * fy - 0.25) ** 3 - fx * fx * (fy ** 3) * 1.2 <= 0 and yy < 11:
                px[24 + yy][48 + xx] = (235, 60, 80, 255)
    frame(px, 64, 24, 16, 8, (20, 24, 40, 255), (110, 120, 160, 255), (10, 12, 20, 255), corner=1)  # bar back
    frame(px, 80, 24, 16, 8, (80, 210, 110, 255), (170, 255, 190, 255), (30, 120, 60, 255), corner=1)  # bar fill
    frame(px, 96, 24, 16, 12, (250, 206, 70, 255), (255, 250, 210, 255), (170, 120, 20, 255), corner=1)  # row
    # 16 item icons: simple original shapes in distinct colours
    colours = [(230, 70, 70), (70, 200, 110), (80, 140, 240), (240, 200, 60), (190, 90, 220), (60, 210, 210),
               (240, 140, 50), (200, 200, 210), (150, 100, 60), (250, 120, 180), (120, 230, 60), (90, 90, 220),
               (230, 230, 100), (100, 180, 160), (220, 100, 100), (160, 160, 255)]
    for k, (r, g, b) in enumerate(colours):
        ox, oy = (k % 8) * 16, 40 + (k // 8) * 16
        shape = k % 4
        for yy in range(16):
            for xx in range(16):
                dx, dy = xx - 7.5, yy - 7.5
                inside = (shape == 0 and dx * dx + dy * dy < 36) or \
                         (shape == 1 and abs(dx) + abs(dy) < 7) or \
                         (shape == 2 and 2 <= xx <= 13 and 3 <= yy <= 12) or \
                         (shape == 3 and abs(dx) < 2 + (yy - 2) * 0.45 and 2 <= yy <= 13)
                if inside:
                    edge = (r // 2, g // 2, b // 2, 255)
                    px[oy + yy][ox + xx] = (r, g, b, 255) if (xx + yy) % 7 else edge
        px[oy + 4][ox + 6] = px[oy + 5][ox + 5] = (255, 255, 255, 220)  # a highlight
    out = Path(__file__).resolve().parent.parent / "assets" / "art" / "ui.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    write_png(out, W, H, px)


if __name__ == "__main__":
    main()
