#pragma once
/// @file nnh/transition.hpp
/// Scene transitions (spec 023, decision D38): scene changes that cover the screen with a fade, wipe, split, blinds,
/// iris or the game's own effect, change the stack exactly when the screen is fully covered, optionally load in
/// slices while covered, then reveal the new scene — on the frame clock, with input blocked meanwhile. See
/// docs/api/transition.md.
///
/// @code
/// nnh::transition::replace(level_, {.effect = nnh::transition::fade()});               // fade to black and back
/// nnh::transition::replace(next_, {.effect = nnh::transition::iris(player_pos), .tall = true,
///                                  .out = nnh::Frames{36}, .in = nnh::Frames{36}});
/// @endcode
///
/// Optional (constitution XI): a game that never calls these links none of the module. One transition runs at a
/// time; it is engine state reached only through these functions (never a game object), run by the scene stack's
/// frame (`nnh::run()` or `nnh::scenes::update_and_draw()`) and cancelled silently by `nnh::shutdown()`.

#include <cstdint>

#include "nnh/detail/finish_event.hpp"
#include "nnh/detail/small_function.hpp"
#include "nnh/ease.hpp"
#include "nnh/gfx.hpp"
#include "nnh/time.hpp"

namespace nnh {

class Scene; // <nnh/scene.hpp>

namespace transition {

/// What one screen shows in one phase.
enum class Kind : std::uint8_t {
    Same,   ///< in `reveal`/`bottom`/`bottom_reveal`: derived (see Options); invalid in `effect`
    None,   ///< this screen is never covered
    Fade,   ///< the whole screen in the colour, alpha = coverage
    Wipe,   ///< an opaque rectangle growing from one edge
    Split,  ///< two rectangles closing from opposite edges towards the middle
    Blinds, ///< N bars, each growing from its own edge
    Iris,   ///< everything outside a circle or diamond whose radius shrinks to 0
    Custom, ///< the game's own drawing (Options::custom)
};
enum class Edge : std::uint8_t { Left, Right, Top, Bottom };  ///< wipe and blinds: where the cover grows from
enum class SplitAxis : std::uint8_t { LeftRight, TopBottom }; ///< split: the two halves come from these edges
enum class IrisShape : std::uint8_t { Circle, Diamond };
/// How the in phase draws (FR-5): the out effect backwards, or its continuation (wipe and blinds leave by the
/// opposite edge, split opens from the middle; fade, iris and custom effects look the same either way).
enum class Reveal : std::uint8_t { Reverse, Continue };
/// None when no transition runs; the last in frame still reports In (with coverage 0).
enum class Phase : std::uint8_t { None, Out, Covered, In };

/// A point on a screen, in screen pixels. Default: the middle of the screen (or of the tall area). `{player_pos}` is
/// a top-screen point; `{p, nnh::Screen::Bottom}` a bottom-screen one. The screen matters only in the tall area
/// (Options::tall); otherwise each screen's iris uses the point in its own coordinates.
struct IrisCenter {
    constexpr IrisCenter() = default;
    constexpr IrisCenter(Vec2 p, Screen s = Screen::Top) : point(p), screen(s), given(true) {} // NOLINT: implicit
    Vec2 point{};
    Screen screen = Screen::Top;
    bool given = false;
};

/// One effect. Covers are opaque: the colour's alpha is ignored (a fade's alpha is the coverage).
struct Effect {
    Kind kind = Kind::Fade;
    Color color = colors::black;
    Edge edge = Edge::Left;                 ///< Wipe, Blinds
    SplitAxis split = SplitAxis::LeftRight; ///< Split
    int bars = 8;                           ///< Blinds: 2..32
    IrisShape shape = IrisShape::Circle;    ///< Iris
    int band = 4;                           ///< Iris: band height in pixels, 1..16 (the edge's staircase step)
    IrisCenter center{};                    ///< Iris
};

/// The effect makers, for designated initializers: `{.effect = nnh::transition::wipe(Edge::Left)}`.
constexpr Effect same() { return Effect{.kind = Kind::Same}; }
constexpr Effect none() { return Effect{.kind = Kind::None}; }
constexpr Effect fade(Color color = colors::black) { return Effect{.kind = Kind::Fade, .color = color}; }
constexpr Effect wipe(Edge from, Color color = colors::black) {
    return Effect{.kind = Kind::Wipe, .color = color, .edge = from};
}
constexpr Effect split(SplitAxis axis = SplitAxis::LeftRight, Color color = colors::black) {
    return Effect{.kind = Kind::Split, .color = color, .split = axis};
}
constexpr Effect blinds(Edge from = Edge::Top, int bars = 8, Color color = colors::black) {
    return Effect{.kind = Kind::Blinds, .color = color, .edge = from, .bars = bars};
}
constexpr Effect iris(IrisCenter center = {}, IrisShape shape = IrisShape::Circle, Color color = colors::black,
                      int band = 4) {
    return Effect{.kind = Kind::Iris, .color = color, .shape = shape, .band = band, .center = center};
}
constexpr Effect custom() { return Effect{.kind = Kind::Custom}; }

/// What a custom effect draws from (FR-8). Called once per frame for each screen whose effect in this phase is
/// Custom, every transition frame (coverage 0 too), after the scenes' draws.
struct CustomFrame {
    Screen screen;
    float coverage; ///< 0..1, this frame
    Phase phase;
    int frame;      ///< frames elapsed in the phase (1 on its first frame)
    float loading;  ///< the loading step's progress (0..1; 1 without a step)
    bool tall;      ///< Options::tall
};
/// A custom effect: a function or a lambda with at most 16 bytes of trivially copyable captures (`[this]`).
using CustomEffect = detail::SmallFunction<void(const CustomFrame&)>;
/// What a loading step reports: its progress (clamped to 0..1, NaN → 0) and whether it is done.
struct LoadStep {
    float progress = 1;
    bool done = true;
};
/// A loading step (FR-13), called once per covered frame until it reports done (≤ 16 bytes of captures).
using LoadingStep = detail::SmallFunction<LoadStep()>;
/// on_covered / on_finished (≤ 16 bytes of captures).
using Callback = detail::SmallFunction<void()>;

/// A transition's options, as designated initializers. Keep one constant per kind of change and pass it.
/// Durations are whole 60 Hz frames (a Duration is rounded to the nearest frame, half up), each 0..10 s.
struct Options {
    Effect effect = fade();         ///< out, both screens unless `bottom` says otherwise; never same()
    Effect reveal = same();         ///< in: same() = `effect` (drawn backwards, or continued: reveal_style)
    Effect bottom = same();         ///< bottom screen, out: same() = `effect`; none() = never covered
    Effect bottom_reveal = same();  ///< same() = the top's reveal if `bottom` is same(), else `bottom`
    Reveal reveal_style = Reveal::Reverse;
    bool tall = false;              ///< draw wipes, splits, blinds and the iris across both screens (FR-6)
    int tall_gap = 0;               ///< pixels between the screens in the tall area, 0..240
    FrameTime out = Frames{24};     ///< the cover grows
    FrameTime hold = Frames{0};     ///< minimum covered time
    FrameTime in = Frames{24};      ///< the cover shrinks
    Easing out_ease{};              ///< frame progress → coverage during out
    Easing in_ease{};               ///< the same during in (applied to the remaining fraction: out backwards)
    bool update_out = false;        ///< the outgoing top scene keeps updating during out (FR-10)
    bool update_in = false;         ///< the incoming top scene updates during in
    bool block_input = true;        ///< game input reads idle while the transition runs (FR-12)
    CustomEffect custom{};          ///< for Kind::Custom (both phases)
    LoadingStep loading{};          ///< work in slices while covered (FR-13)
    Callback on_covered{};          ///< after the stack change, before the first loading step
    Callback on_finished{};         ///< in the last frame, before the next scene update
    detail::FinishEvent publish{};  ///< the game's own event, queued when done: `.publish = DoorDone{}` (≤ 32 bytes)
};

// ---- Requests (FR-1, FR-19) ----
// Each returns false and changes nothing while another transition runs (one debug log per running transition), and
// for misuse (FR-22: asserts in Debug/Development, an error log in Release). No allocation. The transition starts in
// the next scenes::update_and_draw() (this frame's, when requested before it: from an event handler or on_enter()).

/// Covers, replaces the top scene with `scene` (the stack change is spec 001's deferred replace), reveals.
bool replace(Scene& scene, const Options& options = {});
/// Covers, pushes `scene`, reveals.
bool push(Scene& scene, const Options& options = {});
/// Covers, pops the top scene of that moment (nothing if the game emptied the stack meanwhile: an error log),
/// reveals. Popping the last scene ends nnh::run() after the covered phase (no in phase).
bool pop(const Options& options = {});
/// Covers and reveals with no stack change: on_covered is where the room changes.
bool cover(const Options& options = {});
/// The reveal's iris centre, known only once the new scene is in (call it from on_enter() or on_covered): replaces
/// the centre of both screens' in effects. @return false when no transition is before its in phase.
bool reveal_at(IrisCenter center);

// ---- Queries (FR-15) ----
bool running();                ///< from the request until the end of its last frame (false in on_finished)
Phase phase();                 ///< None when not running
float coverage(Screen screen); ///< this frame, 0..1; 0 for a screen whose effect in this phase is none()
float phase_progress();        ///< out/in: k / n; covered: the hold's progress (1 without a hold)
float loading_progress();      ///< the loading step's last progress (0 before it runs; 1 without a step; 0 when idle)
int frames();                  ///< transition frames so far (1 on the first)
struct Timing {
    Frames out;
    Frames hold;
    Frames in;
};
Timing timing();               ///< of the running (or the last) transition, in whole frames
float seconds(Frames frames);  ///< frames / 60: `nnh::music::stop(nnh::transition::seconds(t.out))`

// ---- Effect geometry without a transition (FR-9) ----

/// Most rectangles any effect draws on one screen: an iris with 1-pixel bands (2 × 240 + 2).
inline constexpr int kMaxEffectRects = 482;

/// Where effect_rects()/draw_effect() place an effect.
struct Placement {
    bool tall = false;      ///< the tall area: both screens as one 400 × (480 + gap) area
    int tall_gap = 0;       ///< 0..240
    bool continued = false; ///< draw the effect as a Reveal::Continue reveal
    int layer = 15;         ///< draw_effect only
};
/// Most rectangles `effect` draws on one screen at its settings: fade and wipe 1, split 2, blinds `bars`, iris
/// 2 × ⌈240 / band⌉ + 2 (in the tall area, a band may straddle the bottom screen's edge: one more band), none and
/// custom 0. Pure.
int max_rects(const Effect& effect, bool tall = false);
/// The rectangles (whole pixels, screen coordinates, drawing order) `effect` draws on `screen` at `coverage`
/// (clamped to 0..1, NaN → 0); writes at most `capacity` of them to `out`, returns how many there are. Pure: no
/// frame, no init, no scene needed; no allocation. Kind::Same/None/Custom draw nothing.
int effect_rects(Screen screen, const Effect& effect, float coverage, Rect* out, int capacity,
                 const Placement& placement = {});
/// The colour drawn: the effect's colour, opaque; a fade's alpha is round(coverage × 255).
Color effect_color(const Effect& effect, float coverage);
/// Records the effect now (gfx::fill_rect on placement.layer, ignore_camera). Only inside a frame. No allocation.
void draw_effect(Screen screen, const Effect& effect, float coverage, const Placement& placement = {});

} // namespace transition
} // namespace nnh
