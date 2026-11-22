#include "self_check.hpp"

#include <bit>
#include <cstdio>

namespace {

constexpr std::uint32_t kFnvStart = 2166136261u;
constexpr std::uint32_t kFnvPrime = 16777619u;

nnh::CameraSettings start_settings() {
    return {.dead_zone = nnh::CameraSettings::centered_zone(48, 32),
            .smoothing = {0.25f, 0.125f},
            .max_shake = {10, 7},
            .trauma_decay = 1.0f / 90,
            .shake_speed = 0.37f,
            .seed = 0x0018CA3Eu};
}

} // namespace

CameraSelfCheck::CameraSelfCheck() : rig_(start_settings()), sum_(kFnvStart) {
    rig_.set_limits({0, 0, 1280, 768});
    rig_.snap(target());
}

nnh::Vec2 CameraSelfCheck::target() const {
    // Runs right with small steps, stops, jumps (teleports) at 200, walks back up and left, stops again.
    // Every factor is a short binary fraction, so the path itself is exact (no rounding the compiler could
    // fuse).
    const float t = static_cast<float>(frame_);
    if (frame_ < 120) return {300 + t * 3.25f, 200 + (frame_ % 40 < 20 ? 0.5f : -0.5f) * t};
    if (frame_ < 160) return {690, 260}; // stop
    if (frame_ < 200) return {690 + (t - 160) * 7.125f, 260 - (t - 160) * 1.375f};
    if (frame_ < 330) return {150 + (t - 200) * 1.75f, 600 - (t - 200) * 2.875f}; // after a jump
    if (frame_ < 400) return {371.5f, 223.25f};
    return {1200 - (t - 400) * 4.375f, 100 + (t - 400) * 3.375f};
}

void CameraSelfCheck::script_frame() {
    switch (frame_) {
    case 60:
        rig_.add_trauma(0.6f);
        break;
    case 140:
        rig_.set_zoom(2);
        break;
    case 200:
        rig_.snap(target());
        break; // the jump: no glide
    case 230:
        rig_.set_limits({0, 0, 640, 384});
        break; // a smaller room: the camera glides into it
    case 260:
        rig_.add_trauma(1);
        break;
    case 300:
        rig_.set_layout(nnh::CameraLayout::TallWorld);
        break;
    case 320:
        rig_.set_zoom(0.5f);
        break;
    case 360:
        rig_.set_offset({24, -8});
        break;
    case 400:
        rig_.set_limits({0, 0, 1280, 768});
        break;
    case 450:
        rig_.set_zoom(1.5f);
        break;
    case 470:
        rig_.add_trauma(0.45f);
        break;
    case 520:
        rig_.set_layout(nnh::CameraLayout::Single, nnh::Screen::Bottom);
        break;
    case 560:
        rig_.clear_limits();
        break;
    default:
        break;
    }
}

void CameraSelfCheck::mix(float value) {
    const std::uint32_t word = std::bit_cast<std::uint32_t>(value);
    for (int k = 0; k < 4; ++k)
        sum_ = (sum_ ^ ((word >> (8 * k)) & 0xFFu)) * kFnvPrime;
}

bool CameraSelfCheck::update() {
    if (done()) return false;
    script_frame();
    rig_.update(target());
    ++frame_;
    mix(rig_.followed().x);
    mix(rig_.followed().y);
    mix(rig_.position().x);
    mix(rig_.position().y);
    mix(rig_.trauma());
    return frame_ % kCheckpointEvery == 0;
}

void format_camera_checkpoint(char* out, std::size_t size, int frame, std::uint32_t sum) {
    std::snprintf(out, size, "camera: %d sum %08X", frame, static_cast<unsigned>(sum));
}

void format_camera_verdict(char* out, std::size_t size, std::uint32_t sum) {
    if (sum == kCameraCheckGolden) {
        std::snprintf(out, size, "camera: PASS %08X", static_cast<unsigned>(sum));
    } else {
        std::snprintf(out, size, "camera: FAIL sum %08X expected %08X", static_cast<unsigned>(sum),
                      static_cast<unsigned>(kCameraCheckGolden));
    }
}
