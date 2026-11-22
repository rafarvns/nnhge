// A Tiled map file → the map model (see tiled_map.hpp).

#include "tiled_map.hpp"

#include <algorithm>

#include "json_fields.hpp"
#include "tiled_layer_data.hpp"
#include "tiled_objects.hpp"
#include "tiled_properties.hpp"
#include "tiled_tileset.hpp"

namespace nnh::assetc {

namespace {

namespace fmt = nnh::detail::mapfmt;

/// Type, orientation, finiteness, sizes and the parallax origin.
bool read_header(json::Ref root, const std::string& file, Context& ctx, Map& out) {
    Fields f(root, "map");
    const std::string type = f.string("type", "map");
    if (f.ok() && type != "map") return ctx.fail(file, "this file is a " + type + ", not a map");
    const std::string orientation = f.required_string("orientation");
    if (f.ok() && orientation != "orthogonal") {
        return ctx.fail(file, orientation + " maps aren't supported: NNHGE draws orthogonal maps "
                                            "(Map > Map Properties > Orientation)");
    }
    if (f.boolean("infinite", false)) {
        return ctx.fail(file, "infinite maps aren't supported: turn off Map > Map Properties > Infinite");
    }
    out.width = static_cast<int>(f.required_integer("width", 1, fmt::kMaxMapSize));
    out.height = static_cast<int>(f.required_integer("height", 1, fmt::kMaxMapSize));
    out.tile_width = static_cast<int>(f.required_integer("tilewidth", 1, 4096));
    out.tile_height = static_cast<int>(f.required_integer("tileheight", 1, 4096));
    out.parallax_origin_x = static_cast<float>(f.number("parallaxoriginx", 0, -1e9, 1e9));
    out.parallax_origin_y = static_cast<float>(f.number("parallaxoriginy", 0, -1e9, 1e9));
    if (!f.ok()) return ctx.fail(file, f.error());
    const auto tile_ok = [](int n) { return n >= fmt::kMinTileSize && n <= fmt::kMaxTileSize; };
    if (!tile_ok(out.tile_width) || !tile_ok(out.tile_height)) {
        return ctx.fail(file, "tiles are " + std::to_string(out.tile_width) + "x" + std::to_string(out.tile_height) +
                                  ": NNHGE tiles are 8 to 64 pixels per side");
    }
    return true;
}

/// Every tileset, sorted by first gid, with NNHGE's consecutive first ids.
bool read_tilesets(json::Ref root, const std::string& file, Context& ctx, Map& out) {
    Fields f(root, "map");
    const json::Ref sets = f.array("tilesets");
    if (!f.ok()) return ctx.fail(file, f.error());
    if (sets.size() > fmt::kMaxTilesets) return ctx.fail(file, "more than 8 tilesets: a map can use at most 8");
    for (json::Ref entry = sets.first(); entry; entry = entry.next()) {
        Tileset set;
        if (!read_tileset(entry, file, out.tile_width, out.tile_height, ctx, set)) return false;
        out.tilesets.push_back(std::move(set));
    }
    std::stable_sort(out.tilesets.begin(), out.tilesets.end(),
                     [](const Tileset& a, const Tileset& b) { return a.first_gid < b.first_gid; });
    int next = 1;
    for (std::size_t i = 0; i < out.tilesets.size(); ++i) {
        if (i > 0 && out.tilesets[i].first_gid == out.tilesets[i - 1].first_gid) {
            return ctx.fail(file, "two tilesets start at firstgid " + std::to_string(out.tilesets[i].first_gid));
        }
        out.tilesets[i].first_id = next;
        next += out.tilesets[i].tile_count;
    }
    if (next - 1 > fmt::kMaxTileIds) {
        return ctx.fail(file, "the tilesets have " + std::to_string(next - 1) +
                                  " tiles in all: a map can use at most 8191");
    }
    return true;
}

/// Tile and object layers, in Tiled's order within each kind.
bool read_layers(json::Ref root, const std::string& file, Context& ctx, Map& out) {
    Fields f(root, "map");
    const json::Ref layers = f.array("layers");
    if (!f.ok()) return ctx.fail(file, f.error());
    long long tile_layers = 0; // checked before any layer's data is read
    for (json::Ref layer = layers.first(); layer; layer = layer.next()) {
        tile_layers += layer.get("type").string() == "tilelayer" ? 1 : 0;
    }
    const long long cells = 1LL * out.width * out.height * tile_layers;
    if (cells > fmt::kMaxTotalCells) {
        return ctx.fail(file, "the tile layers have " + std::to_string(cells) +
                                  " cells in all: a map can have at most 4194304 (4 Mi)");
    }
    for (json::Ref layer = layers.first(); layer; layer = layer.next()) {
        const std::string type(layer.get("type").string());
        const std::string name(layer.get("name").string());
        if (type == "tilelayer") {
            if (out.tile_layers.size() == fmt::kMaxTileLayers) return ctx.fail(file, "more than 16 tile layers");
            TileLayer tiles;
            if (!read_tile_layer(layer, out, file, ctx, tiles)) return false;
            out.tile_layers.push_back(std::move(tiles));
        } else if (type == "objectgroup") {
            if (out.object_layers.size() == fmt::kMaxObjectLayers) return ctx.fail(file, "more than 16 object layers");
            ObjectLayer objects;
            const int index = static_cast<int>(out.object_layers.size());
            if (!read_object_layer(layer, index, file, ctx, objects, out.objects)) return false;
            out.object_layers.push_back(std::move(objects));
        } else if (type == "imagelayer") {
            return ctx.fail(file, "layer \"" + name + "\": image layers aren't supported: draw the image as a sprite");
        } else if (type == "group") {
            return ctx.fail(file, "layer \"" + name + "\": group layers aren't supported: move its layers out of it");
        } else {
            return ctx.fail(file, "layer \"" + name + "\": unknown layer type \"" + type + "\"");
        }
    }
    return true;
}

} // namespace

bool read_tiled_map(const std::string& map_file, Context& ctx, Map& out) {
    out = {};
    std::string text;
    if (!ctx.files.read(map_file, json::kMaxDocument + 1, text)) return ctx.fail(map_file, "file not found");
    json::Document doc;
    if (!doc.parse(text)) return ctx.fail(map_file, doc.error());
    const json::Ref root = doc.root();
    if (!root.is_object()) return ctx.fail(map_file, "a map must be a JSON object");
    if (!read_header(root, map_file, ctx, out)) return false;
    std::string error;
    if (!read_properties(root.get("properties"), "map", out.properties, error)) return ctx.fail(map_file, error);
    return read_tilesets(root, map_file, ctx, out) && read_layers(root, map_file, ctx, out);
}

} // namespace nnh::assetc
