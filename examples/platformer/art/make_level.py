"""Writes the platformer's Tiled level (spec 012 T7.1), in Tiled's own JSON formats so it opens and edits in Tiled
(1.10 or newer); original, made for NNHGE, zlib like the rest. Called by make_art.py (`python -I make_art.py`);
`python -I make_level.py --show` prints the level as text.

../assets/tiles/world.tsj    the tileset: ../art/tiles.png, 16x16 tiles; plank `one_way`, ground tiles `solid`,
                             the water animated (two frames of 400 ms)
../assets/levels/level1.tmj  the level, 128x20 tiles: layers back to front
    clouds     parallax 0.25 x 0.5    (a far backdrop)
    hills      parallax 0.5 x 1       (a near backdrop)
    ground     property collision     (every tile solid)
    platforms                         (one-way planks, flowers, the door's arch)
    things     objects: spawn (point), coin (rectangles, int value), door (rectangle, string to = "bonus")
    front      opacity 0.85           (bushes, grass tufts, the water: drawn over the player)
    map properties: title, music (a string the game shows; the example has no music files)

The build converts the .tmj into levels/level1.nnhmap (nnh-assetc); the .tsj is read by the converter, not packed.
"""

import json
import sys
from pathlib import Path

WIDTH, HEIGHT = 128, 20
TILE = 16

# Tile ids of tiles.png (art.hpp): index in the sheet + 1, which is also the map's gid (one tileset, firstgid 1).
GRASS, DIRT, BRICKS, PLANK, COIN, WATER, WATER2, ARROW = 1, 2, 3, 4, 5, 6, 7, 8
HILL, HILL_TOP, CLOUD_L, CLOUD_M, CLOUD_R, BUSH, FLOWER, STONE = 9, 10, 11, 12, 13, 14, 15, 16
DARK_BRICKS, LATTICE, STAR, TUFT = 17, 18, 19, 20
FLIP_X = 0x80000000  # Tiled's horizontal flip bit in a gid

# --- the level, as features (cells: x right, y down, 0-based) -------------------------------------------------

GROUND = [  # (from x, to x, top row); the columns go down to the bottom; gaps are pits with water
    (0, 17, 16), (21, 34, 15), (38, 48, 16), (54, 74, 16), (78, 95, 14), (100, 127, 16)]
STONE_STEPS = [(58, 15), (59, 14), (60, 13), (61, 13), (62, 13)]  # (x, top row): a stone staircase
BRICKS_ROWS = [(25, 28, 12), (84, 88, 11)]  # (from, to, row): solid blocks to jump on or walk under
PLANKS = [  # (from, to, row): one-way
    (11, 14, 14), (41, 45, 13), (44, 48, 10), (48, 55, 14),  # the last one bridges a pit: drop through it
    (64, 67, 14), (68, 71, 12), (64, 67, 10), (68, 71, 8),  # a ladder of planks up to the sky
    (104, 108, 14)]
COINS = [  # (from, to, row)
    (8, 10, 15), (11, 14, 13), (25, 28, 11), (44, 48, 9), (49, 53, 13), (60, 62, 12), (68, 71, 7),
    (84, 88, 10), (90, 93, 13), (104, 108, 13), (112, 116, 15)]
FLOWERS = [6, 13, 24, 32, 40, 57, 66, 82, 92, 103, 110, 118]
BUSHES = [16, 29, 46, 72, 86, 106, 114]
TUFTS = [2, 15, 22, 33, 39, 56, 69, 80, 94, 101, 117, 125]
SPAWN = (3, 16)  # cell whose bottom centre the player's feet start on
DOOR = (122, 14)  # top-left cell of a 1x2 door; the arch around it is dark bricks
CLOUDS = [(2, 4, 7), (10, 13, 6), (18, 19, 9), (26, 28, 7), (34, 36, 6), (42, 43, 8), (50, 53, 7),
          (60, 61, 9), (68, 70, 6)]
HILLS = [(0, 8, 4), (6, 14, 6), (13, 20, 3), (19, 30, 5), (28, 36, 7), (35, 44, 4), (42, 52, 6),
         (50, 62, 5), (60, 70, 3), (68, 80, 6)]  # (from, to, height in cells)


def grid():
    return [[0] * WIDTH for _ in range(HEIGHT)]


def ground_top(x):
    for start, end, top in GROUND:
        if start <= x <= end:
            return top
    return None


def ground_layer():
    cells = grid()
    for x in range(WIDTH):
        top = ground_top(x)
        if top is None:
            continue
        for y in range(top, HEIGHT):
            cells[y][x] = GRASS if y == top else DIRT
    for x, top in STONE_STEPS:
        for y in range(top, HEIGHT):
            cells[y][x] = STONE
    for start, end, row in BRICKS_ROWS:
        for x in range(start, end + 1):
            cells[row][x] = BRICKS
    return cells


def platforms_layer():
    cells = grid()
    for start, end, row in PLANKS:
        for x in range(start, end + 1):
            cells[row][x] = PLANK
    for x in FLOWERS:
        cells[ground_top(x) - 1][x] = FLOWER  # not marked: no collision
    dx, dy = DOOR
    for y in range(dy - 1, dy + 2):
        cells[y][dx - 1] = DARK_BRICKS
        cells[y][dx + 1] = DARK_BRICKS
    cells[dy - 1][dx] = DARK_BRICKS
    return cells


def front_layer():
    cells = grid()
    for x in range(WIDTH):
        if ground_top(x) is None:
            cells[HEIGHT - 1][x] = WATER  # animated in the tileset
    for x in BUSHES:
        cells[ground_top(x) - 1][x] = BUSH
    for i, x in enumerate(TUFTS):
        cells[ground_top(x) - 1][x] = TUFT | (FLIP_X if i % 2 else 0)  # Tiled flips, kept by the converter
    return cells


def clouds_layer():
    cells = grid()
    for start, end, row in CLOUDS:
        cells[row][start - 1] = CLOUD_L
        for x in range(start, end + 1):
            cells[row][x] = CLOUD_M
        cells[row][end + 1] = CLOUD_R
    return cells


def hills_layer():
    cells = grid()
    for x in range(WIDTH):
        height = max([h for start, end, h in HILLS if start <= x <= end] or [0])
        if height == 0:
            continue
        cells[HEIGHT - height][x] = HILL_TOP
        for y in range(HEIGHT - height + 1, HEIGHT):
            cells[y][x] = HILL
    return cells


# --- Tiled JSON ------------------------------------------------------------------------------------------------


def prop(name, kind, value):
    return {"name": name, "type": kind, "value": value}


def tile_layer(layer_id, name, cells, **extra):
    layer = {"data": cells, "height": HEIGHT, "id": layer_id, "name": name, "opacity": 1, "type": "tilelayer",
             "visible": True, "width": WIDTH, "x": 0, "y": 0}
    layer.update(extra)
    return layer


def rect_object(object_id, name, kind, x, y, w, h, properties=None):
    obj = {"height": h, "id": object_id, "name": name, "rotation": 0, "type": kind, "visible": True,
           "width": w, "x": x, "y": y}
    if properties:
        obj["properties"] = properties
    return obj


def things_layer(layer_id):
    sx, sy = SPAWN
    objects = [{"height": 0, "id": 1, "name": "spawn", "point": True, "rotation": 0, "type": "spawn",
                "visible": True, "width": 0, "x": sx * TILE + TILE // 2, "y": sy * TILE}]
    next_id = 2
    for start, end, row in COINS:
        for x in range(start, end + 1):
            objects.append(rect_object(next_id, "coin", "coin", x * TILE + 2, row * TILE + 2, 12, 12,
                                       [prop("value", "int", 1)]))
            next_id += 1
    dx, dy = DOOR
    objects.append(rect_object(next_id, "door", "door", dx * TILE, dy * TILE, TILE, 2 * TILE,
                               [prop("to", "string", "bonus")]))
    layer = {"draworder": "topdown", "id": layer_id, "name": "things", "objects": objects, "opacity": 1,
             "type": "objectgroup", "visible": True, "x": 0, "y": 0}
    return layer, next_id + 1


def level_map():
    things, next_object = things_layer(5)
    layers = [
        tile_layer(1, "clouds", clouds_layer(), parallaxx=0.25, parallaxy=0.5),
        tile_layer(2, "hills", hills_layer(), parallaxx=0.5),
        tile_layer(3, "ground", ground_layer(), properties=[prop("collision", "bool", True)]),
        tile_layer(4, "platforms", platforms_layer()),
        things,
        tile_layer(6, "front", front_layer(), opacity=0.85),
    ]
    return {
        "compressionlevel": -1, "height": HEIGHT, "infinite": False, "layers": layers, "nextlayerid": 7,
        "nextobjectid": next_object, "orientation": "orthogonal",
        # Parallax layers line up with the map when the view's centre is here: the first screen, camera at
        # the bottom of the map (where the player starts).
        "parallaxoriginx": 200, "parallaxoriginy": HEIGHT * TILE - 120,
        "properties": [prop("music", "string", "meadow"), prop("title", "string", "LEVEL 1: HILLS")],
        "renderorder": "right-down", "tiledversion": "1.10.2", "tileheight": TILE,
        "tilesets": [{"firstgid": 1, "source": "../tiles/world.tsj"}],
        "tilewidth": TILE, "type": "map", "version": "1.10", "width": WIDTH,
    }


def tileset():
    solid = [prop("solid", "bool", True)]
    return {
        "columns": 8, "image": "../art/tiles.png", "imageheight": 64, "imagewidth": 128, "margin": 0,
        "name": "world", "spacing": 0, "tilecount": 32, "tiledversion": "1.10.2", "tileheight": TILE,
        "tiles": [
            {"id": GRASS - 1, "properties": solid},
            {"id": DIRT - 1, "properties": solid},
            {"id": BRICKS - 1, "properties": solid},
            {"id": PLANK - 1, "properties": [prop("one_way", "bool", True)]},
            {"id": WATER - 1, "animation": [{"duration": 400, "tileid": WATER - 1},
                                            {"duration": 400, "tileid": WATER2 - 1}]},
            {"id": STONE - 1, "properties": solid},
        ],
        "tilewidth": TILE, "type": "tileset", "version": "1.10",
    }


def to_json(document):
    """Like Tiled: one-space indents, and each layer's data as rows of the map's width."""
    rows = {}
    for i, layer in enumerate(document.get("layers", [])):
        if layer["type"] == "tilelayer":
            key = f"@@data{i}@@"
            rows[key] = layer["data"]
            layer["data"] = key
    text = json.dumps(document, indent=1)
    for key, cells in rows.items():
        lines = ",\n  ".join(", ".join(str(v) for v in row) for row in cells)
        text = text.replace(f'"{key}"', "[" + lines + "]")
    return text + "\n"


def write(assets):
    """Writes tiles/world.tsj and levels/level1.tmj under the assets folder."""
    for folder, name, document in (("tiles", "world.tsj", tileset()), ("levels", "level1.tmj", level_map())):
        path = Path(assets) / folder / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(to_json(document), encoding="utf-8", newline="\n")
        print(f"wrote {folder}/{name}")


def show():
    """The level as text: # ground, S stone, B bricks, = plank, c coin, @ spawn, D door, ~ water."""
    chars = {GRASS: "#", DIRT: "#", STONE: "S", BRICKS: "B", PLANK: "=", DARK_BRICKS: "K", FLOWER: ","}
    canvas = [["." for _ in range(WIDTH)] for _ in range(HEIGHT)]
    for layer in (ground_layer(), platforms_layer()):
        for y in range(HEIGHT):
            for x in range(WIDTH):
                if layer[y][x] in chars:
                    canvas[y][x] = chars[layer[y][x]]
    for y, row in enumerate(front_layer()):
        for x, v in enumerate(row):
            if v == WATER:
                canvas[y][x] = "~"
    for start, end, row in COINS:
        for x in range(start, end + 1):
            canvas[row][x] = "c"
    canvas[SPAWN[1] - 1][SPAWN[0]] = "@"
    canvas[DOOR[1]][DOOR[0]] = canvas[DOOR[1] + 1][DOOR[0]] = "D"
    print("\n".join("".join(row) for row in canvas))


if __name__ == "__main__":
    if "--show" in sys.argv:
        show()
    else:
        write(Path(__file__).resolve().parent.parent / "assets")
