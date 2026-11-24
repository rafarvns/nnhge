"""Generates the transitions example's music loop (original, made for NNHGE, zlib like the rest):
run `python -I examples/transitions/art/make_music.py` (needs: pip install --user soundfile numpy).

assets/music/loop.ogg   32 kHz stereo Ogg Vorbis, 4 bars at 120 bpm in D minor (Dm, Bb, F, A), looping from 0
                        (LOOPSTART=0): a soft bass, a plucked arpeggio and a slow lead.
"""

import math
import os
import sys

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "scripts"))
import ogg_comments  # noqa: E402

RATE = 32000
BPM = 120


def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def envelope(n, attack=0.005, release=0.05):
    env = np.ones(n, dtype=np.float32)
    a = max(1, int(attack * RATE))
    r = max(1, int(release * RATE))
    env[:a] = np.linspace(0, 1, a)
    env[-r:] = np.minimum(env[-r:], np.linspace(1, 0, r))
    return env


def triangle(freq, n):
    t = np.arange(n) / RATE
    return (2 * np.abs(2 * ((t * freq) % 1) - 1) - 1).astype(np.float32)


def pluck(freq, n):
    t = np.arange(n) / RATE
    return (np.sign(np.sin(2 * math.pi * freq * t)) * np.exp(-t / 0.15)).astype(np.float32)


def main():
    beat = 60.0 / BPM
    frames = int(round(16 * beat * RATE))
    left = np.zeros(frames, dtype=np.float32)
    right = np.zeros(frames, dtype=np.float32)

    def add(start_beat, beats, wave, gain, pan=0.0):
        start = int(round(start_beat * beat * RATE))
        n = min(int(round(beats * beat * RATE)), frames - start)
        sound = wave(n) * envelope(n) * gain
        left[start:start + n] += sound * (1.0 - max(0.0, pan))
        right[start:start + n] += sound * (1.0 + min(0.0, pan))

    chords = [(50, 53, 57), (46, 50, 53), (53, 57, 60), (45, 49, 52)]  # Dm, Bb, F, A
    lead = [74, 72, 69, 70, 70, 69, 65, 62, 69, 72, 77, 76, 73, 76, 69, 73]
    for bar, chord in enumerate(chords):
        base = bar * 4
        for b in range(4):
            add(base + b, 0.95, lambda n, f=hz(chord[0] - 12): triangle(f, n), 0.35)
        for step in range(8):
            note = chord[step % 3] + 12
            add(base + step * 0.5, 0.45, lambda n, f=hz(note): pluck(f, n), 0.08, -0.4)
        for step in range(4):
            add(base + step, 0.9, lambda n, f=hz(lead[bar * 4 + step]): triangle(f, n), 0.18, 0.3)
    data = np.stack([left, right], axis=1)
    data = 0.8 * data / max(1e-9, float(np.max(np.abs(data))))
    out = os.path.join(HERE, "..", "assets", "music")
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, "loop.ogg")
    sf.write(path, data.astype(np.float32), RATE, format="OGG", subtype="VORBIS")
    ogg_comments.add_comments(path, ["LOOPSTART=0"])
    print(f"loop.ogg: {frames} frames, {os.path.getsize(path)} bytes")


if __name__ == "__main__":
    main()
