// Fake platform: frames and drawing (2D passes and tile batches) are recorded, nothing is shown (contract
// nnh/platform/video.hpp).

#include <cstring>

#include "fake_internal.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {

using platform_fake::detail::mutable_state;

namespace {

void log_call(char call) {
    platform_fake::State& s = mutable_state();
    const std::size_t n = std::strlen(s.calls);
    if (n + 1 < sizeof s.calls) {
        s.calls[n] = call;
        s.calls[n + 1] = '\0';
    }
}

std::uint16_t read_u16(const unsigned char* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

} // namespace

void begin_frame(Color top_clear, Color bottom_clear) {
    platform_fake::State& s = mutable_state();
    ++s.frames_begun;
    s.frame_open = true;
    s.top_clear = top_clear;
    s.bottom_clear = bottom_clear;
    s.item_count = 0;
    s.calls[0] = '\0';
    s.tiles.batch_count = 0;
    s.tiles.quad_count = 0;
}

void end_frame() {
    platform_fake::State& s = mutable_state();
    ++s.frames_ended;
    s.frame_open = false;
    s.last_frame_item_count = s.item_count;
    std::memcpy(s.last_frame_items, s.items, sizeof s.items);
    std::memcpy(s.last_frame_calls, s.calls, sizeof s.calls);
    s.last_frame_tiles = s.tiles;
}

// Test image format: "FAKE", width, height (16-bit little endian).
bool texture_create(const void* data, std::size_t size, TextureInfo& out) {
    out = TextureInfo{};
    const auto* bytes = static_cast<const unsigned char*>(data);
    if (size < 8 || std::memcmp(bytes, "FAKE", 4) != 0) return false;
    static TextureHandle next_handle = 0x1000;
    out.handle = ++next_handle;
    out.width = read_u16(bytes + 4);
    out.height = read_u16(bytes + 6);
    out.bytes = static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height) * 4;
    ++mutable_state().live_textures;
    return true;
}

bool texture_create_rgba(const void* pixels, int width, int height, TextureInfo& out) {
    out = TextureInfo{};
    if (pixels == nullptr || width <= 0 || height <= 0) return false;
    static TextureHandle next_handle = 0x80000;
    out.handle = ++next_handle;
    out.width = width;
    out.height = height;
    out.bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    ++mutable_state().live_textures;
    ++mutable_state().rgba_textures_created;
    return true;
}

void texture_destroy(TextureHandle handle) {
    if (handle != 0) --mutable_state().live_textures;
}

void draw_2d(Screen screen, const DrawItem2D* items, int count) {
    platform_fake::State& s = mutable_state();
    log_call(screen == Screen::Top ? 'T' : 'B');
    for (int i = 0; i < count; ++i) {
        if (s.item_count < platform_fake::kMaxRecordedItems) s.items[s.item_count] = items[i];
        ++s.item_count;
    }
}

void draw_tiles(Screen screen, const TileBatch2D& batch) {
    platform_fake::State& s = mutable_state();
    platform_fake::FakeTiles& t = s.tiles;
    log_call(screen == Screen::Top ? 't' : 'b');
    if (t.batch_count < platform_fake::kMaxRecordedTileBatches) {
        platform_fake::FakeTileBatch& recorded = t.batches[t.batch_count];
        recorded.screen = screen;
        recorded.batch = batch;
        recorded.batch.quads = nullptr; // core reuses the buffer
        recorded.first_quad = t.quad_count;
    }
    ++t.batch_count;
    for (int i = 0; i < batch.count; ++i) {
        if (t.quad_count < platform_fake::kMaxRecordedTileQuads) t.quads[t.quad_count] = batch.quads[i];
        ++t.quad_count;
    }
}

} // namespace nnh::platform
