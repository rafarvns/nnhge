#pragma once
/// @file nnh/platform_fake.hpp
/// Controls and recordings of the fake platform (nnh::platform_fake), used by unit and conformance
/// tests to drive nnh::core without hardware. Not available to games.

#include <cstdint>
#include <initializer_list>

#include "nnh/gfx.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/audio.hpp"
#include "nnh/platform/crash.hpp"
#include "nnh/platform/video.hpp"
#include "nnh/system.hpp"
#include "nnh/time.hpp"

namespace nnh::platform_fake {

inline constexpr int kMaxRecordedItems = 64;
inline constexpr int kMaxRecordedCalls = 32;
inline constexpr int kMaxFakeVoices = 24;

/// What a voice of the fake audio output is doing (spec 008).
struct FakeVoice {
    bool playing = false;
    bool paused = false;
    bool loop = false;
    platform::SampleHandle sample = 0;
    std::uint32_t count = 0; ///< frames
    int channels = 0;
    platform::VoiceParams params{};
    int starts = 0; ///< voice_start() calls on this voice
    int stops = 0;  ///< voice_stop() calls
};

/// Everything the fake platform recorded since the last reset().
struct State {
    int init_calls = 0;
    int shutdown_calls = 0;
    int frames_begun = 0;
    int frames_ended = 0;
    bool initialized = false;
    bool frame_open = false;
    Color top_clear{};
    Color bottom_clear{};
    int console_lines = 0;                          ///< log lines written with log_console()
    char last_console_line[log::kMaxLineLength] = {}; ///< the latest of them
    int vram_writes_by_core = 0; ///< Vram blocks found modified when freed (must stay 0)
    int fs_calls = 0;            ///< calls into the fs contract (invalid paths must not add any)
    // 2D drawing (spec 004)
    int live_textures = 0;      ///< created and not yet destroyed
    int rgba_textures_created = 0; ///< texture_create_rgba() calls that succeeded
    int asset_reads = 0;        ///< asset_size() + asset_read() calls (invalid paths must not add any)
    int item_count = 0;         ///< draw_2d() items in the current frame (first kMaxRecordedItems kept)
    platform::DrawItem2D items[kMaxRecordedItems];
    char calls[kMaxRecordedCalls] = {}; ///< current frame: 'T'/'B' = draw_2d() on the top/bottom screen
    int last_frame_item_count = 0;      ///< the same, for the frame that ended last
    platform::DrawItem2D last_frame_items[kMaxRecordedItems];
    char last_frame_calls[kMaxRecordedCalls] = {};
    // Audio (spec 008)
    bool audio_initialized = false;
    int audio_voices = 0;    ///< voices asked for by audio_init()
    int live_samples = 0;    ///< created and not yet destroyed
    int samples_ready = 0;   ///< sample_ready() calls
    FakeVoice voices[kMaxFakeVoices];
};

const State& state();

/// Forgets recordings and controls (call between tests while the engine is shut down).
void reset();

/// Buttons that read_input() reports from now on (mask of nnh::Button bits).
void set_held_buttons(std::uint32_t mask);

/// Stick readings read_input() reports from now on (platform scale: about [-1, 1], y up, no dead zone).
void set_circle_pad(float x, float y);
void set_c_stick(float x, float y);

/// The bottom screen is touched at (x, y) from now on, until release_touch().
void set_touch(int x, int y);
void release_touch();

/// Adds a read-only asset (spec 007): `data` must stay alive until reset(). The game "has assets" (mount
/// succeeds) once one is added. Paths are matched exactly, case included.
void add_asset(const char* path, const void* data, std::size_t size);

/// When false, audio_init() fails like a system without audio output (default true).
void set_audio_available(bool available);

/// A non-looping voice reached its end: voice_playing() returns false from now on.
void finish_voice(int voice);

/// The frames core wrote into a sample (interleaved), or nullptr for a stale handle.
const std::int16_t* sample_frames(platform::SampleHandle sample);

/// When true, poll_events() returns false (the system asks the app to quit).
void set_quit_requested(bool quit);

/// When true, init() fails.
void set_init_fails(bool fails);

/// Sets the manual clock returned by ticks_ns() (starts at 0 after reset()).
void set_time_ns(std::int64_t ns);

/// Moves the manual clock forward.
void advance_time(Duration elapsed);

/// Bytes region_alloc() may hand out in a region (defaults: heap 64 MB, linear 32 MB, vram 6 MB).
void set_region_capacity(MemRegion region, std::size_t bytes);

/// The next `count` fs_write() calls fail after writing half the data (a torn write, e.g. power loss).
/// Files live in memory and survive nnh::shutdown()/init() until reset().
void fail_next_writes(int count);

/// Flips the bits of one byte of a stored file (simulates corruption on the SD card).
void corrupt_file(const char* path, std::size_t byte_index);

/// Capabilities reported by the platform (default: SecondScreen only).
void set_capabilities(std::initializer_list<Capability> capabilities);

/// Makes the next poll_dev_requests() ask to toggle the debug overlay (like F1 on PC).
void request_overlay_toggle();

/// The last crash screen requested through crash_show().
struct Crash {
    char top[2048] = {};
    char bottom[2048] = {};
};

/// Runs `fn`; if it reaches crash_show(), the crash is recorded (last_crash()) and control returns
/// here instead of ending the process. @return true if a crash happened.
bool run_expecting_crash(void (*fn)());
const Crash& last_crash();

/// Calls the CPU fault handler core installed with crash_install_hooks(), as if the CPU faulted.
/// Use inside run_expecting_crash().
void simulate_cpu_fault(const platform::CpuFault& fault);

} // namespace nnh::platform_fake
