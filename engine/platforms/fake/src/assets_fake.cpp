// Fake platform: assets from the in-memory table filled by tests (contract nnh/platform/assets.hpp).

#include <cstring>

#include "fake_internal.hpp"
#include "nnh/platform/assets.hpp"

namespace nnh::platform {
namespace {

const platform_fake::detail::FakeAsset* find(const char* path) {
    platform_fake::detail::Controls& c = platform_fake::detail::controls();
    for (int i = 0; i < c.asset_count; ++i) {
        if (std::strcmp(c.assets[i].path, path) == 0) return &c.assets[i];
    }
    return nullptr;
}

} // namespace

bool assets_mount() { return platform_fake::detail::controls().asset_count > 0; }

void assets_unmount() {}

bool asset_size(const char* path, std::size_t& out) {
    ++platform_fake::detail::mutable_state().asset_reads;
    const auto* asset = find(path);
    if (asset == nullptr) return false;
    out = asset->size;
    return true;
}

bool asset_read(const char* path, void* buffer, std::size_t capacity, std::size_t& read) {
    ++platform_fake::detail::mutable_state().asset_reads;
    read = 0;
    const auto* asset = find(path);
    if (asset == nullptr || capacity < asset->size) return false;
    if (asset->size > 0) std::memcpy(buffer, asset->data, asset->size);
    read = asset->size;
    return true;
}

} // namespace nnh::platform
