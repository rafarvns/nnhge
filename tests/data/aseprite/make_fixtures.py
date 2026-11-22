#!/usr/bin/env python3
"""Writes the Aseprite export fixtures of the sprite converter's ctests (spec 016 T7.5): good.ase.json (Hash layout,
trimmed frames, every direction, repeat, @ marker tags, a pivot slice), good_array.ase.json (Array layout, no tags),
and one bad_<case>.ase.json per refusal, each with its PNG beside it. Standard library only. Run from anywhere:

    python3 tests/data/aseprite/make_fixtures.py

The JSON is written in Aseprite's own layout (File > Export Sprite Sheet, JSON Data); no Aseprite needed."""

import json
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def png(path, w, h):
    """A w x h RGBA PNG: a checker of two colours."""
    rows = b""
    for y in range(h):
        rows += b"\0" + b"".join(
            (b"\xff\x80\x00\xff" if ((x // 4) + (y // 4)) % 2 else b"\x20\x40\xff\xff") for x in range(w))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def frame(x, y=0, w=16, h=16, ms=100, sx=0, sy=0, **extra):
    f = {"frame": {"x": x, "y": y, "w": w, "h": h}, "rotated": False, "trimmed": (w, h) != (16, 16),
         "spriteSourceSize": {"x": sx, "y": sy, "w": w, "h": h}, "sourceSize": {"w": 16, "h": 16}, "duration": ms}
    f.update(extra)
    return f


def export(name, frames, tags=(), slices=(), array=False, image=None, size=(64, 16)):
    if array:
        body = [dict({"filename": "%s %d.aseprite" % (name, i)}, **f) for i, f in enumerate(frames)]
    else:
        body = {"%s %d.aseprite" % (name, i): f for i, f in enumerate(frames)}
    return {"frames": body,
            "meta": {"app": "https://www.aseprite.org/", "version": "1.3.7-x64", "image": image or name + ".png",
                     "format": "RGBA8888", "size": {"w": size[0], "h": size[1]}, "scale": "1",
                     "frameTags": list(tags), "layers": [{"name": "Layer 1", "opacity": 255, "blendMode": "normal"}],
                     "slices": list(slices)}}


def tag(name, a, b, direction="forward", repeat=None):
    t = {"name": name, "from": a, "to": b, "direction": direction, "color": "#000000ff"}
    if repeat is not None:
        t["repeat"] = repeat
    return t


def write(name, doc, png_size=(64, 16), text=None):
    with open(os.path.join(HERE, name + ".ase.json"), "w", newline="\n") as f:
        f.write(text if text is not None else json.dumps(doc, indent=1) + "\n")
    if png_size:
        png(os.path.join(HERE, name + ".png"), *png_size)


ROW = [frame(0), frame(16, 2, 10, 12, 50, 3, 4), frame(32, ms=70), frame(48)]
GOOD_TAGS = [tag("idle", 0, 1), tag("back", 0, 3, "reverse"), tag("run", 1, 3, "pingpong_reverse", "2"),
             tag("spin", 0, 3, "pingpong"), tag("once", 2, 3, "forward", "1"), tag("@step", 1, 2),
             tag("@hit=2", 3, 3)]
PIVOT = [{"name": "pivot", "color": "#0000ffff",
          "keys": [{"frame": 0, "bounds": {"x": 3, "y": 2, "w": 9, "h": 14}, "pivot": {"x": 5, "y": 14}}]}]

write("good", export("good", ROW, GOOD_TAGS, PIVOT))
write("good_array", export("good_array", ROW, array=True))
BAD = {
    "rotated": export("bad_rotated", [frame(0), frame(16, rotated=True)]),
    "direction": export("bad_direction", ROW, [tag("run", 0, 3, "sideways")]),
    "range": export("bad_range", ROW, [tag("run", 0, 4)]),
    "duplicate": export("bad_duplicate", ROW, [tag("run", 0, 1), tag("run", 2, 3)]),
    "duration": export("bad_duration", [frame(0, ms=0)]),
    "image": export("bad_image", ROW, image="hero.png"),
    "size": export("bad_size", ROW, size=(128, 16)),
    "marker": export("bad_marker", ROW, [tag("@hit=lots", 0, 1)]),
    "repeat": export("bad_repeat", ROW, [tag("run", 0, 3, "forward", "x")]),
}
for case, doc in BAD.items():
    write("bad_" + case, doc)
write("bad_missing_png", export("bad_missing_png", ROW), png_size=None)
write("bad_broken_json", None, png_size=None, text='{\n "frames": {\n  "a": {"frame": 3,,}\n }\n}\n')
