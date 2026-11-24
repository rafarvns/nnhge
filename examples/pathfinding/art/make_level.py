"""Writes the pathfinding example's level, ../assets/levels/keep.tmj, a Tiled map on ../assets/art/tiles.png (made by
make_art.py): run `python -I make_level.py`.

48 x 30 tiles of 16 px. Layer "ground": floor, roads, swamp, shallow water and open doorways; each ground tile has an
int property `path_cost` (road 1, floor and doorways 2, swamp 4, water 8) that the game turns into path weights
(docs/api/pathfinding.md#weights-from-tiled). Layer "walls" (bool property `collision`): walls and deep water, solid.
A thick wall splits the keep: a door at (16, 6) and a one-cell corridor through the thick part at row 20 (a 2 x 2 hero
can't pass it); a wall along row 15 has doors at (30, 15) and (40, 15). The lake (top right) has deep water around an
island nobody can reach. Object layer "spots": `hero` (a point), `crowd` (the chasers' area) and `door` points in
order (the game toggles them: tile 7 on "walls" when closed).
"""

import json
from pathlib import Path

W, H = 48, 30
FLOOR, WALL, WATER, SWAMP, ROAD, DOOR_OPEN, DOOR_CLOSED, DEEP = range(1, 9)
DOORS = [(16, 6), (30, 15), (40, 15)]


def build():
    ground = [[FLOOR] * W for _ in range(H)]
    walls = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            if x in (0, W - 1) or y in (0, H - 1):
                walls[y][x] = WALL
    for y in range(1, H - 1):  # the dividing wall, thick around the corridor
        walls[y][16] = WALL
        if 18 <= y <= 22:
            walls[y][17] = walls[y][18] = WALL
    for x in range(17, W - 1):  # the wall along row 15
        walls[15][x] = WALL
    for x in (16, 17, 18):  # the corridor
        walls[20][x] = 0
    for (x, y) in DOORS:
        walls[y][x] = 0
        ground[y][x] = DOOR_OPEN
    for x in range(1, W - 1):  # roads
        if walls[24][x] == 0:
            ground[24][x] = ROAD
    for y in range(1, H - 1):
        ground[y][8] = ROAD
    for y in range(17, 23):  # swamp, bottom left
        for x in range(2, 14):
            if x != 8 and (x * 5 + y * 3) % 7 != 0:
                ground[y][x] = SWAMP
    for y in range(2, 13):  # the lake: shallow water, deep water, an island
        for x in range(25, 44):
            ground[y][x] = WATER
    for y in range(4, 11):
        for x in range(28, 41):
            walls[y][x] = DEEP
    for y in range(6, 9):
        for x in range(31, 38):
            walls[y][x] = 0
            ground[y][x] = FLOOR
    return ground, walls


def flat(rows):
    return [v for row in rows for v in row]


def prop(name, kind, value):
    return {"name": name, "type": kind, "value": value}


def tileset():
    costs = {FLOOR: 2, WATER: 8, SWAMP: 4, ROAD: 1, DOOR_OPEN: 2}
    tiles = [{"id": t - 1, "properties": [prop("path_cost", "int", c)]} for t, c in sorted(costs.items())]
    tiles.append({"id": DOOR_CLOSED - 1, "properties": [prop("door", "bool", True)]})
    return {"firstgid": 1, "name": "keep", "image": "../art/tiles.png", "imagewidth": 128, "imageheight": 16,
            "tilewidth": 16, "tileheight": 16, "tilecount": 8, "columns": 8, "margin": 0, "spacing": 0,
            "tiles": tiles}


def point(oid, name, kind, x, y):
    return {"id": oid, "name": name, "type": kind, "x": x, "y": y, "width": 0, "height": 0, "rotation": 0,
            "visible": True, "point": True}


def spots():
    out = [point(1, "hero", "hero", 3 * 16 + 8, 3 * 16 + 8)]
    out.append({"id": 2, "name": "crowd", "type": "crowd", "x": 26 * 16, "y": 18 * 16, "width": 16 * 18,
                "height": 16 * 9, "rotation": 0, "visible": True})
    for i, (x, y) in enumerate(DOORS):
        out.append(point(3 + i, "door%d" % (i + 1), "door", x * 16 + 8, y * 16 + 8))
    return out


def main():
    ground, walls = build()
    level = {
        "type": "map", "version": "1.10", "tiledversion": "1.10.2", "orientation": "orthogonal",
        "renderorder": "right-down", "infinite": False, "width": W, "height": H, "tilewidth": 16, "tileheight": 16,
        "nextlayerid": 4, "nextobjectid": 6, "tilesets": [tileset()],
        "layers": [
            {"id": 1, "type": "tilelayer", "name": "ground", "x": 0, "y": 0, "width": W, "height": H, "opacity": 1,
             "visible": True, "data": flat(ground)},
            {"id": 2, "type": "tilelayer", "name": "walls", "x": 0, "y": 0, "width": W, "height": H, "opacity": 1,
             "visible": True, "data": flat(walls), "properties": [prop("collision", "bool", True)]},
            {"id": 3, "type": "objectgroup", "name": "spots", "draworder": "topdown", "opacity": 1, "visible": True,
             "objects": spots()},
        ],
    }
    path = Path(__file__).resolve().parent.parent / "assets" / "levels" / "keep.tmj"
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(level, f, indent=1)
        f.write("\n")


if __name__ == "__main__":
    main()
