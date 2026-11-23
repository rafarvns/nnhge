"""Generates the tileset images of the map converter fixtures (spec 012 T5.7, T5.9) and the prefab objects map
good_prefabs.tmj (spec 021 T5.1). The outputs are committed; rerun only to change them:
python -I tests/data/maps/make_fixtures.py   (standard library only).

The other Tiled files (*.tmj, *.tsj) next to them are written by hand. The converter reads only a PNG's size; the
images are real PNGs anyway, so the fixtures open in Tiled.
"""

import json
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def png(name, width, height, gray=False):
    """A plain image: RGBA stripes, or all-black grayscale for large ones (compresses to almost nothing)."""
    if gray:
        rows = b"".join(b"\0" + bytes(width) for _ in range(height))
        header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    else:
        rows = b""
        for y in range(height):
            row = bytearray()
            for x in range(width):
                shade = 60 + 40 * ((x // 8 + y // 8) % 4)
                row += bytes((shade, 120, 200 - shade // 2, 255))
            rows += b"\0" + bytes(row)
        header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    path = os.path.join(HERE, "tiles", name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def obj(oid, name, kind, x, y, w=0, h=0, point=False, props=None):
    o = {"id": oid, "name": name, "type": kind, "x": x, "y": y, "width": w, "height": h, "rotation": 0,
         "visible": True}
    if point:
        o["point"] = True
    if props:
        o["properties"] = props
    return o


def prop(name, kind, value):
    return {"name": name, "type": kind, "value": value}


def prefab_map():
    """Objects naming prefabs (spec 021 AC-3): by type, by a `prefab` property, a skipped door, spawn point and empty
    type, a misspelt `prefab`, a `prefab` that is not a string; a second object layer."""
    things = [
        obj(1, "coin_a", "coin", 16, 32, 12, 10, props=[prop("value", "int", 5)]),
        obj(2, "coin_b", "coin", 40, 8, point=True),
        obj(3, "w1", "walker", 64, 48, point=True),
        obj(4, "w2", "walker", 80, 16, 16, 16, props=[prop("prefab", "string", "flying_walker")]),
        obj(5, "door", "door", 96, 0, 16, 32, props=[prop("to", "string", "next")]),
        obj(6, "spawn", "spawn", 8, 8, point=True),
        obj(7, "marker", "", 24, 24, point=True),
        obj(12, "coin", "coin", 48, 40, 12, 12, props=[prop("prefab", "string", "coinn")]),
        obj(13, "w3", "walker", 32, 48, point=True, props=[prop("prefab", "int", 3)]),
    ]
    more = [obj(14, "coin_c", "coin", 104, 40, 8, 8, props=[prop("value", "int", 2)])]
    data = {
        "type": "map", "version": "1.10", "tiledversion": "1.10.2", "orientation": "orthogonal",
        "renderorder": "right-down", "infinite": False, "width": 8, "height": 4, "tilewidth": 16, "tileheight": 16,
        "nextlayerid": 3, "nextobjectid": 15, "tilesets": [],
        "layers": [
            {"id": 1, "type": "objectgroup", "name": "things", "draworder": "topdown", "opacity": 1,
             "visible": True, "objects": things},
            {"id": 2, "type": "objectgroup", "name": "more", "draworder": "topdown", "opacity": 1,
             "visible": True, "objects": more},
        ],
    }
    with open(os.path.join(HERE, "good_prefabs.tmj"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=1)
        f.write("\n")


def main():
    png("good.png", 64, 32)       # 8 tiles of 16x16 (tiles/good.tsj)
    png("second.png", 64, 64)     # 9 tiles of 16x16 with margin 1 and spacing 4 (embedded in good.tmj)
    png("npot.png", 100, 60)      # not a power of two
    png("wide.png", 64, 16)       # 2 tiles of 32x16 (non-square)
    png("big.png", 1024, 1024, gray=True)  # 16384 tiles of 8x8: over 8191 ids
    prefab_map()


if __name__ == "__main__":
    main()
