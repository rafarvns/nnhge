// Tiled tilesets → the map model (see tiled_tileset.hpp).

#include "tiled_tileset.hpp"

#include "json_fields.hpp"
#include "paths.hpp"
#include "png_size.hpp"
#include "tiled_properties.hpp"
#include "tilemap/map_layout.hpp"

namespace nnh::assetc {

namespace {

std::string size_text(int w, int h) { return std::to_string(w) + "x" + std::to_string(h); }

bool ends_with(const std::string& s, const char* tail) {
    const std::string t = tail;
    return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

bool texture_side(int n) { return n >= 8 && n <= 1024 && (n & (n - 1)) == 0; }

/// The image: a PNG inside the assets folder, the size Tiled says, usable as a texture.
bool read_image(Fields& f, const std::string& file, Context& ctx, Tileset& out) {
    const std::string written = f.required_string("image");
    out.image_width = static_cast<int>(f.required_integer("imagewidth", 1, 1 << 20));
    out.image_height = static_cast<int>(f.required_integer("imageheight", 1, 1 << 20));
    if (!f.ok()) return ctx.fail(file, f.error());
    const std::string full = join_path(parent_path(file), written);
    std::string inside;
    if (!ends_with(full, ".png")) return ctx.fail(file, "tileset image " + written + " must be a .png file");
    if (!path_inside(ctx.assets_root, full, inside)) {
        return ctx.fail(file, "tileset image " + written + " is outside the assets folder " + ctx.assets_root +
                                  ": move it inside");
    }
    std::string header;
    if (!ctx.files.read(full, kPngHeaderBytes, header)) return ctx.fail(file, "tileset image " + inside + " not found");
    ctx.depend_on(full);
    int w = 0, h = 0;
    if (!png_size(header, w, h)) return ctx.fail(file, "tileset image " + inside + ": not a PNG file");
    if (w != out.image_width || h != out.image_height) {
        return ctx.fail(file, "tileset image " + inside + " is " + size_text(w, h) + " but the tileset says " +
                                  size_text(out.image_width, out.image_height) + ": reopen the tileset in Tiled");
    }
    if (!texture_side(w) || !texture_side(h)) {
        return ctx.fail(file, "tileset image " + inside + " is " + size_text(w, h) +
                                  ": tileset images need power-of-two sides from 8 to 1024 (they are textures)");
    }
    out.image = inside.substr(0, inside.size() - 4);
    return true;
}

/// The grid: the tileset's tile size, margin, spacing, columns and count against the map and the image.
bool read_grid(Fields& f, const std::string& file, int tile_w, int tile_h, Context& ctx, Tileset& out) {
    const int tw = static_cast<int>(f.required_integer("tilewidth", 1, 4096));
    const int th = static_cast<int>(f.required_integer("tileheight", 1, 4096));
    out.margin = static_cast<int>(f.integer("margin", 0, 0, 4096));
    out.spacing = static_cast<int>(f.integer("spacing", 0, 0, 4096));
    out.columns = static_cast<int>(f.required_integer("columns", 0, 65535));
    out.tile_count = static_cast<int>(f.required_integer("tilecount", 1, 65535));
    if (!f.ok()) return ctx.fail(file, f.error());
    if (tw != tile_w || th != tile_h) {
        return ctx.fail(file, "tiles are " + size_text(tw, th) + " but the map's are " + size_text(tile_w, tile_h) +
                                  ": a tileset must use the map's tile size");
    }
    const int columns = nnh::detail::tiles_along(out.image_width, tw, out.margin, out.spacing);
    const int rows = nnh::detail::tiles_along(out.image_height, th, out.margin, out.spacing);
    if (out.columns != columns) {
        return ctx.fail(file, "the tileset says " + std::to_string(out.columns) + " columns, but " +
                                  std::to_string(columns) + " tiles of " + std::to_string(tw) +
                                  " px fit across the image (NNHGE keeps the margin on every side)");
    }
    if (out.tile_count > columns * rows) {
        return ctx.fail(file, "the tileset says " + std::to_string(out.tile_count) + " tiles, but only " +
                                  std::to_string(columns * rows) + " fit in the image");
    }
    return true;
}

/// One entry of "tiles": collision from `solid`/`one_way`, the other properties.
bool read_tile(json::Ref tile, const std::string& file, Context& ctx, Tileset& out) {
    Fields f(tile, "tileset tile");
    const int id = static_cast<int>(f.required_integer("id", 0, out.tile_count - 1));
    if (!f.ok()) return ctx.fail(file, f.error());
    const std::string where = "tile " + std::to_string(id);
    if (tile.get("image")) {
        return ctx.fail(file, "image collection tilesets (one image per tile) aren't supported: use one tileset image");
    }
    if (tile.get("animation").size() > 0) {
        return ctx.fail(file, where + " has an animation: animated tiles aren't supported yet");
    }
    TileInfo info;
    std::string error;
    bool solid = false, one_way = false;
    if (!read_properties(tile.get("properties"), where, info.properties, error) ||
        !take_bool(info.properties, "solid", where, solid, error) ||
        !take_bool(info.properties, "one_way", where, one_way, error)) {
        return ctx.fail(file, error);
    }
    if (solid && one_way) return ctx.fail(file, where + " is both solid and one_way: keep one");
    info.collision = solid ? 1 : one_way ? 2 : 0;
    if (out.tiles.count(id) != 0) return ctx.fail(file, where + " is listed twice");
    if (info.collision != 0 || !info.properties.empty()) out.tiles[id] = std::move(info);
    return true;
}

/// A tileset object (embedded in the map or the root of a .tsj); `file` is where it was read from.
bool read_body(json::Ref body, const std::string& file, int tile_w, int tile_h, Context& ctx, Tileset& out) {
    if (!body.is_object()) return ctx.fail(file, "a tileset must be a JSON object");
    Fields f(body, "tileset");
    if (body.get("wangsets").size() > 0) {
        return ctx.fail(file, "wang sets (terrains) aren't supported: remove them from the tileset");
    }
    if (!body.get("image")) {
        return ctx.fail(file, "image collection tilesets (one image per tile) aren't supported: use one tileset image");
    }
    const json::Ref offset = body.get("tileoffset");
    if (offset.get("x").number() != 0 || offset.get("y").number() != 0) {
        return ctx.fail(file, "tile offset isn't supported: set the tileset's drawing offset to 0, 0");
    }
    if (!read_image(f, file, ctx, out) || !read_grid(f, file, tile_w, tile_h, ctx, out)) return false;
    const json::Ref tiles = f.array("tiles");
    if (!f.ok()) return ctx.fail(file, f.error());
    for (json::Ref tile = tiles.first(); tile; tile = tile.next()) {
        if (!read_tile(tile, file, ctx, out)) return false;
    }
    return true;
}

} // namespace

bool read_tileset(json::Ref entry, const std::string& map_file, int tile_width, int tile_height, Context& ctx,
                  Tileset& out) {
    Fields f(entry, "tileset");
    out.first_gid = static_cast<int>(f.required_integer("firstgid", 1, (1 << 28) - 1));
    const std::string source = f.string("source");
    if (!f.ok()) return ctx.fail(map_file, f.error());
    if (!entry.get("source")) return read_body(entry, map_file, tile_width, tile_height, ctx, out);

    if (ends_with(source, ".tsx")) {
        return ctx.fail(map_file, "tileset " + source + " is XML (.tsx): export the tileset as JSON (.tsj)");
    }
    const std::string path = join_path(parent_path(map_file), source);
    std::string text;
    if (!ctx.files.read(path, json::kMaxDocument + 1, text)) {
        return ctx.fail(map_file, "tileset " + source + " not found");
    }
    ctx.depend_on(path);
    json::Document doc;
    if (!doc.parse(text)) return ctx.fail(path, doc.error());
    return read_body(doc.root(), path, tile_width, tile_height, ctx, out);
}

} // namespace nnh::assetc
