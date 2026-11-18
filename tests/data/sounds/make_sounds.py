"""Generates the sound fixtures of the audio tests (spec 008). The outputs are committed; rerun only to
change them:  python tests/data/sounds/make_sounds.py   (needs: pip install --user soundfile numpy;
not `python -I`, which hides user-installed packages).

Every sound is synthesized here (no third-party audio). Ogg Vorbis files get extra comment tags
(LOOPSTART) through scripts/ogg_comments.py.
"""

import math
import os
import sys

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "scripts"))
import ogg_comments  # noqa: E402


def sweep(seconds, rate, start_hz, end_hz, amplitude=0.5):
    """A sine sweep: every position sounds different, so a wrong seek shows up in the samples."""
    t = np.arange(int(seconds * rate)) / rate
    phase = 2 * math.pi * (start_hz * t + (end_hz - start_hz) * t * t / (2 * seconds))
    return (amplitude * np.sin(phase)).astype(np.float32)


def main():
    mono = sweep(16.0, 22050, 200, 3000)  # long enough for several audio pages (seek tests)
    sf.write(os.path.join(HERE, "sweep_mono_22k.ogg"), mono, 22050, format="OGG", subtype="VORBIS")

    stereo = np.stack([sweep(1.0, 32000, 300, 1200), sweep(1.0, 32000, 2000, 500, 0.3)], axis=1)
    path = os.path.join(HERE, "sweep_stereo_32k_loop.ogg")
    sf.write(path, stereo, 32000, format="OGG", subtype="VORBIS")
    ogg_comments.add_comments(path, ["LOOPSTART=12345"])

    # Build-check fixtures (cmake/nnh_check_sound.cmake): one valid file of each kind, then bad ones.
    short = sweep(0.1, 16000, 400, 800)
    sf.write(os.path.join(HERE, "ok_mono16.wav"), short, 16000, subtype="PCM_16")
    sf.write(os.path.join(HERE, "bad_24bit.wav"), short, 16000, subtype="PCM_24")
    sf.write(os.path.join(HERE, "bad_3ch.wav"), np.stack([short] * 3, axis=1), 16000, subtype="PCM_16")
    sf.write(os.path.join(HERE, "bad_96k.wav"), sweep(0.05, 96000, 400, 800), 96000, subtype="PCM_16")
    with open(os.path.join(HERE, "bad_not_ogg.ogg"), "wb") as f:
        f.write(b"this is not an Ogg file\n")


if __name__ == "__main__":
    main()
