"""Generates the data example's art and sounds into ../assets (original, made for NNHGE, zlib like the rest):
run `python -I make_art.py`.

art/goblin.png, imp.png, slime.png, yeti.png   32x32 enemy sprites, drawn in greys: the game tints them with the
                                               colour each enemy's data file gives (`tint = "#RRGGBB"`)
art/potion.png, ether.png, crystal.png         16x16 item icons
sfx/squish.wav, sfx/roar.wav                   hurt sounds (16-bit mono, 22050 Hz)
music/theme.wav                                a 2-second loop (16-bit mono, 22050 Hz)

Pure Python (zlib, struct, wave, math), no image or sound library needed; deterministic, so the files are
reproducible.
"""

import math
import struct
import wave
import zlib
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "assets"
CLEAR = (0, 0, 0, 0)


def write_png(path, size, pixel):
    """pixel(x, y) -> (r, g, b, a) for a size x size image."""
    raw = b"".join(b"\x00" + b"".join(bytes(pixel(x, y)) for x in range(size)) for y in range(size))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def grey(level):
    return (level, level, level, 255)


def inside_ellipse(x, y, cx, cy, rx, ry):
    return ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1.0


def eyes(x, y, left, right, row):
    """Two dark 2x2 eyes."""
    return row <= y <= row + 1 and (left <= x <= left + 1 or right <= x <= right + 1)


# --- enemies: 32x32, light greys so the tint shows -----------------------------------------------------------


def goblin(x, y):
    if eyes(x, y, 11, 19, 11):
        return grey(30)
    if (x in (6, 7) or x in (24, 25)) and 6 <= y <= 12:  # ears
        return grey(200)
    if inside_ellipse(x, y, 16, 13, 9, 8):  # head
        return grey(230)
    if 10 <= x <= 21 and 20 <= y <= 30:  # body
        return grey(170 if (x + y) % 6 else 150)
    return CLEAR


def imp(x, y):
    if eyes(x, y, 12, 18, 13):
        return grey(255) if (x + y) % 2 else grey(40)
    if (y < 8 and (abs(x - 9) <= (8 - y) // 3 or abs(x - 22) <= (8 - y) // 3)):  # horns
        return grey(120)
    if inside_ellipse(x, y, 16, 15, 8, 8):
        return grey(220)
    if y > 23 and abs(x - 16) <= 2 + (y - 23) // 3 and y < 31:  # tail-ish body
        return grey(180)
    return CLEAR


def slime(x, y):
    if eyes(x, y, 11, 19, 17):
        return grey(25)
    if y >= 10 and inside_ellipse(x, y, 16, 24, 14, 13):
        return grey(250 if inside_ellipse(x, y, 11, 16, 3, 2) else 200)  # a shine
    return CLEAR


def yeti(x, y):
    if eyes(x, y, 12, 18, 9):
        return grey(20)
    if inside_ellipse(x, y, 16, 10, 7, 7):
        return grey(245)
    if inside_ellipse(x, y, 16, 23, 11, 9):
        return grey(235 if (x * 7 + y * 3) % 5 else 205)  # fur
    return CLEAR


# --- item icons: 16x16 ---------------------------------------------------------------------------------------


def potion(x, y):
    if 6 <= x <= 9 and 1 <= y <= 4:
        return (150, 110, 80, 255)  # cork
    if inside_ellipse(x, y, 8, 10, 6, 5.5):
        return (230, 60, 70, 255) if y > 8 else (250, 200, 210, 255)
    return CLEAR


def ether(x, y):
    if 6 <= x <= 9 and 1 <= y <= 4:
        return (150, 110, 80, 255)
    if inside_ellipse(x, y, 8, 10, 6, 5.5):
        return (70, 120, 240, 255) if y > 8 else (200, 220, 255, 255)
    return CLEAR


def crystal(x, y):
    if abs(x - 7.5) + abs(y - 7.5) * 0.6 <= 6:
        return (140, 240, 230, 255) if x < 8 else (60, 180, 200, 255)
    return CLEAR


# --- sounds --------------------------------------------------------------------------------------------------

RATE = 22050


def write_wav(path, samples):
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", max(-32767, min(32767, int(s * 32767)))) for s in samples))


def squish():
    n = int(RATE * 0.25)
    return [0.6 * math.sin(2 * math.pi * (300 - 900 * i / n) * i / RATE) * (1 - i / n) for i in range(n)]


def roar():
    n = int(RATE * 0.4)
    out = []
    seed = 12345
    for i in range(n):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        noise = seed / 0x7FFFFFFF * 2 - 1
        tone = math.sin(2 * math.pi * 90 * i / RATE)
        out.append((0.35 * noise + 0.45 * tone) * min(1, i / 400) * (1 - i / n))
    return out


def theme():
    """Two seconds, four notes of a soft arpeggio: loops cleanly (every note ends at zero)."""
    notes = [220.0, 277.18, 329.63, 277.18]
    out = []
    per = RATE // 2
    for f in notes:
        for i in range(per):
            env = math.sin(math.pi * i / per)
            out.append(0.25 * env * (math.sin(2 * math.pi * f * i / RATE) + 0.3 * math.sin(4 * math.pi * f * i / RATE)))
    return out


def main():
    for name, fn in (("goblin", goblin), ("imp", imp), ("slime", slime), ("yeti", yeti)):
        write_png(ASSETS / "art" / f"{name}.png", 32, fn)
    for name, fn in (("potion", potion), ("ether", ether), ("crystal", crystal)):
        write_png(ASSETS / "art" / f"{name}.png", 16, fn)
    write_wav(ASSETS / "sfx" / "squish.wav", squish())
    write_wav(ASSETS / "sfx" / "roar.wav", roar())
    write_wav(ASSETS / "music" / "theme.wav", theme())


if __name__ == "__main__":
    main()
