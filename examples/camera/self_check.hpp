#pragma once
// The camera example's self-check (spec 018 AC-2, AC-6): a fixed 600-update script on a real nnh::CameraRig —
// a target path with jumps and stops, a dead zone, smoothing, limits changed mid-way, trauma, zoom changes
// and the tall-world layout. An FNV-1a checksum takes the bits of followed() and position() after every
// update; it must be the same on every compiler and on the 3DS. Pure code (no drawing, no input, no engine),
// so the unit tests run it (tests/camera_example/test_self_check.cpp).

#include <cstddef>
#include <cstdint>
#include <nnh/camera.hpp>

/// The checksum the simulator recorded for the whole script. A different value on any system is a determinism
/// hole (or a change of the script or of the camera module: then record the new value from pc-debug, and the
/// checkpoint lines of examples/camera/tests/check.nnhs).
inline constexpr std::uint32_t kCameraCheckGolden = 0x5201E6CFu; // pc-debug, WinLibs GCC 16

class CameraSelfCheck {
  public:
    static constexpr int kFrames = 600;
    static constexpr int kCheckpointEvery = 60;

    CameraSelfCheck();

    /// Plays one update. @return true when that frame is a checkpoint (every 60 frames, the last included).
    bool update();
    bool done() const { return frame_ >= kFrames; }
    int frame() const { return frame_; }
    std::uint32_t checksum() const { return sum_; }
    const nnh::CameraRig& rig() const { return rig_; }

  private:
    void mix(float value);
    void script_frame(); ///< settings the script changes at fixed frames
    nnh::Vec2 target() const;

    nnh::CameraRig rig_;
    std::uint32_t sum_;
    int frame_ = 0;
};

/// "camera: <frame> sum XXXXXXXX" (a checkpoint log line).
void format_camera_checkpoint(char* out, std::size_t size, int frame, std::uint32_t sum);
/// "camera: PASS XXXXXXXX", or "camera: FAIL sum XXXXXXXX expected XXXXXXXX".
void format_camera_verdict(char* out, std::size_t size, std::uint32_t sum);
