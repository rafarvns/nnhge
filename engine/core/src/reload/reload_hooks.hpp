#pragma once
// The hooks that keep hot reload opt-in by linking (spec 031, plan question 9's alternative, taken at stage 5 to bring
// the Debug/Development `hello` back near the approved growth): core's Release-linked files (app.cpp, assets.cpp,
// music.cpp, data_index.cpp) and the remote session reach the reload core only through these pointers, which are null
// until a reload kind is registered (the first load_texture, load_sound, load_font, load_tilemap, loc::start or data
// index of a Debug/Development game: reload::install()). A game that loads none of those — e.g. one that only reads
// plain files — links no reload core: a ReloadBegin is answered Rejected{Type} and `nnh` stops watching, as for a game
// built before spec 031. Definitions in reload_hooks.cpp (tiny); Release references none of them.

#include <cstddef>
#include <cstdint>

#include "nnh/platform/assets.hpp"
#include "remote/remote_wire.hpp"

namespace nnh::detail {

class AssetIndex;
struct AssetRedirect;

/// The session's side (remote_reload.cpp).
struct ReloadSessionOps {
    void (*message)(const remote::Message& m, std::uint32_t frame);
    void (*payload)(const std::uint8_t* data, std::size_t size, std::uint32_t frame);
    void (*apply)(const remote::Message& m, std::uint32_t frame);
    std::uint8_t* (*window)(std::size_t& room);
    void (*flush)();
    void (*detached)();
};

/// Reads through the overlay (reload_assets.cpp).
struct ReloadAssetOps {
    bool (*lookup)(const char* path, AssetRedirect& out);
    const char* (*case_hint)(const char* path);
    bool (*read_file)(const AssetRedirect& file, void* buffer, std::size_t& read);
    bool (*open)(const char* path, platform::AssetStream& out, bool shared);
};

/// Null until reload::install().
extern const ReloadSessionOps* g_reload_session;
extern const ReloadAssetOps* g_reload_asset_ops;
/// Set while a batch waits for its frame; app.cpp calls it at the apply point.
extern void (*g_reload_apply)(std::uint32_t frame);
/// True from the first applied batch to shutdown: assets.cpp asks g_reload_asset_ops instead of its own index.
extern bool g_reload_assets_active;
/// The assets module's own index (set by assets_init() in Debug/Development builds).
extern const AssetIndex* g_reload_original_index;

namespace reload {
/// Links and turns on the reload core (the kinds call it through register_kind()). Idempotent.
void install();
} // namespace reload

} // namespace nnh::detail
