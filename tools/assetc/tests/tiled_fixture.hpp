#pragma once
// Shared by nnh-assetc's tests: PNG headers, files in memory under a fake assets folder "/game/assets", and short
// ways to run each conversion step on JSON text.

#include <cstdint>
#include <string>

#include "convert_context.hpp"
#include "files.hpp"
#include "map_model.hpp"

namespace tiled_fixture {

inline constexpr const char* kRoot = "/game/assets";

/// The 33 bytes a PNG starts with (signature + IHDR chunk) for a `width` × `height` RGBA image.
std::string png(std::uint32_t width, std::uint32_t height);

bool write_file(const char* path, const std::string& bytes);

/// Files under kRoot plus a context reading them; `problem()` is "file: reason" of the last failure.
struct Files {
    nnh::assetc::MemoryFiles memory;
    nnh::assetc::Context ctx{memory, kRoot, {}, {}};

    /// Adds kRoot + "/" + `name`.
    void add(const std::string& name, std::string bytes) { memory.add(std::string(kRoot) + "/" + name, std::move(bytes)); }
    std::string problem() const { return ctx.problem.file + ": " + ctx.problem.reason; }
    bool mentions(const char* part) const { return problem().find(part) != std::string::npos; }
};

/// Reads the tileset entry `entry_json` (one element of a map's "tilesets") of the map kRoot/levels/map.tmj with
/// tiles of `tile_w` × `tile_h`.
bool read_tileset(Files& files, const std::string& entry_json, nnh::assetc::Tileset& out, int tile_w = 16,
                  int tile_h = 16);

/// Converts kRoot/levels/map.tmj (`map_json`) into a model.
bool read_map(Files& files, const std::string& map_json, nnh::assetc::Map& out);

/// A valid orthogonal 4x2 map of 16x16 tiles with no layers: insert members with `extra` (",\"layers\": [...]").
std::string map_json(const std::string& extra = "");

} // namespace tiled_fixture
