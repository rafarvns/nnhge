#pragma once
/// @file nnh/animation.hpp
/// Sprite animation (spec 016): sprite sheets described as frames, named clips with modes and per-frame durations,
/// frame markers, a small state machine, and a plain animator value per sprite that plays them on the deterministic
/// frame clock. Libraries are built in code or loaded from an Aseprite export converted at build time
/// (`<name>.ase.json` → `load_sprite_library("<name>")`). See docs/api/animation.md.
///
/// Optional (constitution XI): a game that doesn't include this header links none of it. Frames work without clips,
/// clips without the state machine, code without Aseprite.
///
/// @code
/// auto lib = nnh::SpriteLibrary::create({.frames = 8, .clips = 2, .clip_frames = 6,
///                                        .pivot = nnh::SpritePivot::BottomCenter}).value();
/// (void)lib.add_grid({.frame_width = 16, .frame_height = 16, .count = 8, .columns = 8});
/// const nnh::SpriteClipId run = lib.add_clip({.name = "run", .first = 1, .count = 4}).value();
/// nnh::SpriteAnimator anim{lib};
/// anim.play(run);
/// // every frame:
/// anim.update();
/// nnh::gfx::draw(nnh::Screen::Top, texture, pos, anim.draw_options());
/// @endcode

#include <cstddef>
#include <cstdint>

#include "nnh/detail/name_hash.hpp"
#include "nnh/detail/small_function.hpp"
#include "nnh/math.hpp"
#include "nnh/result.hpp"
#include "nnh/time.hpp"

namespace nnh {

struct DrawOptions; // <nnh/gfx.hpp>: include it to draw (draw_options() returns one)

namespace detail {
struct SpriteBlock;
}

// ---- Limits (FR-5) ----

inline constexpr int kSpriteMaxFrames = 4096;      ///< frames per library
inline constexpr int kSpriteMaxClips = 256;        ///< clips per library
inline constexpr int kSpriteMaxClipLength = 256;   ///< frames per clip
inline constexpr int kSpriteMaxClipFrames = 8192;  ///< clip frames over all clips
inline constexpr int kSpriteMaxMarkers = 1024;     ///< markers per library
inline constexpr int kSpriteMaxStates = 32;        ///< state-machine states per library
inline constexpr int kSpriteMaxTransitions = 128;  ///< transition rules per library
inline constexpr int kSpriteMaxNameLength = 31;    ///< bytes of a clip, state or marker name
inline constexpr int kSpriteMaxMarkersPerFrame = 8; ///< markers on one clip frame
inline constexpr int kSpriteMaxRepeat = 65535;     ///< plays of a Repeat clip, passes of a ping-pong clip
inline constexpr int kSpriteMaxQueue = 4;          ///< clips waiting in an animator's queue
inline constexpr int kSpriteMaxNotices = 32;       ///< notices one update()/advance() returns
inline constexpr int kSpriteMaxCrossings = 32;     ///< frames one update()/advance() crosses with notices
inline constexpr int kSpriteMaxCallbackDepth = 8;  ///< callbacks changing the clip within one update

/// 32-bit FNV-1a of a name: compare markers without strings (`case nnh::sprite_name("step"):`).
constexpr std::uint32_t sprite_name(const char* name) {
    return detail::fnv1a32(name);
}

/// A clip of one library (from add_clip() or find_clip()). Keep it: playing by id costs no lookup.
struct SpriteClipId {
    std::uint16_t value = 0xFFFF;
    constexpr bool valid() const { return value != 0xFFFF; }
    constexpr bool operator==(const SpriteClipId&) const = default;
};

/// A state of a library's state machine (from add_state() or find_state()). any() is the wildcard of rules.
struct SpriteStateId {
    std::uint8_t value = 0xFF;
    constexpr bool valid() const { return value < 0xFE; }
    static constexpr SpriteStateId any() { return {0xFE}; }
    constexpr bool operator==(const SpriteStateId&) const = default;
};

/// Default pivot of the frames of a library that don't give one, in the untrimmed frame.
enum class SpritePivot : std::uint8_t { TopLeft, Center, BottomCenter };
/// Loop: forever. Once: one play, then holds its last frame. Repeat: `repeat` plays, then holds. PingPong: forward
/// then back without repeating the turning frames (`0 1 2 3 2 1 0 1 …`), forever or a number of round trips.
enum class SpriteMode : std::uint8_t { Loop, Once, Repeat, PingPong };
/// Reverse starts from the last frame (ping-pong reverse: `3 2 1 0 1 2 3 …`).
enum class SpriteDirection : std::uint8_t { Forward, Reverse };
/// When a requested state change happens (FR-13): now, at the end of the current clip or loop, or never.
enum class SpriteSwitch : std::uint8_t { Now, AtEnd, Never };

/// Capacities of a library, all required (0 = none of that kind; there is no hidden default).
struct SpriteLibraryDesc {
    int frames = 0;      ///< 1..kSpriteMaxFrames
    int clips = 0;       ///< 0..kSpriteMaxClips
    int clip_frames = 0; ///< 0..kSpriteMaxClipFrames: the frames of all clips together
    int markers = 0;     ///< 0..kSpriteMaxMarkers
    int states = 0;      ///< 0..kSpriteMaxStates
    int transitions = 0; ///< 0..kSpriteMaxTransitions
    SpritePivot pivot = SpritePivot::TopLeft; ///< default pivot (top-left = gfx::draw's default)
};

/// Frames of a sheet as a grid of equal cells, read row by row (FR-1).
struct SpriteGridDesc {
    int frame_width = 0, frame_height = 0;
    int count = 0;        ///< frames to add
    int columns = 0;      ///< 0: as many as fit in image_width
    int image_width = 0;  ///< 0: unknown (no fit check; columns required)
    int image_height = 0; ///< 0: unknown (no fit check)
    int margin = 0;       ///< pixels around the grid
    int spacing = 0;      ///< pixels between cells
};

/// One frame given by its region (FR-1). Trimmed frames give where the region sits in the untrimmed frame.
struct SpriteFrameDesc {
    Region region{};                         ///< texture pixels, w and h ≥ 1
    Vec2 pivot{-1, -1};                      ///< in the untrimmed frame's pixels (1/16 px kept); negative = default
    int offset_x = 0, offset_y = 0;          ///< where the trimmed region sits in the untrimmed frame
    int source_width = 0, source_height = 0; ///< untrimmed size; 0 = the region's (not trimmed)
};

/// One clip (FR-3). Durations: a Duration (rounded once to the frame clock, half up) or nnh::Frames.
struct SpriteClipDesc {
    const char* name = nullptr;           ///< unique in the library, 1..31 bytes of UTF-8
    const int* frames = nullptr;          ///< sheet frame indices; nullptr = the range [first, first + count)
    int first = 0;
    int count = 0;                        ///< frames in the clip (1..256), also the length of `frames`
    FrameTime frame_time = Frames{6};     ///< duration of each frame
    const FrameTime* durations = nullptr; ///< optional, one per frame, overrides frame_time
    SpriteMode mode = SpriteMode::Loop;
    int repeat = 0;          ///< Repeat: plays (1..65535). PingPong: round trips (0 = forever)
    int pingpong_passes = 0; ///< advanced: one-way passes (Aseprite's tag `repeat`); overrides `repeat`
    SpriteDirection direction = SpriteDirection::Forward;
};

/// A transition rule of the state machine (FR-13): from a state or any(), to a state or any().
struct SpriteTransition {
    SpriteStateId from = SpriteStateId::any();
    SpriteStateId to = SpriteStateId::any();
    SpriteSwitch when = SpriteSwitch::Now;
    bool keep_time = false; ///< Now only: the new clip continues at the same position and time
};

/// A frame of the sheet: its region, its pivot in the untrimmed frame and where the region sits in it.
struct SpriteFrame {
    Region region{};
    Vec2 pivot{};
    Vec2 offset{};
};

/// Frames, clips, markers and a state machine (FR-5): one block (memory tag `animation`) reserved at creation and
/// freed by the destructor; adding allocates nothing. Read-only while animators use it: build it (or load it), then
/// play. It must outlive every animator using it (Debug/Development assert on the animator's next use; Release
/// doesn't check). Moving keeps the block, so animators stay valid. Move-only. Data errors are returned
/// (`Result`), never asserted.
class SpriteLibrary {
public:
    /// One block sized by the capacities. Errors: a capacity outside its range (InvalidArgument), no memory.
    static Result<SpriteLibrary> create(const SpriteLibraryDesc& desc);

    SpriteLibrary() = default; ///< empty: every query answers "nothing", every add fails
    ~SpriteLibrary();
    SpriteLibrary(SpriteLibrary&& other) noexcept;
    SpriteLibrary& operator=(SpriteLibrary&& other) noexcept;
    SpriteLibrary(const SpriteLibrary&) = delete;
    SpriteLibrary& operator=(const SpriteLibrary&) = delete;
    explicit operator bool() const { return block_ != nullptr; }

    // Building (FR-1..FR-3, FR-8, FR-12, FR-13, FR-18). All or nothing: a refused call adds nothing.

    /// Adds `grid.count` frames. @return the index of the first one.
    Result<int> add_grid(const SpriteGridDesc& grid);
    /// Adds one frame. @return its index.
    Result<int> add_frame(const SpriteFrameDesc& frame);
    Result<SpriteClipId> add_clip(const SpriteClipDesc& clip);
    /// A marker on frame `position` (0-based, in the clip's frame list) of `clip`: fires each time an animator
    /// enters that frame. Markers of one frame fire in the order they were added (≤ 8 per frame).
    Result<void> add_marker(SpriteClipId clip, int position, const char* name, int value = 0);
    Result<SpriteStateId> add_state(const char* name, SpriteClipId clip);
    /// When the state's clip finishes (Once, Repeat, finite ping-pong) the animator moves on to `next` by itself.
    Result<void> set_next_on_finish(SpriteStateId state, SpriteStateId next);
    /// A rule; with no matching rule a request switches now. The most specific wins: from→to, from→any,
    /// any→to, any→any.
    Result<void> add_transition(const SpriteTransition& rule);

    // Queries (FR-5, FR-11). Bad indices and ids answer "nothing" (an empty frame, an invalid id, "").

    int frame_count() const;
    SpriteFrame frame(int index) const;
    /// Region, pivot (in the region, flip-adjusted) and flips of a frame, for gfx::draw without an animator.
    DrawOptions draw_options(int frame, bool flip_x = false, bool flip_y = false) const;
    int clip_count() const;
    /// Linear over ≤ 256 hashes: keep the id rather than looking it up every frame.
    SpriteClipId find_clip(const char* name) const;
    const char* clip_name(SpriteClipId clip) const;
    int clip_length(SpriteClipId clip) const;
    FrameTime clip_duration(SpriteClipId clip) const; ///< one play (ping-pong: one round trip)
    int state_count() const;
    SpriteStateId find_state(const char* name) const;
    const char* state_name(SpriteStateId state) const;
    std::size_t memory_used() const; ///< bytes of the block

private:
    friend struct SpriteLibraryAccess;
    detail::SpriteBlock* block_ = nullptr;
};

/// Extra room in a loaded library for markers, states and rules added in code (FR-16, FR-18).
struct LoadSpriteLibraryOptions {
    int extra_frames = 0, extra_clips = 0, extra_clip_frames = 0, extra_markers = 0, extra_states = 0,
        extra_transitions = 0;
};

/// Loads `<path>.nnhsprite`, made by the build from `<path>.ase.json` (Aseprite export, D31). The file is validated
/// whole first (Corrupted, Unsupported for a newer format, LimitReached), then copied into a block sized exactly for
/// its contents plus `options`. Allocates once (tag `animation`); call at load time, not every frame.
/// @code
/// auto hero = nnh::load_sprite_library("art/hero"); // + nnh::load_texture("art/hero") for the pixels
/// @endcode
Result<SpriteLibrary> load_sprite_library(const char* path, const LoadSpriteLibraryOptions& options = {});

// ---- Notifications (FR-8) ----

enum class SpriteNoticeKind : std::uint8_t { Marker, Looped, Finished, StateChanged };

/// One notification of an update (16 bytes).
struct SpriteNotice {
    SpriteNoticeKind kind = SpriteNoticeKind::Marker;
    SpriteStateId state{};    ///< StateChanged: the new state
    SpriteStateId previous{}; ///< StateChanged: the state left
    SpriteClipId clip{};      ///< the clip the notice belongs to
    std::uint16_t position = 0; ///< Marker: frame position in the clip
    std::uint32_t marker = 0;   ///< Marker: sprite_name() of its name
    std::int32_t value = 0;     ///< Marker: its value
};

/// The notices of one update, in order. A view of a module-wide buffer: valid until the next update()/advance()
/// of any animator (copy what you keep).
struct SpriteNotices {
    const SpriteNotice* data = nullptr;
    int count = 0;
    const SpriteNotice* begin() const { return data; }
    const SpriteNotice* end() const { return data + count; }
};

class SpriteAnimator;
/// Called for each notice, inside update()/advance(): a function or a lambda with ≤ 16 bytes of trivially copyable
/// captures (`[this]`, `[this, id]`). It may call any command of this animator (or others), not update()/advance().
using SpriteCallback = detail::SmallFunction<void(SpriteAnimator&, const SpriteNotice&)>;

// ---- The animator (FR-6..FR-10, FR-12..FR-14) ----

/// Plays clips of one library (FR-6): a plain value (no heap, no virtual; ≤ 64 bytes on 3DS), one per sprite.
/// Advances only in update()/advance(), on the frame clock, so the same calls give the same frames and notices on
/// every system. Commands never notify: frames they enter notify at the next update. Misuse (a command on an
/// animator without library, an invalid id, a negative speed) asserts in Debug/Development and is ignored with an
/// error log in Release.
class SpriteAnimator {
public:
    SpriteAnimator() = default;                            ///< no library: queries answer "no clip"
    explicit SpriteAnimator(const SpriteLibrary& library); ///< no clip yet
    SpriteAnimator(const SpriteLibrary& library, SpriteStateId start); ///< state machine, in `start`

    // Clips (FR-6). play() and queue() leave the state machine (state() becomes invalid).

    /// Starts `clip` from its first frame; the same clip still playing is left alone unless `restart`.
    void play(SpriteClipId clip, bool restart = false);
    void play(const char* name, bool restart = false); ///< find_clip() + play(): a lookup
    void play_from(SpriteClipId clip, int position);    ///< starts at a frame position of the clip
    /// Plays `clip` when the current one finishes or (looping) ends its loop; at once when nothing plays.
    /// Error (LimitReached) when 4 clips wait; the queue is kept.
    Result<void> queue(SpriteClipId clip);
    void clear_queue();
    void stop(); ///< no clip
    void pause();
    void resume();
    void set_frame(int position); ///< jumps to a frame position of the current clip (time in frame 0)
    void set_speed(float speed);  ///< 0..64, rounded once to 1/4096 (0.5, 1.5 exact); 0 freezes
    float speed() const;
    void set_flip(bool flip_x, bool flip_y = false);
    void on_notice(SpriteCallback callback); ///< empty = none

    // State machine (FR-12, FR-13).

    /// Asks for `state`, normally every update from the game's own conditions; the rules decide when.
    /// Requesting the current state does nothing (and cancels a waiting request).
    void request(SpriteStateId state);
    void force(SpriteStateId state); ///< switches now, ignoring the rules (death)

    // Clock (FR-7). Both × speed. The returned notices are also passed to the callback, in the same order.

    SpriteNotices update();                 ///< exactly one 60 Hz frame
    SpriteNotices advance(FrameTime time);  ///< any span (time::FixedStep, a skip); remainder carried

    // Queries (FR-6, FR-10, FR-13).

    bool playing() const;  ///< a clip, not finished (true while paused)
    bool paused() const;
    bool finished() const; ///< a finite clip ended and holds its last frame
    SpriteClipId clip() const;
    const char* clip_name() const; ///< "" with no clip
    int position() const;          ///< frame position in the clip (-1: no clip)
    int frame() const;             ///< sheet frame index (-1: no clip)
    Region region() const;
    Vec2 pivot() const;  ///< in the region, offset applied, mirrored by the flips (what gfx::draw wants)
    Vec2 offset() const; ///< where the trimmed region sits in the untrimmed frame
    FrameTime time_in_frame() const;
    float progress() const; ///< 0..1: of the whole clip (finite) or of the current loop (looping)
    int loops() const;      ///< loops completed (saturates at 65535)
    SpriteStateId state() const;
    SpriteStateId previous_state() const;
    FrameTime time_in_state() const;
    bool flip_x() const;
    bool flip_y() const;
    /// Region, pivot and flips; the game adds layer, tint, opacity: `gfx::draw(screen, tex, pos, a.draw_options())`.
    DrawOptions draw_options() const;

private:
    friend struct SpriteAnimatorAccess;
    SpriteCallback callback_{};
    const detail::SpriteBlock* block_ = nullptr;
    std::int32_t elapsed_ = 0;    ///< ticks into the current frame
    std::int32_t speed_ = 4096;   ///< ticks per update
    std::int32_t state_time_ = 0; ///< ticks in the current state
    std::uint16_t clip_ = 0xFFFF;
    std::uint16_t step_ = 0;      ///< position in the clip's cycle
    std::uint16_t cycles_ = 0;    ///< cycles completed
    std::uint16_t remainder_ = 0; ///< advance(): sub-tick carry
    std::uint16_t serial_ = 0;    ///< the library's serial (lifetime check)
    std::uint8_t queue_[kSpriteMaxQueue] = {};
    std::uint8_t queue_size_ = 0;
    std::uint8_t state_ = 0xFF;
    std::uint8_t previous_ = 0xFF;
    std::uint8_t pending_ = 0xFF; ///< a state waiting for the end of the clip
    std::uint8_t flags_ = 0;
    std::uint8_t changes_ = 0;    ///< counts commands that change the clip (re-entrancy)
};

static_assert(sizeof(void*) != 4 || sizeof(SpriteAnimator) <= 64, "nnh::SpriteAnimator is 64 bytes on 3DS");
static_assert(sizeof(SpriteAnimator) <= 96, "nnh::SpriteAnimator grew");

} // namespace nnh
