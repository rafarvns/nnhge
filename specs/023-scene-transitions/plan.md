# Plan 023: Scene transitions

Status: implemented 2026-11-24, Linux checks T9.1 passed 2026-11-24, console run 1 done 2026-11-24 (logic and doors
PASS; bench FAIL split, loading engine), optimisation round T10.3 done on the PC, console rerun pending (approved
2026-11-23 overnight)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D38 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 001 (the scene stack, deferred changes, `nnh::run()`), spec 004 (rectangles, layers, `ignore_camera`,
recording and submit), spec 015 (the libm-free easings, `SmallFunction` callbacks, `FinishEvent`; D30), spec 017
(actions read the input queries; D32), spec 018 (the tall-world layout), spec 002 (asserts, logs, profiler), spec 013
(scripts; D28's remote input), spec 008 (music fades, in the example only)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ⚠️→✅ | Pressure point: "the same frames and pixels". Answered: the module is portable core code with no platform part; phases count whole frames; coverage is spec 015's `ease()` of an exact integer ratio, clamped and ratcheted with plain compares; every rectangle is computed in integers from a 16.16 coverage (`q = ⌊c·65536 + ½⌋`, exact as IEEE arithmetic); the transition sources are compiled with `-ffp-contract=off`; AC-3's golden on every host compiler and the console. One documented difference: the fade's blend is the GPU's (±1 per channel between PC and 3DS); geometry never differs |
| II 3DS constraint | ⚠️→✅ | Pressure points: the iris draws up to 122 rectangles per screen (244 per frame) and citro2d's per-rectangle cost is the unknown; AC-8's targets predate any measurement and specs 019–022 showed estimates 1.3–2× low. Answered by the cost plan below (per-rectangle cost counted all-in: record, sort, build, `C2D_DrawRectSolid`, cache misses), merged fully covered rows, one division per effect per screen, no `__aeabi_*` per band, and a disassembly check before the console (T7.7). The iris's worst frame is the tight one; AC-8's measurement-with-margin rule applies. Memory: a 244-byte state on the 3DS, nothing allocated |
| III Build speed | ✅ | `<nnh/transition.hpp>` includes `<nnh/gfx.hpp>`, `<nnh/time.hpp>`, a new light `<nnh/ease.hpp>` (the curves moved out of `<nnh/tween.hpp>`, which includes it: source-compatible), `nnh/detail/small_function.hpp` and a new `nnh/detail/finish_event.hpp`; no tween set, audio, tilemap, actions or containers. Measured before/after (T0.1, T8.4) |
| IV Event-driven | ✅ | No engine events (FR-16): state is queried; a game's own finish event goes through the bus; input blocking removes input events at their source |
| V No hidden costs | ✅ | No exceptions, RTTI or `virtual` (callbacks are `SmallFunction`s, effects plain enums); no allocation ever; a fixed per-frame cost plus the overlay's rectangles, inside one profile scope; no logging in the normal path |
| VI Spec + test first | ✅ | Geometry, timeline, requests, scene rules and the input filter are pure or run on the fake platform, tested first; AC-2 checks every submitted frame; AC-3's golden; mutation pass (stage 8) |
| VII Small files | ✅ | One module (`transition`, 10 sources ≤ 200 lines each), one public header; the input block and the scene hook are one small file each in their modules |
| VIII Documented API | ✅ | `docs/api/transition.md` (with pictures), scene, input, actions, audio and tween pages, `architecture/scenes.md`, D38, `examples/transitions`, the platformer's doors |
| IX Clean and legal | ✅ | Own code from the published designs of the spec's prior art; original art and music (scripts in the example) |
| X Safe by default | ✅ | No parser, no file. Misuse asserts in Debug/Development and is clamped or refused (error log) in Release (FR-22); a stuck loader warns; a geometry fuzz target checks bounds |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure points: the scene stack's hook and the input block touch every game. Answered: `scenes::update_and_draw()` pays one null check (`hello` uses no scenes: nothing), `nnh::frame()`'s input path one load and branch through a filter pointer that only the block code sets (`input_block.o` is linked only by games that block); the transition module, the geometry and the block are linked only when called; `scenes::replace` stays instant; `hello`'s growth is bounded and measured (≤ 96 bytes of text on the 3DS) |

## Design

### Overview

```
<nnh/transition.hpp>   requests (replace/push/pop/cover, reveal_at) · queries · Options/Effect · geometry (FR-9)
   │  set while a transition runs                     │ while blocking
   ▼                                                  ▼
scene/scene_hook.hpp: g_scene_transition ──> transition_frame / transition_cancel     input/input_block.hpp: g_input_filter
   scenes::update_and_draw(): one null check            nnh::frame() → input_update(): one null check
   run(): loops while a transition runs                 the filter idles the sample, keeps held controls blocked
   shutdown / end of run(): cancel                      raw masks for engine controls (overlay)

src/transition/: timeline (pure) · requests · frame (the hook) · overlay · loading · queries ·
                 effect_fill (fade, wipe, split, blinds) · effect_iris · effect_area (screens, tall area, reveal) · effect_query
```

### Decision D38: transitions are engine-owned module state run by a scene-stack hook, drawn as rectangles computed in integers; input is blocked at its source

**Where the transition lives.** One running transition at a time is module state of `transition` (like the scene
stack: one per engine, reached only through `nnh::transition` functions and reset by `nnh::shutdown()`), not a game
object. It spans two scenes, so neither may own it (the Godot pitfall); a game object would have to be reachable from
the stack's path anyway. This is not a game-level hidden global: games hold nothing, query everything, and a game that
never requests a transition links none of it.

**How the stack runs it (the hook).** `engine/core/src/scene/scene_hook.hpp` declares
`extern const SceneTransitionHook* g_scene_transition` (`{void (*frame)(); void (*cancel)();}`), defined null in
`scenes.cpp` and set by the transition module **only while a transition runs** (from the request to the end of its
last frame). `scenes::update_and_draw()` becomes `if (g_scene_transition) [[unlikely]] g_scene_transition->frame();
else g_stack.update_and_draw();` — one null check per frame for every game with scenes, nothing linked. The hook's
frame function steps the timeline, then calls the stack's two halves, now public members of `SceneStack`:
`update_top()` (D30's `BeforeUpdate`, then the top `update()`) when the phase allows it, and `draw_all()` (bottom to
top), then records the overlay. `run()` loops `while (frame() && (size() > 0 || g_scene_transition))` so a transition
that popped the last scene finishes its covered phase (FR-19), and calls `scenes::update_and_draw()`. Before the stack
is cleared (`scenes_shutdown()`, the end of `run()`), a running transition is **cancelled** through the hook: no
callbacks, no event, its waiting scene released, the block lifted. The scene module never names the transition
module (dependencies point down; D30 and D32 did the same).

**Stack changes.** A transition's change is enqueued through the stack's normal deferred path (`request_push`,
`request_replace`, or a new tolerant `Op::TransitionPop` that pops nothing on an empty stack and logs an error) at the
end of the frame whose coverage reaches 1 (or at the request when the out time is 0), so `on_exit()`, D30's
`AfterExit`, `on_enter()`, limits and ownership checks are spec 001's. From the request until it is enqueued (or the
transition is cancelled), the target scene's `pending_` counter holds one more count (`SceneAccess`): destroying it is
caught by `~Scene`'s check, pushing it elsewhere by `do_push`'s, exactly as for a pending push.

**Drawing.** Built-in effects are rectangles computed in integers (below) and recorded with `gfx::fill_rect` on layer
15 with `ignore_camera`, after every scene's `draw()`: above everything the scenes drew, below the debug pass, never
zoomed; game code drawing after `scenes::update_and_draw()` is above it (same layer = call order). No graphics or
contract change; `C2D_Fade` is not used (open question 2: the fake platform and the PC see the same data).

**Input is blocked at its source.** The input module gains a block (public, open question 1): while any blocker is on
(the game's `input::set_blocked(true)` or a transition), `nnh::frame()`'s sample is replaced by an idle one before the
input module reads it (buttons up, sticks centred, no touch), so queries, `ButtonPressed`/`Released`, touch events,
D32's action evaluation and D28's remote input (merged into the sample before) all see nothing. When the last blocker
lifts, every control held at that moment (a button, a stick out of its dead zone, a touch) stays filtered until it is
released. Engine controls keep the raw input: the debug overlay's SELECT + L reads raw masks. The filter is a function
pointer (`detail::g_input_filter`) set by the block code while blocking or while something stays filtered: games that
never block pay one load and a null check per frame and link no block code.

**Rejected:** a game-owned `Transitions` object (needs to live in both scenes and in the stack's path); a D30
attachment on the outgoing scene (dies with its `on_exit()`); game-called `transition::update()/draw()` (the
forgettable call D30/D32 removed; can't stop the top scene's update); a `virtual` hook on `Scene`; an engine layer above
15 in gfx (no case needs it; the escape hatch "draw after `update_and_draw()`" is useful); `C2D_Fade` (different data
per system, invisible to tests); a texture mask iris (texture memory, per-GPU sampling); wall-clock timing; blocking
input only by not updating scenes (subscribers and actions still see presses); filtering in each query (every query
pays, events and actions disagree with queries); engine transition events; an audio option (links Ogg into every game
using transitions).

### Public API (sketch)

```cpp
// <nnh/ease.hpp> (new; moved verbatim from <nnh/tween.hpp>, which includes it): only <cstdint>.
namespace nnh {
enum class Ease : std::uint8_t { Linear, QuadIn, /* ... the 19 curves, unchanged */ BounceInOut };
inline constexpr int kEaseCount = 19;
using EaseFn = float (*)(float t);
float ease(Ease curve, float t);
struct Easing { /* unchanged: builtin or custom, operator() */ };
}

// <nnh/detail/finish_event.hpp> (new; FinishEvent moved verbatim from tween_detail.hpp, which includes it).

// <nnh/transition.hpp>: <nnh/gfx.hpp>, <nnh/time.hpp>, <nnh/ease.hpp>, nnh/detail/small_function.hpp,
// nnh/detail/finish_event.hpp. Optional (XI): a game that never calls these links none of the module.
namespace nnh {
class Scene; // <nnh/scene.hpp>

namespace transition {

/// What one screen shows in one phase.
enum class Kind : std::uint8_t {
    Same,   ///< in `reveal`/`bottom`/`bottom_reveal`: derived (see Options); invalid in `effect`
    None,   ///< this screen is never covered
    Fade, Wipe, Split, Blinds, Iris,
    Custom, ///< the game's own drawing (Options::custom)
};
enum class Edge : std::uint8_t { Left, Right, Top, Bottom };        ///< wipe and blinds: where the cover grows from
enum class SplitAxis : std::uint8_t { LeftRight, TopBottom };       ///< split: the two halves come from these edges
enum class IrisShape : std::uint8_t { Circle, Diamond };
enum class Reveal : std::uint8_t { Reverse, Continue };             ///< FR-5
enum class Phase : std::uint8_t { None, Out, Covered, In };

/// A point on a screen (FR-4, open question 3: screen points only). Default: the middle of the screen (of the tall
/// area). `{player_pos}` is a top-screen point; `{p, nnh::Screen::Bottom}` a bottom-screen one.
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
    Edge edge = Edge::Left;                     ///< Wipe, Blinds
    SplitAxis split = SplitAxis::LeftRight;     ///< Split
    int bars = 8;                               ///< Blinds: 2..32
    IrisShape shape = IrisShape::Circle;        ///< Iris
    int band = 4;                               ///< Iris: band height in pixels, 1..16 (the edge's staircase step)
    IrisCenter center{};                        ///< Iris
};
constexpr Effect same();
constexpr Effect none();
constexpr Effect fade(Color color = colors::black);
constexpr Effect wipe(Edge from, Color color = colors::black);
constexpr Effect split(SplitAxis axis = SplitAxis::LeftRight, Color color = colors::black);
constexpr Effect blinds(Edge from = Edge::Top, int bars = 8, Color color = colors::black);
constexpr Effect iris(IrisCenter center = {}, IrisShape shape = IrisShape::Circle, Color color = colors::black,
                      int band = 4);
constexpr Effect custom();

/// What a custom effect draws from (FR-8). Called once per frame for each screen whose effect in this phase is
/// Custom, every transition frame (coverage 0 too), after the scenes' draws.
struct CustomFrame {
    Screen screen;
    float coverage;   ///< 0..1, this frame
    Phase phase;
    int frame;        ///< frames elapsed in the phase (1 on its first frame)
    float loading;    ///< the loading step's progress (0..1; 1 without a step)
    bool tall;        ///< Options::tall
};
using CustomEffect = detail::SmallFunction<void(const CustomFrame&)>;   ///< ≤ 16 bytes of captures
struct LoadStep { float progress = 1; bool done = true; };
using LoadingStep = detail::SmallFunction<LoadStep()>;                 ///< called once per covered frame
using Callback = detail::SmallFunction<void()>;

/// A transition's options, as designated initializers; keep one `constexpr`-like value per kind of change.
struct Options {
    Effect effect = fade();          ///< out, both screens unless `bottom` says otherwise
    Effect reveal = same();          ///< in: same() = `effect` (or its continuation, see reveal_style)
    Effect bottom = same();          ///< bottom screen, out: same() = `effect`; none() = never covered
    Effect bottom_reveal = same();   ///< same() = the top's reveal if `bottom` is same(), else `bottom`
    Reveal reveal_style = Reveal::Reverse;
    bool tall = false;               ///< draw directional effects and the iris across both screens (FR-6)
    int tall_gap = 0;                ///< pixels between the screens in the tall area, 0..240
    FrameTime out = Frames{24};      ///< each 0..10 s, rounded to whole frames
    FrameTime hold = Frames{0};      ///< minimum covered time
    FrameTime in = Frames{24};
    Easing out_ease{};
    Easing in_ease{};
    bool update_out = false;         ///< the outgoing top scene keeps updating during out (FR-10)
    bool update_in = false;          ///< the incoming top scene updates during in
    bool block_input = true;         ///< FR-12
    CustomEffect custom{};           ///< for Kind::Custom (both phases)
    LoadingStep loading{};           ///< FR-13
    Callback on_covered{};           ///< after the stack change, before the first loading step
    Callback on_finished{};          ///< in the last frame, before the next scene update
    FinishEvent publish{};           ///< the game's own event, queued when done (≤ 32 bytes)
};

// ---- Requests (FR-1, FR-19) ----
/// Each returns false and changes nothing while another transition runs (one debug log per running transition),
/// and for misuse (FR-22: asserts in Debug/Development, error log in Release). No allocation.
bool replace(Scene& scene, const Options& options = {});
bool push(Scene& scene, const Options& options = {});
bool pop(const Options& options = {});
bool cover(const Options& options = {});  ///< no stack change: on_covered is where the room changes
/// The reveal's iris centre, known only once the new scene is in (call it from on_enter() or on_covered): false
/// when no transition is before its in phase.
bool reveal_at(IrisCenter center);

// ---- Queries (FR-15) ----
bool running();
Phase phase();                 ///< None when not running; the last in frame reports In with coverage 0
float coverage(Screen screen); ///< this frame; 0 for a screen with none()
float phase_progress();        ///< out/in: k / n; covered: the hold's progress (1 without a hold)
float loading_progress();
int frames();                  ///< transition frames so far (1 on the first)
struct Timing { Frames out; Frames hold; Frames in; };
Timing timing();               ///< of the running transition (whole frames); seconds = frames / 60
float seconds(Frames frames);  ///< frames / 60, for music::stop(seconds(t.out))

// ---- Effect geometry without a transition (FR-9) ----
inline constexpr int kMaxEffectRects = 482;  ///< per screen: an iris with 1-pixel bands (2 × 240 + 2)
int max_rects(const Effect& effect, bool tall = false); ///< per screen at its settings (fade/wipe 1, split 2, blinds
                                                         ///< bars, iris 2 × ⌈240 / band⌉ + 2)
struct Placement {
    bool tall = false; int tall_gap = 0;
    bool continued = false;   ///< draw the effect as a Reveal::Continue reveal
    int layer = 15;           ///< draw_effect only
};
/// The rectangles (whole pixels, screen coordinates, in drawing order) `effect` draws on `screen` at `coverage`;
/// writes at most `capacity`, returns how many there are. Pure: no frame, no init, no scene needed.
int effect_rects(Screen screen, const Effect& effect, float coverage, Rect* out, int capacity,
                 const Placement& placement = {});
Color effect_color(const Effect& effect, float coverage); ///< the colour drawn (a fade's alpha = round(c × 255))
/// Records the effect now (fill_rect, ignore_camera, placement.layer). Only inside a frame. No allocation.
void draw_effect(Screen screen, const Effect& effect, float coverage, const Placement& placement = {});

} // namespace transition

// <nnh/input.hpp> additions (FR-12, open question 1):
namespace input {
/// Blocks the game's input (cut-scenes, dialogs): from the next nnh::frame(), queries read idle and no input or
/// action event is queued; when unblocked, controls still held stay blocked until released. Engine controls (the
/// overlay, the simulator's keys) are never blocked. A transition's block is separate: one never lifts the other.
void set_blocked(bool blocked);
bool blocked(); ///< by the game or a transition
}
} // namespace nnh
```

Exact names may still change while building; stage notes record any change. Templates: none new (the
`SmallFunction` constructors).

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| (none) | — | — | — |

The engine publishes nothing (FR-16). `Options::publish` queues a copy of the game's own event when the transition is
done, through `detail::events_publish` as spec 015's `.publish`. Input events are not queued while blocked.

### Semantics fixed by the plan

**The frame timeline** (`transition_timeline.cpp`, pure). Durations become whole frames: `n = ⌊(ticks + 2048) /
4096⌋`, 0..600 (`O`, `H`, `I`). A transition's **first frame is the first `scenes::update_and_draw()` after the
request** (a request in `update()` starts next frame; one from an event handler or `on_enter()`, before the frame's
`update_and_draw()`, starts this frame). Each frame:

| Phase | Frame k (1-based in the phase) | Coverage c (each screen not `none()`) | Ends after the frame where |
|-------|-------------------------------|-----------------------------------------|----------------------------|
| Out (O > 0) | 1..O | `ratchet_up(clamp(out_ease(k / O)))`; exactly 1 at k = O | k = O: the stack change is enqueued |
| Covered | j = 1, 2, … | 1 | the loading step reported done (or there is none) **and** j ≥ max(H, 1) |
| In (I > 0) | 1..I | `ratchet_down(clamp(in_ease((I − k) / I)))`; exactly 0 at k = I | k = I: done |

`k / O` is `float(k) / float(O)` (exact integers, one IEEE division); `(I − k) / I` likewise, so the default in
(`in_ease` = linear, or the same curve as out) plays the out backwards frame for frame. `clamp` maps NaN and values
below 0 to 0 and above 1 to 1; the ratchets keep coverage monotonic for overshooting curves (Back, Elastic, Bounce
show plateaus: documented). With O = 0 the change is enqueued at the request and the first frame is covered; with
I = 0 the transition is done at the end of its last covered frame; all zero = the change plus one covered frame. Phase
frames: O + C + I, C ≥ 1. The 10-second warning: covered frame 600 with the step not done (Debug/Development, once).

**One frame of the hook** (`transition_frame.cpp`):

1. `NNH_PROFILE_SCOPE("transition")` opens (it covers steps 2 and 5–6, not the scenes' own update and draw);
2. step the timeline; on covered frame 1: `on_covered`, then the first loading call; each covered frame: the loading
   step (its progress clamped to 0..1, NaN → 0) and the hold;
3. the top scene updates (`update_top()`: D30's `BeforeUpdate`, then `update()`) only in out with `update_out` and in
   in with `update_in`;
4. every scene draws (`draw_all()`);
5. the overlay: for each screen, its effect for the phase — built-in through the geometry into `fill_rect` (layer 15,
   `ignore_camera`), custom through `Options::custom`;
6. end of frame: out complete → enqueue the change, phase Covered; covered complete → In (or done if I = 0 or the stack
   is empty: an in phase over nothing is skipped, FR-19); in complete → done. **Done**: the input block lifted, the
   hook pointer cleared, the state marked idle, then `on_finished` (from a local copy: it may start the next
   transition, which starts in the next frame), then the finish event queued.

**Requests** (`transition_request.cpp`). In order: refused if running (false, one debug log per running transition);
misuse (FR-22) — durations invalid or over 10 s (assert; Release: invalid → 0, long → 600 frames, error log), `bars`,
`band`, `tall_gap` out of range (clamped), a NaN or infinite iris centre (assert; Release: the middle), an iris centre
beyond ±4096 px (clamped: keeps r² in 32 bits), `effect.kind == Same` or `Custom` without `custom` (assert; Release:
fade), `push`/`replace` of a scene already on the stack (assert; Release: refused, error log), `pop` on an empty stack
(the same). Then the options are resolved into the state (below), the target's pending count taken, the hook pointer
set, the transition's input blocker turned on (unless `block_input` is false), and with O = 0 the change enqueued.

**Resolving screens** (`effect_area.cpp`): top out = `effect`; top in = `reveal` if given, else `effect`; bottom out =
`bottom` if given, else `effect`; bottom in = `bottom_reveal` if given, else (`bottom` given ? bottom out : top in).
`reveal_style = Continue` draws every in effect as its continuation. `tall` draws directional effects and the iris
once over the tall area (fade and custom stay per screen; `bottom`/`bottom_reveal` must be `same()`: assert, ignored).
`reveal_at` replaces the iris centre of both in effects (before the in phase only).

**Geometry** (`effect_fill.cpp`, `effect_iris.cpp`, `effect_area.cpp`), each effect a function of a 16.16 coverage
`q = ⌊c · 65536 + ½⌋` (0..65536: exact) and an area W × H (a screen: 400 or 320 × 240; the tall area: 400 × (480 +
gap)); `ext(L) = (q · L + 32768) >> 16` (32-bit, no division). `q = 0` draws nothing; `q = 65536` covers the area.

| Effect | Rectangles (area coordinates, drawing order) | Count |
|--------|----------------------------------------------|-------|
| Fade | `{0, 0, W, H}`, colour alpha `(q · 255 + 32768) >> 16` (none when 0) | ≤ 1 |
| Wipe from Left/Right/Top/Bottom | e = ext(W or H); `{0,0,e,H}` / `{W−e,0,e,H}` / `{0,0,W,e}` / `{0,H−e,W,e}` | ≤ 1 |
| Split LeftRight (TopBottom alike) | e = ext(W); a = ⌈e/2⌉ from the left edge, b = ⌊e/2⌋ from the right | ≤ 2 |
| Blinds N from Top (others alike) | bar i spans [B_i, B_{i+1}) with B_i = ⌊i·H/N⌋ (one `udivmod` for H/N and H%N, then an exact running remainder); each bar covers ext(B_{i+1} − B_i) from its own top | ≤ N |
| Iris | below | ≤ 2·⌈H/band⌉ + 2 |
| Continue (in phase) | wipe and blinds: the opposite edge; split: one centred band of width e (shrinking to the middle); fade, iris and custom: unchanged | as above |

**Iris.** The centre `(cx, cy)` is the given point rounded half up to whole pixels (area coordinates; the tall area
puts a bottom-screen point at `(x + 40, y + 240 + gap)`). `R0 = ⌈√D⌉` with `D` the largest squared distance to the
area's four corners (an exact integer square root by the bit method: no libm, no VFP). Radius `r = R0 − ext(R0)`.
Rows go in bands of `band` pixels from the top; a band `[y0, y1)` is sampled at its middle row `ym = y0 + ⌊(y1 −
y0)/2⌋`, `dy = |ym − cy|`; if `dy ≥ r` the band is fully covered (consecutive fully covered bands merge into one
rectangle: at most one above and one below the shape); otherwise the half-width is `w = ⌊√(r² − dy²)⌋` (circle) or `w =
r − dy` (diamond) and the band gets `[0, cx − w)` and `[cx + w, W)`, each clipped to the area and emitted only when not
empty. At `q = 0` nothing is drawn (the shape contains the area); at `q = 65536`, `r = 0`, one rectangle covers the
area. Bands are emitted top to bottom, left piece first.

**Tall area → screens.** Geometry is computed once in area coordinates and clipped: the top screen gets the part in
`[0, 400) × [0, 240)`; the bottom screen the part in `[40, 360) × [240 + gap, 480 + gap)`, moved by `(−40, −240 −
gap)`; the gap is drawn nowhere. A clipped-away rectangle is not emitted. Per screen the counts above hold.

**The sink.** Geometry never fills a buffer of its own: each rectangle goes to a sink (`void (*)(void* ctx, Rect)`):
the overlay's records a `fill_rect`, `effect_rects`' writes into the caller's array (counting past its capacity),
the tests' collects. No stack array larger than a few words.

**The input filter** (`input/input_block.{hpp,cpp}`, pure part `detail::InputBlock`, 24 bytes): blockers (a bit for the
game, one for transitions), the filtered button mask, three "held when blocked" flags (touch, Circle Pad, C-Stick) and
the raw masks for engine controls. Per frame, given the raw sample: if any blocker is on, everything held is marked
filtered and the sample becomes idle; otherwise filtered buttons are `filtered & raw` (released ones drop out), a
filtered stick reads (0, 0) until its shaped value is (0, 0) once, a filtered touch reads "not touching" until lifted;
then the module's normal path runs on the result (the D-Pad mirror is computed from the filtered Circle Pad, so it is
filtered too). The filter clears its own pointer when no blocker is on and nothing is filtered. `touch_position()`
keeps the last unfiltered point. Engine controls: `detail::input_engine_held_mask()`/`pressed_mask()` return the raw
masks while the filter runs (the overlay's SELECT + L toggle), the module's masks otherwise.

### Data and memory

| Item | Size (3DS / 64-bit PC) | Notes |
|------|------------------------|-------|
| transition state | **244 B** / 308 B | 4 callbacks (`SmallFunction` 24/32) 96/128 · `FinishEvent` 40/48 · 4 resolved effects × 12 B (kind, edge/axis, shape, bars, band, flags, colour, centre as `i16` × 2) 48 · 2 easings 16/32 · target scene, op, flags, gap, phase 8/16 · frame counters (O, H, I, k as `u16`; covered and total frames `u32`) 16 · coverage × 2, phase progress, loading progress, ratchet 20. `static_assert`: ≤ 256 B on 32-bit, ≤ 320 B on 64-bit (FR-21's target read for the 3DS, as earlier plans' sizes) |
| input block | 24 B + filter pointer 4/8 | in `input_block.o` (only games that block) |
| scene hook pointer | 4/8 B | in `scenes.cpp` |
| per frame | 0 | sink on the stack (a pointer and a context); geometry locals |
| overlay draws | ≤ 1 (fade, wipe), 2 (split), N (blinds), 122 (iris, band 4) per screen; 482 with band 1 | count in `max_draws` (default 2048) |

No allocation anywhere. The fake platform's recorded items grow from 64 to 1024 (test-only memory, ≈ 40 KB × 2) so
AC-2 sees whole frames.

### ARM11 cost plan (AC-8)

Lessons built in (specs 019–022 on the Old 3DS: 268 MHz ARM11, 16 KB L1D with 32-byte lines, no L2, no prefetcher,
≈ 110 cycles per miss; instruction-count estimates were 1.3–2× low): count misses, not only instructions; keep
divisions out of loops (ARM11 has no divide instruction: `__aeabi_uidivmod`); one out-of-line sink call per rectangle
is fine (the rectangle itself costs far more); no `vmrs` in loops (geometry is integer).

**The rectangle is the unit.** A built-in rectangle's cost all-in, through gfx's normal path:

| Step | Work | Cycles |
|------|------|--------|
| record (`fill_rect` → `DrawList::record`) | `drawing()` loads, an 80-byte `DrawCommand` built and copied (≈ 60–80 instructions, 2.5 lines into the write buffer) | 150–220 |
| build at submit (layer sort, `DrawItem2D`) | counting sort entry, conversion to whole pixels (≈ 70–100 instructions); the command read back: 2–3 misses (the list is long past L1 by submit) | 350–500 |
| `draw_2d` → `C2D_DrawRectSolid` | the switch, colour conversion, citro2d's four corners and six 32-byte vertices into the linear vertex buffer, its state checks (≈ 150–220 instructions, 192 B written) | 300–550 |
| **all-in** | | **800–1270 ≈ 3.0–4.7 µs; × 1.3 → 3.9–6.2 µs** |

For reference, spec 012 measured ≈ 12.8 µs per *sprite* all-in including the game's update and a texture lookup.

| AC-8 workload (both screens) | Hot path | Estimate (upper × 1.3) | Target | Verdict |
|------------------------------|----------|------------------------|--------|---------|
| fade, wipe or split | hook + timeline step (≈ 150 instructions + one `ease()` ≈ 40–120) ≈ 2–4 µs; 2 rectangles (split 4) | fade/wipe 0.010–0.017 ms, split 0.018–0.029 ms | < 0.1 ms | PASS |
| blinds, 8 bars | step; one `udivmod` per screen (≈ 60 cycles); 16 bars × ≈ 20 instructions; 16 rectangles | 0.066–0.105 ms | < 0.3 ms | PASS |
| iris (circle, band 4, centre in the middle), **average** over the out phase | per band ≈ 100–130 instructions (bit-method square root ≈ 70–90, compare, two clips, sink calls); ≈ 140 rectangles per frame on average (both screens) | 0.60–0.95 ms (1.3) | < 2 ms | PASS |
| iris, **worst frame** (r between 120 and 160: every band cut on both screens) | 120 bands ≈ 0.08 ms + 244 rectangles | 1.05–1.60 ms (1.3) | < 2 ms (logged) | expected PASS; **at risk** if the per-rectangle cost is ≥ 7.5 µs (risk 1) |
| the frame with the loading step (engine share) | hook + step + bookkeeping of the step call + a full fade cover (2 rectangles) | 0.012–0.020 ms | < 0.05 ms | PASS |
| platformer level 1, ten door irises (tall area, band 4) | the level's draws without its update + ≤ 244 rectangles | each transition frame ≤ the level's normal frame + 1.6 ms | no frame > 25 ms | expected PASS (the replay judges 60 Hz on this level today) |

The bench judges each workload on its **average** over 300 measured frames (as plans 019–022) and logs the worst; the
iris line also logs its largest rectangle count. As AC-8 says, the targets stay as written; T7.7 checks the
disassembly before the console, the console measures (T10.2), a missed target is optimised first (risk 1's list), then
the budget is set from the measurement with ≈ 25 % margin and written in the spec and the docs.

**How the bench measures "step plus recording and submitting".** The scope `transition` (step + overlay recording)
comes from the profiler; submit cost comes from a new Development-only `NNH_PROFILE_SCOPE("gfx submit")` in
`detail::gfx_submit()` (Release unchanged). Each workload's cost per frame = `transition` + (`gfx submit` − the base
run's average `gfx submit`), the base being 300 frames of the same bench scene with no transition. The loading
workload subtracts its own nested `bench slice` scope (the game's slice). *Amended by T10.3 (see "Optimisation round
(T10.3)"): the profiler's own cost is taken out (two `transition` scopes a frame, each worth an empty `bench probe`
scope of the same frame), the loading step does no work of its own and opens no scope, the showcase's music is stopped,
and a `rect` run (no transition, one game-drawn solid rectangle per screen) logs gfx's share, not judged.*

### Self-check and bench (AC-3, AC-6, AC-8)

- **The script** (`examples/transitions/check_script.{hpp,cpp}`, engine calls only, compiled into the example and
  `nnh_tests`): three plain check scenes A, B, C (each draws a full-screen rectangle of its own colour on both screens
  at layer 0 and a small square at layer 15) and a fixed table of ≈ 24 steps over 1200 frames, each "at frame f, call
  …": replace fade black (24/0/24, linear); push wipe Left (QuadOut/QuadIn); pop wipe Top with Continue; replace split
  LeftRight (20/10/20); replace split TopBottom with Continue; cover blinds Top 2 bars; cover blinds Left 32 bars with
  Continue; replace iris circle at (200, 120) band 4 with `reveal_at((40, 200))`; replace iris diamond at (−30, 260)
  band 16 with its own reveal colour; per-screen: top wipe Right, bottom fade white; top only (bottom `none()`), iris
  circle band 1; tall wipe Top gap 0; tall iris centred on a bottom-screen point, gap 24; a loading step of 7 slices
  with hold 12 and a custom effect; zero lengths (0/0/0, 0/0/20, 20/0/0); easings CubicInOut, BackOut (plateau),
  SineIn, BounceOut; `update_out`/`update_in`; a refused request in the middle of the iris; a plain `scenes::push(C)`
  and `pop()` during a covered phase. Per frame the checksum (FNV-1a 32) takes the frame number, `phase()`, both
  screens' `coverage()` bits, `loading_progress()` bits and, for each screen, the rectangles and colour of FR-9's
  `effect_rects`/`effect_color` for the step's effect at that coverage (what the overlay drew: AC-2 proves they are
  equal), and the frame numbers of every `on_exit`, `on_enter`, first `update()` after a transition, `on_covered`,
  `on_finished`, custom call and loading call. Logged every 120 frames: `transitions: <frame> sum <checksum>`, at the
  end `transitions: PASS <checksum>` or `transitions: FAIL frame <f> expected <x> got <y>`. Goldens recorded from
  pc-debug in `kTransitionsCheckGolden`/`kTransitionsCheckpoints`, equal in pc-release; the unit test runs the same
  script on the fake platform (fast) and ctest `transitions_self_check_pc` runs the example (`TRANSITIONS_DEMO=check`,
  ≈ 21 s at 60 Hz).
- **The bench** (`examples/transitions/bench.{hpp,cpp}`, `bench_scene.{hpp,cpp}`): workloads `base` (no transition),
  `fade`, `wipe` (Left), `split`, `blinds8`, `iris` (circle, band 4, middle), `loading` (cover, fade, out 0, 300
  covered frames each calling a slice that spins a fixed integer loop inside `NNH_PROFILE_SCOPE("bench slice")`), each
  a `cover()` with out 300 frames (in 0), measured per frame as above. Lines: `transitions bench: <name> avg X ms worst Y
  ms (target < Z ms) PASS|FAIL` (iris adds `rects R`), then `transitions bench: PASS` or `transitions bench: FAIL
  (<names>)`; Debug logs `not judged (Debug)`. `TRANSITIONS_DEMO=bench` starts it; Y in the showcase too.

### The example `examples/transitions` (AC-5)

Original art (`art/make_art.py`: a 16 × 16 tile set for two rooms, the player square, curtain pieces and a
loading-bar frame) and music (`art/make_music.py`, numpy + soundfile as `examples/audio`: a 4-bar 32 kHz Ogg loop
`assets/music/loop.ogg`). Scenes: **title** (press A), **room** ×2 (tile layers on the top screen, the room's map and
HUD on the bottom; a square moved with the Circle Pad), **loading scene** (FR-14: replaces the room, loads in 40 slices
showing a tip and a bar, then starts a second transition to the other room). Controls in a room: D-Pad Left/Right the
effect (fade, wipe, split, blinds, iris circle, iris diamond, curtain = custom with the example's sprites), L the reveal
(reverse, continue), R the screens (both, top only, tall), A a room change, B a room change with a loading step (40
slices, a progress bar drawn by the curtain, hold 20 frames), X a room change through the loading scene, a touch on
the bottom screen the iris centre (logged `iris centre: bottom <x>,<y>`), SELECT the self-check, Y the bench, START
quits. Music: `music::stop(seconds(t.out))` when a room change starts, `music::play("music/loop", {.fade_in =
seconds(t.in)})` from `on_covered` (FR-17). The HUD shows `phase()`, both coverages, `loading_progress()` and
`frames()`. Logs: `transition: start <op> <effect> <screens> <reveal> out O hold H in I`, `transition: done frame F
seen N` (N = inputs the scenes saw while it ran: events and queries counted by the rooms, must be 0), `input: <button>`
for each press a room sees. Screenshot mode: `TRANSITIONS_SHOT=<fade|wipe|split|blinds|iris|diamond|curtain|tall|
loading>` starts the room with that effect and begins a change at frame 20 with out 40 (mid-coverage at frame 40,
full at 60/61, the loading bar from 62).

### The platformer (AC-7)

- `ShowDemo` gains `nnh::Vec2 at{}` (the player's centre in world pixels) and `bool door = false`; `LevelPlay::
  enter_doors()` publishes `ShowDemo{to, centre(player box), true}` (one line). The replay's `ShowDemo{demo}` and SELECT
  are unchanged and stay instant.
- `Demos::on_show`: a door request (and no replay running) becomes `nnh::transition::replace(scene, kDoorTransition)`
  with the out iris at the player's top-screen point, computed from the camera still in place
  (`(at − camera.position) × zoom`); `kDoorTransition` (new `door_transition.{hpp,cpp}`): iris circle, band 4, black,
  `.tall = true`, out 36 frames QuadIn, hold 6, in 36 frames QuadIn, input blocked (default). The demo becomes current
  when the request is accepted; a refused request (a second door the same frame) changes nothing.
- `LevelScene::on_enter()`: after the restart, `if (nnh::transition::running()) nnh::transition::reveal_at(<the
  player's top-screen point from play_.sample()>)` — the iris reopens on the spawn point.
- **Door run** (`door_run.{hpp,cpp}`, new; tested in `tests/platformer/test_door_run.cpp`): Y in a level (or
  `PLATFORMER_DEMO=doors` at start) starts it: `LevelPlay::start_at_door(true)` (one public setter and one line in
  `restart()`: the player starts 24 px left of the level's first door) and the level's input becomes "run right" from
  the run, so each level walks into its door; Demos counts door transitions; after 10 it logs the verdict from a
  `FrameMeter` over the run's frames and stops (the player stands at the last level's door start). Lines: `doors:
  started`, `doors: <n> <from> -> <to> frame <f>` per transition, `doors: done 10 irises frames <F> worst <W> ms avg
  <A> ms: 60 Hz PASS|FAIL`. The same run is the AC-7 screenshot source (`PLATFORMER_DEMO=doors
  NNH_EXIT_AFTER_FRAMES=<f>` at mid-iris, recorded at T6.4).
- **Replay lines: 023 never edits them.** The replay never reaches a door, never starts a transition, and no blocker
  is ever on, so its sums cannot change. The stage-end guard runs ctest `platformer_replay_matches_script` with the
  lines **as they are in the tree at that moment** (spec 021's `B2418A28` if the worktree branched before spec 022's
  T6.3; spec 022's regenerated lines after the merge) and must pass with `examples/platformer/tests/level.nnhs`
  untouched. If it ever fails in this spec's work, the change that did it is wrong (stop and say why in this plan);
  the lines are never regenerated by spec 023. The bench, gate, zoom and flip demos are untouched.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/ease.hpp` | new | `Ease`, `kEaseCount`, `EaseFn`, `ease`, `Easing` (moved verbatim) |
| `engine/core/include/nnh/tween.hpp` | changed | includes `ease.hpp`; the moved block removed |
| `engine/core/include/nnh/detail/finish_event.hpp`, `detail/tween_detail.hpp` | new / changed | `FinishEvent` moved verbatim; `tween_detail.hpp` includes it |
| `engine/core/include/nnh/transition.hpp` | new | the public API above |
| `engine/core/src/transition/transition_internal.hpp` | new | the state struct, resolved effects, `static_assert`s, log channel `transition` |
| `engine/core/src/transition/effect_geometry.hpp` | new | the sink, the area, `q`, `ext`, the per-effect entry points |
| `engine/core/src/transition/effect_fill.cpp` | new | fade, wipe, split, blinds (with continue) |
| `engine/core/src/transition/effect_iris.cpp` | new | integer square root, R0, bands, circle and diamond |
| `engine/core/src/transition/effect_area.cpp` | new | screens vs the tall area, clipping, per-screen resolution, `reveal_at` application |
| `engine/core/src/transition/effect_query.cpp` | new | `effect_rects`, `effect_color`, `max_rects`, `draw_effect` |
| `engine/core/src/transition/transition_timeline.cpp` | new | frames per phase, coverage, ratchets (pure) |
| `engine/core/src/transition/transition_request.cpp` | new | the four requests, validation, resolution, `reveal_at` |
| `engine/core/src/transition/transition_frame.cpp` | new | the hook (frame, cancel), stack change, finish |
| `engine/core/src/transition/transition_overlay.cpp` | new | built-in and custom drawing per screen |
| `engine/core/src/transition/transition_loading.cpp` | new | the loading step, hold, the 10-second warning |
| `engine/core/src/transition/transition_queries.cpp` | new | queries, `timing`, `seconds` |
| `engine/core/src/scene/scene_hook.hpp` | new | `SceneTransitionHook`, `g_scene_transition`, pending-count access |
| `engine/core/src/scene/scene_stack.{hpp,cpp}`, `scenes.cpp` | changed | `update_top()`/`draw_all()` public, `Op::TransitionPop`, the hook check, `run()`'s condition, cancel before clear |
| `engine/core/include/nnh/scene.hpp` | changed | comments only ("only the top scene updates — except during a transition") |
| `engine/core/include/nnh/input.hpp` | changed | `set_blocked`, `blocked` |
| `engine/core/src/input/input_block.{hpp,cpp}` | new | `detail::InputBlock` (pure filter), the public functions, `detail::input_block(InputBlocker, bool)` |
| `engine/core/src/input/input.cpp`, `input_internal.hpp` | changed | the filter pointer check in `input_update`, engine masks |
| `engine/core/src/app/app.cpp` | changed | `overlay_update` gets the engine masks |
| `engine/core/src/gfx/gfx_frame.cpp` | changed | `NNH_PROFILE_SCOPE("gfx submit")` (Development/Debug only) |
| `engine/platforms/fake/include/nnh/platform_fake.hpp` | changed | `kMaxRecordedItems` 64 → 1024 |
| `engine/core/CMakeLists.txt` | changed | `src/input/input_block.cpp` (main list); a `_nnh_transition_sources` block with `-ffp-contract=off` |
| `tests/transition/transition_fixture.hpp`, `test_effect_fill.cpp`, `test_effect_iris.cpp`, `test_effect_area.cpp`, `test_effect_query.cpp`, `test_geometry_properties.cpp`, `test_timeline.cpp`, `test_requests.cpp`, `test_scene_updates.cpp`, `test_stack_change.cpp`, `test_overlay_order.cpp`, `test_custom_effect.cpp`, `test_loading.cpp`, `test_notifications.cpp`, `test_interruptions.cpp`, `test_plain_loop.cpp`, `test_no_wrong_frame.cpp`, `test_transition_input.cpp`, `test_transition_actions.cpp`, `test_transition_cost.cpp` | new | AC-1, AC-2, AC-4 |
| `tests/input/test_input_block.cpp`, `test_input_block_api.cpp` | new | the filter, the public block, engine controls |
| `tests/scene/test_scene_hook.cpp` | new | stack halves = `update_and_draw`, tolerant pop, delegation, `run()`'s condition |
| `tests/transitions_example/test_self_check.cpp` | new | AC-3 golden |
| `tests/fuzz/fuzz_transition_geometry.cpp` | new | random effects, coverage bits, placements → bounds, counts, no UB |
| `tests/platformer/test_door_run.cpp` | new | door transitions, the replay's ShowDemo stays instant, the door start |
| `tests/CMakeLists.txt` | changed | one `target_sources` block for all of the above; ctests `transitions_self_check_pc`, `transitions_check_script_pc`, `platformer_doors_script_pc` |
| `examples/transitions/` | new | `main.cpp`, `demos.hpp` (`TRANSITIONS_DEMO`, `TRANSITIONS_SHOT`), `options_menu.{hpp,cpp}`, `title_scene.{hpp,cpp}`, `room_scene.{hpp,cpp}`, `loading_scene.{hpp,cpp}`, `curtain.{hpp,cpp}`, `hud.{hpp,cpp}`, `input_log.{hpp,cpp}`, `check_script.{hpp,cpp}`, `check_scenes.{hpp,cpp}`, `check_scene.{hpp,cpp}`, `bench.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `art/make_art.py`, `art/make_music.py`, `assets/…`, `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(transitions)` |
| `examples/platformer/game_events.hpp`, `demos.{hpp,cpp}`, `level_scene.{hpp,cpp}`, `main.cpp`, `door_transition.{hpp,cpp}`, `door_run.{hpp,cpp}`, `CMakeLists.txt`, `tests/doors.nnhs` | changed / new | doors (above) |
| `examples/platformer/level_play.{hpp,cpp}` | changed | two lines: the door's `ShowDemo` fields; `start_at_door` |
| docs (stage 11) | new / changed | `docs/api/transition.md`, `docs/images/transitions/*.png` (new); `scene.md`, `input.md`, `actions.md`, `audio.md`, `tween.md`, `gfx.md` (the overlay's layer), `architecture/scenes.md`, `decisions.md` (D38 implemented), `patterns.md`, `docs/REFERENCES.md`, `CLAUDE.md`, `docs/ROADMAP.md`, `specs/README.md` |

### Platform work

- **pc / 3ds:** none (portable core; the overlay is rectangles through the existing contract). The 3DS runs the
  check, the showcase, the bench and the platformer's door run (stage 10).
- **fake:** `kMaxRecordedItems` 1024 (AC-2 reads whole frames). Everything else is tested on it as is.

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows, VM 120 with GCC 13/14 and clang 18, ASan/UBSan). The
fixture builds scenes that record their calls (`on_enter`, `on_exit`, `update`, `draw`) with frame numbers, drives
`nnh::frame()` + `scenes::update_and_draw()` (or `nnh::run()` with a frame budget), and reads the fake's recorded items.
A brute-force rasteriser (a W × H byte map) checks coverage claims in AC-2 and the property test.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `test_requests.cpp`: each request starts and returns true; `scenes::push/pop/replace` unchanged and instant (`test_scene_stack.cpp` untouched) |
| FR-2 | `test_timeline.cpp`: frame counts O + C + I for many (O, H, I) incl. zeros and all zero (one covered frame); first frame rule (request in `update()` vs in an event handler) |
| FR-3 | `test_timeline.cpp`: whole-frame rounding of `Duration`s and `Frames`, 10 s limit, each easing per phase, coverage monotonic with Back/Elastic/Bounce (ratchet), in = out backwards frame for frame; `test_plain_loop.cpp`: a long frame (fake time jump) skips nothing |
| FR-4 | `test_effect_fill.cpp`, `test_effect_iris.cpp`: rectangles at q = 0, 65536 and fixed points; wipe ×4, split ×2, blinds 2/8/32 × 4 edges, iris circle/diamond with centres inside, on an edge, outside, band 1/4/16; `test_geometry_properties.cpp`: inside the area, nothing at 0, full cover at 1 (rasterised), covered area monotonic in q, counts ≤ `max_rects` |
| FR-5 | `test_effect_area.cpp`: reverse (in = out backwards), continue (opposite edge, centred split band), own reveal effect, colour, `reveal_at` centre |
| FR-6 | `test_effect_area.cpp`: per-screen effects, `none()`, the resolution rules, the tall area with gaps 0 and 24 (top/bottom clipping, the inset, nothing in the gap) |
| FR-7 | `test_overlay_order.cpp`: overlay items after every scene's items, above a scene's layer-15 draw, below debug text (the debug pass), `ignore_camera` with a moved and zoomed camera, a game draw after `update_and_draw()` above it |
| FR-8 | `test_custom_effect.cpp`: called once per Custom screen per frame with screen, coverage, phase, frame, loading, tall; nothing built-in drawn for it; a lambda with 16 bytes of captures |
| FR-9 | `test_effect_query.cpp`: `effect_rects` with no init and no stack, capacity smaller than the count (returns the total, writes `capacity`), `effect_color`, `max_rects`, `draw_effect` recorded items equal `effect_rects` |
| FR-10 | `test_scene_updates.cpp`: no update in any phase by default; `update_out`/`update_in`; the incoming scene's first `update()` the frame after done; a bound `TweenSet` (D30 `BeforeUpdate`) freezes exactly when its scene doesn't update; `AfterExit` after `on_exit()` |
| FR-11 | `test_stack_change.cpp`: `on_exit`/`on_enter` order and frames; on_enter work (fake time jump) skips no frame; a waiting scene destroyed → crash caught (`run_expecting_crash`); the target pushed elsewhere meanwhile → caught; `test_no_wrong_frame.cpp` (AC-2) |
| FR-12 | `test_input_block.cpp` (pure filter), `test_input_block_api.cpp` (public block, game and transition blockers independent, overlay toggle works while blocked), `test_transition_input.cpp` (queries idle, no button or touch events, held button/stick/touch blocked until released, remote input blocked, `block_input = false`) |
| FR-13 | `test_loading.cpp`: called after `on_covered`, once per covered frame, progress clamped/NaN, passed to the custom effect and `loading_progress()`, covered until done and the hold, the warning at covered frame 600 (once, Debug/Development) |
| FR-14 | `test_stack_change.cpp`: a loading scene that replaces, works in slices and starts a second transition from its `update()`; guarantees hold for both; the example's loading scene |
| FR-15 | `test_requests.cpp` / `test_timeline.cpp`: every query per frame, `timing()`, `seconds()` |
| FR-16 | `test_notifications.cpp`: `on_covered`, `on_finished` and the event exactly once at the documented frames; none on cancel; a request from `on_finished` starts the next frame; callbacks may query |
| FR-17 | the example (music patterns); docs |
| FR-18 | `test_plain_loop.cpp`: a hand-written loop with `scenes::update_and_draw()` gives the same frames as `nnh::run()`; `cover()` with an empty stack and no scenes draws its overlay |
| FR-19 | `test_interruptions.cpp`: refused requests (false, one debug log), plain changes during each phase applied, the transition's pop after the game emptied the stack (tolerant: nothing, error log), pop of the last scene (covered phase runs, `run()` returns, no in phase), `shutdown()` and the end of `run()` in each phase (silent, waiting scene released, block lifted) |
| FR-20 | AC-3's golden on WinLibs GCC, GCC 13/14, clang 18, pc-release and the console |
| FR-21 | `test_transition_cost.cpp`: state size `static_assert`s, the allocation counter unchanged across 1000 transitions, no log line in the normal path, `max_rects` bounds; AC-8 |
| FR-22 | `test_requests.cpp`: each misuse asserts and is clamped/refused on the Release path (pc-release run) |
| FR-23 | include check (`transition.hpp` pulls no tween set, audio, tilemap, actions, camera, entities header); T8.3 probes (`hello` and a scenes-only game link no transition or block code); the input path's cost read in the disassembly |
| AC-2 | `test_no_wrong_frame.cpp`: replace/push/pop/cover × every built-in effect and a custom one, per frame from the fake's submitted items |
| AC-3 | `tests/transitions_example/test_self_check.cpp`, ctest `transitions_self_check_pc`, the console's `transitions: PASS` |
| AC-4 | `test_transition_actions.cpp`: an `ActionSet` with contexts bound to both scenes |
| AC-5 | PC screenshots read back (T7.2–T7.3) |
| AC-6 | `examples/transitions/tests/check.nnhs` with `--pc`, then on the console |
| AC-7 | `test_door_run.cpp`, ctest `platformer_replay_matches_script` unchanged, `doors.nnhs --pc`, a screenshot read back |
| AC-8 | the bench lines on the console; T7.7's table; `doors: … 60 Hz PASS`, `gate.nnhs`, the replay's 60 Hz verdict |
| AC-9 | stage 0 references compared after stages 2, 5 and 6; `hello` sizes; probes; build times |
| AC-10 | stage 11 |

## Risks and alternatives

1. **The iris's worst frame** (1.05–1.60 ms estimated, 2 ms target) depends on citro2d's per-rectangle cost, never
   measured alone. Mitigations in order, after the console says so: (a) an internal batched record
   (`detail::gfx_record_rects`: one command built, positions patched; saves ≈ 30 % of the record step); (b) the iris
   bands' square root by an incremental update from the previous band (fewer instructions per band); (c) merging
   vertically adjacent band pieces with equal extents (changes the geometry: AC-3's golden re-recorded on the PC
   before any console run); (d) AC-8's measurement-with-margin policy. Option (b) of the spec's iris decision (a mask
   texture) is the fallback only if (a)–(c) leave the average over 2 ms.
2. **The scene stack's contract changes** (the top scene doesn't update during a transition). Covered by D38, the
   docs, `test_scene_updates.cpp`; games that never request a transition see no change (`test_scene_stack.cpp`,
   `test_run.cpp` untouched and passing).
3. **The input module's sample path changes for everyone** (one pointer check). Spec 003/017 tests, every example's
   `--pc` script and the actions console script stay green; T7.7 reads `input_update`'s code; `hello`'s growth is
   measured (budget 96 bytes on the 3DS).
4. **Header moves** (`Ease`/`Easing`, `FinishEvent`) could break a game including only `detail/tween_detail.hpp`
   (not a public use) — `tween.hpp` and `tween_detail.hpp` include the new headers, so every existing include compiles;
   `tweens: PASS B44CA3F6` and spec 015's tests are the check.
5. **Fade colour ±1** between GPUs: AC-6 compares geometry exactly and fade colours within one step (documented).
6. **`max_draws` is shared**: a game near its capacity can overflow during an iris with 1-pixel bands (482 per
   screen); documented with each effect's count; the assert names `max_draws`.
7. **The pop of the last scene** keeps `run()` looping through the covered phase with an empty stack: a game that
   relies on `run()` returning the frame the stack empties sees it a few frames later (documented; the in phase is
   skipped).
8. **Merging with spec 022** (parallel worktree): see "Merge notes"; the platformer's `level_play.*` is the only file
   both specs change in the same functions' neighbourhood.
9. **Overshooting easings** show plateaus instead of overshoot (coverage can't exceed 1): documented; Back/Elastic
   still shape the curve's start.

## Merge notes (parallel worktree with spec 022)

Spec 023 is built in its own git worktree (branch `spec-023`, from `main` at this plan's commit) while spec 022 is
implemented on `main`; it is merged after both are done. Rules for the 023 side:

| Shared file | 022's change | 023's change, kept small and apart |
|-------------|--------------|-------------------------------------|
| `engine/core/CMakeLists.txt` | `_nnh_transform_sources`/`_nnh_hierarchy_sources` appended after the prefabs block; new entities/prefab/gfx files | one line `src/input/input_block.cpp` after `src/input/input.cpp` in the main list; the `_nnh_transition_sources` block inserted **after the `_nnh_camera_sources` block** (not at the end) |
| `tests/CMakeLists.txt` | `transform/`, `hierarchy/` lines in the alphabetical list, `hierarchy_example` after `prefabs_example`, ctests near the prefabs ones, platformer test lines in `_platformer` | **one** `target_sources(nnh_tests PRIVATE …)` block holding every 023 test (transition, input, scene, platformer, example) placed **right after the `camera_example/test_self_check.cpp` line**; 023's ctests right after the `camera_self_check_pc` block; nothing in the alphabetical list or `_platformer` |
| `examples/CMakeLists.txt` | `add_subdirectory(hierarchy)` | `add_subdirectory(transitions)` after `text` |
| `engine/core/src/tween/*`, tween headers | `ease_math.{hpp,cpp}` (`sin_quarter` moved) | only `include/nnh/tween.hpp` and `include/nnh/detail/tween_detail.hpp` (blocks moved out); no file in common |
| `examples/platformer/level_play.{hpp,cpp}` | barrels: constructor, `update()` (the pass after the turrets), `draw_turrets`, members | exactly two one-line hunks in the `.cpp` (`enter_doors`' publish, `restart`'s spawn point) and one setter + one member in the `.hpp` placed next to the door members (`in_door_`); re-apply by hand on conflict |
| `examples/platformer/CMakeLists.txt` | maybe `turret` art | two source names appended to the `SOURCES` list |
| `examples/platformer/tests/level.nnhs` | **regenerated** (T6.3) | **never touched**; after the merge, ctest `platformer_replay_matches_script` and `level.nnhs --pc` must pass with 022's lines |
| `docs/ROADMAP.md`, `CLAUDE.md`, `docs/REFERENCES.md`, `docs/architecture/decisions.md` change log, `specs/README.md` | their own lines/paragraphs | 023's own checkbox line; its CLAUDE.md paragraph after the "Camera helpers" paragraph and its command after the `queries` command; its REFERENCES block appended; one change-log entry appended; its own index row — on conflict keep both sides |
| `docs/architecture/scenes.md`, `patterns.md` | the "not a scene tree" section; its own row | a "Transitions (D38)" section between "Frame flow with scenes" and "Scene attachments (D30)" plus the frame-flow line; 023's row (already on `main` with this plan) |

After the merge, the lead runs: the full ctest (pc-debug, pc-release), every example's `--pc` script, AC-3's golden,
spec 022's goldens and the replay ctest with 022's lines; and one 3ds-release build. Neither spec's goldens may move
because of the other (023 changes no entity, transform or prefab code; 022 changes no scene, input or gfx-submit code).

## Stage 10: console run (prepared)

T10.1 on the PC first: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh cmake --build --preset
3ds-dev --target transitions_3dsx platformer_3dsx actions_3dsx`, without a warning; T7.7's table still describes the
objects; 3ds-release `hello.elf` text within T0.1 + 96 bytes. Then on an Old 3DS, nobody touching it after Y. Four
runs, each: open the Homebrew Launcher, press **Y**, then on the PC (`nnh` builds the Development `.3dsx`, sends it,
plays the script and brings the console back to the Homebrew Launcher; logs and screenshots in `build/logs/`):

1. `build/pc-debug/tools/nnh/nnh run transitions --script examples/transitions/tests/check.nnhs` (≈ 1.5 min).
   Expected, as on the PC (pc-debug), with the values recorded in "Stage 5 results" and "Stage 7 results":
   `transitions: check started`, `transitions: 120 sum <c1>` … `transitions: 1200 sum <c10>`, **`transitions: PASS
   <golden>`**; then the showcase: `transitions: showcase`, for each scripted change `transition: start replace fade
   both reverse out 24 hold 0 in 24` (and the iris, tall and loading variants the script uses), each followed by
   `transition: done frame <f> seen 0` (the B pressed during it never appears as `input: B`), `iris centre: bottom
   100,120` after the touch, the loading change's `transition: done … seen 0`; then `transitions bench: started`,
   `transitions bench: base gfx submit avg X ms`, `transitions bench: fade avg X ms worst Y ms (target < 0.1 ms) PASS`,
   `wipe …`, `split …`, `blinds8 … (target < 0.3 ms)`, `iris avg X ms worst Y ms rects R (target < 2 ms)`, `loading
   engine avg X ms worst Y ms (target < 0.05 ms)`, the verdict `transitions bench: PASS` or `transitions bench: FAIL
   (<names>)`; `nnh: PASS (<n> lines, …)`. **Expected from the ARM pre-check** (T7.7 refines it): every logic line
   equal to the PC's; fade/wipe ≈ 0.01–0.02 ms, split ≈ 0.02–0.03, blinds8 ≈ 0.07–0.11, iris avg 0.6–0.95 (worst
   1.05–1.6, rects 244), loading ≈ 0.012–0.02 — PASS; the iris worst is the one at risk. A FAIL fails the run (`fail-on
   "transitions bench: FAIL"`) after every number is logged: record them in "Console run 1"; T10.3 applies risk 1's
   list, or the budgets are set from the measurement with margin. Read the screenshots (mid-coverage of each effect,
   full cover, the tall wipe, the loading bar): the same geometry as the `--pc` run's at the same frames, fade colours
   within one step.
2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/doors.nnhs` (≈ 40 s). Expected, as on
   the PC: `doors: started`, ten `doors: <n> <from> -> <to> frame <f>` lines with the PC's frames (recorded in "Stage
   6 results"), `doors: done 10 irises frames <F> worst <W> ms avg <A> ms: 60 Hz PASS`; the extra door entry's
   screenshot `door-iris` (the iris closing on the player over both screens); `nnh: PASS`.
3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` (≈ 40 s). Expected:
   exactly the `replay: … sum` lines **in the tree at that time** (after the merge: spec 022's), `replay: done 900
   frames sum <last>: 60 Hz PASS`; `nnh: PASS (79 lines, …)`. A different sum is a defect (023 must not move it).
4. `build/pc-debug/tools/nnh/nnh run actions --script examples/actions/tests/check.nnhs` (≈ 30 s). Expected: every line
   as before spec 023 (the input path's filter check changed nothing for a game that never blocks); `nnh: PASS`.

(`gate.nnhs` is re-run only if T8.4's PC numbers moved: 023 changes no tile or sprite code.)

## Decisions taken (overnight, to review)

The spec's "Decisions to take" are approved as recommended. Its open questions and these plan-level decisions were
taken overnight, each the recommended or simplest option; the maintainer reviews them in the morning:

1. **Names**: module `transition` (`<nnh/transition.hpp>`, `nnh::transition::replace/push/pop/cover`, `reveal_at`,
   `running/phase/coverage/phase_progress/loading_progress/frames/timing/seconds`, `Options`, `Effect` and its makers
   `same/none/fade/wipe/split/blinds/iris/custom`, `Kind`, `Edge`, `SplitAxis`, `IrisShape`, `Reveal`, `Phase`,
   `IrisCenter`, `CustomFrame`, `LoadStep`; FR-9's `effect_rects`, `effect_color`, `max_rects`, `draw_effect`,
   `Placement`); input `set_blocked`/`blocked`; example `examples/transitions` (`TRANSITIONS_DEMO`, `TRANSITIONS_SHOT`).
2. **Open question 1:** the input block is **public** (`input::set_blocked`, `input::blocked`); the game's and the
   transitions' blocks are separate blockers (one never lifts the other).
3. **Open question 2:** the fade is a **rectangle on every system**; `C2D_Fade` is not used.
4. **Open question 3:** iris centres are **screen points** (`IrisCenter{point, screen}`).
5. **The first frame** is the first `scenes::update_and_draw()` after the request; out frames show `ease(k/O)` (full at
   k = O), in frames `ease((I − k)/I)` (0 at k = I); the change is enqueued at the end of the last out frame (at the
   request when O = 0); at least one covered frame.
6. **Durations** are rounded to whole frames (half up); defaults out = in = 24 frames (0.4 s), hold 0.
7. **Overshooting curves** are clamped to [0, 1] and ratcheted (monotonic coverage, plateaus).
8. **Integer geometry** from a 16.16 coverage; colours' own alpha ignored (covers are opaque); the fade's alpha
   `round(c × 255)`.
9. **Effect definitions** as in "Geometry" (split halves ⌈e/2⌉/⌊e/2⌋; blinds boundaries ⌊i·L/N⌋; the iris sampled at
   each band's middle row, `R0 = ⌈√max corner distance²⌉`, fully covered rows merged above and below; continue =
   opposite edge for wipe and blinds, a centred band for split, nothing for the iris).
10. **The tall area** is 400 × (480 + gap), the bottom screen at x 40; gap 0..240 whole pixels; fade and custom stay
    per screen; per-screen effects can't be combined with `tall`.
11. **Per-screen resolution** with `same()` as above; `effect` can't be `same()`.
12. **One custom callable** for both phases, called every transition frame for each Custom screen (coverage 0 too).
13. **The hook** is one pointer to a constant `{frame, cancel}` set only while a transition runs;
    `scenes::update_and_draw()` delegates; `run()` keeps looping while a transition runs on an empty stack; the cancel
    runs before the stack is cleared.
14. **Pop of the last scene**: the covered phase runs (on_covered, loading, hold) with no scene, then the transition
    finishes (on_finished, event queued) and `run()` returns; manual loops use `while (frame() && (scenes::size() > 0
    || transition::running()))` (documented).
15. **The transition's pop is tolerant** (nothing on a stack the game emptied, an error log) through a new
    `Op::TransitionPop`; push/replace keep spec 001's checks.
16. **A refused request** logs at debug level once per running transition.
17. **Loading**: `LoadStep{progress, done}` from a `SmallFunction`; progress clamped, NaN → 0; the warning at covered
    frame 600, once.
18. **Input block semantics**: from the first transition frame's input to the frame after it is done; held buttons,
    sticks out of the dead zone and touches filtered until released; `touch_position()` keeps the last unfiltered
    point; the D-Pad mirror follows the filtered Circle Pad; engine controls read raw masks; a filter pointer in
    `input_update` (one load and branch for games that never block).
19. **FR-18's per-frame step** for games without scenes is `scenes::update_and_draw()` itself (it runs a transition
    with an empty stack).
20. **`transition::reveal_at()`** is added (the reveal's iris centre is known only once the new scene is in).
21. **No "Done" phase value**: the last in frame reports In with coverage 0; `running()` is false from `on_finished` on.
22. **Header moves**: the easing curves to `<nnh/ease.hpp>`, `FinishEvent` to `<nnh/detail/finish_event.hpp>`, both
    included by the tween headers (source-compatible).
23. **State size**: 244 B on the 3DS (≤ 256, FR-21), 308 B on 64-bit PCs (≤ 320: pointers double), `static_assert`ed.
24. **gfx gains a Development-only profile scope** `gfx submit`, so the bench measures submit cost; Release unchanged.
25. **The fake platform records 1024 items** (was 64).
26. **Bench method**: each workload a 300-frame out phase, judged on its average with the base run's submit cost
    subtracted, the worst logged (and the iris's largest rectangle count).
27. **Platformer**: `ShowDemo{demo, at, door}`; doors go through `transition::replace` with a tall iris (band 4, 36/6/36
    frames, QuadIn) when no replay runs; the new level calls `reveal_at` in `on_enter()`; a door run (Y,
    `PLATFORMER_DEMO=doors`, levels starting at their first door, 10 irises, a 60 Hz verdict) and `tests/doors.nnhs`;
    the replay lines are never edited.
28. **Example layout and controls** as described above (Old 3DS buttons only).
29. **Console stage**: four scripted runs (transitions `check.nnhs` with the bench; platformer `doors.nnhs`;
    `level.nnhs` with the tree's lines; actions `check.nnhs` for the input path).
30. **AC-8 targets kept as written**; the iris's worst frame is flagged at risk.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Portable core; whole frames; one IEEE division and spec 015's `ease()` per phase frame; integer geometry; `-ffp-contract=off`; AC-3 on every compiler and the console; the fade's GPU blend difference documented |
| II | ✅ | 244 B of state, no allocation; per-rectangle cost counted all-in with misses; merged rows, no per-band division; disassembly checked before the console; the iris's worst frame at risk with AC-8's fallback |
| III | ✅ | A light header (gfx, time, ease, two small detail headers); measured budgets |
| IV | ✅ | No engine events; queries; the game's own finish event |
| V | ✅ | No allocation, `virtual`, RTTI or exceptions; one profile scope |
| VI | ✅ | Tests first; a rasteriser for coverage; AC-2 per frame; AC-3 golden; mutation pass |
| VII | ✅ | One module of 10 small sources; one file each for the block and the hook |
| VIII | ✅ | API page with pictures, linked pages, example, platformer doors, D38 |
| IX | ✅ | Own code, prior art cited, original art and music |
| X | ✅ | No parser; misuse loud in Debug, harmless in Release; a stuck loader warns; geometry fuzzed |
| XI | ✅ | One null check in the scene path, one in the input path; nothing linked without a request or a block; `scenes::replace` instant; `hello` within 96 bytes |

No constitution exception is requested. D38 is new (see [decisions.md](../../docs/architecture/decisions.md)); D30 is
not amended (its notifications follow the scene's `update()`); D32 is not amended (actions read the filtered
queries); D28 noted (remote input is game input and is filtered); spec 001's "the top scene updates every frame" gains
D38's exception. The patterns page gains an "Optional frame hook" row (D30, D32, D38). The iris's worst frame is
flagged at risk and keeps the spec's measurement-with-margin fallback.

## Stage 0 results (2026-11-24, worktree branch from `main` at 117b27b)

Windows, WinLibs GCC, Ninja; devkitARM through `scripts/dkp.sh`. Scripts in `build/ref-023/` (not committed):
`measure.sh` (sizes, `nm` lists, disassembly), `headers.sh` (header costs), `objsum.sh` (object disassembly sums),
`edit_time.sh`, `compare.sh` (AC-9 screenshots).

| Item | Value |
|------|-------|
| ctest | pc-debug 268/268, pc-release 258/258; `nnh_tests` 2353 tests (`kMaxTests` 4096: room for this spec's ≈ 200) |
| Warnings | 0 in pc-debug, pc-release, 3ds-release, 3ds-dev |
| `hello` | pc-release `.exe` 278 856 B (text 130 732), pc-debug 1 304 762 B; 3ds-release `.elf` text **201 188** (`.3dsx` 246 576 B); 3ds-dev `.3dsx` 268 108 B |
| 3ds-release `.text` | `scenes` 206 492, `actions` 246 092, `platformer` 361 880 |
| Headers (one-line TU, `g++ -E` lines / `-fsyntax-only` best of 3) | `scene.hpp` 565 / 300 ms, `gfx.hpp` 779 / 303 ms, `tween.hpp` 8 413 / 451 ms |
| Edit `examples/scenes/title_scene.cpp` | simulator 2.0–2.3 s, `.3dsx` 3.3 s |
| Goldens | replay `B2418A28` (spec 021's lines: this tree predates spec 022's T6.3), tweens `B44CA3F6`, entities `C8FB9A41`, prefabs `7BB7901E` |
| AC-9 references | `build/ref-023/ref/`: hello, sprites, scenes title (30) and level (90, `NNH_SIM_INPUT=buttons=A`), tweens, actions, camera `zoom1`, platformer level 1 (90), replay 64 and 312; `compare.sh` retakes them identical |
| Disassembly | 3ds-dev `input.o`, `scenes.o`, `scene_stack.o` in `build/ref-023/m-t0/dis/`; `input_update` is 54 instructions with one call (`shape_stick`) per stick; 3ds objects are built with one section per function (the linker drops unreferenced functions) |

## Stage 1 results

- `<nnh/ease.hpp>` holds `Ease`, `kEaseCount`, `EaseFn`, `ease()` and `Easing` verbatim; `<nnh/tween.hpp>` includes it.
  A one-line TU including only `<nnh/ease.hpp>` preprocesses to `<cstdint>` and the system headers it pulls (461
  lines, 314 ms). `FinishEvent` moved verbatim to `<nnh/detail/finish_event.hpp>`, included by `tween_detail.hpp`.
- The 3ds-dev disassembly of every `src/tween/*.o` is identical to stage 0's (`objsum.sh`); `tweens: PASS B44CA3F6`
  (`check.nnhs --pc`); spec 015's tests and compile-fail tests pass unchanged; stage-end guards green (ctest 268/258,
  0 warnings on the four presets, actions and input scripts PASS, `compare.sh` identical, docs 0 broken links).
- `tween.hpp` grew by 32 preprocessed lines (the new headers' comments): 8 445, `-fsyntax-only` unchanged.
- Refinement (tooling): the actions example saves rebound controls (`controls.bin`) when its script runs, which
  changes its screenshot; `compare.sh` deletes that file before retaking the reference.

## Stage 2 results and refinements

- Tests: `tests/input/test_input_block.cpp` (9, the pure filter) and `test_input_block_api.cpp` (9: queries idle, no
  input event while blocked — not even the release of what was held when the block started —, held controls wait for
  their release, `touch_position()` keeps the last point, the game's block never lifts a transition's, init lifts the
  block, the overlay toggle works while blocked, remote input blocked, actions idle and held bindings waiting); seen red
  with a stub filter, then green. `nnh_tests` 2371, ctest 268/258, guards green.
- **Refinement: the raw buttons for engine controls live in `input.cpp`** (an 8-byte `InputState engine`, updated
  every frame) instead of in `InputBlock`: `detail::input_engine_held_mask()`/`input_engine_pressed_mask()` always
  return the raw buttons (the old masks differed only by the D-Pad mirror's bits, which SELECT + L never reads), and
  `InputBlock` is 8 bytes (filtered mask, blockers, three flags).
- **Refinement: the filter applies the sample itself.** `detail::g_input_filter` is `void (*)(const InputSample&)`;
  when set, `input_update()` hands it the raw sample and returns; the filter copies, filters, calls
  `detail::input_apply()` and, while blocking, `detail::input_forget_edges()` (a blocked frame has no edge, so the
  buttons held when the block starts are not "released"). Both helpers are in `input.cpp` but referenced only by
  `input_block.cpp`, so the 3DS linker drops them from games that never block. A first version (the filter returning
  a filtered copy to `input_update()`) cost 160 bytes of `hello` text; this one costs **56 bytes** (201 188 → 201 244,
  within the 96-byte budget): `input_update` is now 11 instructions (the engine mask, one load and branch, a tail call
  to `input_apply`).
- `hello`'s 3ds-release `nm` list differs only by the input path: `+ input_apply`, `+ g_input_filter`,
  `+ input_engine_held_mask`, `+ input_engine_pressed_mask`, `− input_pressed_mask`, `− input::held_mask` (no longer
  called by `app.cpp`); no `InputBlock` symbol. pc-release `hello.exe` +812 bytes (the PC links whole objects).
- `input_reset()` (from `nnh::init()`) clears the filter pointer: a block never survives `shutdown()`/`init()`.

## Stage 3 results and refinements

- Tests (42, seen red with stubbed geometry, then green): `test_effect_fill.cpp` (q and ext rounding, fade alpha table,
  wipes ×4 and continued, split ⌈e/2⌉/⌊e/2⌋ and the continued centred band, blinds ⌊i·L/N⌋ for 2/7/8/32 bars both
  ways and continued, clamps), `test_effect_iris.cpp` (isqrt exact below 2²⁰ and on 200 000 seeded values, the
  documented bands of a half-closed circle and diamond at (200, 120), merged rows, centres inside/outside/±4096, bands
  1/4/16, counts), `test_effect_area.cpp` (tall wipes with gaps 0 and 24, the inset, the gap drawn nowhere, a tall iris
  on a bottom point, the per-screen resolution table), `test_effect_query.cpp` (no init, capacity, colour, `max_rects`,
  `draw_effect` = `effect_rects` with a moved and zoomed camera), `test_geometry_properties.cpp` (2000 seeded effects ×
  10 growing coverages: inside, disjoint, area monotonic, full at 1, counts; area summed over disjoint rectangles
  instead of a rasteriser: 0.3 s in Debug instead of 16 s), mini-fuzz `test_fuzz_transition_geometry.cpp` (300 random
  inputs, NaN/∞/huge centres and coverages, enum bytes out of range) on `tests/fuzz/fuzz_transition_geometry.cpp`
  (`fuzz_transition_geometry` with `NNH_BUILD_FUZZERS`, declared in 023's CMake block).
- **Refinement: the sink takes an internal `PixelRect` (ints)**; `effect_rects` converts to `Rect` (exact).
- **Refinement: `ResolvedEffect` is 14 bytes** (kind, direction, shape, bars, band, continued, centre `i16` × 2,
  colour); the centre is resolved per screen (non-tall: the point in that screen's coordinates, default its middle;
  tall: in area pixels, a bottom-screen point moved by (40, 240 + gap)).
- **Refinement: `max_rects(iris, tall)` counts one more band** (2 × (⌊(240 + band − 2) / band⌋ + 1) + 2: a band
  may straddle the bottom screen's top edge when the gap isn't a multiple of the band); band 4: 122 per screen, 124
  in the tall area (FR-21's 128 holds); band 1: 482 either way.
- The iris visits only the bands that meet the target screen's rows (one division per screen for the first band).
- `<nnh/transition.hpp>` pulls only `ease`, `gfx`, `math`, `time`, `assert`, `build` and the two small detail headers
  (no tween set, audio, tilemap, actions, camera or entities); 8 476 preprocessed lines / 432 ms `-fsyntax-only`
  (`small_function.hpp`'s `<new>`/`<type_traits>`/`<utility>` are most of it; `scene.hpp` + `gfx.hpp`: 565 + 779).
- Guards green (ctest 268/258, `nnh_tests` 2413, 0 warnings on four presets, scripts PASS, `compare.sh` identical);
  `hello` unchanged since stage 2 (text 201 244, same `nm` list).

## Stage 4 results and refinements

- Tests: `tests/scene/test_scene_hook.cpp` (9: the halves equal `update_and_draw()` call for call, nothing on an empty
  stack, the transition change through the deferred path, the tolerant pop, delegation to a test hook, the cancel at
  `shutdown()` and at the end of `run()`, `run()` looping while the hook runs on an empty stack, the pending count),
  `tests/transition/test_timeline.cpp` (11), `test_requests.cpp` (11: each request, plain changes instant, the refusal
  logged once per running transition, each misuse asserting or clamped/refused on the Release path, queries frame by
  frame, `reveal_at`, the waiting scene's count); `test_scene_stack.cpp`, `test_run.cpp`, `test_scene_attachments.cpp`
  unchanged and green. `nnh_tests` 2444; guards green.
- **Refinement: the transition's tolerant pop logs in the transition module**, not in the stack:
  `Op::TransitionPop` pops nothing on an empty stack silently, and `transition_enqueue_change()` logs the error when it
  enqueues a pop on a stack the game emptied. The stack's log string cost `hello` 100 bytes (every game links
  `apply_pending`); without it the scene hook costs `hello` **32 bytes** (`scenes_shutdown`'s cancel check and the
  4-byte pointer): **`hello` +88 bytes of 3DS text since stage 0** (201 188 → 201 276, budget 96). `scenes`: `+`
  the null check in `update_and_draw` (8 bytes: `0x20`), `run()` 132 bytes.
- **Refinement: queries read between frames show the state the next frame starts in** (the end of a frame already
  moved the phase: after the last out frame, `phase()` is Covered with `phase_progress()` 0). Inside a frame (scenes'
  `update`/`draw`, callbacks, custom effects) they are that frame's, as the tests read them.
- **Refinement: a change enqueued during the frame it is requested in** (O = 0 and a request before
  `update_and_draw()`, from an event handler or `on_enter()`) is applied at the start of the next frame, so that
  first frame is drawn covered without counting as a covered frame (on_covered always follows the change).
- **Refinement: the in phase is skipped only over a stack the transition's own change emptied** (`op != Cover` and an
  empty stack): a `cover()` in a game without scenes still reveals (FR-18).
- `TransitionState`: 248 bytes on 32-bit (the callbacks 96, `FinishEvent` 40, four resolved effects 56, easings 16,
  timeline 20, counters and flags), ≤ 320 on 64-bit; `static_assert`ed.

## Stage 5 results and refinements

- Tests (42 more; `nnh_tests` 2486 → 2489 with stage 6): `test_scene_updates` (4), `test_stack_change` (5),
  `test_overlay_order` (4), `test_custom_effect` (3), `test_loading` (4), `test_notifications` (3),
  `test_interruptions` (6), `test_plain_loop` (3), `test_transition_input` (4), `test_transition_actions` (AC-4),
  `test_no_wrong_frame` (AC-2: replace/push/pop/cover × fade, wipe ×4, split ×2, blinds, iris circle and diamond,
  each with both reveal styles and BackOut, plus a tall wipe with a 24-pixel gap, a custom cover and all-zero
  durations: 92 transitions checked frame by frame from the fake's submitted items), `test_transition_cost` (state size,
  1000 transitions of every kind with no `operator new`, no engine allocation and no log line), and
  `tests/transitions_example/test_self_check.cpp` (AC-3). The fake platform records 1024 items per frame.
- **AC-3 goldens** (pc-debug, equal in pc-release and in the simulator's real engine, `TRANSITIONS_DEMO=check`):
  checkpoints every 120 frames `89F4F944 4BF8CFEE 58AEC725 8284B4EA E271C1BB 70D4E35E C82DAF6F BF71A011 11D7022E`,
  final **`B32C7A79`** (`kTransitionsCheckGolden`).
- **Refinement: the self-check is driven by a bottom "driver" scene** (`CheckDriver`, check_scenes.hpp) whose
  `draw()` plays one script frame: scenes don't update during transitions but every scene draws every frame, so the
  same script runs under `nnh::run()` in the example and in a manual loop on the fake platform. A and B take turns,
  C is pushed and popped; the steps live in `check_steps.cpp` (24 steps; the refused request at frame 340 mid-iris,
  `reveal_at((40, 200))` from the iris's `on_covered`, plain `push(C)`/`pop()` at 955/965 in a covered phase). Notes
  (enter/exit/update/callbacks) count only inside the script's frames (not the exits when it leaves).
- **Refinement: frame times and rectangles are checksummed from the public queries** (`effect_rects` of what each
  screen shows by the documented resolution); AC-2 proves they are what the overlay drew.
- `scenes` (3ds-release) links no transition or input-block symbol (only `g_scene_transition`); `hello` unchanged
  since stage 4 (text 201 276).

## Stage 6 results and refinements

- `ShowDemo{demo, at, door}`; `LevelPlay::enter_doors()` publishes the player's centre (one hunk), `restart()` starts
  24 px left of the first door when `start_at_door_` (one hunk; the spawn takes the feet: the door's bottom), one
  setter and one member next to `in_door_`. `Demos` takes a `DoorRun&`; a door request with no replay running
  becomes `transition::replace(level, door_transition(at − camera))` (`door_transition.{hpp,cpp}`: tall iris, band 4,
  36/6/36, QuadIn); `LevelScene::on_enter()` calls `reveal_at(player − camera)` while a transition runs; Y (or
  `PLATFORMER_DEMO=doors`) starts the door run (`door_run.{hpp,cpp}`, frame times from the levels' `draw()`, which
  runs every frame).
- `tests/platformer/test_door_run.cpp` (3: the door transition's options, ten doors' lines and the verdict, a slow frame
  fails); ctest `platformer_doors_script_pc` (`examples/platformer/tests/doors.nnhs`).
- **PC door run** (pc-debug, frames since `doors: started`): `doors: 1 level1 -> bonus frame 12`, `2 … 101`, `3 …
  190`, `4 … 279`, `5 … 368`, `6 … 457`, `7 … 546`, `8 … 635`, `9 … 724`, `10 … 813`; `doors: done 10 irises frames
  891 worst 17.2 ms avg 16.7 ms: 60 Hz PASS`. One run of three on the loaded PC measured a 89.6 ms frame (Debug,
  another build running): the same flakiness as the replay's 60 Hz verdict in pc-debug; the console is the judge.
- **AC-7 screenshot**: `doors.nnhs`'s `door-iris` (frame 967, 42 frames after RIGHT): the circle closing on the player
  at the door, a 4-pixel staircase edge, continued over the bottom screen — read back.
- `platformer_replay_matches_script` passes with `level.nnhs` untouched (`B2418A28`).

## Stage 7 results and refinements

- `examples/transitions` (13 sources ≤ 200 lines, `art/make_art.py` → `assets/art/tiles.png`, `art/make_music.py` →
  `assets/music/loop.ogg`, 32 kHz, 4 bars at 120 bpm, `LOOPSTART=0`): title → room 1 (fade, music faded in from
  `on_covered`), two rooms as code-built tile maps (`world.cpp`), the HUD on the bottom screen, LEFT/RIGHT effect, L
  reveal, R screens, touch → iris centre, A change, B loading step (curtain + bar, 40 slices, hold 20), X loading scene
  (`title_scene.cpp`'s `LoadingScene`, a second transition from its `update()`), SELECT self-check, Y bench, START.
  Refinement: `title_scene.cpp` holds the title and the loading scene (one file pair), and the check scene's glue is in
  `main.cpp` (`log_checkpoint`); `bench.{hpp,cpp}` + `bench_scene.{hpp,cpp}` as planned.
- **Refinement: `loading_progress()` reads 0 when no transition runs** (it kept the last transition's value, so the
  self-check's sum depended on what ran before it: started by SELECT after the title's fade it differed from
  `TRANSITIONS_DEMO=check`). **AC-3 goldens re-recorded** (pc-debug = pc-release = the simulator, from boot or from
  SELECT): checkpoints `DA198EA8 E37BCD73 611CBFC8 6926151E 772D4A96 32B87B97 2576DBF6 DABFC1BC 836B1ECE`, final
  **`BFE1D810`** (stage 5's values are superseded).
- **Refinement: the `gfx submit` profile scope is Development-only** (`#if NNH_FLAVOR_DEVELOPMENT` in `gfx_submit()`, as
  the plan's text says): in Debug it added a scope to every frame and broke spec 002's profiler tests, which count
  scopes. Release and Debug objects are unchanged.
- The input log counts what scenes see from the transition's first frame (`frames() > 0`): the request's own frame
  still read the press that started it.
- `examples/transitions/tests/check.nnhs`: PASS twice with `--pc` (112 lines, frames 1–3824): the ten checkpoints
  and `transitions: PASS BFE1D810`; fade with B tapped during it → `transition: done frame 1377 seen 0`; four RIGHTs →
  `settings: iris both reverse`; touch → `iris centre: bottom 100,120`; iris `seen 0`; R R and LEFT ×3 → `wipe tall`,
  `seen 0`; B → `transition: start replace curtain tall reverse out 24 hold 20 in 24`, `seen 0`; the bench lines and
  `transitions bench: done`. Lines that come in one frame are expected once (the verdict and the room's return; the
  bench's loading line, verdict and `done`); every input waits ≥ 10 frames after an `expect` (the tool warns otherwise).
  ctests `transitions_self_check_pc` and `transitions_check_script_pc` (023's ctest block after `camera_self_check_pc`).
- Screenshots read back (`TRANSITIONS_SHOT=<mode> NNH_EXIT_AFTER_FRAMES=40`, coverage 0.25 with QuadIn): fade, wipe,
  split, blinds, iris (a 4-pixel staircase circle on each screen), diamond, curtain (velvet rows and the gold hem over
  the top quarter of both screens), tall (the wipe's first 120 rows of the 480-row area: half the top screen); at 61
  the fade covers both screens; `TRANSITIONS_SHOT=loading` at 80: the curtain down on both screens with the bar half
  full. Four are in `docs/images/transitions/`.
- **PC bench (pc-dev, `TRANSITIONS_DEMO=bench`)**: base submit 0.133 ms; fade 0.440, wipe 0.273, split 0.294, blinds8
  0.252, iris 0.305 (rects 240 of 244), loading engine 0.435 ms average → `FAIL (fade, wipe, split, loading engine)`.
  Not representative: on the PC the submit includes the SDL software renderer filling every overlay pixel (a
  full-screen rectangle pair costs ≈ 0.3 ms there; on the 3DS the GPU fills them after `C2D_DrawRectSolid` has queued
  six vertices). The `transition` scope alone is a few µs. The console judges AC-8 (T10.2); the PC bench is not.
  pc-debug logs `not judged (Debug)`.

## Stage 7 ARM pre-check (T7.7, 3ds-dev objects, before any console run)

| Hot path | Disassembly | Plan's assumption | Verdict |
|----------|-------------|-------------------|---------|
| `scenes::update_and_draw` | 8 instructions: one load, one branch, a tail call | one load and branch | ✓ |
| `input_update` | 11 instructions: the engine mask (2 loads, 2 stores), one load and branch, a tail call to `input_apply` (the old body, unchanged) | one load and branch | ✓ (+1 call per frame) |
| iris band loop (`geometry_iris`) | per cut band ≈ 20 instructions of setup, the inlined bit-method square root (≈ 7 × 3 to place the bit + ≈ 8 × 8 iterations for r² < 2¹⁶), 2 calls to `RectTarget::emit` (≈ 15 instructions each, then the sink); ≈ 120 instructions per band; no `__aeabi_*`, no `vmrs`, no `memcpy` in the loop; one `__aeabi_idivmod` per call, only when the target starts below row 0 (the tall bottom screen) | 100–130 | ✓ |
| blinds (`geometry_fill`) | one `__aeabi_idivmod` per call; per bar ≈ 14 instructions and one emit | one udivmod per screen | ✓ |
| the step (`transition_frame` → `timeline_step`) | one `vdiv` and one `ease()` call per out/in frame; `vmrs` only in the clamp/ratchet compares of the step (4 per frame), not per band | one ease, one vdiv | ✓ |
| `effect_area` (`resolve_effect`, `max_rects`) | float compares (`vmrs`) and two `__aeabi_idiv` in `max_rects_resolved`: request time only | — | ✓ (not per frame) |
| the rectangle path | unchanged gfx code: `fill_rect` → `DrawList::record` → build → `draw_2d` (counted in the plan's table) | 800–1270 cycles, × 1.3 | — |

Expected console numbers (the plan's table, unchanged by the disassembly): fade/wipe 0.010–0.017 ms, split
0.018–0.029, blinds8 0.066–0.105, iris average 0.60–0.95 ms (worst 1.05–1.60, rects 244), loading engine 0.012–0.020
— PASS, the iris's worst frame the one at risk (risk 1). A cheap win kept in reserve (no result changes): start the
square root's bit at 2¹⁶ when r² < 2³² is known to fit (saves ≈ 20 instructions per band).

## Stage 10: prepared commands (completed by stage 7)

On the PC first (T10.1): `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh cmake --build --preset
3ds-dev --target transitions_3dsx platformer_3dsx actions_3dsx` (both 0 warnings at stage 7); `hello.elf` text 201 276
(T0.1 + 88). Then on the Old 3DS, nobody touching it after Y — open the Homebrew Launcher, press **Y**, then:

1. `build/pc-debug/tools/nnh/nnh run transitions --script examples/transitions/tests/check.nnhs` (≈ 65 s): expected
   `transitions: 120 sum DA198EA8` … `transitions: 1080 sum 836B1ECE`, **`transitions: PASS BFE1D810`**, the showcase's
   `seen 0` lines and `iris centre: bottom 100,120`, the bench lines and `transitions bench: PASS` (or `FAIL (...)`:
   record the numbers in "Console run 1"); screenshots `fade-mid`, `iris-mid`, `tall-mid`, `curtain-mid`,
   `loading-bar` compared with the `--pc` run's (same geometry, fade colours within one step).
2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/doors.nnhs` (≈ 20 s): `doors: 1
   level1 -> bonus frame 12` … `doors: 10 bonus -> level1 frame 813`, `doors: done 10 irises frames 891 … 60 Hz
   PASS`, the `door-iris` screenshot.
3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` (≈ 40 s): the tree's
   replay lines (`B2418A28` here; spec 022's after the merge), `60 Hz PASS`.
4. `build/pc-debug/tools/nnh/nnh run actions --script examples/actions/tests/check.nnhs` (≈ 30 s): PASS as before.

## Mutation testing (T8.1)

56 hand mutants (`build/ref-023/mutate2.py`, not committed), each applied alone with the whole `nnh_tests` (pc-debug):
the geometry (split halves, blinds' running remainder, edges, continue forms, fade alpha rounding, the iris's ⌈√⌉, band
sampling, `dy ≥ r`, the diamond, the square root's compare, the last merged run, the tall inset and gap, rounding of
centres and q, the tall band count, the bottom-in resolution), the timeline (both ratchets, at least one covered
frame, the in phase's skip, the in ratio, half-up rounding of durations), the frame (the block lifted, the waiting count
released, `on_finished`, the finish event, the waiting frame, which scene updates, the in phase skipped over an emptied
stack), loading (`on_covered` only once, no step after done, clamping), the overlay (layer 15, the in effects), the
queries (`none()` coverage), the requests (refusal, the immediate change with O = 0, the pending count, `reveal_at`
before the in phase only, `block_input`), the input block (released buttons dropping out, held buttons filtered,
touch, sticks, edge-free blocked frames, the filter leaving the path, init's reset, raw engine masks), the scene hook
(the cancel at shutdown, `run()`'s condition, the tolerant pop) and the platformer's door code (ten doors, the first
frame unmeasured, the tall iris).

- **55 killed, 1 equivalent: score 100 % of the non-equivalent mutants.** Two survivors of the first pass became tests:
  `the_filter_leaves_the_input_path_once_nothing_is_held` (the filter pointer cleared once nothing is held) and
  `the_door_runs_first_frame_is_not_measured`; a third, `a_zero_out_request_from_an_event_handler_covers_after_the_change`,
  was written for the waiting frame (killing the mutant that skips it).
- Equivalent: `at_least = max(H, 1)` → `H` — the covered phase can only end once the loading step reported done, which
  needs one stepped covered frame, so "at least one covered frame" holds either way (the max stays for clarity).
- The Release path (misuse clamped/refused with logs) is covered by `test_requests.cpp` in pc-release (ctest 258/258);
  no separate Release mutants were run (deviation from plans 019–022: their Release mutants were in modules with more
  Release-only code).

## Stage 8 results

- `scripts/check_release_strings.sh`: ok on the 3ds-release `transitions.elf` and the pc-release `transitions.exe`.
- Opt-in by linking (T8.3): `hello`'s 3ds-release text 201 276 (T0.1 + 88 ≤ 96), its `nm` list differing from T0.1
  only by `input_apply`, `g_input_filter`, `input_engine_held_mask`, `input_engine_pressed_mask` (−`input_pressed_mask`,
  −`input::held_mask`) and `g_scene_transition`; `scenes` links no `transition` or input-block symbol (only
  `g_scene_transition`); the geometry objects (`effect_query.o`, `effect_area.o`, `effect_fill.o`, `effect_iris.o`)
  reference only each other, `gfx::fill_rect` and the C runtime: a game calling only `draw_effect`/`effect_rects` links
  no `transition_frame`/`transition_request` code.
- 3ds-release `.text`: `scenes` 206 636 (+144), `actions` 246 268 (+176: the input path), `platformer` 373 904 (+12 024:
  the transition module and the doors), `transitions` 330 524.
- Regression (T8.4): every example's console script with `--pc` (pc-debug unless noted): actions, animation, camera,
  data, entities, foundation `assert_crash` and `cpu_fault` (both PASS; the simulator then waits on its crash screen
  for START, as before), input, prefabs, queries, tweens (`B44CA3F6`), transitions (`BFE1D810`), platformer `doors`
  PASS. Platformer `level`: the replay's sums are the tree's (`B2418A28`, `level.nnhs` untouched) with `60 Hz PASS`
  in pc-dev (one Debug run on the loaded PC missed one frame: 29.4 ms); `gate` and `zoom_gate` PASS in pc-dev with the default renderer
  (with the offscreen software renderer the bench misses every frame in any flavor: that renderer is not what the gate
  measures; 023 changes no tile or sprite code). ctest `platformer_replay_matches_script` passes at every stage.
  `compare.sh` identical at every stage.
- Build times: edit `examples/transitions/room_scene.cpp` → simulator 2.0–2.1 s (stage 0's `scenes/title_scene.cpp`:
  2.0–2.3 s), → `.3dsx` 3.9 s (3.3 s; the example links more of the engine). `nnh_tests` 2492 (+139), ctest pc-debug
  271 (+3), pc-release 258.
- Final stage-end guards (stage 8): ctest 271/258, `nnh_tests` 2492, 0 warnings on pc-debug, pc-release, 3ds-release
  and 3ds-dev, tweens/actions/input scripts PASS, `level.nnhs` untouched, `compare.sh` identical (the actions example
  also keeps a `controls.bin.bak`: `compare.sh` deletes both before its shot), docs 0 broken links.

## Stage 9 results

On `nnhge-build` (VM 120), 2026-11-24, `main` with specs 022–025 merged, then branch `fix-024`:

- The first round found nothing in the transition code. Its GCC 13 and 14 build failures and the ASan leak were all
  in spec 024's particles. See [024's Stage 9 results](../024-particles/plan.md#stage-9-results): `ParticleDesc`'s
  `= {}` arrays, a GCC 13 crash on `draw(screen, {.layer = n})` with `Texture` incomplete, and a test-only leak.
- After those fixes: GCC 13, GCC 14 and clang 18 ASan/UBSan ctest 271/271 each (`-E "nnh_script_pc_smoke|_script_pc"`).
  `nnh_tests` 2870 under ASan with leaks on: clean. The transitions self-check prints `BFE1D810` and the tweens check
  `B44CA3F6` on all three.
- `platformer_doors_script_pc` failed under ASan in the first round. The cause was the environment, not this spec:
  the VM's build folder isn't a preset name, so `nnh run --no-build` fell back to `pc-dev`, which wasn't built.
  Re-run with `NNH_PRESET=pc-asan` linked to the ASan build: all ten doors at the PC's frames, then `60 Hz FAIL`
  (average 36.1 ms, worst 76.2 ms under the sanitiser and the software renderer). That is the wall-clock verdict, an
  environment limit like the bench under the offscreen renderer (Stage 8 results). `doors.nnhs` now waits 10 frames
  after its last `expect` (it had 3; `nnh` warned that the next input would reach the console late).
- The 12 self-check and replay ctests pass on all three compilers (leaks off for the simulator processes: Mesa's EGL
  allocation in SDL's offscreen init is reported by LeakSanitizer).
- libFuzzer `fuzz_transition_geometry` ran 600 s: 6 500 974 runs, cov 198, no finding.

## Console run 1 (T10.2, 2026-11-24, Old 3DS, Development build)

Run 1 (`transitions` `check.nnhs`): the self-check **`transitions: PASS BFE1D810`** (every checkpoint equal to the PC's);
the bench (average ms per frame over 300 frames):

| Workload | Average | Worst | Target | Verdict |
|----------|---------|-------|--------|---------|
| base gfx submit | 0.26 | — | — | — |
| fade | 0.0957 | — | < 0.1 | PASS |
| wipe | 0.0962 | — | < 0.1 | PASS |
| split | 0.1106 | — | < 0.1 | **FAIL** |
| blinds8 | 0.1747 | — | < 0.3 | PASS |
| iris | 0.96 | 1.75 | < 2 | PASS |
| loading engine | 0.1046 | — | < 0.05 | **FAIL** |

Run 2 (`platformer` `doors.nnhs`): the ten doors, **`60 Hz PASS`**, worst frame 20.9 ms. Runs 3 (`level.nnhs`) and 4
(`actions`) were not part of this report.

Reading the numbers: one more rectangle per screen costs ≈ 5.6 µs (blinds8 − fade over 14 rectangles; split − fade
gives 7.5 µs over 2), close to the plan's 3.9–6.2 µs. But fade, with two rectangles, costs 0.096 ms where the plan
expected 0.010–0.017: ≈ 0.084 ms a frame is fixed, the same in every workload, and nothing in the plan's instruction
counts explains it. Loading − fade = 0.0089 ms although a loading frame does less geometry than a fade frame.

## Optimisation round (T10.3)

### What was slow (3ds-dev disassembly, `transitions.elf`)

1. **The profiler measured itself (a measurement artefact: ≈ 0.009 ms in every workload, ≈ 0.018 ms in loading).**
   `NNH_PROFILE_SCOPE` reads `platform::ticks_ns()` twice: `svcGetSystemTick` (a supervisor call), then two
   `__aeabi_uldivmod` calls (libgcc's `__udivmoddi4`, a bit-by-bit loop: ≈ 12 iterations for `ticks / 268 111 856`,
   ≈ 30 for `rest · 10⁹ / 268 111 856`, ≈ 330 instructions); the destructor also searches the frame's scope table before
   its read. About one `ticks_ns()` of that falls inside the scope it measures. The hook opens **two** `transition`
   scopes a frame (around the step, then around the overlay and the end of the frame), so every workload carried two
   scopes' own cost. The loading workload carried a third: its slice ran a 5000-step loop inside a nested `bench slice`
   scope whose time was subtracted, but the nested scope's own entry and exit (both reads) stayed in the outer one.
   Loading − fade = 0.0089 ms is that nested scope; one scope's share inside its parent is therefore ≈ 4.4 µs.
2. **citro2d's texture → solid switch (real, gfx's: ≈ 0.03–0.05 ms).** The bench scene draws only text (textured
   quads). The overlay's solid rectangle changes citro2d's source: `C2D_DrawRectangle` → `C2Di_Update` with the source
   dirty flushes the batch (`C3D_DrawElements` → `C3Di_UpdateContext`, which runs `C3D_UpdateUniforms` twice, each a
   scan of 96 dirty bytes: ≈ 1 000 instructions per flush) and rewrites four texenv stages. Per transition frame: the
   top screen's rectangle (text → solid), the bottom screen's text (solid → texture) and rectangle, and the next frame's
   first text: ≈ 2 extra flushes and 4 texenv rebuilds, ≈ 2 600–3 400 instructions, plus ≈ 2–3 KB of citro2d code the
   base frames never run (cold every frame). Any game drawing a solid shape after textured draws pays this once per
   switch: it is part of the overlay's real cost, but not the transition module's code.
3. **The engine's own fixed path is short but cold.** Hook, step, `ease()`, the overlay on two screens and two recorded
   rectangles ≈ 600 instructions (≈ 3 µs warm) spread over ≈ 45 lines of code that the rest of the frame (text, submit,
   citro2d, the remote poll) evicts: ≈ 0.02 ms. In run 1 the showcase's music also kept decoding on the main thread
   (stb_vorbis between frames), evicting more.
4. **Per rectangle, the recording went through the public call.** `to_gfx` → `gfx::fill_rect` (`frame_open()`,
   `gfx_state()` twice, a 64-byte `memset`, the 80-byte command built on the stack) → `gfx_record` →
   `DrawList::record`: ≈ 187 instructions per rectangle before the submit's own share.
5. **The iris's square root** started its bit at 2³⁰ and shifted it down two places at a time to the value: ≈ 8 steps
   (24 instructions) per cut band for the usual r² < 2¹⁶.

### What changed (no result moved)

| Change | Where | Instructions (3ds-dev) | Kind |
|--------|-------|------------------------|------|
| The overlay records its rectangles directly: one `gfx_drawing()` check and one `DrawCommand` per screen (what `fill_rect` builds: `FillRect`, the clamped layer, `ignore_camera`, the colour); per rectangle only the position and size are set before `detail::gfx_record` (risk 1 (a)) | `transition/effect_query.cpp` | per rectangle 187 → 73; per screen ≈ 60 once (`gfx_drawing`, a 69-byte `memset`, the clamp) | engine |
| `isqrt` places its first bit with one `clz` (`std::countl_zero`) | `transition/effect_iris.cpp` | per cut band ≈ 120 → ≈ 102 (placement 24 → 6) | engine |
| The profiler's own cost taken out: an empty `bench probe` scope each frame; a workload frame's cost = `transition` − 2 × probe + (submit − base submit); the base line logs the probe's average | `examples/transitions/bench_scene.cpp` | — | measurement |
| The loading step does no work and opens no scope (nothing of the game's left to subtract) | `bench_scene.cpp` | — | measurement |
| The bench stops the music when it starts | `bench_scene.cpp` | — | measurement |
| Scopes summed by name (an unoptimised build may keep two copies of the `"transition"` literal, the profiler's key) | `bench_scene.cpp` | — | measurement |
| A `rect` run after the base: no transition, the bench scene draws one 1 × 1 solid rectangle per screen at layer 15 and logs `transitions bench: rect avg X ms worst Y ms (gfx share, one solid rectangle per screen, not judged)`: the console's own number for item 2 | `bench.{hpp,cpp}`, `bench_scene.cpp`, `tests/check.nnhs` (one `expect`) | — | measurement (informative) |

Targets unchanged. Tests: `draw_effect_records_exactly_what_fill_rect_records` (new, `test_effect_query.cpp`: every
field of every item and the screen calls equal to `fill_rect`'s, for six effects, layers 99 and −5 and a zoomed
camera); the existing exhaustive `isqrt` tests. Mutants: 8 on the record path and 5 on `isqrt`, 12 killed; the one
survivor (removing the `v == 0` guard) is equivalent on both CPUs — the guard only avoids an undefined shift. Every golden
holds: `transitions: PASS BFE1D810` (ctest `transitions_self_check_pc` and the unit test), `doors.nnhs --pc`, the full
pc-debug ctest.

### Expected on the Old 3DS (console rerun)

Run 1's numbers less what was removed (one scope ≈ 4.4 µs; a rectangle ≈ 114 instructions ≈ 0.55 µs cheaper to record;
a cut iris band ≈ 18 instructions cheaper); the music's share is unknown (0–0.01 ms more off each):

| Workload | Run 1 | Expected | Target |
|----------|-------|----------|--------|
| fade | 0.0957 | 0.076–0.086 | < 0.1: PASS |
| wipe | 0.0962 | 0.076–0.086 | < 0.1: PASS |
| split | 0.1106 | 0.090–0.100 | < 0.1: borderline |
| blinds8 | 0.1747 | 0.145–0.157 | < 0.3: PASS |
| iris avg (worst) | 0.96 (1.75) | 0.86–0.87 (≈ 1.59) | < 2: PASS |
| loading engine | 0.1046 | 0.075–0.086 | < 0.05: **still FAIL** |
| rect (new, not judged) | — | 0.03–0.05 | — |

What is left in fade and loading is item 2 (gfx's batch break, which the `rect` line now shows on its own) and item 3
(cold code): neither changes without changing how citro2d is used or how the code is laid out. If the rerun still misses
a target, the maintainer's policy applies: the budget is the measurement + ≈ 25 %, written in the spec's AC-8,
`bench.cpp` and `docs/api/transition.md` ("Console run 2 and budgets set").

Rerun (Homebrew Launcher open, press **Y** for each):

```sh
build/pc-debug/tools/nnh/nnh run transitions --script examples/transitions/tests/check.nnhs
build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/doors.nnhs
```

## Console run 2 and budgets set (2026-11-25, Old 3DS, Development build)

Scripts own the controls since spec 013's amendment (nobody's input counts during a run). Logs:
`build/logs/transitions-20261125-090223.txt`, `build/logs/platformer-20261125-092129.txt`.

- `check.nnhs`: `transitions: PASS BFE1D810`, every showcase line as before; the bench failed only on `loading engine`
  (old target), as expected.
- `doors.nnhs`: ten irises at the PC's frames, `doors: done 10 irises frames 891 worst 20.4 ms avg 16.7 ms: 60 Hz PASS`.

| Workload | Run 1 | Expected | Run 2 avg (worst) | Old target | Budget (run 2 + ≈ 25 %) |
|----------|-------|----------|-------------------|------------|--------------------------|
| base submit / probe | — | — | 0.2651 / 0.0067 | — | — |
| rect (not judged) | — | 0.03–0.05 | 0.0628 (0.178) | — | — |
| fade | 0.0957 | 0.076–0.086 | 0.0829 (0.199) | < 0.1 | **< 0.11 ms** |
| wipe | 0.0962 | 0.076–0.086 | 0.0845 (0.193) | < 0.1 | **< 0.11 ms** |
| split | 0.1106 | 0.090–0.100 | 0.0956 (0.213) | < 0.1 | **< 0.12 ms** |
| blinds8 | 0.1747 | 0.145–0.157 | 0.1526 (0.280) | < 0.3 | **< 0.19 ms** |
| iris | 0.96 (1.75) | 0.86–0.87 (≈ 1.59) | 0.8351 (1.456) | < 2 | **< 1.05 ms** |
| loading engine | 0.1046 | 0.075–0.086 | 0.0825 (0.198) | < 0.05 | **< 0.11 ms** |

Every number landed in its expected range. The `rect` line confirms item 2: one solid rectangle per screen after text
costs gfx ≈ 0.063 ms, i.e. three quarters of a fade; the transition module's own share of a fade is ≈ 0.02 ms. The
budgets replace the old targets (the maintainer's policy: no target predating the measurement) in AC-8,
`examples/transitions/bench.cpp` and `docs/api/transition.md`. The platformer's door verdict (no frame over 25 ms) is
unchanged.
