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
inline constexpr int kMaxFakeAssets = 32;

struct Controls {
    platform::InputSample input; // what read_input() reports
    bool quit_requested = false;
    bool init_fails = false;
    std::int64_t time_ns = 0; // manual clock read by ticks_ns()
    std::size_t region_capacity[kMemRegionCount] = {64u << 20, 32u << 20, 6u << 20};
    std::size_t region_used[kMemRegionCount] = {};
    std::uint32_t capabilities = 1u << static_cast<unsigned>(Capability::SecondScreen);
    std::uint32_t dev_requests = 0;
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
void set_last_error(const char* message); // app_fake.cpp

} // namespace nnh::platform_fake::detail
