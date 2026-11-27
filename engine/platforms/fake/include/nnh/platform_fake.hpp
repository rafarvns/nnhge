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
#include "nnh/platform/video3d.hpp"
#include "nnh/system.hpp"
#include "nnh/time.hpp"

namespace nnh::platform_fake {

inline constexpr int kMaxRecordedItems = 1024; ///< whole frames of transition overlays (spec 023 AC-2)
inline constexpr int kMaxRecordedCalls = 32;
inline constexpr int kMaxRecordedTileBatches = 16;
inline constexpr int kMaxRecordedTileQuads = 512;

/// A draw_tiles() call: the batch (its `quads` pointer cleared) and where its quads were recorded.
struct FakeTileBatch {
    Screen screen = Screen::Top;
    platform::TileBatch2D batch{}; ///< batch.quads is nullptr; batch.count = quads it had
    int first_quad = 0; ///< index in tile_quads of its first quad (only the first kMaxRecordedTileQuads kept)
};

/// What draw_tiles() received in one frame (spec 012).
struct FakeTiles {
    int batch_count = 0; ///< draw_tiles() calls (first kMaxRecordedTileBatches kept)
    FakeTileBatch batches[kMaxRecordedTileBatches];
    int quad_count = 0; ///< quads over all calls (first kMaxRecordedTileQuads kept)
    platform::TileQuad2D quads[kMaxRecordedTileQuads];
};
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

/// A stream of the fake audio output (music, spec 008).
struct FakeStream {
    bool active = false;
    bool paused = false;
    int channels = 0;
    platform::VoiceParams params{};
    std::uint32_t queued = 0;    ///< frames submitted and not consumed
    std::uint32_t submitted = 0; ///< frames submitted since stream_start()
    int starts = 0;
    int stops = 0;
};

/// Everything the fake platform recorded since the last reset().
struct State {
    int init_calls = 0;
    int shutdown_calls = 0;
    int frames_begun = 0;
    int frames_ended = 0;
    bool initialized = false;
    bool frame_open = false;
    /// A frame ended and the GPU may still draw it: set by end_frame(), cleared by begin_frame() and finish_frames(),
    /// which wait for the GPU on a console (decision D47).
    bool gpu_busy = false;
    int finish_frames_calls = 0;
    int destroyed_while_gpu_busy = 0; ///< textures and meshes destroyed while gpu_busy (must stay 0, D47)
    Color top_clear{};
    Color bottom_clear{};
    int console_lines = 0;                          ///< log lines written with log_console()
    char last_console_line[log::kMaxLineLength] = {}; ///< the latest of them
    int vram_writes_by_core = 0; ///< Vram blocks found modified when freed (must stay 0)
    int fs_calls = 0;            ///< calls into the fs contract (invalid paths must not add any)
    // 2D drawing (spec 004)
    int live_textures = 0;      ///< created and not yet destroyed
    int rgba_textures_created = 0; ///< texture_create_rgba() calls that succeeded
    int alpha_textures_created = 0; ///< texture_create_alpha() calls that succeeded (spec 029: font pages)
    platform::TextureHandle last_alpha_texture = 0; ///< the newest of them
    int asset_reads = 0;        ///< asset_size() + asset_read() calls (invalid paths must not add any)
    int item_count = 0;         ///< draw_2d() items in the current frame (first kMaxRecordedItems kept)
    platform::DrawItem2D items[kMaxRecordedItems];
    /// Current frame: 'T'/'B' = draw_2d() on the top/bottom screen, 't'/'b' = draw_tiles() (spec 012).
    char calls[kMaxRecordedCalls] = {};
    int last_frame_item_count = 0; ///< the same, for the frame that ended last
    platform::DrawItem2D last_frame_items[kMaxRecordedItems];
    char last_frame_calls[kMaxRecordedCalls] = {};
    FakeTiles tiles;            ///< draw_tiles() calls of the current frame
    FakeTiles last_frame_tiles; ///< the same, for the frame that ended last
    // Audio (spec 008)
    bool audio_initialized = false;
    int audio_voices = 0;    ///< voices asked for by audio_init()
    int live_samples = 0;    ///< created and not yet destroyed
    int samples_ready = 0;   ///< sample_ready() calls
    FakeVoice voices[kMaxFakeVoices];
    FakeStream streams[platform::kStreams];
    int open_asset_streams = 0; ///< asset_open() minus asset_close()
    // Console remote (spec 013)
    int remote_receive_calls = 0;          ///< remote_receive() calls (connected or not)
    std::size_t remote_last_read = 0;      ///< the `size` of the last remote_receive() call
    int remote_captures = 0;               ///< remote_capture() calls that succeeded
    bool remote_capture_held = false;      ///< a capture buffer is held (until remote_capture_release())
    int remote_capture_releases = 0;       ///< remote_capture_release() calls that freed a held buffer
    int remote_send_calls = 0;             ///< remote_send() calls while connected (spec 030: records are batched)
    // Hot reload (spec 031)
    int reload_stream_opens = 0; ///< reload_stream_open() calls that succeeded (they count in open_asset_streams too)
    int shared_asset_opens = 0;  ///< asset_open_shared() calls that succeeded (they count in open_asset_streams too)
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

/// Adds a read-only asset (spec 007; at most 64): `data` must stay alive until reset(). The game "has assets" (mount
/// succeeds) once one is added. Paths are matched exactly, case included.
void add_asset(const char* path, const void* data, std::size_t size);

/// Replaces the bytes of an asset added with add_asset() (spec 031: the build rewrote the file). Streams already open
/// on it read the new bytes from now on, as a file rewritten underneath them. `data` must stay alive until reset().
/// Does nothing for an unknown path.
void replace_asset(const char* path, const void* data, std::size_t size);

/// When false, audio_init() fails like a system without audio output (default true).
void set_audio_available(bool available);
/// When true, sample_create() fails (out of linear memory; reset() clears it).
void set_sample_create_fails(bool fails);

/// A non-looping voice reached its end: voice_playing() returns false from now on.
void finish_voice(int voice);

/// The frames core wrote into a sample (interleaved), or nullptr for a stale handle.
const std::int16_t* sample_frames(platform::SampleHandle sample);

/// Plays `frames` of what a stream has queued (oldest first), freeing its emptied buffers.
void consume_stream(int stream, std::uint32_t frames);

/// Everything submitted to a stream since its last stream_start(), interleaved (the first 1 M values).
const std::int16_t* stream_log(int stream, std::uint32_t& frames);

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
/// Bytes region_alloc() has handed out and not taken back in a region (what set_region_capacity() limits).
std::size_t region_used(MemRegion region);

/// Fills each new Heap and Linear block with `byte` (0..255) instead of leaving what malloc returns; −1 (the default,
/// and after reset()) turns it off. Proves core initialises every byte it later compares or hashes.
void set_alloc_fill(int byte);

/// The next `count` fs_write() calls fail after writing half the data (a torn write, e.g. power loss).
/// Files live in memory and survive nnh::shutdown()/init() until reset().
void fail_next_writes(int count);

/// The next `count` fs_append() calls fail and write nothing (spec 031: the SD card refuses a received file's bytes).
void fail_next_appends(int count);

/// Flips the bits of one byte of a stored file (simulates corruption on the SD card).
void corrupt_file(const char* path, std::size_t byte_index);

/// A power cut (spec 027 AC-3): the next `ops` mutating fs calls (fs_write, fs_append, fs_rename, fs_remove) run,
/// the one after is cut (a write or append keeps half its bytes when `torn`, else nothing; a rename or remove does
/// nothing), and every later mutating call does nothing and fails until restore_power() or reset(), as after a
/// reboot. Reads keep working.
void cut_power_after(int ops, bool torn = true);
void restore_power();

/// Cuts a stored file to its first `size` bytes (nothing when it is missing or not longer).
void truncate_file(const char* path, std::size_t size);

/// The fs contract calls since reset() or clear_fs_log(), one letter each (the first 255): S fs_size, R fs_read,
/// W fs_write, A fs_append, N fs_rename, X fs_remove.
const char* fs_log();
void clear_fs_log();

/// Capabilities reported by the platform (default: SecondScreen only).
void set_capabilities(std::initializer_list<Capability> capabilities);

/// Makes the next poll_dev_requests() ask to toggle the debug overlay (like F1 on PC).
void request_overlay_toggle();

/// Makes the next poll_dev_requests() ask to pause or resume (the simulator's Pause key; spec 030), to step one frame
/// (F10), or to start held (NNH_HOLD=1, the 3DS's --nnh-hold).
void request_pause();
void request_step();
void request_hold();
/// The masks successive poll_dev_requests() calls return (one each; 0 once used up; at most 64): drives a pause and
/// the paused ticks inside one nnh::frame() (spec 030). Replaces an earlier queue.
void queue_dev_requests(std::initializer_list<std::uint32_t> masks);

/// poll_events() returns true `polls` more times, then false (like set_quit_requested(true) from then on).
void quit_after_polls(int polls);

/// The coverage bytes a texture_create_alpha() texture was made from (a copy; `width` × `height`), or nullptr for a
/// handle that isn't a live coverage texture (spec 029).
const std::uint8_t* alpha_texture_coverage(platform::TextureHandle handle, int& width, int& height);

/// The code platform::system_language() reports from now on (default "en" after reset(); spec 029 FR-27). nullptr or an
/// ill-formed code test core's guard (system::language() then says "en"). Copied (at most 31 bytes).
void set_system_language(const char* code);

/// The value platform::stereo_slider() reports from now on (default 0; spec 026 FR-16). Core reads it only with
/// Capability::Stereo3D (set_capabilities) and clamps it to 0..1.
void set_stereo_slider(float value);

// ---- Console remote (spec 013): an in-memory PC ----------------------------------------------------------

/// A PC is connected: remote_receive() returns the bytes given to remote_push() (0 when none is pending) and
/// remote_send() records. Without it (after reset()) remote_receive() returns -1 like a game started alone.
void remote_connect();

/// Bytes the PC sends, read by remote_receive() in order, in pieces as large as core asks for.
void remote_push(const void* data, std::size_t size);

/// The PC closes the connection: remote_receive() still returns the bytes already pushed, then -1;
/// remote_send() fails from now on.
void remote_disconnect();

/// Everything core sent with remote_send() since reset() or remote_clear_sent(), in order.
const std::uint8_t* remote_sent(std::size_t& size);
void remote_clear_sent();

/// When true, remote_capture() fails (with last_error()).
void set_remote_capture_fails(bool fails);

/// The synthetic picture remote_capture() returns (RGBA8 rows, screen-sized): pixel (x, y) is
/// {x & 0xFF, y, 0x40 on the top screen / 0x80 on the bottom one, 0xFF}.
void remote_capture_pixel(Screen screen, int x, int y, std::uint8_t rgba[4]);
/// remote_capture_right() after a frame that drew two eyes (set_stereo(true), spec 026): {x & 0xFF, y, 0xC0, 0xFF};
/// after any other frame it is the top screen's picture.
void remote_capture_right_pixel(int x, int y, std::uint8_t rgba[4]);

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

// ---- 3D (spec 026, nnh/platform/video3d.hpp) ---------------------------------------------------------------------
// begin_3d() and end_3d() also log '[' and ']' in State::calls, between the screen's 'T'/'B' draw_2d calls;
// select_eye() (stereo, spec 026b) logs 'L' or 'R' there.

inline constexpr int kMaxRecorded3D = 64;

/// One begin_3d / draw_3d / end_3d / select_eye call of the current or last frame.
struct Fake3DCall {
    char kind = 0; ///< 'b' begin_3d, 'd' draw_3d, 'e' end_3d, 's' select_eye
    Screen screen = Screen::Top;
    std::uint8_t eye = 0;     ///< kind 's': 0 left, 1 right; kind 'b'/'d'/'e': the eye selected then
    platform::Pass3D pass{};  ///< kind 'b'
    platform::Draw3D draw{};  ///< kind 'd'
};

/// What the fake 3D backend saw (pass3d_log()).
struct Fake3D {
    int init_calls = 0;
    int shutdown_calls = 0;
    bool initialized = false;
    int max_draws = 0;             ///< from video3d_init
    int meshes_created = 0;
    int live_meshes = 0;
    std::size_t live_mesh_bytes = 0;
    int call_count = 0;            ///< calls in the current frame (first kMaxRecorded3D kept)
    Fake3DCall calls[kMaxRecorded3D];
    int last_frame_call_count = 0; ///< the same for the frame that ended last
    Fake3DCall last_frame_calls[kMaxRecorded3D];
    // Stereo (spec 026b)
    bool stereo = false;            ///< set_stereo() of the current frame (false at begin_frame)
    bool last_frame_stereo = false; ///< the same for the frame that ended last
    int set_stereo_calls = 0;       ///< since reset()
    int stereo_changes = 0;         ///< set_stereo() calls that changed the value shown (3DS: gfxSet3D)
    std::uint8_t eye = 0;           ///< the eye selected now
    Color right_clear{};            ///< the colour the right eye was cleared to this frame
    int right_clears = 0;           ///< right-eye clears this frame (at most 1: at its first selection)
    int last_frame_right_clears = 0; ///< the same for the frame that ended last
};

const Fake3D& pass3d_log();

/// When true, video3d_init() fails (like a console out of VRAM).
void set_video3d_init_fails(bool fails);
/// When true, mesh_create() fails (out of linear memory).
void set_mesh_create_fails(bool fails);
/// When on, begin_3d/draw_3d/end_3d also rasterise into a 400×240 top-screen picture with the engine's CPU
/// rasteriser (spec 026 FR-25): cleared to the top clear colour at begin_frame (the fake draws no 2D pixels).
/// Textures: RGBA copies of texture_create_rgba's pixels; "FAKE" images are a gradient (r = x·255/(w−1),
/// g = y·255/(h−1), b = 0, a = 255).
void enable_pixels_3d(bool on);
/// A pixel of the picture of the frame that ended last (the left eye's, which is the only one in mono frames).
Color pixel_3d(int x, int y);
/// A pixel of one eye's picture (0 left, 1 right) of the frame that ended last; the right eye's is cleared to the top
/// clear colour at its first selection (only stereo frames draw it).
Color pixel_3d(std::uint8_t eye, int x, int y);

/// The bytes a mesh was created with (vertices then indices), or nullptr for an unknown handle.
const std::uint8_t* mesh_data(platform::MeshHandle handle, std::size_t& size);

} // namespace nnh::platform_fake
