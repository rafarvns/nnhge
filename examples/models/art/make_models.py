#!/usr/bin/env python3
"""The models example's art (spec 026 T8.1, plan P26): original low-poly models and textures, written as glTF 2.0
(the format Blender exports) and PNG. Run: python -I examples/models/art/make_models.py — rewrites
examples/models/assets/models/ (ship.gltf + ship.bin, pylon.gltf + pylon.bin) and examples/models/assets/art/
(ship.png, fence.png, checker.png). Committed outputs; no third-party data."""

import json
import os
import struct
import zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets")


def write(rel, data):
    path = os.path.join(ROOT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w" if isinstance(data, str) else "wb", **({"newline": "\n"} if isinstance(data, str) else {})) as f:
        f.write(data)


def png(w, h, pixel):
    raw = b"".join(b"\x00" + b"".join(bytes(pixel(x, y)) for x in range(w)) for y in range(h))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def ship_texture(x, y):
    stripe = (y // 8) % 2 == 0
    base = (200, 210, 225) if stripe else (150, 165, 190)
    if 24 <= x < 40 and 4 <= y < 20:  # cockpit
        return (40, 120, 220, 255)
    if (x + y) % 16 == 0:
        return (90, 100, 120, 255)
    return base + (255,)


def fence_texture(x, y):
    post = x % 16 < 3
    rail = 4 <= y % 16 < 7
    return (150, 100, 50, 255) if post or rail else (0, 0, 0, 0)


def checker(x, y):
    return (230, 230, 230, 255) if ((x // 8) + (y // 8)) % 2 == 0 else (70, 90, 70, 255)


class Bin:
    def __init__(self):
        self.data, self.views, self.accessors = b"", [], []

    def add(self, fmt, values, comps, ctype, kind, normalized=False):
        while len(self.data) % 4:
            self.data += b"\x00"
        offset = len(self.data)
        self.data += struct.pack("<" + fmt * len(values), *values)
        self.views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(self.data) - offset})
        acc = {"bufferView": len(self.views) - 1, "componentType": ctype, "count": len(values) // comps, "type": kind}
        if normalized:
            acc["normalized"] = True
        if kind == "VEC3" and ctype == 5126:
            acc["min"] = [min(values[i::3]) for i in range(3)]
            acc["max"] = [max(values[i::3]) for i in range(3)]
        self.accessors.append(acc)
        return len(self.accessors) - 1


def triangles(tris):
    """Unwelded triangles (flat shading comes from the converter: no NORMAL)."""
    pos = []
    for t in tris:
        for p in t:
            pos += list(p)
    return pos


def ship():
    nose, tail = (0, 0, -1.2), (0, 0.05, 0.8)
    top, bottom, left, right = (0, 0.32, 0.1), (0, -0.2, 0.1), (-0.38, 0, 0.1), (0.38, 0, 0.1)
    hull = [(nose, left, top), (nose, top, right), (nose, bottom, left), (nose, right, bottom),
            (tail, top, left), (tail, right, top), (tail, left, bottom), (tail, bottom, right)]
    wl, wr = (-1.25, -0.02, 0.55), (1.25, -0.02, 0.55)
    wings = [((-0.3, 0, -0.2), wl, (-0.3, 0, 0.6)), ((0.3, 0, -0.2), (0.3, 0, 0.6), wr),
             ((-0.3, 0, -0.2), (-0.3, 0, 0.6), wl), ((0.3, 0, -0.2), wr, (0.3, 0, 0.6))]
    b = Bin()
    hp = triangles(hull)
    huv = []
    for i in range(0, len(hp), 3):
        huv += [hp[i] * 0.6 + 0.5, hp[i + 2] * 0.45 + 0.55]
    p0 = b.add("f", hp, 3, 5126, "VEC3")
    t0 = b.add("f", huv, 2, 5126, "VEC2")
    p1 = b.add("f", triangles(wings), 3, 5126, "VEC3")
    doc = {"asset": {"version": "2.0", "generator": "NNHGE make_models.py"}, "scene": 0,
           "scenes": [{"nodes": [0]}], "nodes": [{"name": "ship", "mesh": 0}],
           "meshes": [{"primitives": [{"attributes": {"POSITION": p0, "TEXCOORD_0": t0}, "material": 0},
                                      {"attributes": {"POSITION": p1}, "material": 1}]}],
           "materials": [{"name": "hull", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}},
                         {"name": "wings", "pbrMetallicRoughness": {"baseColorFactor": [0.85, 0.2, 0.15, 1]}}],
           "textures": [{"source": 0, "sampler": 0}], "images": [{"uri": "../art/ship.png"}],
           "samplers": [{"magFilter": 9728, "minFilter": 9728}],
           "buffers": [{"uri": "ship.bin", "byteLength": len(b.data)}], "bufferViews": b.views,
           "accessors": b.accessors}
    write("models/ship.gltf", json.dumps(doc, indent=1) + "\n")
    write("models/ship.bin", b.data)


def pylon():
    apex, base = (0, 1.5, 0), [(-0.4, 0, -0.4), (0.4, 0, -0.4), (0.4, 0, 0.4), (-0.4, 0, 0.4)]
    tris = [(apex, base[(i + 1) % 4], base[i]) for i in range(4)]
    b = Bin()
    p = b.add("f", triangles(tris), 3, 5126, "VEC3")
    cols = []
    for t in tris:
        cols += [255, 230, 80, 255] + [200, 60, 40, 255] * 2
    c = b.add("B", cols, 4, 5121, "VEC4", normalized=True)
    doc = {"asset": {"version": "2.0", "generator": "NNHGE make_models.py"}, "scene": 0,
           "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}],
           "meshes": [{"primitives": [{"attributes": {"POSITION": p, "COLOR_0": c}}]}],
           "buffers": [{"uri": "pylon.bin", "byteLength": len(b.data)}], "bufferViews": b.views,
           "accessors": b.accessors}
    write("models/pylon.gltf", json.dumps(doc, indent=1) + "\n")
    write("models/pylon.bin", b.data)


def main():
    write("art/ship.png", png(64, 64, ship_texture))
    write("art/fence.png", png(64, 32, lambda x, y: fence_texture(x, y)))
    write("art/checker.png", png(64, 64, checker))
    ship()
    pylon()


if __name__ == "__main__":
    main()
