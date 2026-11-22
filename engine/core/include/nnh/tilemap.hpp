#pragma once
/// @file nnh/tilemap.hpp
/// Tilemaps: a grid of tiles in layers, drawn from tilesets (textures) by gfx::draw_tilemap, with objects,
/// properties and a collision grid for <nnh/collision.hpp>. Every part can be built in code; maps converted
/// by the build (`.nnhmap`, from the Tiled editor) load into the same object with load_tilemap().
/// See docs/api/tilemap.md.
///
/// A map is one block of memory (tag "tilemap", Heap) allocated by create() with fixed capacities:
/// nothing grows while playing, queries and edits never allocate. Coordinates: the map's cell (0, 0)
/// starts at world (0, 0); cell (x, y) covers [x·tile_width, (x+1)·tile_width) × [y·tile_height, ...).
///
/// @code
/// auto made = nnh::Tilemap::create({.width = 40, .height = 15});
/// nnh::Tilemap map = std::move(made.value());
/// const int first = map.add_tileset({.texture = &tiles}).value();   // tile ids first, first + 1, ...
/// const int ground = map.add_tile_layer({.name = "ground", .collision = true}).value();
/// map.fill(ground, 0, 14, 40, 1, {.id = static_cast<std::uint16_t>(first)});
/// const auto moved = nnh::collision::move(map.collision_grid(), player, velocity);
/// @endcode

#include <cstdint>

#include "nnh/collision.hpp"
#include "nnh/gfx.hpp"
#include "nnh/math.hpp"
#include "nnh/result.hpp"
#include "nnh/time.hpp"

namespace nnh {

class Texture;

/// Highest tile id of a map (ids count across all its tilesets; 0 is the empty cell).
inline constexpr int kMaxTileId = 8191;

/// One cell of a tile layer: a tile id (0 = empty) and Tiled's flips (diagonal is applied first).
struct Tile {
    std::uint16_t id = 0;
    bool flip_x = false;
    bool flip_y = false;
    bool flip_diagonal = false; ///< swap x and y; only on maps with square tiles
    bool empty() const { return id == 0; }
    friend bool operator==(const Tile&, const Tile&) = default;
};

enum class PropertyType : std::uint8_t { None, Bool, Int, Float, String };

/// A read-only view of the properties of one owner (the map, a layer, a tile, an object). Valid while the
/// map lives and until properties are set again on that map (setting may move them: get a new view).
/// Getters return `fallback` when the name is missing or holds another type; get_float also reads ints.
class Properties {
public:
    bool has(const char* name) const;
    PropertyType type(const char* name) const; ///< None if missing
    bool get_bool(const char* name, bool fallback = false) const;
    int get_int(const char* name, int fallback = 0) const;
    float get_float(const char* name, float fallback = 0) const;
    const char* get_string(const char* name, const char* fallback = "") const;
    int count() const { return count_; }
    const char* name(int index) const; ///< "" outside 0..count()-1

private:
    friend struct TilemapAccess;
    const std::uint8_t* block_ = nullptr;
    int first_ = 0;
    int count_ = 0;
};

/// Where a property lives.
struct PropertyOwner {
    enum class Kind : std::uint8_t { Map, TileLayer, ObjectLayer, Tile, Object } kind = Kind::Map;
    int index = 0; ///< tile layer index, object layer index, tile id or object index (unused for Map)

    static constexpr PropertyOwner map() { return {}; }
    static constexpr PropertyOwner tile_layer(int layer) { return {Kind::TileLayer, layer}; }
    static constexpr PropertyOwner object_layer(int layer) { return {Kind::ObjectLayer, layer}; }
    static constexpr PropertyOwner tile(int tile_id) { return {Kind::Tile, tile_id}; }
    static constexpr PropertyOwner object(int index) { return {Kind::Object, index}; }
};

enum class ObjectShape : std::uint8_t { Rect, Point };

/// An object of an object layer (a view: its strings and properties live in the map).
struct MapObject {
    int id = 0;            ///< Tiled's object id, or the one given in code (0 = no such object)
    const char* name = "";
    const char* type = ""; ///< Tiled's class
    ObjectShape shape = ObjectShape::Rect;
    Rect bounds{};         ///< world pixels; a Point has w = h = 0
    int object_layer = -1;
    Properties properties{};
};

/// Capacities of a map, fixed at creation (nothing grows later). See docs/api/tilemap.md for the memory
/// each one costs and the limits.
struct TilemapDesc {
    int width = 0, height = 0;            ///< cells, 1..1024 each
    int tile_width = 16, tile_height = 16; ///< pixels, 8..64 each (square or not)
    int max_tile_layers = 4;   ///< ≤ 16; and width·height·max_tile_layers ≤ 4 Mi cells
    int max_tilesets = 2;      ///< ≤ 8
    int max_tile_ids = 256;    ///< tiles over all tilesets, ≤ 8191
    int max_object_layers = 1; ///< ≤ 16
    int max_objects = 64;      ///< ≤ 4096
    int max_properties = 64;   ///< ≤ 16384, over all owners
    int string_bytes = 2048;   ///< names and string values (each ≤ 255 bytes + NUL), ≤ 64 KiB; 1 byte is ""
    int max_animations = 0;       ///< animated tiles, ≤ 256 (0: set_tile_animation is refused)
    int max_animation_frames = 0; ///< frames over all animations, ≤ 4096
};

/// A tileset: a texture cut into tiles of the map's tile size, left to right, top to bottom.
struct TilesetDesc {
    const Texture* texture = nullptr; ///< borrowed: keep the texture alive while the map is drawn
    int margin = 0;                   ///< pixels around the tiles
    int spacing = 0;                  ///< pixels between tiles
    int tile_count = 0;               ///< 0 = every tile that fits
    const char* name = "";
};

struct TileLayerDesc {
    const char* name = "";
    Vec2 offset{};          ///< drawing only (stage 3): collision and coordinates ignore it
    Vec2 parallax{1, 1};    ///< drawing only (stage 3)
    float opacity = 1;      ///< 0..1, drawing only (stage 3)
    bool visible = true;
    bool collision = false; ///< every non-empty tile of this layer is solid
};

struct ObjectDesc {
    const char* name = "";
    const char* type = "";
    ObjectShape shape = ObjectShape::Rect;
    Rect bounds{}; ///< a Point keeps x, y and gets w = h = 0
    int id = 0;    ///< 0 = automatic (highest id so far + 1)
};

/// A tilemap. Move-only; owns one block of memory (tag "tilemap"), freed by the destructor or reset().
/// Queries and edits are cheap, never allocate and are safe per frame. Calling them on an empty map is
/// harmless: queries return empty values, edits do nothing.
class Tilemap {
public:
    /// Allocates a map with `desc`'s capacities: tile layers start empty (every cell id 0), no tilesets.
    /// Load or build time only (one allocation). Errors: InvalidArgument (size < 1, tile size outside 8..64,
    /// negative capacities), LimitReached (over a limit), OutOfMemory.
    static Result<Tilemap> create(const TilemapDesc& desc);

    Tilemap() = default;
    ~Tilemap() { reset(); }
    Tilemap(Tilemap&& other) noexcept;
    Tilemap& operator=(Tilemap&& other) noexcept;
    Tilemap(const Tilemap&) = delete;
    Tilemap& operator=(const Tilemap&) = delete;

    /// Frees the map now; it becomes empty. Views (Properties, MapObject strings) and grids taken from it
    /// become invalid.
    void reset();
    bool valid() const { return block_ != nullptr; }

    int width() const;       ///< cells
    int height() const;      ///< cells
    int tile_width() const;  ///< pixels
    int tile_height() const; ///< pixels

    /// The world rectangle of cell (x, y) (any x, y, also outside the map).
    Rect cell_rect(int x, int y) const;
    /// The cell containing `world` (floor of world / tile size) in `x`, `y`, set even outside the map
    /// (clamped to ±2^30; 0 for NaN). Returns true when the cell is inside the map.
    bool world_to_cell(Vec2 world, int& x, int& y) const;

    /// Adds a tileset cut from `desc.texture` (valid, at least one tile) and returns the id of its first
    /// tile; its tiles get the next `tile count` ids. Build time only. Errors: InvalidArgument (no or empty
    /// texture, nothing fits, tile_count more than fits, negative margin/spacing, name over 255 bytes),
    /// LimitReached (max_tilesets, max_tile_ids or string_bytes full).
    Result<int> add_tileset(const TilesetDesc& desc);
    int tileset_count() const;
    int tile_id_count() const; ///< tile ids in use: 1..tile_id_count()

    /// Adds an empty tile layer on top of the others and returns its index.
    /// Errors: InvalidArgument (name over 255 bytes), LimitReached (max_tile_layers or string_bytes full).
    Result<int> add_tile_layer(const TileLayerDesc& desc);
    int tile_layer_count() const;
    int find_tile_layer(const char* name) const; ///< first layer with that name, or -1
    const char* tile_layer_name(int layer) const; ///< "" for an invalid layer

    /// The tile at a cell; empty outside the map or for an invalid layer.
    Tile tile(int layer, int x, int y) const;
    /// The tile under a world position; empty outside the map.
    Tile tile_at(int layer, Vec2 world) const;
    /// Changes one cell; seen by the next draw and collision query. Cells outside the map are ignored.
    /// Asserts (and does nothing in Release) on an invalid layer, an unknown tile id or a diagonal flip on
    /// a map with non-square tiles.
    void set_tile(int layer, int x, int y, Tile tile);
    /// set_tile over a w × h block of cells, clipped to the map.
    void fill(int layer, int x, int y, int w, int h, Tile tile);

    void set_layer_offset(int layer, Vec2 offset);
    void set_layer_parallax(int layer, Vec2 factor);
    void set_layer_opacity(int layer, float opacity); ///< clamped to 0..1
    void set_layer_visible(int layer, bool visible);
    void set_layer_collision(int layer, bool collision);
    Vec2 layer_offset(int layer) const;
    Vec2 layer_parallax(int layer) const;
    float layer_opacity(int layer) const;
    bool layer_visible(int layer) const;
    bool layer_collision(int layer) const;

    /// The world point where parallax layers line up with the map when the view's centre is on it (Tiled's
    /// map "parallax origin"; default (0, 0)). Drawing only.
    void set_parallax_origin(Vec2 origin);
    Vec2 parallax_origin() const;

    /// Marks a tile id solid or one-way (Tiled: tile property `solid` / `one_way`). If tiles were already
    /// placed, layers using its tileset are scanned once: call it while building. Asserts on unknown ids.
    void set_tile_collision(int tile_id, collision::Cell kind);
    collision::Cell tile_collision(int tile_id) const; ///< Empty for unknown ids
    /// What cell (x, y) is to collision: Solid if a `collision` layer has a tile there or a tile there is
    /// solid; else OneWay if a tile there is one-way; else Empty (also outside the map).
    collision::Cell collision_at(int x, int y) const;
    /// A grid for collision::move and friends over this map (origin 0, tile-sized cells, outside Empty: set
    /// `outside` to wall the level in). Reads the map live, so edits are seen at once. Valid while the map
    /// lives (moving the Tilemap object is fine). Empty map: a grid of size 0.
    collision::Grid collision_grid() const;

    /// One frame of an animated tile: the tile shown and for how long.
    struct AnimationFrame {
        std::uint16_t tile_id = 0; ///< a tile of the same tileset as the animated tile
        std::uint16_t ms = 0;      ///< 1..65535 milliseconds
    };
    /// Animates a tile id (Tiled: a tile's animation): wherever it is placed, drawing shows `frames` in turn, looping
    /// (all cells in step, by the clock of gfx::TilemapDrawOptions::animation_time). Collision, properties and
    /// tile() still see the tile id itself. Replaces the tile's animation; `count` 0 removes it. Build time
    /// (shifts the frame table); never allocates. Errors: InvalidArgument (unknown tile id, a frame of another
    /// tileset or of 0 ms, frames null, negative count), LimitReached (more than 32 frames, max_animations or
    /// max_animation_frames full; a loaded map has room only for its own animations).
    /// @code
    /// const nnh::Tilemap::AnimationFrame water[] = {{.tile_id = 9, .ms = 200}, {.tile_id = 10, .ms = 200}};
    /// map.set_tile_animation(9, water, 2);
    /// @endcode
    Result<void> set_tile_animation(int tile_id, const AnimationFrame* frames, int count);
    /// Copies up to `capacity` frames of a tile's animation into `frames` (may be null when capacity is 0) and
    /// returns how many it has (0: not animated, unknown id).
    int tile_animation(int tile_id, AnimationFrame* frames, int capacity) const;
    /// The tile id an animated tile shows at `time` (frames loop over their total duration; negative times count
    /// as 0), or `tile_id` itself when it is not animated.
    int animated_tile_id(int tile_id, Duration time) const;

    /// Adds an object layer and returns its index. Errors: InvalidArgument (name over 255 bytes),
    /// LimitReached (max_object_layers or string_bytes full).
    Result<int> add_object_layer(const char* name);
    int object_layer_count() const;
    const char* object_layer_name(int index) const; ///< "" for an invalid index

    /// Adds an object to an object layer and returns its index (creation order, stable).
    /// Errors: InvalidArgument (unknown object layer, negative size, names over 255 bytes),
    /// LimitReached (max_objects or string_bytes full).
    Result<int> add_object(int object_layer, const ObjectDesc& desc);
    int object_count() const;
    MapObject object(int index) const;                                   ///< id 0 for an invalid index
    int find_object(const char* name) const;                             ///< first index with that name, or -1
    int find_objects(const char* type, int* indices, int capacity) const; ///< writes ≤ capacity, returns total

    /// The properties of an owner (empty for an unknown owner).
    Properties properties(PropertyOwner owner = {}) const;
    /// Sets (adds or replaces) a property. Names and string values: ≤ 255 bytes, stored once in the map's
    /// strings (equal strings are shared). Errors: InvalidArgument (unknown owner, empty or long name),
    /// LimitReached (max_properties or string_bytes full). Never allocates.
    Result<void> set_bool(PropertyOwner owner, const char* name, bool value);
    Result<void> set_int(PropertyOwner owner, const char* name, int value);
    Result<void> set_float(PropertyOwner owner, const char* name, float value);
    Result<void> set_string(PropertyOwner owner, const char* name, const char* value);

private:
    friend struct TilemapAccess;
    std::uint8_t* block_ = nullptr;
};

/// Extra room in a loaded map, so code can add to it later (a file has no spare capacity of its own). Each extra
/// costs what docs/api/tilemap.md#memory says (a tile layer: 40 B + 2 B per cell). Every field ≥ 0.
struct LoadTilemapOptions {
    int extra_tile_layers = 0;  ///< for add_tile_layer()
    int extra_objects = 0;      ///< for add_object() (object layers can't be added to a loaded map)
    int extra_properties = 0;   ///< for set_*(); each can also give one more tile its first property
    int extra_string_bytes = 0; ///< for new names and string values (each takes its length + 1)
};

/// Loads a map converted by the build from the game's assets: `path` is the asset path without extension
/// ("levels/level1" → levels/level1.nnhmap, made from levels/level1.tmj), then loads its tilesets' images (as
/// load_texture(); two tilesets with the same image share one texture). The map owns those textures: they are
/// freed with it. The file is checked completely before it is used (a damaged file is an error, never a crash).
/// The result is a Tilemap like one built in code: every query and edit works on it.
/// Load time only: allocates the map's block (tag "tilemap": the file's size plus the extras) and reads the file
/// into a temporary buffer first (tag "assets"). Errors: InvalidArgument (null or bad path, negative extras),
/// NotFound (no such map, or a tileset image missing: the message names the image), Corrupted (damaged or
/// truncated file, or a tileset image of another size than at conversion), Unsupported (before init(), or made
/// for a newer engine), LimitReached (over a map limit with the extras, or AppConfig::gfx.max_textures full),
/// OutOfMemory.
/// @code
/// auto loaded = nnh::load_tilemap("levels/level1");
/// if (!loaded) { NNH_LOG_ERROR(game, "%s", loaded.error().message); return; }
/// nnh::Tilemap level = std::move(loaded.value());
/// @endcode
Result<Tilemap> load_tilemap(const char* path, const LoadTilemapOptions& options = {});

namespace gfx {

/// How gfx::draw_tilemap() draws. Every field is optional:
/// @code
/// nnh::gfx::draw_tilemap(nnh::Screen::Top, map, {.map_layer = ground, .layer = 2});
/// @endcode
struct TilemapDrawOptions {
    int map_layer = -1;         ///< one tile layer, or -1 = every visible tile layer, in map order
    int layer = 0;              ///< draw layer 0..15, as sprites: same layer = call order
    float opacity = 1;          ///< 0..1, times each map layer's opacity
    Vec2 offset{};              ///< added to each map layer's offset
    bool ignore_camera = false; ///< draw in screen pixels (parallax then uses camera (0, 0))
    /// The animated tiles' clock. Negative (the default): frame_count() at 60 Hz (frame · 1000 / 60 ms), so the
    /// simulator and the console show the same frame on the same frame number. Pass your own (a pause-aware
    /// time) to control it.
    Duration animation_time{-1};
};

/// Draws a map's tile layers on `screen`, at the end of the frame like sprites (ordered by `layer`, then
/// call), so a sprite can sit between two map layers. Records one tile batch per (map layer, tileset): only
/// the cells the camera sees are expanded, at the end of the frame, with the camera, tiles and layer settings
/// of that moment (a set_tile() after this call is drawn). Layer offset and parallax follow Tiled; the
/// layer's position is rounded to whole pixels once, so tiles never show seams. Hidden layers, empty layers
/// and opacity 0 draw nothing. Needs AppConfig::gfx.max_tiles > 0 (asserts otherwise); tiles past it are
/// dropped (assert in Debug/Development). Each batch also takes one of AppConfig::gfx.max_draws. Only inside
/// a frame. No allocation. Asserts on an unknown `map_layer` and on a tileset whose texture was destroyed
/// (that tileset is skipped).
void draw_tilemap(Screen screen, const Tilemap& map, const TilemapDrawOptions& options = {});

} // namespace gfx

} // namespace nnh
