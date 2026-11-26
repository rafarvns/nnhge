// The game's read-only assets (nnh/assets.hpp): the index read at init, path rules and lookups in
// core, file access through the platform contract (nnh/platform/assets.hpp).

#include "nnh/assets.hpp"

#include <cstdio>

#include "assets/asset_index.hpp"
#include "assets/asset_path.hpp"
#include "assets/assets_internal.hpp"
#include "log/engine_channels.hpp"
#include "nnh/build.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/assets.hpp"
#include "reload/reload_assets.hpp"

namespace nnh {

NNH_MEM_TAG(assets);

/// Builds an AssetData around engine memory (friend of AssetData).
struct AssetDataAccess {
    static AssetData make(void* data, std::size_t size) {
        AssetData a;
        a.data_ = data;
        a.size_ = size;
        return a;
    }
};

namespace {

bool g_ready = false;   // between assets_init() and assets_shutdown()
bool g_mounted = false;
detail::AssetIndex g_index;
char* g_index_text = nullptr;
const char** g_index_entries = nullptr;

void free_index() {
    mem::free(g_index_text);
    mem::free(g_index_entries);
    g_index_text = nullptr;
    g_index_entries = nullptr;
    g_index = detail::AssetIndex{};
}

void read_index() {
    std::size_t size = 0;
    if (!platform::asset_size(detail::kAssetIndexFile, size)) {
        NNH_LOG_WARN(assets, "assets found but no %s index: rebuild the game", detail::kAssetIndexFile);
        return;
    }
    if (size > detail::kMaxAssetIndexBytes) {
        NNH_LOG_WARN(assets, "asset index is %zu bytes (max %zu): assets disabled", size, detail::kMaxAssetIndexBytes);
        return;
    }
    g_index_text = static_cast<char*>(mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(assets), size + 1));
    std::size_t read = 0;
    if (g_index_text == nullptr || !platform::asset_read(detail::kAssetIndexFile, g_index_text, size, read) ||
        read != size) {
        NNH_LOG_WARN(assets, "asset index could not be read: assets disabled");
        free_index();
        return;
    }
    const int lines = detail::AssetIndex::count_lines(g_index_text, size);
    if (lines > 0) {
        g_index_entries = static_cast<const char**>(mem::alloc(
            MemRegion::Heap, NNH_MEM_TAG_REF(assets), sizeof(const char*) * static_cast<std::size_t>(lines)));
    }
    if ((lines > 0 && g_index_entries == nullptr) || !g_index.parse(g_index_text, size, g_index_entries, lines)) {
        NNH_LOG_WARN(assets, "asset index is invalid: assets disabled");
        free_index();
        return;
    }
    // The entries point into the text: keep the text, drop nothing.
}

/// Checks `path` and finds it. On failure returns the error to report.
Result<const char*> locate(const char* path) {
    if (!g_ready) return Error::make(ErrorCode::Unsupported, "assets used before nnh::init()");
    if (!detail::valid_asset_path(path)) {
        return Error::make(ErrorCode::InvalidArgument, "invalid asset path \"%.40s\"", path != nullptr ? path : "");
    }
    if constexpr (build::kDevToolsEnabled) { // hot reload (spec 031): the newest index and the received files
        if (detail::g_reload_assets_active) {
            detail::AssetRedirect r;
            if (detail::reload_asset_lookup(path, r)) return r.stored;
            if (const char* hint = detail::reload_asset_case_hint(path)) {
                NNH_LOG_WARN(assets, "asset \"%s\" not found; paths are case-sensitive: did you mean \"%s\"?", path, hint);
            }
            return Error::make(ErrorCode::NotFound, "asset \"%.44s\" not found", path);
        }
    }
    if (const char* stored = g_index.find(path)) return stored;
    if (const char* hint = g_index.find_ignoring_case(path)) {
        NNH_LOG_WARN(assets, "asset \"%s\" not found; paths are case-sensitive: did you mean \"%s\"?", path, hint);
    }
    return Error::make(ErrorCode::NotFound, "asset \"%.44s\" not found", path);
}

} // namespace

MemTag& detail::assets_memory_tag() { return NNH_MEM_TAG_REF(assets); }

void detail::assets_init() {
    free_index();
    g_mounted = platform::assets_mount();
    if (g_mounted) read_index();
    g_ready = true;
    if constexpr (build::kDevToolsEnabled) detail::g_reload_original_index = &g_index; // spec 031
}

void detail::assets_shutdown() {
    free_index();
    if (g_mounted) platform::assets_unmount();
    g_mounted = false;
    g_ready = false;
}

const char* detail::asset_lookup(const char* path) {
    if constexpr (build::kDevToolsEnabled) {
        if (detail::g_reload_assets_active) {
            detail::AssetRedirect r;
            return g_ready && valid_asset_path(path) && detail::reload_asset_lookup(path, r) ? r.stored : nullptr;
        }
    }
    return g_ready && valid_asset_path(path) ? g_index.find(path) : nullptr;
}

AssetData::AssetData(AssetData&& other) noexcept : data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
}

AssetData& AssetData::operator=(AssetData&& other) noexcept {
    if (this != &other) {
        reset();
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

void AssetData::reset() {
    mem::free(data_);
    data_ = nullptr;
    size_ = 0;
}

bool assets::exists(const char* path) { return detail::asset_lookup(path) != nullptr; }

Result<std::size_t> assets::size(const char* path) {
    auto found = locate(path);
    if (!found) return found.error();
    std::size_t size = 0;
    if constexpr (build::kDevToolsEnabled) {
        detail::AssetRedirect r;
        if (detail::g_reload_assets_active && detail::reload_asset_lookup(path, r) && r.file != 0) return r.size;
    }
    if (!platform::asset_size(found.value(), size)) {
        return Error::make(ErrorCode::IoError, "asset \"%.44s\" can't be opened", path);
    }
    return size;
}

Result<std::size_t> assets::read(const char* path, void* buffer, std::size_t capacity) {
    auto size = assets::size(path);
    if (!size) return size.error();
    if (capacity < size.value() || (buffer == nullptr && size.value() > 0)) {
        return Error::make(ErrorCode::LimitReached, "asset needs %zu bytes, buffer has %zu", size.value(), capacity);
    }
    std::size_t read = 0;
    if constexpr (build::kDevToolsEnabled) {
        detail::AssetRedirect r;
        if (detail::g_reload_assets_active && detail::reload_asset_lookup(path, r) && r.file != 0) {
            if (!detail::reload_read_file(r, buffer, read) || read != size.value()) {
                return Error::make(ErrorCode::IoError, "asset \"%.44s\" could not be read", path);
            }
            return read;
        }
    }
    if (!platform::asset_read(path, buffer, capacity, read) || read != size.value()) {
        return Error::make(ErrorCode::IoError, "asset \"%.44s\" could not be read", path);
    }
    return read;
}

Result<AssetData> assets::load(const char* path) {
    auto size = assets::size(path);
    if (!size) return size.error();
    // At least one byte, so an empty asset is still a valid AssetData.
    void* data = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(assets), size.value() > 0 ? size.value() : 1);
    if (data == nullptr) return Error::make(ErrorCode::OutOfMemory, "no memory for asset (%zu bytes)", size.value());
    AssetData owned = AssetDataAccess::make(data, size.value());
    auto read = assets::read(path, data, size.value());
    if (!read) return read.error();
    return owned;
}

} // namespace nnh
