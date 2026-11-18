"""Draws the package art of examples/audio (spec 009): icon.png (48x48), banner.png (256x128) and
banner.wav (the HOME Menu jingle, 2 s). Original art, reusing the pixel font of scripts/make_package_art.py.
Outputs are committed; rerun only to change them:  python examples/audio/make_art.py
(needs: pip install --user pillow soundfile numpy; not `python -I`).
"""

import math
import os
import sys

import numpy as np
import soundfile as sf
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))
import make_package_art as art  # noqa: E402

TOP, BOTTOM = (120, 60, 160), (30, 14, 50)


def note(draw, x, y, size, color):
    """An eighth note: head, stem and flag, `size` pixels tall."""
    head = size // 3
    draw.ellipse([x, y + size - head, x + head + 2, y + size], fill=color)
    draw.rectangle([x + head, y, x + head + max(1, size // 12), y + size - head // 2], fill=color)
    draw.polygon([(x + head, y), (x + head + size // 3, y + size // 4), (x + head, y + size // 3)], fill=color)


def icon():
    image = art.gradient(48, 48, TOP, BOTTOM)
    mask = Image.new("L", (48, 48), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, 47, 47], radius=8, fill=255)
    out = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    out.paste(image, (0, 0), mask)
    draw = ImageDraw.Draw(out)
    draw.rounded_rectangle([0, 0, 47, 47], radius=8, outline=(200, 150, 240, 255))
    note(draw, 9, 6, 24, art.YELLOW)
    note(draw, 25, 10, 20, art.WHITE)
    art.draw_text(draw, (48 - art.text_width("AUDIO", 1)) // 2, 37, "AUDIO", 1, art.WHITE, shadow=False)
    out.save(os.path.join(HERE, "icon.png"))


def banner():
    image = art.gradient(256, 128, TOP, BOTTOM)
    draw = ImageDraw.Draw(image)
    for i, x in enumerate(range(10, 256, 34)):
        note(draw, x, 8 + (i % 3) * 6, 18, (255, 255, 255, 60) if i % 2 else (255, 210, 80, 90))
    art.draw_text(draw, (256 - art.text_width("AUDIO", 6)) // 2, 44, "AUDIO", 6, art.YELLOW)
    art.draw_text(draw, (256 - art.text_width("MADE WITH NNHGE", 1)) // 2, 104, "MADE WITH NNHGE", 1, art.WHITE,
                  shadow=False)
    image.save(os.path.join(HERE, "banner.png"))


def jingle():
    rate = 22050
    total = int(2.0 * rate)
    data = np.zeros(total, dtype=np.float32)
    for i, midi in enumerate([67, 71, 74, 79, 83]):  # G major, rising
        freq = 440.0 * 2 ** ((midi - 69) / 12)
        start = int(i * 0.1 * rate)
        t = np.arange(total - start) / rate
        tone = np.sin(2 * math.pi * freq * t) + 0.3 * np.sin(4 * math.pi * freq * t)
        data[start:] += (tone * np.exp(-t / (0.15 if i < 4 else 0.5))).astype(np.float32)
    data *= 0.8 / np.max(np.abs(data))
    sf.write(os.path.join(HERE, "banner.wav"), data, rate, subtype="PCM_16")


if __name__ == "__main__":
    icon()
    banner()
    jingle()
