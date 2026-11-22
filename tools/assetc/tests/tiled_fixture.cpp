// Shared helpers of nnh-assetc's tests (see tiled_fixture.hpp).

#include "tiled_fixture.hpp"

#include <cstdio>

#include "json.hpp"
#include "test.hpp"
#include "tiled_map.hpp"
#include "tiled_tileset.hpp"

namespace tiled_fixture {

namespace {

void put_u32(std::string& out, std::uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8) out += static_cast<char>((v >> shift) & 0xFF);
}

} // namespace

std::string png(std::uint32_t width, std::uint32_t height) {
    std::string out("\x89PNG\r\n\x1a\n", 8);
    put_u32(out, 13);
    out += "IHDR";
    put_u32(out, width);
    put_u32(out, height);
    out += std::string("\x08\x06\x00\x00\x00", 5); // 8-bit RGBA, no interlace
    put_u32(out, 0);                               // CRC: not checked by the converter
    return out;
}

bool write_file(const char* path, const std::string& bytes) {
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    return std::fclose(file) == 0 && ok;
}

bool read_tileset(Files& files, const std::string& entry_json, nnh::assetc::Tileset& out, int tile_w, int tile_h) {
    nnh::assetc::json::Document doc;
    CHECK(doc.parse(entry_json));
    out = {};
    return nnh::assetc::read_tileset(doc.root(), std::string(kRoot) + "/levels/map.tmj", tile_w, tile_h, files.ctx,
                                     out);
}

bool read_map(Files& files, const std::string& map_json, nnh::assetc::Map& out) {
    const std::string path = std::string(kRoot) + "/levels/map.tmj";
    files.memory.add(path, map_json);
    return nnh::assetc::read_tiled_map(path, files.ctx, out);
}

std::string map_json(const std::string& extra) {
    return R"({"type": "map", "version": "1.10", "orientation": "orthogonal", "renderorder": "right-down",
               "infinite": false, "width": 4, "height": 2, "tilewidth": 16, "tileheight": 16)" + extra + "}";
}

} // namespace tiled_fixture
