#pragma once
// Mutable state shared by the fake platform files. Internal to engine/platforms/fake/.

#include <cstddef>
#include <cstdint>

#include "nnh/memory.hpp"
#include "nnh/system.hpp"

#include "nnh/platform/input.hpp"
#include "nnh/platform/video.hpp"
#include "nnh/platform_fake.hpp"

namespace nnh::platform_fake::detail {

struct FakeAsset {
    const char* path = nullptr; // points into the caller's string literal or storage
    const void* data = nullptr;
    std::size_t size = 0;
};
inline constexpr int kMaxFakeAssets = 64;

struct Controls {
    platform::InputSample input; // what read_input() reports
    bool quit_requested = false;
    bool init_fails = false;
    std::int64_t time_ns = 0; // manual clock read by ticks_ns()
    std::size_t region_capacity[kMemRegionCount] = {64u << 20, 32u << 20, 6u << 20};
    std::size_t region_used[kMemRegionCount] = {};
    int alloc_fill = -1; // set_alloc_fill(): the byte new Heap/Linear blocks get, or −1
    std::uint32_t capabilities = 1u << static_cast<unsigned>(Capability::SecondScreen);
    std::uint32_t dev_requests = 0;
    std::uint32_t dev_queue[64] = {}; // queue_dev_requests(): one mask per poll_dev_requests() (spec 030)
    int dev_queue_count = 0;
    int dev_queue_next = 0;
    int quit_after_polls = -1; // quit_after_polls(): poll_events() calls left before it returns false
    float stereo_slider = 0; // set_stereo_slider()
    FakeAsset assets[kMaxFakeAssets];
    int asset_count = 0;
    bool audio_unavailable = false;
};

State& mutable_state();
Controls& controls();
void fs_reset(); // fs_fake.cpp
void remote_reset(); // remote_fake.cpp
void video3d_reset(); // video3d_fake.cpp
void video3d_frame_begun(); // video3d_fake.cpp: begin_frame() starts a new 3D log
void video3d_frame_ended(); // video3d_fake.cpp
/// video3d_fake.cpp: a texture was created (rgba null: the "FAKE" format, filled with a gradient) or destroyed, for the
/// 3D pixel tests.
void texture_recorded(platform::TextureHandle handle, int width, int height, const void* rgba);
void texture_forgotten(platform::TextureHandle handle);
void language_reset();                                       // language_fake.cpp
void alpha_reset();                                          // texture_alpha_fake.cpp
void alpha_texture_forgotten(platform::TextureHandle handle); // texture_alpha_fake.cpp
void set_last_error(const char* message); // app_fake.cpp
/// fs_fake.cpp (spec 031): a copy of a stored file's bytes into `out` (capacity `capacity`); its size, or -1 if missing
/// (or longer than `capacity`). With `out` null: only its size.
long long fs_copy(const char* path, unsigned char* out, std::size_t capacity);
/// reload_fake.cpp (spec 031): asset_close() of a stream reload_stream_open() opened frees its slot.
void reload_stream_closed(const FakeAsset* asset);
void reload_reset();

} // namespace nnh::platform_fake::detail
