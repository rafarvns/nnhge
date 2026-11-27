"""Draws tools/nnh/template/assets/art/player.png, the sprite of the `nnh new` template (spec 033, FR-8): a 32x32 RGBA
smiling block made for the engine (zlib licence, like the rest of the repository). Deterministic: the same bytes every
run. Run from the repository root: python -I tools/nnh/make_template_player.py
"""
import struct
import zlib

SIZE = 32
BODY = (90, 170, 250, 255)
EDGE = (30, 70, 140, 255)
EYE = (255, 255, 255, 255)
PUPIL = (20, 20, 40, 255)
CLEAR = (0, 0, 0, 0)


def pixel(x, y):
    # A rounded square: the four corner pixels pairs stay clear.
    if (x in (0, SIZE - 1) and y in (0, 1, SIZE - 2, SIZE - 1)) or (y in (0, SIZE - 1) and x in (1, SIZE - 2)):
        return CLEAR
    if x in (0, SIZE - 1) or y in (0, SIZE - 1) or x in (1, SIZE - 2) and y in (1, SIZE - 2):
        return EDGE
    for ex in (10, 21):  # eyes: 4x6 white with a 2x3 pupil
        if ex - 2 <= x <= ex + 1 and 8 <= y <= 13:
            return PUPIL if ex - 1 <= x <= ex and 10 <= y <= 12 else EYE
    if 20 <= y <= 22 and 9 <= x <= 22 and (y == 22 or x in (9, 22)):  # the smile
        return EDGE
    return BODY


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def main():
    rows = b"".join(b"\x00" + b"".join(bytes(pixel(x, y)) for x in range(SIZE)) for y in range(SIZE))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    with open("tools/nnh/template/assets/art/player.png", "wb") as out:
        out.write(png)


if __name__ == "__main__":
    main()
