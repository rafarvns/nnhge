# Generates the hot reload example's art, sounds, map and models (spec 031), and the fixtures its scripts swap in.
# Everything is drawn here (no third-party assets). Run from the repository root:
#   python -I examples/reload/art/make_assets.py
import json, math, os, struct, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
ASSETS = os.path.join(ROOT, "assets")
FIXTURES = os.path.join(ROOT, "tests", "fixtures")


def out(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return path


def png(path, w, h, pixel):
    rows = b"".join(b"\0" + b"".join(bytes(pixel(x, y)) for x in range(w)) for y in range(h))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    open(out(path), "wb").write(data)


def wav(path, frames, rate, sample):
    pcm = b"".join(struct.pack("<h", sample(i)) for i in range(frames))
    head = b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16)
    open(out(path), "wb").write(head + b"data" + struct.pack("<I", len(pcm)) + pcm)


def backdrop(x, y):
    return (20 + y, 30 + x // 2, 70 + (x ^ y) % 40, 255)


def hero(size, colour):
    def pixel(x, y):
        c = size / 2
        inside = (x - c) ** 2 + (y - c) ** 2 < (size * 0.42) ** 2
        eye = abs(x - c * 1.3) < size / 10 and abs(y - c * 0.8) < size / 10
        return (0, 0, 0, 0) if not inside else ((20, 20, 30, 255) if eye else colour)
    return pixel


def tiles(x, y):
    tile = x // 16
    colours = [(0, 0, 0, 0), (120, 90, 60, 255), (90, 160, 80, 255), (200, 200, 90, 255)]
    edge = x % 16 in (0, 15) or y in (0, 15)
    r, g, b, a = colours[tile]
    return (r // 2, g // 2, b // 2, a) if edge and a else (r, g, b, a)


def room(platform_x, broken=False):
    w, h = 20, 15
    data = []
    for y in range(h):
        for x in range(w):
            data.append(2 if y == h - 1 else (3 if y == 10 and 4 <= x < 9 else (1 if x in (0, w - 1) else 0)))
    m = {
        "type": "map", "version": "1.10", "tiledversion": "1.10.2", "orientation": "orthogonal",
        "renderorder": "right-down", "infinite": False, "width": w, "height": h, "tilewidth": 16, "tileheight": 16,
        "nextlayerid": 3, "nextobjectid": 2,
        "tilesets": [{"firstgid": 1, "source": "tiles.tsj"}],
        "layers": [
            {"id": 1, "type": "tilelayer", "name": "ground", "width": w, "height": h, "x": 0, "y": 0,
             "opacity": 1, "visible": True, "data": data},
            {"id": 2, "type": "objectgroup", "name": "things", "draworder": "topdown", "opacity": 1, "visible": True,
             "objects": [{"id": 1, "name": "platform", "type": "platform", "x": platform_x, "y": 120, "width": 48,
                          "height": 8, "rotation": 0, "visible": True}]},
        ],
    }
    text = json.dumps(m, indent=1)
    return text[: len(text) // 2] if broken else text


TILESET = {"type": "tileset", "version": "1.10", "name": "tiles", "image": "tiles.png", "imagewidth": 64,
           "imageheight": 16, "tilewidth": 16, "tileheight": 16, "tilecount": 4, "columns": 4, "margin": 0,
           "spacing": 0}


def octahedron():
    p = [(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)]
    faces = [(0, 2, 4), (4, 2, 1), (1, 2, 5), (5, 2, 0), (4, 3, 0), (1, 3, 4), (5, 3, 1), (0, 3, 5)]
    return [p[i] for f in faces for i in f]


def tetrahedron():
    p = [(1, 1, 1), (-1, -1, 1), (-1, 1, -1), (1, -1, -1)]
    faces = [(0, 1, 2), (0, 3, 1), (0, 2, 3), (1, 3, 2)]
    return [p[i] for f in faces for i in f]


def gltf(name, positions, colour):
    raw = b"".join(struct.pack("<3f", *v) for v in positions) + bytes(colour) * len(positions)
    n = len(positions)
    doc = {
        "asset": {"version": "2.0", "generator": "NNHGE make_assets.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "COLOR_0": 1}}]}],
        "buffers": [{"byteLength": len(raw)}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": n * 12},
                        {"buffer": 0, "byteOffset": n * 12, "byteLength": n * 4}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": n, "type": "VEC3",
             "min": [min(v[i] for v in positions) for i in range(3)],
             "max": [max(v[i] for v in positions) for i in range(3)]},
            {"bufferView": 1, "componentType": 5121, "count": n, "type": "VEC4", "normalized": True},
        ],
    }
    js = json.dumps(doc).encode()
    js += b" " * (-len(js) % 4)
    raw += b"\0" * (-len(raw) % 4)
    body = struct.pack("<II", len(js), 0x4E4F534A) + js + struct.pack("<II", len(raw), 0x004E4942) + raw
    open(out(name), "wb").write(struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body)  # a .glb: one file


png(os.path.join(ASSETS, "art", "backdrop.png"), 128, 128, backdrop)
png(os.path.join(ASSETS, "art", "hero.png"), 32, 32, hero(32, (230, 120, 60, 255)))
png(os.path.join(ASSETS, "levels", "tiles.png"), 64, 16, tiles)
open(out(os.path.join(ASSETS, "levels", "tiles.tsj")), "w", newline="\n").write(json.dumps(TILESET, indent=1) + "\n")
open(out(os.path.join(ASSETS, "levels", "room.tmj")), "w", newline="\n").write(room(64) + "\n")
wav(os.path.join(ASSETS, "sfx", "blip.wav"), 4000, 16000, lambda i: int(6000 * math.sin(i * 0.25)))
gltf(os.path.join(ASSETS, "models", "gem.glb"), octahedron(), (90, 200, 255, 255))

png(os.path.join(FIXTURES, "hero_big.png"), 64, 64, hero(64, (80, 200, 120, 255)))
open(out(os.path.join(FIXTURES, "room_moved.tmj")), "w", newline="\n").write(room(160) + "\n")
open(out(os.path.join(FIXTURES, "room_broken.tmj")), "w", newline="\n").write(room(64, broken=True) + "\n")
wav(os.path.join(FIXTURES, "blip2.wav"), 12000, 16000, lambda i: int(5000 * math.sin(i * 0.11)))
gltf(os.path.join(FIXTURES, "gem2.glb"), tetrahedron(), (255, 160, 60, 255))
