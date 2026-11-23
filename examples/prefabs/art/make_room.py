"""Writes the prefabs example's room, ../assets/rooms/room.tmj, a Tiled map on ../assets/tiles/room.png (made by
make_art.py): run `python -I make_room.py`.

25 x 15 tiles of 16 px (400 x 240, the top screen). Layer "room": walls around, a rug, a door in the right wall.
Object layer "things" (spec 021 AC-5): 6 objects of type `walker`, 2 of type `walker` whose `prefab` property says
`flying_walker` (a variant), 5 objects of type `coin`, 1 `coin` with an int property `value` of 5 (a gold coin) and
a `door` the game reads itself (a skipped type). Spawned in this order: "map: spawned 14 skipped 1 unknown 0 misses 0".
"""

import json
from pathlib import Path

W, H = 25, 15
FLOOR, WALL, DOOR, RUG = 1, 2, 3, 4


def cells():
    data = []
    for y in range(H):
        for x in range(W):
            if x == W - 1 and y in (7, 8):
                data.append(DOOR)
            elif x in (0, W - 1) or y in (0, H - 1):
                data.append(WALL)
            elif 9 <= x <= 15 and 5 <= y <= 9:
                data.append(RUG)
            else:
                data.append(FLOOR)
    return data


def obj(oid, name, kind, x, y, w=0, h=0, props=None):
    o = {"id": oid, "name": name, "type": kind, "x": x, "y": y, "width": w, "height": h, "rotation": 0,
         "visible": True}
    if w == 0 and h == 0:
        o["point"] = True
    if props:
        o["properties"] = props
    return o


def things():
    out = []
    walkers = [(40, 40), (120, 72), (200, 40), (280, 104), (60, 168), (240, 184)]
    for i, (x, y) in enumerate(walkers):
        out.append(obj(len(out) + 1, "walker%d" % (i + 1), "walker", x, y))
    for i, (x, y) in enumerate([(100, 128), (320, 56)]):
        out.append(obj(len(out) + 1, "flyer%d" % (i + 1), "walker", x, y,
                       props=[{"name": "prefab", "type": "string", "value": "flying_walker"}]))
    for i, (x, y) in enumerate([(48, 96), (96, 200), (176, 136), (304, 160), (352, 32)]):
        out.append(obj(len(out) + 1, "coin%d" % (i + 1), "coin", x, y, 8, 8))
    out.append(obj(len(out) + 1, "gold", "coin", 196, 112, 8, 8, [{"name": "value", "type": "int", "value": 5}]))
    out.append(obj(len(out) + 1, "door", "door", 384, 112, 16, 32))
    return out


def main():
    here = Path(__file__).resolve().parent
    objects = things()
    room = {
        "type": "map", "version": "1.10", "tiledversion": "1.10.2", "orientation": "orthogonal",
        "renderorder": "right-down", "infinite": False, "width": W, "height": H, "tilewidth": 16, "tileheight": 16,
        "nextlayerid": 3, "nextobjectid": len(objects) + 1,
        "tilesets": [{"firstgid": 1, "name": "room", "image": "../tiles/room.png", "imagewidth": 64,
                      "imageheight": 16, "tilewidth": 16, "tileheight": 16, "tilecount": 4, "columns": 4}],
        "layers": [
            {"id": 1, "type": "tilelayer", "name": "room", "x": 0, "y": 0, "width": W, "height": H, "opacity": 1,
             "visible": True, "data": cells()},
            {"id": 2, "type": "objectgroup", "name": "things", "draworder": "topdown", "opacity": 1, "visible": True,
             "objects": objects},
        ],
    }
    path = here.parent / "assets" / "rooms" / "room.tmj"
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(room, f, indent=1)
        f.write("\n")


if __name__ == "__main__":
    main()
