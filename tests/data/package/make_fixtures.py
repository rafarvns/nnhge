"""Generates the fixtures of the packaging checks (spec 009). The outputs are committed; rerun only to
change them:  python tests/data/package/make_fixtures.py   (needs: pip install --user pillow soundfile
numpy; not `python -I`, which hides user-installed packages).
"""

import os

import numpy as np
import soundfile as sf
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))


def png(name, size):
    Image.new("RGBA", size, (40, 120, 220, 255)).save(os.path.join(HERE, name))


def tone(name, seconds, subtype="PCM_16", rate=8000, channels=1):
    t = np.arange(int(seconds * rate)) / rate
    data = 0.3 * np.sin(2 * np.pi * 440 * t)
    if channels == 2:
        data = np.stack([data, data], axis=1)
    sf.write(os.path.join(HERE, name), data.astype(np.float32), rate, subtype=subtype)


def main():
    png("icon_48.png", (48, 48))
    png("icon_64.png", (64, 64))
    png("banner_256x128.png", (256, 128))
    png("banner_200x120.png", (200, 120))
    with open(os.path.join(HERE, "not_png.png"), "wb") as f:
        f.write(b"not an image\n")
    tone("jingle_ok.wav", 1.5, channels=2)
    tone("jingle_3s.wav", 3.0)
    tone("jingle_long.wav", 3.2)
    tone("jingle_24bit.wav", 1.0, subtype="PCM_24")


if __name__ == "__main__":
    main()
