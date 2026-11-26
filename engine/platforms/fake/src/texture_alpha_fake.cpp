// Fake platform: coverage textures (contract nnh/platform/video.hpp texture_create_alpha, spec 029 D49). A copy of the
// coverage per handle, so tests can read the pages a font load made; bytes = width × height, as on the console.

#include <cstring>
#include <map>
#include <vector>

#include "fake_internal.hpp"
#include "nnh/platform/video.hpp"

namespace nnh {
namespace {

struct Coverage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bytes;
};

std::map<platform::TextureHandle, Coverage>& coverages() {
    static std::map<platform::TextureHandle, Coverage> map;
    return map;
}

} // namespace

bool platform::texture_create_alpha(const void* coverage, int width, int height, TextureInfo& out) {
    out = TextureInfo{};
    if (coverage == nullptr || width <= 0 || height <= 0) return false;
    static TextureHandle next_handle = 0xA00000;
    out.handle = ++next_handle;
    out.width = width;
    out.height = height;
    out.bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    Coverage& c = coverages()[out.handle];
    c.width = width;
    c.height = height;
    c.bytes.assign(static_cast<const std::uint8_t*>(coverage), static_cast<const std::uint8_t*>(coverage) + out.bytes);
    platform_fake::State& s = platform_fake::detail::mutable_state();
    ++s.live_textures;
    ++s.alpha_textures_created;
    s.last_alpha_texture = out.handle;
    return true;
}

const std::uint8_t* platform_fake::alpha_texture_coverage(platform::TextureHandle handle, int& width, int& height) {
    const auto it = coverages().find(handle);
    if (it == coverages().end()) return nullptr;
    width = it->second.width;
    height = it->second.height;
    return it->second.bytes.data();
}

void platform_fake::detail::alpha_reset() { coverages().clear(); }

void platform_fake::detail::alpha_texture_forgotten(platform::TextureHandle handle) { coverages().erase(handle); }

} // namespace nnh
