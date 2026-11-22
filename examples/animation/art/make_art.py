"""Generates the animation example's art into ../assets (original, made for NNHGE, zlib like the rest):
run `python -I make_art.py`. Pure Python (zlib + struct), deterministic, so the outputs are reproducible.

art/gallery.png  128x16: a numbered strip of 8 frames of 16x16 (the code-built gallery library).
art/hero.png     128x128 + art/hero.ase.json: a small knight, 13 frames drawn on a 32x32 canvas, each trimmed to
                 its pixels and packed row by row, described in Aseprite's own JSON (Hash layout, as File > Export
                 Sprite Sheet writes it with "Trim Cels"): tags idle, run, jump, fall, attack, hurt, the marker tags
                 @step (the run frames where a foot lands) and @hit=2 (the attack frame that hits), and a `pivot`
                 slice at the feet. So the example builds without Aseprite.
sfx/step.wav     a 60 ms footstep (16-bit mono, 22 050 Hz).
"""

import json
import math
import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "assets"
CLEAR = (0, 0, 0, 0)


def write_png(path, width, height, pixels):
    raw = b"".join(b"\x00" + b"".join(bytes(p) for p in row) for row in pixels)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    data = chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + data)


def canvas(w, h):
    return [[CLEAR] * w for _ in range(h)]


def rect(img, x, y, w, h, color):
    for yy in range(max(0, y), min(len(img), y + h)):
        for xx in range(max(0, x), min(len(img[0]), x + w)):
            img[yy][xx] = color


DIGITS = {  # 3x5
    "0": ["111", "101", "101", "101", "111"], "1": ["010", "110", "010", "010", "111"],
    "2": ["111", "001", "111", "100", "111"], "3": ["111", "001", "111", "001", "111"],
    "4": ["101", "101", "111", "001", "001"], "5": ["111", "100", "111", "001", "111"],
    "6": ["111", "100", "111", "101", "111"], "7": ["111", "001", "010", "010", "010"],
}


def gallery():
    img = canvas(128, 16)
    for i in range(8):
        hue = [(230, 80, 80), (230, 160, 60), (220, 220, 70), (90, 200, 90), (70, 200, 200), (80, 120, 230),
               (150, 90, 220), (220, 90, 180)][i]
        rect(img, i * 16 + 1, 1, 14, 14, hue + (255,))
        for row, bits in enumerate(DIGITS[str(i)]):
            for col, bit in enumerate(bits):
                if bit == "1":
                    rect(img, i * 16 + 5 + col * 2, 3 + row * 2, 2, 2, (20, 20, 30, 255))
    write_png(OUT / "art" / "gallery.png", 128, 16, img)


ARMOR, SKIN, DARK, SWORD, RED = (180, 190, 210, 255), (240, 200, 160, 255), (40, 44, 60, 255), (230, 230, 240, 255), \
    (230, 70, 70, 255)


def knight(legs=(0, 0), lift=0, sword=None, tint=None, crouch=0):
    """One 32x32 frame: feet on row 31 around x 16. legs: (left, right) forward offsets; lift: whole body up."""
    img = canvas(32, 32)
    body = tint or ARMOR
    top = 10 + crouch - lift
    rect(img, 11, top, 10, 12, body)               # torso
    rect(img, 12, top - 7, 8, 7, SKIN)             # head
    rect(img, 12, top - 8, 8, 2, DARK)             # helmet rim
    rect(img, 17, top - 5, 2, 2, DARK)             # eye
    for i, offset in enumerate(legs):              # two legs
        x = (12 if i == 0 else 17) + offset
        rect(img, x, top + 12, 3, 32 - (top + 12) - (lift > 0 and 2 or 0), DARK)
    if sword == "up":
        rect(img, 21, top - 6, 2, 12, SWORD)
    elif sword == "out":
        rect(img, 21, top + 4, 11, 2, SWORD)
    elif sword == "down":
        rect(img, 21, top + 6, 2, 12, SWORD)
    return img


def trim(img):
    ys = [y for y, row in enumerate(img) if any(p[3] for p in row)]
    xs = [x for x in range(32) if any(img[y][x][3] for y in range(32))]
    x0, y0, x1, y1 = min(xs), min(ys), max(xs) + 1, max(ys) + 1
    return x0, y0, [row[x0:x1] for row in img[y0:y1]]


def hero():
    frames = [
        ("idle", knight(), 300), ("idle", knight(crouch=1), 300),
        ("run", knight(legs=(-2, 2)), 100), ("run", knight(legs=(0, 0), lift=1), 100),
        ("run", knight(legs=(2, -2)), 100), ("run", knight(legs=(0, 0), lift=1), 100),
        ("jump", knight(legs=(-1, 2), lift=2), 100), ("fall", knight(legs=(1, -1), lift=1), 100),
        ("attack", knight(sword="up"), 80), ("attack", knight(sword="out"), 120), ("attack", knight(sword="down"), 80),
        ("hurt", knight(tint=RED), 80), ("hurt", knight(crouch=2), 80),
    ]
    sheet = canvas(128, 128)
    entries = {}
    x = y = row_h = 0
    for i, (_, img, ms) in enumerate(frames):
        ox, oy, cut = trim(img)
        w, h = len(cut[0]), len(cut)
        if x + w > 128:
            x, y, row_h = 0, y + row_h, 0
        for yy in range(h):
            sheet[y + yy][x:x + w] = cut[yy]
        entries["hero %d.aseprite" % i] = {
            "frame": {"x": x, "y": y, "w": w, "h": h}, "rotated": False, "trimmed": True,
            "spriteSourceSize": {"x": ox, "y": oy, "w": w, "h": h}, "sourceSize": {"w": 32, "h": 32}, "duration": ms}
        x, row_h = x + w + 1, max(row_h, h + 1)
    write_png(OUT / "art" / "hero.png", 128, 128, sheet)

    def tag(name, a, b, direction="forward", repeat=None):
        t = {"name": name, "from": a, "to": b, "direction": direction, "color": "#000000ff"}
        if repeat:
            t["repeat"] = repeat
        return t

    meta = {"app": "https://www.aseprite.org/", "version": "1.3.7-x64", "image": "hero.png", "format": "RGBA8888",
            "size": {"w": 128, "h": 128}, "scale": "1",
            "frameTags": [tag("idle", 0, 1, "pingpong"), tag("run", 2, 5), tag("jump", 6, 6), tag("fall", 7, 7),
                          tag("attack", 8, 10, "forward", "1"), tag("hurt", 11, 12, "forward", "2"),
                          tag("@step", 2, 2), tag("@step", 4, 4), tag("@hit=2", 9, 9)],
            "layers": [{"name": "knight", "opacity": 255, "blendMode": "normal"}],
            "slices": [{"name": "pivot", "color": "#0000ffff",
                        "keys": [{"frame": 0, "bounds": {"x": 12, "y": 30, "w": 8, "h": 2}, "pivot": {"x": 4, "y": 2}}]}]}
    text = json.dumps({"frames": entries, "meta": meta}, indent=1)
    (OUT / "art" / "hero.ase.json").write_text(text + "\n", newline="\n")


def step_wav():
    rate, n = 22050, 22050 * 60 // 1000
    seed, samples = 12345, []
    for i in range(n):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        noise = (seed >> 16) / 32768.0 - 0.5
        thump = math.sin(i * 2 * math.pi * 90 / rate)
        env = math.exp(-i / (rate * 0.015))
        samples.append(int(max(-1, min(1, (0.6 * thump + 0.5 * noise) * env)) * 20000))
    data = struct.pack("<%dh" % n, *samples)
    header = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " + struct.pack(
        "<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) + b"data" + struct.pack("<I", len(data))
    (OUT / "sfx").mkdir(parents=True, exist_ok=True)
    (OUT / "sfx" / "step.wav").write_bytes(header + data)


if __name__ == "__main__":
    gallery()
    hero()
    step_wav()
