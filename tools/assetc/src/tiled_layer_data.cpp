// A Tiled tile layer → the map model (see tiled_layer_data.hpp).

#include "tiled_layer_data.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "json_fields.hpp"
#include "tiled_properties.hpp"

namespace nnh::assetc {

namespace {

namespace fmt = nnh::detail::mapfmt;

constexpr std::uint32_t kFlipX = 0x80000000u, kFlipY = 0x40000000u, kDiagonal = 0x20000000u;
constexpr std::uint32_t kHexRotation = 0x10000000u, kGidMask = 0x0FFFFFFFu;

/// Standard base64 (with '=' padding) → bytes. False on any other character or a bad length.
bool decode_base64(std::string_view text, std::string& out) {
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        return c == '+' ? 62 : c == '/' ? 63 : -1;
    };
    while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\r')) text.remove_suffix(1);
    while (!text.empty() && (text.front() == ' ' || text.front() == '\n' || text.front() == '\r')) text.remove_prefix(1);
    if (text.size() % 4 != 0) return false;
    out.clear();
    for (std::size_t i = 0; i < text.size(); i += 4) {
        const bool last = i + 4 == text.size();
        const int pad = last ? (text[i + 3] == '=') + (text[i + 2] == '=' && text[i + 3] == '=') : 0;
        std::uint32_t bits = 0;
        for (int k = 0; k < 4; ++k) {
            const int v = k >= 4 - pad ? 0 : value(text[i + static_cast<std::size_t>(k)]);
            if (v < 0) return false;
            bits = bits << 6 | static_cast<std::uint32_t>(v);
        }
        out += static_cast<char>(bits >> 16);
        if (pad < 2) out += static_cast<char>(bits >> 8 & 0xFF);
        if (pad < 1) out += static_cast<char>(bits & 0xFF);
    }
    return true;
}

/// The layer's gids, from a CSV array or uncompressed base64.
bool read_gids(json::Ref layer, Fields& f, std::size_t cells, std::vector<std::uint32_t>& gids) {
    const std::string encoding = f.string("encoding", "csv");
    const std::string compression = f.string("compression", "");
    if (!f.ok()) return false;
    if (!compression.empty()) {
        return f.refuse("compressed layer data (" + compression + ") isn't supported: save the layer format as CSV " +
                        "(Map > Map Properties > Tile Layer Format)");
    }
    const json::Ref data = layer.get("data");
    const std::string count_problem = "the layer data must have " + std::to_string(cells) + " cells (width x height)";
    if (encoding == "base64") {
        std::string bytes;
        if (!data.is_string() || !decode_base64(data.string(), bytes)) return f.refuse("\"data\" is not valid base64");
        if (bytes.size() != cells * 4) return f.refuse(count_problem);
        for (std::size_t i = 0; i < cells; ++i) {
            std::uint32_t gid = 0;
            for (int k = 3; k >= 0; --k) gid = gid << 8 | static_cast<unsigned char>(bytes[i * 4 + static_cast<std::size_t>(k)]);
            gids.push_back(gid);
        }
        return true;
    }
    if (encoding != "csv") return f.refuse("unknown layer encoding \"" + encoding + "\"");
    if (!data.is_array()) return f.refuse("\"data\" must be an array of tile gids");
    if (static_cast<std::size_t>(data.size()) != cells) return f.refuse(count_problem);
    for (json::Ref v = data.first(); v; v = v.next()) {
        const double n = v.number();
        if (!v.is_number() || std::floor(n) != n || n < 0 || n > 4294967295.0) {
            return f.refuse("tile gids must be whole numbers from 0 to 4294967295");
        }
        gids.push_back(static_cast<std::uint32_t>(n));
    }
    return true;
}

/// One gid → an NNHGE cell; false with a reason.
bool to_cell(std::uint32_t gid, const Map& map, int x, int y, Fields& f, std::uint16_t& cell) {
    const std::string at = "cell (" + std::to_string(x) + ", " + std::to_string(y) + ")";
    if ((gid & kHexRotation) != 0) return f.refuse(at + " has the hexagonal 120-degree rotation bit");
    const std::uint32_t id = gid & kGidMask;
    if (id == 0) {
        cell = 0;
        return true;
    }
    const Tileset* set = nullptr;
    for (const Tileset& s : map.tilesets) {
        if (static_cast<std::uint32_t>(s.first_gid) <= id) set = &s;
    }
    if (set == nullptr || id - static_cast<std::uint32_t>(set->first_gid) >= static_cast<std::uint32_t>(set->tile_count)) {
        return f.refuse(at + " uses tile gid " + std::to_string(id) + ", which is in no tileset");
    }
    const bool diagonal = (gid & kDiagonal) != 0;
    if (diagonal && map.tile_width != map.tile_height) {
        return f.refuse(at + " is flipped diagonally, which needs square tiles");
    }
    const int ours = set->first_id + static_cast<int>(id - static_cast<std::uint32_t>(set->first_gid));
    cell = fmt::encode_cell(ours, (gid & kFlipX) != 0, (gid & kFlipY) != 0, diagonal);
    return true;
}

bool read_settings(json::Ref layer, Fields& f, TileLayer& out) {
    if (layer.get("chunks")) return f.refuse("chunked layer data (infinite maps) isn't supported");
    if (layer.get("tintcolor")) return f.refuse("layer tint colours aren't supported: remove the tint");
    out.offset_x = static_cast<float>(f.number("offsetx", 0, -1e9, 1e9));
    out.offset_y = static_cast<float>(f.number("offsety", 0, -1e9, 1e9));
    out.parallax_x = static_cast<float>(f.number("parallaxx", 1, -1e6, 1e6));
    out.parallax_y = static_cast<float>(f.number("parallaxy", 1, -1e6, 1e6));
    out.opacity = static_cast<float>(f.number("opacity", 1, 0, 1));
    out.visible = f.boolean("visible", true);
    return f.ok();
}

} // namespace

bool read_tile_layer(json::Ref layer, const Map& map, const std::string& file, Context& ctx, TileLayer& out) {
    out.name = std::string(layer.get("name").string());
    const std::string where = "layer \"" + out.name + "\"";
    Fields f(layer, where);
    if (out.name.size() > static_cast<std::size_t>(fmt::kMaxStringLength)) {
        return ctx.fail(file, "a layer name is longer than 255 bytes");
    }
    const int w = static_cast<int>(f.integer("width", map.width, 1, fmt::kMaxMapSize));
    const int h = static_cast<int>(f.integer("height", map.height, 1, fmt::kMaxMapSize));
    if (f.ok() && (w != map.width || h != map.height)) {
        f.refuse("the layer is " + std::to_string(w) + "x" + std::to_string(h) + " but the map is " +
                 std::to_string(map.width) + "x" + std::to_string(map.height));
    }
    if (!f.ok() || !read_settings(layer, f, out)) return ctx.fail(file, f.error());
    std::string error;
    if (!read_properties(layer.get("properties"), where, out.properties, error) ||
        !take_bool(out.properties, "collision", where, out.collision, error) ||
        !check_collision_layers(out.properties, where, "untick collision instead", error)) {
        return ctx.fail(file, error);
    }
    std::vector<std::uint32_t> gids;
    const std::size_t cells = static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.height);
    if (!read_gids(layer, f, cells, gids)) return ctx.fail(file, f.error());
    out.cells.resize(cells);
    for (std::size_t i = 0; i < cells; ++i) {
        const int x = static_cast<int>(i % static_cast<std::size_t>(map.width));
        const int y = static_cast<int>(i / static_cast<std::size_t>(map.width));
        if (!to_cell(gids[i], map, x, y, f, out.cells[i])) return ctx.fail(file, f.error());
    }
    return true;
}

} // namespace nnh::assetc
