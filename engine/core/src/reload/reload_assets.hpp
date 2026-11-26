#pragma once
// Asset reads through hot reload (spec 031 D53, FR-5, FR-12): what the assets module, music and the data index consult
// in Debug/Development builds. Once the first batch is staged (g_reload_assets_active), a path is looked up in this
// order: the files of the batch being prepared (puts found, removes not), the current asset index (the newest batch's
// `.nnh_assets`, else the original), and the overlay (a received file replaces the packed one). Release references
// none of it (every use sits in a discarded `if constexpr` branch). Tests: tests/assets/test_assets_overlay.cpp.

#include <cstddef>
#include <cstdint>

#include "nnh/platform/assets.hpp"

namespace nnh::detail {

class AssetIndex;

/// What the reload layer says about an asset path it knows.
struct AssetRedirect {
    const char* stored = nullptr; ///< the path as the current index (or the batch) stores it
    std::uint16_t file = 0;       ///< a received file replaces the packed asset (0: read the packed one)
    std::uint32_t size = 0;       ///< that file's bytes
};

/// True from the first staged batch to shutdown: assets.cpp asks reload_asset_lookup() instead of its own index.
extern bool g_reload_assets_active;
/// The assets module's own index (set by assets_init() in Debug/Development builds).
extern const AssetIndex* g_reload_original_index;

/// @return false when `path` (a valid asset path) is not an asset now (NotFound).
bool reload_asset_lookup(const char* path, AssetRedirect& out);
/// The stored path equal to `path` ignoring case, for the not-found hint (current index), or nullptr.
const char* reload_asset_case_hint(const char* path);
/// Reads a received file whole into `buffer` (its size checked by the caller). @return false on failure.
bool reload_read_file(const AssetRedirect& file, void* buffer, std::size_t& read);
/// Opens an asset as a stream: a received file when the overlay has one, else the packed asset (`shared`: so the
/// build may rewrite it meanwhile, nnh/platform/reload.hpp). For music and the data index, Debug/Development only.
bool reload_asset_open(const char* path, platform::AssetStream& out, bool shared);

namespace reload {

struct BatchFile;
class OverlayTable;

/// The batch being prepared (null when none): its puts are found and its removes are gone for every read.
void set_staging(const BatchFile* files, int count);
/// The index lookups use (null: the original). Owned by the asset-index kind (reload_index.cpp).
void set_current_index(const AssetIndex* index);
const AssetIndex* current_index();
/// The overlay table, allocated at the first batch (tag `reload`). Null without memory.
OverlayTable* overlay();
/// Turns the reload layer on (the first staged batch). False without memory for the overlay.
bool activate();
/// Frees the overlay and turns the layer off (shutdown).
void deactivate();

} // namespace reload
} // namespace nnh::detail
