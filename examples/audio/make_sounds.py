"""Synthesizes the sounds of examples/audio into assets/ (spec 008). The outputs are committed; rerun only
to change them:  python examples/audio/make_sounds.py   (needs: pip install --user soundfile numpy; not
`python -I`, which hides user-installed packages). Everything here is original: plain waveforms, no samples.

  assets/sfx/jump.wav, coin.wav, hit.wav   effects (22.05 kHz mono 16-bit WAV)
  assets/sfx/chime.wav                     a "dialogue" blip (Dialogue group)
  assets/music/calm.ogg, action.ogg        32 kHz stereo Ogg Vorbis: a one-bar intro, then a 4-bar loop
                                           (LOOPSTART tag = the intro's length, so the intro plays once)
"""

import math
import os
import sys

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))
import ogg_comments  # noqa: E402

SFX_RATE = 22050
MUSIC_RATE = 32000


def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def envelope(n, rate, attack=0.005, release=0.03):
    env = np.ones(n, dtype=np.float32)
    a = min(n, int(attack * rate))
    r = min(n - a, int(release * rate))
    if a > 0:
        env[:a] = np.linspace(0, 1, a, endpoint=False)
    if r > 0:
        env[n - r:] = np.linspace(1, 0, r)
    return env


def square(freq, n, rate, duty=0.5):
    phase = np.cumsum(np.full(n, freq / rate)) % 1.0
    return np.where(phase < duty, 1.0, -1.0).astype(np.float32)


def triangle(freq, n, rate):
    phase = np.cumsum(np.full(n, freq / rate)) % 1.0
    return (4 * np.abs(phase - 0.5) - 1).astype(np.float32)


def write_wav(name, data):
    data = 0.8 * data / max(1e-9, np.max(np.abs(data)))
    sf.write(os.path.join(HERE, "assets", "sfx", name), data.astype(np.float32), SFX_RATE, subtype="PCM_16")


# --- effects -------------------------------------------------------------------------------------------

def effects():
    os.makedirs(os.path.join(HERE, "assets", "sfx"), exist_ok=True)
    rate = SFX_RATE
    n = int(0.18 * rate)
    freq = 220 * (4 ** (np.arange(n) / n))  # 220 -> 880 Hz
    phase = np.cumsum(freq / rate) % 1.0
    jump = np.where(phase < 0.5, 1.0, -1.0) * np.linspace(1, 0, n) ** 1.5
    write_wav("jump.wav", jump)

    a, b = int(0.07 * rate), int(0.28 * rate)
    coin = np.concatenate([square(hz(83), a, rate) * envelope(a, rate, release=0.005),
                           square(hz(88), b, rate) * np.exp(-np.arange(b) / (0.08 * rate))])
    write_wav("coin.wav", coin)

    n = int(0.25 * rate)
    rng = np.random.default_rng(1)
    noise = np.convolve(rng.uniform(-1, 1, n), np.ones(4) / 4, mode="same")
    thump = np.sin(2 * math.pi * 70 * np.arange(n) / rate)
    hit = (0.7 * noise + 0.6 * thump) * np.exp(-np.arange(n) / (0.05 * rate))
    write_wav("hit.wav", hit)

    n = int(0.6 * rate)
    t = np.arange(n) / rate
    chime = (np.sin(2 * math.pi * 660 * t) + 0.5 * np.sin(2 * math.pi * 1320 * t)
             + 0.25 * np.sin(2 * math.pi * 1980 * t)) * np.exp(-t / 0.15)
    write_wav("chime.wav", chime)


# --- music ---------------------------------------------------------------------------------------------

class Song:
    """A stereo buffer notes are added into, in beats."""

    def __init__(self, bpm, beats):
        self.beat = 60.0 / bpm
        self.frames = int(round(beats * self.beat * MUSIC_RATE))
        self.left = np.zeros(self.frames, dtype=np.float32)
        self.right = np.zeros(self.frames, dtype=np.float32)

    def add(self, start_beat, length_beats, wave, gain=0.2, pan=0.0):
        start = int(round(start_beat * self.beat * MUSIC_RATE))
        n = min(int(round(length_beats * self.beat * MUSIC_RATE)), self.frames - start)
        if n <= 0:
            return
        sound = wave(n) * envelope(n, MUSIC_RATE) * gain
        self.left[start:start + n] += sound * (1.0 - max(0.0, pan))
        self.right[start:start + n] += sound * (1.0 + min(0.0, pan))

    def stereo(self):
        data = np.stack([self.left, self.right], axis=1)
        return 0.85 * data / max(1e-9, np.max(np.abs(data)))


def kick(n):
    t = np.arange(n) / MUSIC_RATE
    freq = 40 + 90 * np.exp(-t / 0.03)
    return (np.sin(2 * math.pi * np.cumsum(freq) / MUSIC_RATE) * np.exp(-t / 0.12)).astype(np.float32)


def noise_hit(n, decay, seed):
    rng = np.random.default_rng(seed)
    t = np.arange(n) / MUSIC_RATE
    return (rng.uniform(-1, 1, n) * np.exp(-t / decay)).astype(np.float32)


def calm():
    """C major, 112 bpm: a rising arpeggio as intro, then a gentle 4-bar loop."""
    song = Song(112, 4 + 16)
    chords = [(60, 64, 67), (57, 60, 64), (53, 57, 60), (55, 59, 62)]  # C, Am, F, G
    for i, note in enumerate([60, 64, 67, 72, 67, 64, 62, 59]):  # intro: one bar of eighths
        song.add(i * 0.5, 0.45, lambda n, f=hz(note): square(f, n, MUSIC_RATE, 0.25), 0.12, 0.3)
    melody = [72, 76, 74, 72, 69, 72, 71, 69, 65, 69, 67, 65, 67, 71, 74, 71]
    for bar, chord in enumerate(chords):
        base = 4 + bar * 4
        for beat in range(4):
            song.add(base + beat, 0.9, lambda n, f=hz(chord[0] - 12): triangle(f, n, MUSIC_RATE), 0.35)
        for step in range(8):
            note = chord[step % 3] + (12 if step >= 4 else 0)
            song.add(base + step * 0.5, 0.4, lambda n, f=hz(note): square(f, n, MUSIC_RATE, 0.25), 0.06, -0.4)
        for step in range(4):
            note = melody[bar * 4 + step]
            song.add(base + step, 0.85, lambda n, f=hz(note): triangle(f, n, MUSIC_RATE), 0.22, 0.2)
    return song, 4


def action():
    """A minor, 150 bpm: one bar of drums as intro, then a driving 4-bar loop."""
    song = Song(150, 4 + 16)
    for beat in range(16 + 4):
        song.add(beat, 0.5, kick if beat % 2 == 0 else (lambda n: noise_hit(n, 0.06, beat)), 0.5 if beat % 2 == 0 else 0.25)
        song.add(beat + 0.5, 0.25, lambda n: noise_hit(n, 0.015, 7), 0.12, 0.3)
    roots = [57, 55, 53, 52]  # Am, G, F, E
    lead = [69, 72, 71, 69, 67, 71, 74, 71, 65, 69, 72, 69, 68, 71, 76, 71]
    for bar, root in enumerate(roots):
        base = 4 + bar * 4
        for step in range(8):
            song.add(base + step * 0.5, 0.45, lambda n, f=hz(root - 12): square(f, n, MUSIC_RATE, 0.5), 0.12)
        for step in range(4):
            song.add(base + step, 0.9, lambda n, f=hz(lead[bar * 4 + step]): square(f, n, MUSIC_RATE, 0.25), 0.1, -0.3)
    return song, 4


def write_music(name, song_and_intro):
    song, intro_beats = song_and_intro
    os.makedirs(os.path.join(HERE, "assets", "music"), exist_ok=True)
    path = os.path.join(HERE, "assets", "music", name)
    sf.write(path, song.stereo().astype(np.float32), MUSIC_RATE, format="OGG", subtype="VORBIS")
    intro = int(round(intro_beats * song.beat * MUSIC_RATE))
    ogg_comments.add_comments(path, [f"LOOPSTART={intro}"])
    print(f"{name}: {song.frames} frames, LOOPSTART={intro}, {os.path.getsize(path)} bytes")


if __name__ == "__main__":
    effects()
    write_music("calm.ogg", calm())
    write_music("action.ogg", action())
