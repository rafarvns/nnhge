"""Generates the platformer example's art into ../assets/art (original, made for NNHGE, zlib like the rest):
run `python -I make_art.py`.

tiles.png  128x64: a 16x16 tileset, 8 tiles per row (tile ids in the game are index + 1):
    row 0: 0 grass, 1 dirt, 2 bricks, 3 plank (one-way), 4 coin, 5 water, 6 water (2nd frame), 7 flip-test arrow
    row 1: 8 hill, 9 hill top, 10 cloud left, 11 cloud middle, 12 cloud right, 13 bush, 14 flower, 15 stone
    row 2: 16 dark bricks, 17 lattice, 18 star, 19 grass tuft (the rest is empty)
player.png  64x16: a small robot, 4 frames of 16x16 facing right: stand, walk 1, walk 2, jump.

Pure Python (zlib + struct), no image library needed; deterministic, so the PNGs are reproducible.
"""

import struct
import sys
import zlib
from pathlib import Path

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


def noise(x, y, seed):
    """Deterministic 0..255 noise."""
    n = (x * 374761393 + y * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return (n ^ (n >> 16)) & 0xFF


def dirt(x, y, seed=1):
    n = noise(x, y, seed)
    if n < 20:
        return (150, 104, 66, 255)
    if n > 234:
        return (80, 52, 34, 255)
    return (120, 82, 52, 255)


# --- tiles: each function returns the color of pixel (x, y) of a 16x16 tile -------------------------------


def grass(x, y):
    blade = 4 + noise(x, 0, 7) % 3
    if y < 3:
        return (98, 190, 76, 255) if noise(x, y, 3) > 50 else (138, 214, 104, 255)
    if y < blade:
        return (64, 150, 58, 255)
    return dirt(x, y)


def bricks(x, y, light=(214, 120, 64), dark=(150, 74, 40), mortar=(236, 206, 170)):
    row = y // 4
    offset = 4 if row % 2 else 0
    if y % 4 == 3 or (x + offset) % 8 == 7:
        return mortar + (255,)
    shade = light if noise((x + offset) // 8, row, 5) > 90 else dark
    return shade + (255,)


def plank(x, y):
    if y > 5:
        return CLEAR
    if y == 0:
        return (226, 178, 110, 255)
    if y == 5:
        return (110, 70, 36, 255)
    if x in (3, 12) and y in (2, 3):
        return (90, 58, 30, 255)  # nails
    return (184, 132, 72, 255) if noise(x // 3, y, 9) > 60 else (166, 116, 62, 255)


def coin(x, y):
    dx, dy = x - 7.5, y - 7.5
    d = dx * dx + dy * dy
    if d > 36:
        return CLEAR
    if d > 26:
        return (176, 120, 20, 255)
    if 6 <= x <= 7 and 4 <= y <= 11:
        return (255, 244, 170, 255)  # shine
    return (250, 200, 50, 255)


def water(x, y, phase):
    wave = 2 + ((x + phase * 4) // 4) % 2
    if y < wave:
        return CLEAR
    if y == wave:
        return (190, 230, 255, 220)
    return (40, 110, 200, 200) if noise(x, y, 13 + phase) > 30 else (70, 140, 220, 200)


def arrow(x, y):
    """Asymmetric on purpose: an arrow pointing right and a yellow dot in the top-left corner, so each of the
    8 flips (Tiled's diagonal + x + y) looks different."""
    if x == 0 or y == 0 or x == 15 or y == 15:
        return (30, 30, 40, 255)
    if 2 <= x <= 4 and 2 <= y <= 4:
        return (255, 220, 40, 255)
    shaft = 6 <= y <= 9 and 3 <= x <= 9
    head = 9 <= x <= 13 and abs(y - 7.5) <= (13 - x) + 0.5
    if shaft or head:
        return (220, 40, 50, 255)
    return (240, 240, 232, 255)


def hill(x, y):
    return (70, 132, 92, 255) if noise(x, y, 21) > 24 else (58, 112, 80, 255)


def hill_top(x, y):
    edge = 4 + int(3 * ((x - 7.5) / 8.0) ** 2)
    if y < edge:
        return CLEAR
    if y == edge:
        return (112, 176, 120, 255)
    return hill(x, y)


def cloud(x, y, part):
    # part 0 = left end, 1 = middle, 2 = right end; a flat bottom at y = 12
    if y > 12:
        return CLEAR
    if part == 1:
        top = 3 + (noise(x // 4, 0, 31) % 3)
    else:
        u = x if part == 0 else 15 - x
        top = 12 - min(9, (u * u) // 20)
    if y < top:
        return CLEAR
    return (250, 252, 255, 235) if y < 11 else (214, 226, 240, 235)


def bush(x, y):
    dx, dy = (x - 7.5) / 8.0, (y - 15) / 10.0
    if dx * dx + dy * dy > 1:
        return CLEAR
    return (46, 140, 62, 255) if noise(x, y, 41) > 70 else (34, 110, 48, 255)


def flower(x, y):
    if 7 <= x <= 8 and y >= 8:
        return (50, 140, 60, 255)
    dx, dy = x - 7.5, y - 5.5
    if dx * dx + dy * dy <= 2:
        return (255, 230, 80, 255)
    if dx * dx + dy * dy <= 9:
        return (230, 90, 150, 255)
    return CLEAR


def stone(x, y):
    if x == 0 or y == 0:
        return (176, 176, 186, 255)
    if x == 15 or y == 15:
        return (84, 84, 96, 255)
    return (132, 132, 144, 255) if noise(x // 2, y // 2, 51) > 40 else (118, 118, 130, 255)


def dark_bricks(x, y):
    return bricks(x, y, (88, 70, 96), (66, 52, 74), (40, 32, 48))


def lattice(x, y):
    if x % 8 in (0, 1) or y % 8 in (0, 1):
        return (180, 220, 255, 120)
    return CLEAR


def star(x, y):
    dx, dy = abs(x - 7.5), abs(y - 7.5)
    if dx + dy <= 2.5 or (dx < 1 and dy < 6) or (dy < 1 and dx < 6):
        return (255, 250, 200, 230)
    return CLEAR


def tuft(x, y):
    if y < 9:
        return CLEAR
    blade = noise(x, 0, 61) % 7
    if y >= 9 + blade:
        return (78, 170, 70, 255) if x % 2 else (60, 140, 56, 255)
    return CLEAR


TILES = [
    grass, lambda x, y: dirt(x, y), bricks, plank, coin, lambda x, y: water(x, y, 0), lambda x, y: water(x, y, 1),
    arrow,
    hill, hill_top, lambda x, y: cloud(x, y, 0), lambda x, y: cloud(x, y, 1), lambda x, y: cloud(x, y, 2), bush,
    flower, stone,
    dark_bricks, lattice, star, tuft,
]


def tileset():
    width, height = 128, 64
    pixels = [[CLEAR] * width for _ in range(height)]
    for i, tile in enumerate(TILES):
        ox, oy = (i % 8) * 16, (i // 8) * 16
        for y in range(16):
            for x in range(16):
                pixels[oy + y][ox + x] = tile(x, y)
    return width, height, pixels


# --- the player ----------------------------------------------------------------------------------------------

PLAYER_COLORS = {
    ".": CLEAR,
    "K": (28, 30, 44, 255),     # outline
    "M": (176, 190, 206, 255),  # metal
    "L": (226, 236, 246, 255),  # highlight
    "V": (60, 200, 240, 255),   # visor
    "R": (240, 70, 70, 255),    # antenna light
    "D": (100, 110, 130, 255),  # dark metal (legs)
}

PLAYER_HEAD = [
    ".......R........",
    ".......K........",
    "....KKKKKKKK....",
    "...KLLLLLLLLK...",
    "...KMMMMVVVVK...",
    "...KMMMMVVVVK...",
    "...KMMMMMMMMK...",
    "....KKKKKKKK....",
    ".....KMMMMK.....",
    "....KMLLMMMK....",
    "....KMMMMMMK....",
    ".....KKKKKK.....",
]

PLAYER_LEGS = [  # 4 rows per frame: stand, walk 1, walk 2, jump
    [".....KD..DK.....", ".....KD..DK.....", ".....KD..DK.....", "....KKK..KKK...."],
    ["....KD....DK....", "...KD......DK...", "...KK.......KK..", "................"],
    [".....KD.DK......", "......KDK.......", "......KDK.......", ".....KKKK......."],
    ["....KD....DK....", "....KD....DK....", "...KK......KK...", "................"],
]


def player():
    width, height = 64, 16
    pixels = [[CLEAR] * width for _ in range(height)]
    for f, legs in enumerate(PLAYER_LEGS):
        rows = PLAYER_HEAD + legs
        assert len(rows) == 16
        for y, row in enumerate(rows):
            assert len(row) == 16, row
            for x, c in enumerate(row):
                pixels[y][f * 16 + x] = PLAYER_COLORS[c]
    return width, height, pixels


def main():
    out = Path(__file__).resolve().parent.parent / "assets" / "art"  # the game's assets (spec 007)
    out.mkdir(parents=True, exist_ok=True)
    for name, make in (("tiles.png", tileset), ("player.png", player)):
        w, h, px = make()
        write_png(out / name, w, h, px)
        print(f"wrote {name} {w}x{h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
