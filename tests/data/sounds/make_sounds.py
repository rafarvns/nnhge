"""Generates the sound fixtures of the audio tests (spec 008). The outputs are committed; rerun only to
change them:  python tests/data/sounds/make_sounds.py   (needs: pip install --user soundfile numpy;
not `python -I`, which hides user-installed packages).

Every sound is synthesized here (no third-party audio). Ogg Vorbis files get extra comment tags
(LOOPSTART) by rewriting the Ogg header pages, since libsndfile only writes its fixed set of tags.
"""

import math
import os
import struct

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))


def sweep(seconds, rate, start_hz, end_hz, amplitude=0.5):
    """A sine sweep: every position sounds different, so a wrong seek shows up in the samples."""
    t = np.arange(int(seconds * rate)) / rate
    phase = 2 * math.pi * (start_hz * t + (end_hz - start_hz) * t * t / (2 * seconds))
    return (amplitude * np.sin(phase)).astype(np.float32)


# --- Ogg pages ----------------------------------------------------------------------------------------

def _crc_table():
    table = []
    for i in range(256):
        r = i << 24
        for _ in range(8):
            r = ((r << 1) ^ 0x04C11DB7) if r & 0x80000000 else (r << 1)
        table.append(r & 0xFFFFFFFF)
    return table


CRC_TABLE = _crc_table()


def ogg_crc(data):
    crc = 0
    for b in data:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ CRC_TABLE[((crc >> 24) & 0xFF) ^ b]
    return crc


def read_pages(data):
    """[(header_type, granule, serial, sequence, segments: [bytes])]"""
    pages, at = [], 0
    while at < len(data):
        assert data[at:at + 4] == b"OggS", "not an Ogg page"
        header_type, granule, serial, sequence = struct.unpack_from("<BqII", data, at + 5)
        count = data[at + 26]
        lacing = data[at + 27:at + 27 + count]
        body_at = at + 27 + count
        segments = []
        for n in lacing:
            segments.append(data[body_at:body_at + n])
            body_at += n
        pages.append((header_type, granule, serial, sequence, segments))
        at = body_at
    return pages


def write_page(header_type, granule, serial, sequence, segments):
    lacing = bytes(len(s) for s in segments)
    header = struct.pack("<4sBBqIIIB", b"OggS", 0, header_type, granule, serial, sequence, 0, len(segments))
    page = bytearray(header + lacing + b"".join(segments))
    struct.pack_into("<I", page, 22, ogg_crc(page))
    return bytes(page)


def packets_to_segments(packet):
    segments = [packet[i:i + 255] for i in range(0, len(packet), 255)]
    if len(packet) % 255 == 0:
        segments.append(b"")
    return segments


def add_comments(path, tags):
    """Appends `tags` ("KEY=value") to the Vorbis comment header of an Ogg Vorbis file."""
    with open(path, "rb") as f:
        pages = read_pages(f.read())
    serial = pages[0][2]
    # Header packets: identification (page 0), then comment and setup (they may span pages).
    packets, current, header_pages = [], b"", 1
    for header_type, granule, _, _, segments in pages[1:]:
        header_pages += 1
        for segment in segments:
            current += segment
            if len(segment) < 255:
                packets.append(current)
                current = b""
        if len(packets) >= 2:
            break
    assert len(packets) == 2 and current == b"", "unexpected header layout"
    comment, setup = packets
    assert comment[:7] == b"\x03vorbis"
    vendor_len = struct.unpack_from("<I", comment, 7)[0]
    count_at = 11 + vendor_len
    count = struct.unpack_from("<I", comment, count_at)[0]
    end = len(comment) - 1  # framing bit
    extra = b"".join(struct.pack("<I", len(t)) + t.encode() for t in tags)
    comment = comment[:count_at] + struct.pack("<I", count + len(tags)) + comment[count_at + 4:end] + extra + b"\x01"

    out = [write_page(*pages[0][:2], serial, 0, pages[0][4])]
    sequence = 1
    # Comment and setup in fresh pages (at most 255 segments each); the setup packet ends the last one.
    segments = packets_to_segments(comment) + packets_to_segments(setup)
    continued = False
    while segments:
        chunk, segments = segments[:255], segments[255:]
        out.append(write_page(0x01 if continued else 0, 0, serial, sequence, chunk))
        continued = len(chunk[-1]) == 255
        sequence += 1
    for header_type, granule, _, _, segs in pages[header_pages:]:
        out.append(write_page(header_type, granule, serial, sequence, segs))
        sequence += 1
    with open(path, "wb") as f:
        f.write(b"".join(out))


# --- fixtures -----------------------------------------------------------------------------------------

def main():
    mono = sweep(16.0, 22050, 200, 3000)  # long enough for several audio pages (seek tests)
    sf.write(os.path.join(HERE, "sweep_mono_22k.ogg"), mono, 22050, format="OGG", subtype="VORBIS")

    stereo = np.stack([sweep(1.0, 32000, 300, 1200), sweep(1.0, 32000, 2000, 500, 0.3)], axis=1)
    path = os.path.join(HERE, "sweep_stereo_32k_loop.ogg")
    sf.write(path, stereo, 32000, format="OGG", subtype="VORBIS")
    add_comments(path, ["LOOPSTART=12345"])

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
