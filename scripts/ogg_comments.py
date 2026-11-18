"""Adds Vorbis comment tags (e.g. LOOPSTART=<frame>) to an Ogg Vorbis file, by rewriting its header pages.

libsndfile (python-soundfile) only writes its fixed set of tags; NNHGE reads LOOPSTART for music loop
points (spec 008). Used by the make_sounds.py scripts of the tests and examples:

    import ogg_comments                      # with scripts/ on sys.path
    ogg_comments.add_comments("music.ogg", ["LOOPSTART=64000"])
"""

import struct


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
    for _, _, _, _, segments in pages[1:]:
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
