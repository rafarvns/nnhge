// Fake platform: hot reload's two stream opens (contract nnh/platform/reload.hpp, spec 031). A received file is a copy
// of the in-memory data folder's file taken at the open (as an open file handle keeps reading what it opened); a shared
// open is asset_open() counted apart. Test-only library, so std containers are fine here.

#include <vector>

#include "fake_internal.hpp"
#include "nnh/platform/fs.hpp"
#include "nnh/platform/reload.hpp"

namespace nnh::platform {
namespace {

using platform_fake::detail::FakeAsset;

constexpr int kStreams = 32;

struct Slot {
    FakeAsset asset;
    std::vector<unsigned char> bytes;
    bool used = false;
};
Slot g_slots[kStreams];

} // namespace

bool reload_stream_open(const char* data_path, AssetStream& out) {
    out = 0;
    for (Slot& s : g_slots) {
        if (s.used) continue;
        const long long size = platform_fake::detail::fs_copy(data_path, nullptr, 0);
        if (size < 0) return false;
        s.bytes.resize(static_cast<std::size_t>(size));
        platform_fake::detail::fs_copy(data_path, s.bytes.data(), s.bytes.size());
        s.used = true;
        s.asset = {data_path, s.bytes.data(), s.bytes.size()};
        out = reinterpret_cast<AssetStream>(&s.asset);
        ++platform_fake::detail::mutable_state().reload_stream_opens;
        ++platform_fake::detail::mutable_state().open_asset_streams;
        return true;
    }
    return false;
}

bool asset_open_shared(const char* path, AssetStream& out) {
    if (!asset_open(path, out)) return false;
    ++platform_fake::detail::mutable_state().shared_asset_opens;
    return true;
}

} // namespace nnh::platform

namespace nnh::platform_fake::detail {

void reload_stream_closed(const FakeAsset* asset) {
    for (auto& s : platform::g_slots) {
        if (s.used && &s.asset == asset) {
            s.used = false;
            s.bytes.clear();
            s.bytes.shrink_to_fit();
        }
    }
}

void reload_reset() {
    for (auto& s : platform::g_slots) {
        s.used = false;
        s.bytes.clear();
        s.bytes.shrink_to_fit();
    }
}

} // namespace nnh::platform_fake::detail
