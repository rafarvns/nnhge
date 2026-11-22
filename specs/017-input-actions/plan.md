# Plan 017: Input actions

Status: implemented (T7.2 passed on the Old 3DS XL, 2026-11-22) — approved 2026-11-22 (maintainer pre-approved); see the stage results at
the end
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D32 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 001 (event bus, scene stack; D30 scene attachments from spec 015), spec 002 (memory tags, asserts, logs,
`Result`, `nnh::save`, profiler), spec 003 (the input sample and its queries), spec 013 (scripts, remote input)

> **Depends on spec 015's D30 code.** The scene attachments (`engine/core/src/scene/scene_attachments.*`,
> `Scene::attachments_`) are implemented on the branch of spec 015 and merged to `main` before stage 3 of this plan.
> Stages 0–2 don't need them.

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | Portable core module `<nnh/actions.hpp>`, no platform part, no contract change. Actions read the engine's own control vocabulary (`Button`, sticks, touch) through the public input queries |
| II 3DS constraint | ⚠️→✅ | Pressure point: per-action state with 4 slots, defaults and names. Answered with compact internal records (≤ 160 B per action on 3DS, `static_assert`), one block per set (tag `actions`): typical 16-action set ≈ 6 KB, the 64-action maximum ≈ 14 KB including a 3 KB bindings-file buffer. Evaluation cost: 64 × 4 slots < 0.1 ms on an Old 3DS (AC-9, measured in the console script) |
| III Build speed | ✅ | One public header with `<cstdint>`, `<type_traits>` and engine headers already light (`input.hpp`, `math.hpp`, `result.hpp`; `Scene` forward-declared). Thin non-template API: the only template is the enum-to-id constructor. Measured before/after (T0.1, T8.4) |
| IV Event-driven | ✅ | Queries for state (values, vectors, held, counters); two events for moments (`ActionPressed`, `ActionReleased`), queued with the frame's other input events and delivered at the one dispatch point. Remap results are a query (they concern only the menu that asked) |
| V No hidden costs | ✅ | No exceptions/RTTI/`virtual`; one allocation at set creation; evaluation, listening, remapping, saving and loading allocate nothing (AC-6); no logging in the normal path |
| VI Spec + test first | ✅ | Slot evaluation and combining are pure functions of an `InputView`; the set is driven by `evaluate(view)` in unit tests; the frame hook, scenes, events and input paths are tested on the fake platform; the example is checked by screenshots and a script |
| VII Small files | ✅ | ≈ 17 core files, one concept each, all planned ≤ 200 lines; one public header |
| VIII Documented API | ✅ | `docs/api/actions.md`, header comments, `examples/actions`, link from `docs/api/input.md` |
| IX Clean and legal | ✅ | Own code; Godot and Unity cited as prior art only (`docs/REFERENCES.md`); shapes-only art in the example |
| X Safe by default | ✅ | The bindings file is untrusted: bounded (≤ 3088 B), validated whole before anything changes, never asserts on content, fuzzed (mini-fuzz + libFuzzer); written atomically through `nnh::save` inside the game folder |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure point: FR-15 needs the frame loop to evaluate sets right after input. Answered by D32: an intrusive hook list whose head lives in `app.cpp`; neither `app` nor `input` names actions; a game without sets pays one null check (measured on `hello`, T3.5). `<nnh/input.hpp>` is unchanged |

## Design

### Overview

```
game (usually one set for the whole game)            engine, every nnh::frame()
 nnh::ActionSet controls{{.actions = 16, .contexts = 2}}
   one block (tag "actions"): header + contexts[8] + actions[n] + file buffer
   registers an InputHook node (inside the block) ──▶ app.cpp: present → scene changes → platform input
                                                       → remote merge (D28) → input_update → input events queued
                                                       → run_input_hooks(): each set, creation order:
                                                           view = public input queries (merged sample)
                                                           contexts: bound → scenes::top() == scene, else flag
                                                           enable edge → block held slots (FR-13)
                                                           listening? → capture step, all actions idle
                                                           else per enabled action: slots → combine → edges,
                                                                counters, repeat → queue ActionPressed/Released
                                                       → audio → events_dispatch() → game update()/draw()
 controls.bind_context(Ctx::Menu, *this)   (in a scene's on_enter)
   D30 attachment per bound context ──▶ AfterExit: the binding ends (context disabled, node detached)
```

### Decision D32: the after-input hook and the saved format (FR-15, FR-20)

**The hook.** The spec requires sets to be evaluated once per `nnh::frame()`, right after the input sample and before
dispatch, without `input` or `app` depending on the actions module, without allocation, and at the cost of at most a
null check for a game that creates no set.

**Chosen: an intrusive list of after-input hooks, head pointer in `app.cpp`.**

```cpp
// engine/core/src/core/input_hooks.hpp (internal)
namespace nnh::detail {
struct InputHook {
    InputHook* next = nullptr;
    void (*run)(InputHook& self) = nullptr; ///< set by the owner
};
extern InputHook* g_input_hooks;            ///< defined in app/app.cpp (always linked: one pointer)
/// Runs every hook in registration order (nnh::frame()). Inline: the loop's first test is the null check.
inline void run_input_hooks() { for (InputHook* h = g_input_hooks; h != nullptr; h = h->next) h->run(*h); }
void add_input_hook(InputHook& hook);       ///< appended (core/input_hooks.cpp, linked only by its users)
void remove_input_hook(InputHook& hook);    ///< no-op if absent
}
```

- `nnh::frame()` calls `detail::run_input_hooks()` right after `detail::input_publish_events()` and before
  `detail::audio_frame()`. At that point the sample is final (remote input merged before `input_update`, D28; D-Pad
  mirror applied), the frame is open (debug text can be recorded), and the frame's `ButtonPressed`/`ButtonReleased`/
  `TouchStarted`/`TouchEnded` are already queued, so action events follow them in the same dispatch (FR-14).
- Each `ActionSet` keeps its node inside its heap block (stable address; the `ActionSet` object itself is one pointer
  and can move) and registers it at creation, removes it at destruction. Several sets run in creation order.
- No re-entrancy is possible: a hook only reads input, updates its own block and queues events (`publish` queues;
  handlers run later in `events_dispatch`). Creating or destroying a set from an event handler happens outside the
  pass. `add/remove_input_hook` assert (Debug/Development) that the node is not already in the list / is in it; no
  "during a pass" flag is needed, since nothing a hook does can run game code.
- Dependencies point down: `actions → input (public queries), scene (top() and D30 attachments), events, core`.
  `app` knows only `core/input_hooks.hpp`. The input module is untouched.

Cost for a game without sets: one global pointer (4 B on 3DS) and one load + compare + branch per frame; the add/remove
functions are in their own object file, linked only by the actions module. Target for `hello` (Release, PC and 3DS):
≤ 16 B of text, measured against the stage 0 baseline (T3.5; AC-10's "same size" is read as "no action code linked";
the spec's constraint allows the null check).

Rejected:

- **A game-called `controls.update()`.** Events would arrive a frame late (after the dispatch that delivered
  `ButtonPressed`), and forgetting the call silently breaks input (the spec's answered question).
- **`input_update()` calling actions.** The input module would depend on an optional module (XI) and on its link.
- **Subscribing to `ButtonPressed`.** Delivered at dispatch, so action events would be a frame late; sticks and touch
  have no per-frame events (and must not, IV).
- **A single function-pointer slot in `app.cpp`.** Same cost, but only one user; the list costs nothing more and lets a
  later module (virtual sticks, gestures) use the same point.
- **Making the frame loop call a `detail::actions_frame()` weak symbol.** Weak symbols behave differently across
  toolchains and static libraries (and are invisible in code review).

**Scene-bound contexts (FR-5) use `scenes::top()` at evaluation, plus a D30 attachment for the binding's life.** At
evaluation a bound context is enabled exactly when `scenes::top() == scene`. Scene changes requested during frame N are
applied at the start of frame N+1, before input, so the first evaluation after a push already sees the menu on top:
the switch takes effect from the next frame's evaluation (FR-15), with no frame in which both the level's `gameplay`
actions and the menu run. D30's `BeforeUpdate` alone would be one frame late (the pushed menu has not updated yet when
the set is evaluated), so it is not used as the signal. The attachment (one node per context, inside the block) only
ends the binding: on `AfterExit` (pop, replace, end of `run()`, `shutdown()`) the context is unbound and disabled and
the node detached. So a binding lasts **one stay on the stack**; games bind in the scene's `on_enter()` (where
`on_stack()` is already true), and a scene object is never left attached when it is destroyed later (D30's destructor
check stays meaningful). Destroying a set while a context is still bound asserts (AC-3) and detaches in Release.

**The saved format (FR-20, FR-21)** is described in "Bindings file" below and recorded in D32 with the hook.

### Public API (sketch)

```cpp
// <nnh/actions.hpp>: includes <cstddef>, <cstdint>, <type_traits>, "nnh/gfx.hpp" (Screen, Color; light),
// "nnh/input.hpp", "nnh/math.hpp", "nnh/result.hpp", "nnh/system.hpp". Forward-declares Scene.
namespace nnh {

class Scene;

enum class ActionKind : std::uint8_t { Button, Axis, Vector };
enum class AnalogStick : std::uint8_t { CirclePad, CStick };
enum class Direction : std::uint8_t { Up, Down, Left, Right };
enum class StickAxis : std::uint8_t { X, Y };
/// Which part of a slot: the whole source, or one button of a composite.
enum class Part : std::uint8_t { Whole, Negative, Positive, Up, Down, Left, Right };

/// A game enum value used as an action or context id (any enum; values 0..capacity-1, dense).
struct ActionId {
    template <typename E> requires std::is_enum_v<E>
    constexpr ActionId(E e) : index(static_cast<std::uint16_t>(e)) {}
    constexpr explicit ActionId(int i) : index(static_cast<std::uint16_t>(i)) {}
    std::uint16_t index;
};
using ContextId = ActionId;

/// What a slot reads (FR-6). Small value type; factories below; compares by value.
struct Source {
    enum class Kind : std::uint8_t { None, Button, StickDirection, Stick, StickAxis, Buttons2, Buttons4, Touch };
    Kind kind = Kind::None;
    std::uint8_t a = 0, b = 0, c = 0, d = 0;      ///< button bit indices, stick, direction/axis
    std::uint16_t x = 0, y = 0, w = 0, h = 0;     ///< touch rectangle (bottom screen pixels)
    static constexpr Source button(Button b);
    static constexpr Source stick_direction(AnalogStick s, Direction d);
    static constexpr Source stick(AnalogStick s);                       ///< vector actions
    static constexpr Source stick_axis(AnalogStick s, StickAxis a);     ///< axis actions
    static constexpr Source buttons(Button negative, Button positive); ///< axis composite
    static constexpr Source buttons(Button up, Button down, Button left, Button right); ///< vector composite
    static constexpr Source dpad();                                     ///< buttons(Up, Down, Left, Right)
    static constexpr Source touch(int x, int y, int w, int h);          ///< button actions; always locked
    friend constexpr bool operator==(const Source&, const Source&) = default;
};

/// One binding slot as declared (FR-6, FR-8, FR-9). Thresholds and dead zone are kept in 1 % steps.
struct Binding {
    Source source{};
    float press = 0.5f, release = 0.4f;   ///< stick directions (button actions)
    float dead_zone = 0.0f;               ///< extra dead zone, whole stick / stick axis (re-scaled)
    bool invert_x = false, invert_y = false;
    bool locked = false;                  ///< never changed by remapping, reset or loading
    bool fallback = false;                ///< used only on systems without `fallback_for`
    Capability fallback_for = Capability::CStick;
};

struct Repeat { std::uint8_t delay = 0, interval = 0; };   ///< frames; 0 = no repeat (FR-12)

/// One action's declaration. Designated initializers:
/// `{.name = "jump", .bindings = {{Source::button(Button::A)}, {Source::button(Button::B)}}}`.
struct ActionDesc {
    const char* name = nullptr;          ///< 1-31 of [a-z0-9_], copied
    ActionKind kind = ActionKind::Button;
    ContextId context = ActionId(0);
    Binding bindings[4] = {};
    Repeat repeat{};
    float press = 0.5f, release = 0.4f;  ///< held threshold of axis/vector actions (FR-3)
};

struct ActionSetConfig {
    int actions = 0;   ///< required: 1..kMaxActions (ids 0..actions-1)
    int contexts = 1;  ///< 1..kMaxContexts (ids 0..contexts-1)
};

inline constexpr int kMaxActions = 64, kMaxSlots = 4, kMaxContexts = 8, kMaxActionName = 31;

enum class ConflictPolicy : std::uint8_t { Swap, Replace, Allow, Reject };
enum class ListenState : std::uint8_t { Idle, Listening, Bound, Cancelled, TimedOut, Rejected };
enum class Resolution : std::uint8_t { None, Swapped, Replaced, Allowed, Rejected };

struct ListenOptions {
    Part part = Part::Whole;
    ConflictPolicy on_conflict = ConflictPolicy::Swap;
    std::uint32_t cancel = static_cast<std::uint32_t>(Button::Start); ///< Button mask; 0 = none
    std::uint32_t exclude = 0;          ///< Button mask never captured
    bool sticks = true;                 ///< sticks may be captured
    std::uint16_t timeout_frames = 0;   ///< 0 = none
};

/// What a listen (or set_source) did. Stays readable until the next listen()/clear_listen_result().
struct RemapResult {
    ListenState state = ListenState::Idle;
    std::uint16_t action = 0; std::uint8_t slot = 0; Part part = Part::Whole;
    Source source{};                    ///< captured
    Resolution resolution = Resolution::None;
    std::uint16_t other_action = 0; std::uint8_t other_slot = 0; Part other_part = Part::Whole;
    std::uint8_t conflicts = 0;         ///< how many bindings shared the source
};

struct Conflict { std::uint8_t slot; Part part; std::uint16_t other_action; std::uint8_t other_slot; Part other_part; };

struct LoadReport {
    bool found = false;           ///< false: no file (a normal "no remaps")
    bool version_ignored = false; ///< the game's bindings version differed: nothing applied
    int applied = 0, skipped = 0;
};

inline constexpr std::size_t kMaxBindingsSize = 3088;   ///< 16 + 256 × 12

class ActionSet {
public:
    ActionSet() = default;                         ///< empty: valid() is false
    explicit ActionSet(const ActionSetConfig& config);   ///< one allocation (tag "actions")
    ~ActionSet();
    ActionSet(ActionSet&&) noexcept; ActionSet& operator=(ActionSet&&) noexcept;
    bool valid() const;
    std::uint16_t id() const;                      ///< matches ActionPressed::set

    // Declaring (FR-1..FR-9)
    bool add_context(ContextId id, const char* name, bool enabled = true); ///< id 0 exists as "default"
    bool add(ActionId id, const ActionDesc& desc);

    // Contexts (FR-4, FR-5, FR-13): changes apply from the next evaluation
    void enable(ContextId id, bool enabled);
    bool enabled(ContextId id) const;
    void bind_context(ContextId id, Scene& scene); ///< enabled while `scene` is on top, until it leaves the stack
    void unbind_context(ContextId id);             ///< back to manual, disabled

    // Reading (FR-11, FR-12)
    bool pressed(ActionId a) const; bool held(ActionId a) const; bool released(ActionId a) const;
    bool repeated(ActionId a) const;
    float value(ActionId a) const;                 ///< button 0..1, axis -1..1, vector: its length
    Vec2 vector(ActionId a) const;                 ///< x right, y up, length ≤ 1
    std::uint32_t held_frames(ActionId a) const;   ///< 1 on the press frame, 0 when not held
    std::uint32_t frames_since_pressed(ActionId a) const; ///< 0 on the press frame; saturates; max = never
    bool usable(ActionId a) const;                 ///< any active slot on this system (FR-9)
    void set_events_enabled(bool on);              ///< FR-14, default on

    // Remapping (FR-16..FR-19)
    void listen(ActionId a, int slot, const ListenOptions& options = {});
    void stop_listening();                         ///< state → Cancelled
    bool listening() const;
    const RemapResult& listen_result() const;
    void clear_listen_result();
    RemapResult set_source(ActionId a, int slot, Source source, ConflictPolicy policy = ConflictPolicy::Swap);
    RemapResult set_part(ActionId a, int slot, Part part, Button button, ConflictPolicy policy = ConflictPolicy::Swap);
    void clear(ActionId a, int slot);
    void set_invert(ActionId a, int slot, bool invert_x, bool invert_y);
    void reset(ActionId a, int slot); void reset(ActionId a);
    void reset_context(ContextId id); void reset_all();
    Source source(ActionId a, int slot) const;     ///< current; Kind::None if empty
    int conflicts(ActionId a, Conflict* out, int capacity) const;
    std::size_t describe(ActionId a, char* out, std::size_t capacity) const; ///< "A / B" (active slots)
    const char* name(ActionId a) const;

    // Saving (FR-20, FR-21)
    Result<void> save(const char* path, std::uint32_t game_version = 0) const;
    Result<LoadReport> load(const char* path, std::uint32_t game_version = 0);
    Result<std::size_t> save_to(void* buffer, std::size_t capacity, std::uint32_t game_version = 0) const;
    Result<LoadReport> load_from(const void* data, std::size_t size, std::uint32_t game_version = 0);

    // Debug view (FR-24): Debug/Development only; nothing in Release
    void debug_draw(Screen screen, int x, int y) const;

private:
    detail::ActionBlock* block_ = nullptr;
};

/// Published for button actions, and for axis/vector actions when `held` changes (FR-14).
struct ActionPressed  { std::uint16_t set; std::uint16_t action;
                        constexpr bool is(const ActionSet& s, ActionId a) const; };   // 4 bytes
struct ActionReleased { std::uint16_t set; std::uint16_t action;
                        constexpr bool is(const ActionSet& s, ActionId a) const; };

namespace actions {
/// Short English name of a single control: "A", "ZL", "D-Pad Up", "Circle Pad Left", "C-Stick", "C-Stick X",
/// "Touch"; "" for None. Static strings, built-in font characters only.
const char* control_name(Source source);
/// Any source, composites included ("D-Pad", "L/R", "X/B/Y/A"). Returns the length written (truncated, NUL-ended).
std::size_t describe(Source source, char* out, std::size_t capacity);
} // namespace actions
} // namespace nnh
```

Exact type names may still change while building (spec: "the plan may adjust the exact type names"); stage notes in
this file record any change. `ActionPressed::is` compares `set == s.id()` (an `id()` call; the struct stays trivially
copyable).

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| `ActionPressed` | each `ActionSet` (events on), in the after-input hook | set id (u16), action id (u16): 4 bytes | gameplay reacting to a moment (jump, open the pause menu), logs, sound |
| `ActionReleased` | same | same | charge attacks (release fires), held-time UI |

Order inside one frame's dispatch: the game's events from the previous frame, then `ButtonPressed`…, `ButtonReleased`…,
`TouchStarted`/`TouchEnded`, then per set (creation order) all `ActionPressed` (declaration order), then all
`ActionReleased` (declaration order), then `SoundFinished`. At most 2 × actions events per set per frame (bounded by
the declarations; the bus's 16 KB default queue holds 680 of them).

### Semantics fixed by the plan

- **Evaluation (per enabled action).** Each active, unblocked slot gives `(held, value, vec)`; FR-7 combines them:
  button: held = any, value = max; axis: the slot with the largest `|value|` (ties: lowest slot); vector: the longest
  (squared lengths compared, ties: lowest slot). Axis/vector `held` uses the action's thresholds with hysteresis on
  `|value|`/length (press 0.5, release 0.4 by default). Edges compare with the previous evaluation.
- **Sources.** Button: held = bit set; value 1. Stick direction: push = component along the direction (≥ 0) of the
  shaped stick; for button actions held with the slot's hysteresis, value = push (even below the threshold); for axis
  actions the value is the push (positive only). Whole stick: shaped stick, optional extra dead zone re-scaled on the
  length (`(len − dz) / (1 − dz)`, from lengths precomputed once per frame in the `InputView`). Stick axis: one
  component, dead zone re-scaled on `|v|`. Composite: axis = positive − negative; vector = (right − left, up − down),
  a diagonal scaled by the constant 0.70710678f (no `sqrt`). Touch: held while touching inside the rectangle
  (`x ≤ px < x + w`, `y ≤ py < y + h`). Invert negates x and/or y after everything else.
- **Inactive slots (FR-9).** Computed when a slot changes, from capabilities read at set creation: a fallback slot is
  active only when the system lacks `fallback_for`; ZL/ZR need `ZButtons`, C-Stick sources `CStick`, Circle Pad
  sources `CirclePad`, touch `TouchScreen`.
- **Counters.** `held_frames` = 1 on the press frame, +1 per held frame, 0 when not held (saturates at 2³²−1).
  `frames_since_pressed` = 0 on the press frame, +1 per evaluation, saturates; it starts at the maximum ("never").
  Counters advance on every evaluation, including frames where the action reads idle.
- **Repeat (FR-12).** `repeated` is true on the press frame and when `held_frames − 1 ≥ delay` and
  `(held_frames − 1 − delay) % interval == 0` (interval 0 = no repeats after the press).
- **Blocking (FR-11, FR-13).** When a context's enabled state goes from off to on (by hand, by a scene, at the set's
  first evaluation, and for every enabled context when a listen ends), every slot of its actions whose source is held
  right then is **blocked**; a blocked slot contributes nothing until its own source is released (composites: all
  parts released; sticks: below the release threshold). So the A that confirmed "Start game" doesn't jump, but a
  player still holding the stick in another slot is not stopped by an unrelated held button. Disabling a context
  makes its actions read idle with no `released` edge and no event; their counters restart.
- **Changes during a frame** (enable, bind, remap, reset, load) apply from the next evaluation; queries in the rest of
  the frame keep this frame's values, so queries and events always agree (FR-11).
- **Listening (FR-16).** At `listen()`: held buttons and stick directions (past 0.5) are recorded as ignored; each is
  forgotten when released (below 0.4). Each evaluation while listening, in this order: timeout; cancel (any `cancel`
  button newly down); capture among newly-down controls that are not ignored, not excluded, present on the system
  (ZL/ZR need `ZButtons`, C-Stick needs `CStick`): buttons in `Button` bit order first, then Circle Pad, then C-Stick
  directions in Up, Down, Left, Right order. What is captured depends on the slot: button action → a button or a stick
  direction; axis action, `Part::Whole` → a stick pushed along X or Y captures that **stick axis** (pushing toward the
  negative side sets `invert_x`) and buttons are ignored; vector action, `Part::Whole` → a stick captures the whole
  stick and buttons are ignored; composite parts (`Negative/Positive`, `Up/Down/Left/Right`) → buttons only. (Spec
  clarification: "as a direction for axis actions" is read as "the axis of that direction", so a rebound axis keeps
  both signs; a one-sided axis is still available through `set_source`.) Listening a part of a non-composite slot
  turns it into an empty composite first. While listening every action of the set reads idle and no action event is
  published; when it ends, every enabled context blocks its held slots (the binding press doesn't act).
- **Conflicts (FR-17).** A binding unit is (action, slot, part). Two units conflict when they are in the same context,
  both remappable or one locked, and read a common control: each source has a footprint (a `Button` mask plus 8 stick
  direction bits: a whole stick covers its 4 directions, an axis 2). Policies: swap — the first conflicting unit
  (action, slot, part order) gets this unit's previous source when it fits that unit's kind (else it is cleared, and
  the result says Replaced), further conflicting units are cleared; replace — all are cleared; allow — nothing changes;
  reject — nothing changes and the result names the first holder. A locked conflicting unit makes every policy except
  allow a rejection. `conflicts()` lists every unit of an action that shares a control with another unit of its context.
- **Display (FR-19).** `control_name` table; `describe` joins composite parts with `/` ("D-Pad" and "Circle Pad" names
  for the canonical cases) and an action's active slots with `" / "`.
- **Names and ids (FR-2).** Ids must be `< config.actions` (contexts `< config.contexts`); names 1–31 of `[a-z0-9_]`;
  duplicate id, duplicate name and 32-bit FNV-1a hash collision (`detail::fnv1a32`) assert in Debug/Development and
  are refused with an error log in Release (`add` returns false). Same for capacity overflow and kind/source mismatch.

### Bindings file (FR-20, FR-21)

Little-endian, `NNHB`:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | magic `N` `N` `H` `B` |
| 4 | 1 | format version (1) — newer → `Unsupported`, nothing changes |
| 5 | 1 | reserved, 0 |
| 6 | 2 | entry count (≤ 256) |
| 8 | 4 | game bindings version (the game's own number) |
| 12 | 4 | CRC-32 of the entries (the core's `crc32`) |
| 16 | 12 × n | entries |

Entry (12 bytes): action name hash u32 · slot u8 · part u8 · source kind u8 · flags u8 (bit 0 invert x, bit 1 invert
y; other bits must be 0) · data[4] (button bit index 0–13, stick 0–1, direction 0–3 / axis 0–1, or the 4 composite
button indices, unused bytes 0).

- **Writer**: for each remappable slot that differs from its default, one entry: if the slot is a composite in both
  and only one part differs (and invert is the same), a part entry (`part` = that part, `data[0]` = the button);
  otherwise a whole entry (`part` = 0) with the full source and the invert flags (an emptied slot is kind None). At
  most one entry per slot, so ≤ 256 entries and ≤ 3088 bytes; entries sorted by (hash, slot, part).
  `save(path)` writes through `nnh::save::write(path, buffer, size, 1)` (atomic, backup, its own checksum); the
  buffer is the set's 3 KB file buffer reserved at creation.
- **Loader**: structural checks first (size ≥ 16, magic, format, reserved 0, size = 16 + 12 × count, count ≤ 256,
  CRC) → `Corrupted`/`Unsupported`, nothing changes. Then game version: different → `version_ignored`, nothing changes.
  Then every remappable slot returns to its default and each entry is applied or skipped (counted): unknown name hash,
  slot ≥ declared slots, part invalid for the slot, unknown kind, kind/source invalid for the action kind, data out of
  range, non-zero unused bytes or flags, touch, a locked slot, or a repeated key (the first wins). Loaded sources the
  system lacks are kept (inactive), so saving again keeps them. One warning log with the counts when anything was
  skipped. `load(path)` reads with `nnh::save::read` into the file buffer; `NotFound` → `found = false`, no change.
- Pure parts (`bindings_format.cpp`: header/entry encode, validate, decode into a list) are shared with the fuzz
  target; applying uses the set's own lookup (name hash → action).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/actions.hpp` | new | the public API above (aim ≤ 250 lines with comments; `Source` factories `constexpr` inline) |
| `engine/core/src/core/input_hooks.hpp` / `.cpp` | new | D32: `InputHook`, `run_input_hooks()` (inline), add/remove, pass flag |
| `engine/core/src/app/app.cpp` | changed | defines `g_input_hooks`; calls `run_input_hooks()` after `input_publish_events()` |
| `engine/core/src/actions/action_block.hpp` | new | `ActionBlock` header, `ContextRec`, `ActionRec`, `SlotRec`, `ListenRec`, flags, `static_assert` size bounds |
| `engine/core/src/actions/actions_internal.hpp` | new | functions shared by the module's files (lookup, slot activity, footprint, `evaluate(block, view)`) |
| `engine/core/src/actions/input_view.hpp` / `.cpp` | new | `InputView` (held mask, shaped sticks, 8 pushes, lengths, touch) built once per hook pass from the public queries |
| `engine/core/src/actions/source_eval.cpp` | new | one slot's `(held, value, vec)` per source kind, thresholds, dead zone, composites, touch, invert (pure) |
| `engine/core/src/actions/action_combine.cpp` | new | FR-7 combining, axis/vector held hysteresis (pure) |
| `engine/core/src/actions/slot_read.hpp` | new (console bench refinement) | the slot readings and FR-7 folds inline, the percent table; `source_eval.cpp` is their out-of-line form |
| `engine/core/src/actions/read_slots.cpp` | new (console bench refinement) | the per-frame slot loop, one per action kind, controls at rest skipped |
| `engine/core/src/actions/action_set.cpp` | new | construction/destruction (one `mem::alloc`, tag `actions`), move, ids, hook registration, log channel `actions` |
| `engine/core/src/actions/action_declare.cpp` | new | `add_context`, `add`: name rules, hash, capacities, kind/source checks, defaults |
| `engine/core/src/actions/action_eval.cpp` | new | the per-frame evaluation: contexts, blocking, edges, counters, repeat, listen dispatch, profile scope |
| `engine/core/src/actions/action_events.cpp` | new | publishing presses then releases in declaration order |
| `engine/core/src/actions/action_queries.cpp` | new | public queries, `usable`, `source`, `name` |
| `engine/core/src/actions/action_contexts.cpp` | new | enable/disable, scene binding (D30 node per context, `AfterExit` → unbind), top-scene check, FR-9 warning |
| `engine/core/src/actions/action_listen.cpp` | new | listen state machine and capture rules |
| `engine/core/src/actions/action_conflicts.cpp` | new | footprints, conflict search, policies, `conflicts()` |
| `engine/core/src/actions/action_edit.cpp` | new | `set_source`, `set_part`, `clear`, `set_invert`, resets |
| `engine/core/src/actions/source_names.cpp` | new | `control_name`, `describe` (source and action) |
| `engine/core/src/actions/bindings_format.hpp` / `.cpp` | new | `NNHB` encode/validate/decode (pure; fuzzed) |
| `engine/core/src/actions/bindings_io.cpp` | new | `save`/`load`/`save_to`/`load_from`: diff against defaults, apply with skips, report |
| `engine/core/src/actions/action_debug.cpp` | new | `debug_draw` (Debug/Development; empty in Release) |
| `engine/core/CMakeLists.txt` | changed | explicit sources; `-ffp-contract=off` on `src/actions/*` (GCC/Clang), as tweens do |
| `tests/actions/test_*.cpp`, `tests/actions/actions_fixture.hpp` | new | see Test strategy |
| `tests/app/test_input_hooks.cpp` | new | D32 list: order, add/remove, null head, no-op remove |
| `tests/fuzz/fuzz_bindings.cpp`, `tests/fuzz/libfuzzer_bindings.cpp`, `fuzz_targets.hpp`, `test_fuzz_smoke.cpp`, `tests/CMakeLists.txt` | new/changed | `bindings_target`, seed, mini-fuzz, libFuzzer executable |
| `tests/actions_example/test_self_check.cpp` | new | the example's pure self-check logic and checksum |
| `examples/actions/` | new | `main.cpp`, `controls.{hpp,cpp}` (enums, declarations, defaults), `play_scene.{hpp,cpp}`, `controls_scene.{hpp,cpp}` (remap menu), `bench_scene.{hpp,cpp}`, `action_log.{hpp,cpp}` (fixed-format log lines), `self_check.{hpp,cpp}` (pure verdict), `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(actions)` |
| docs (stage 8) | changed/new | `docs/api/actions.md`; `docs/api/input.md` (link instead of "later spec"); `docs/api/scene.md` (binding contexts); `docs/architecture/events.md` and `scenes.md` (frame order, second attachment user); `docs/architecture/project-structure.md` if needed; `docs/REFERENCES.md`; `CLAUDE.md` layout; `docs/ROADMAP.md`; `specs/README.md` |

### Data and memory

One block per set, `mem::alloc(MemRegion::Heap, actions, …)`: `[ActionBlock][ActionRec × n][file buffer 3088 B]`.

| Record | 3DS (32-bit) | Notes |
|--------|--------------|-------|
| `ActionBlock` header: id, capacities, counts, caps mask, flags, `InputHook`, `ContextRec × 8` (enabled, was-enabled, bound `Scene*`, `SceneAttachment` 12 B, name 16 B), `ListenRec` (≈ 40 B), `RemapResult`, declaration order (64 B) | ≈ 560 B | contexts always 8 (small) |
| `ActionRec`: name 32 B, hash 4, kind/context/flags 4, repeat 2, thresholds 2, runtime (held bits, blocked mask, slot hysteresis bits, held_frames 4, since_pressed 4, value 4, vec 8) ≈ 28, `SlotRec × 4` (current source packed in 6 B — kind + 4 data bytes + invert bits —, default 6 B, press/release/dead-zone % 3 B, flags and touch-area index 2 B, ≈ 18 B; touch rectangles in a small per-action table of the defaults) ≈ 80 B | ≤ 160 B | `static_assert(sizeof(ActionRec) <= 160)` on 32-bit; touch is always locked, so its rectangle is stored once, with the defaults (8 B per touch slot) |
| file buffer | 3088 B | for `save(path)`/`load(path)`; `save_to`/`load_from` use the caller's memory |

Typical set (16 actions): ≈ 0.6 + 2.6 + 3.1 ≈ 6.3 KB. The maximum (64 actions): ≈ 14 KB. Against the Old 3DS heap
this is noise; the spec's "a few KB" holds for typical sets, and the 64-action figure is documented. `ActionSet` itself
is one pointer. Sets alive at `nnh::shutdown()` appear in the leak report under tag `actions` (destroy them first, as
the example does).

### Platform work

- pc: none. The simulator's keyboard/mouse/gamepad mapping and `NNH_SIM_INPUT` already produce the sample.
- 3ds: none. Builds with the module; the console script checks behaviour and cost (AC-8, AC-9).
- fake (tests): none expected; existing `set_held_buttons`, sticks, touch, `set_capabilities` (Old 3DS), remote push
  and file faults cover every test. If a missing helper appears, it is added in `engine/platforms/fake/` with its test.

### Performance (AC-9)

Per frame and set: build the `InputView` once per hook pass (shared by all sets: the pass builds it lazily for the
first set and reuses it), then for each enabled action at most 4 slot evaluations that are table lookups plus a few
compares (no `sqrt`: lengths precomputed; one division only for an extra dead zone). Target on an Old 3DS: 64 actions ×
4 slots < 0.1 ms (≈ 100 cycles per slot). Measured by `examples/actions` bench mode (a second set of 64 × 4 mixed
slots, 600 frames, Development build, `profile::slowest` scope `actions`), logged as `actions: bench avg … ms worst …
ms` and part of the console verdict. If the target is missed: first skip actions of disabled contexts earlier
(per-context lists), then cache per-slot pushes; the measured number goes into `docs/api/actions.md` either way.

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows and the Linux VM 120); the module's pure files are
also compiled into the fuzz target.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/actions/test_action_set.cpp`: capacities, one allocation (tag stats), move, destroy frees, overflow asserts (Debug) / refused (Release path via the `NNH_ASSERT` test hook) |
| FR-2 | `test_action_declare.cpp`: ids, names (`[a-z0-9_]`, 1–31), duplicate id/name, forced hash collision (two names with equal FNV-1a, found offline and pinned in the test) |
| FR-3 | `test_action_kinds.cpp`: button/axis/vector values, `held` thresholds and hysteresis for axis/vector |
| FR-4 | `test_action_contexts.cpp`: enable/disable, idle when disabled, two contexts sharing A both see it |
| FR-5 | `test_action_scenes.cpp` (fake platform + scene stack): bound context on top only, push/pop switch, binding ends after exit, set destroyed while bound asserts, rebind after re-push |
| FR-6 | `test_source_eval.cpp` (pure, `InputView`): every source kind, thresholds/hysteresis, dead-zone re-scale, composites (opposites cancel, diagonal length 1), touch slide in/out, invert x/y |
| FR-7 | `test_action_combine.cpp`: any-held, max value, largest `|axis|`, longest vector, ties → lowest slot, duplicate controls |
| FR-8 | `test_action_edit.cpp` / `test_action_listen.cpp`: locked slots untouched by remap/reset/load; touch always locked |
| FR-9 | `test_action_fallback.cpp`: fake Old 3DS caps → fallback active, ZL/C-Stick inactive, `usable`, one warning on enable (log capture) |
| FR-10 | `test_action_input_paths.cpp`: same results from fake buttons/sticks/touch, from spec 013 remote messages, and with the D-Pad mirror on/off |
| FR-11 | `test_action_queries.cpp`: edges, `held_frames`, `frames_since_pressed` (saturation, never), held at creation not pressed |
| FR-12 | `test_action_repeat.cpp`: delay/interval frame table, interval 0 |
| FR-13 | `test_action_blocking.cpp`: block on enable per slot, release unblocks, other slot still works, disable is silent |
| FR-14 | `test_action_events.cpp`: order after `ButtonPressed`/`TouchStarted`, presses before releases, declaration order, queries agree inside handlers, events off, axis/vector events on held change |
| FR-15 | `test_input_hooks.cpp` + `test_action_events.cpp`: evaluated once per frame after input, before dispatch; scene-bound change visible on the next frame |
| FR-16 | `test_action_listen.cpp`: held-at-start ignored, button capture, stick → direction/axis/whole stick, parts, cancel, exclude, `sticks = false`, timeout, ZL on fake Old 3DS not captured, all idle while listening, block after end |
| FR-17 | `test_action_conflicts.cpp`: swap/replace/allow/reject, swap that doesn't fit → cleared, locked → rejected, other context never conflicts, `conflicts()` list |
| FR-18 | `test_action_edit.cpp`: set/clear/invert, reset slot/action/context/all, effect from next evaluation |
| FR-19 | `test_source_names.cpp`: every control name, composites, `describe` of an action, truncation |
| FR-20 | `test_bindings_format.cpp` (encode, sizes, only changed slots, part vs whole entries) + `test_bindings_io.cpp` (round trip through `nnh::save` and a buffer) |
| FR-21 | `test_bindings_io.cpp`: enum reordered / actions added keep choices; game version ignored; each skip reason counted; damaged (fake `corrupt_file`), too-new and missing files; ZL kept but inactive on fake Old 3DS then saved again; `bindings_target` mini-fuzz + libFuzzer (ASan/UBSan, VM 120) |
| FR-22 | `test_action_determinism.cpp`: a recorded input sequence run twice → identical per-frame log of queries, events and listen results |
| FR-23 | `test_action_alloc.cpp` (AC-6, `alloc_counter` + `mem::live_allocations`); bench in the example (AC-9) |
| FR-24 | `test_action_debug.cpp`: `debug_draw` records lines in Development (debug list count), nothing outside a frame; Release build of the example has no action debug strings (`check_release_strings.sh`) |
| FR-25 | T3.5: `hello` sizes vs stage 0 (PC and 3DS, Release); `examples/input`, `scenes`, `platformer` unchanged and passing; spec 003 tests untouched |
| AC-7/AC-8/AC-9 | `examples/actions`: PC screenshots read back; `nnh run actions --pc --script examples/actions/tests/check.nnhs` PASS; the same script on the console (T7.2) |

## Risks and alternatives

- **One-frame lag or leak around scene switches.** Mitigated by checking `scenes::top()` at evaluation (exact from the
  first evaluation after the change) and blocking held slots on enable. Alternative kept in reserve: none needed.
- **Evaluation cost on the Old 3DS** (AC-9). Precomputed `InputView`, no `sqrt`, packed records; measured in the console
  run; fallbacks listed in "Performance".
- **`hello` size** (AC-10). The hook adds a pointer and a few instructions to `frame()`. It can't be compiled out in
  Release (actions must work there), so if the text grows more than 16 bytes the loop moves out of line into
  `app.cpp` behind the null check; the spec's own constraint allows the null check, and the measured number is
  recorded in this plan and in `docs/api/actions.md`.
- **Memory of a 64-action set** (≈ 14 KB, above "a few KB"). Acceptable on 3DS; if a game needs less, the file buffer
  could become optional (`ActionSetConfig::file_buffer = false`, then `save(path)` returns `Unsupported`) — not
  planned unless asked.
- **Spec 015 not merged yet.** Stage 3 needs D30's attachment; stages 0–2 don't. If 015's internal API changes before
  merge, only `action_contexts.cpp` follows it.
- **Listening with the D-Pad mirror on**: pushing the Circle Pad makes D-Pad bits too; buttons are checked first, so a
  button action captures "D-Pad Up". Documented; menus that want sticks turn the mirror off while listening.
- **Interpretation choices** recorded above (axis listen captures an axis; one save entry per slot; bindings last one
  stay on the stack; counters advance while idle). They follow the spec's intent; the maintainer can revisit them in
  review.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | No platform code; same evaluation code, `-ffp-contract=off`, no `sqrt` in the per-slot path; PC and 3DS run the same script |
| II | ✅ | ≤ 160 B per action, ≈ 6 KB typical, ≈ 14 KB maximum per set, documented; < 0.1 ms target checked on the console |
| III | ✅ | Header includes only light engine headers; measured in T0.1/T8.4 |
| IV | ✅ | Two events for moments, everything else queried; delivered at the one dispatch point, after input events |
| V | ✅ | One allocation per set; AC-6 test; no `virtual`, no RTTI, no exceptions; hook is a function pointer per set |
| VI | ✅ | Every FR has a unit test written first; example + script for the rest |
| VII | ✅ | 17 small core files, one public header |
| VIII | ✅ | API page, header comments, example; architecture pages updated when the code lands (T8.2) |
| IX | ✅ | No third-party code |
| X | ✅ | Bounded, validated-before-applying, fuzzed bindings loader; atomic writes through `nnh::save` |
| XI | ✅ | D32: `app` holds one pointer, nothing names actions; actions need no other optional module (scenes and the bus are core) |

No constitution exception is requested.

## Stage 0 results

`hello` Release before any change (WinLibs GCC 16.2 / devkitARM, 2026-11-22): PC `hello.exe` text 129 376, data
21 988, bss 207 568; 3DS `hello.elf` text 216 260, data 28 328, bss 102 584. Build times and the header line count of
T0.1 were not measured in this session (open item, with T8.4).

## Stage 1–2 refinements

- A file `actions/action_slots.cpp` was added (packing sources, which source fits which kind, the active flag of
  FR-9, footprints), so `source_eval.cpp` only evaluates.
- `Source::kNoButton` (0xFF) marks an empty composite part (a part listened into a simple slot, a cleared part);
  `detail::button_index()` in the public header serves the `constexpr` factories. A `Button` value that is not one
  button is refused at declaration.
- Context names are 1–15 characters (`kMaxContextName`); `add_context(0, …)` renames the `default` context.
- `ActionRec` is 160 bytes on every system (no pointers): name 32, a stored `value` (vector length), 4 slots of
  24 bytes each holding current and default source (5 bytes each), percent thresholds, flags, runtime bits and the
  touch rectangle (`static_assert(sizeof(ActionRec) <= 160)`).
- A control that goes down on the very evaluation that enables its context is blocked too: that evaluation is "the
  moment the context becomes enabled" (FR-13).
- The FR-9 warning is logged once per context per set. Disabling a context restarts the counters (`held_frames` 0,
  `frames_since_pressed` "never"); while listening, actions read idle with `held_frames` 0 and
  `frames_since_pressed` still counting.
- `enable()` on a scene-bound context asserts (unbind it first).
- Every action has `kMaxSlots` slots for remapping, declared or not (a player may bind an empty slot), so the
  loader's "slot ≥ declared slots" is "slot ≥ 4".

## Stage 3 results and refinements

- **The hook is a function pointer.** The inline loop on a head pointer in `app.cpp` cost 40 bytes of `hello` text
  on the 3DS (budget 16). As the plan's risk section foresaw, the loop moved out of line: `app.cpp` defines
  `detail::g_run_input_hooks` (null), `nnh::frame()` does `if (g_run_input_hooks) g_run_input_hooks();`, and
  `core/input_hooks.cpp` (linked only by its users) holds the list head, the loop, and sets/clears the pointer as the
  list fills and empties. Measured: 3DS `hello.elf` text 216 276 (+16, four instructions), bss +8; PC `frame()` +14
  bytes of code (the PE `.text` grows 64 because of 16-byte function alignment), bss +32. `nm`: only
  `g_run_input_hooks`, no action symbol.
- Each set builds its own `InputView` (the hook list has no pass identity to share one; it is a dozen loads and two
  square roots).
- `examples/input`, `scenes` and `platformer` build unchanged; every ctest passes; the platformer's `level.nnhs` and
  `gate.nnhs` pass with `--pc`.

## Stage 4 refinements

- Conflicts look at every slot of the context, active or not, so the same remap gives the same result on every
  system (FR-22). When a whole slot is assigned, the slot's other units are skipped; when a part is assigned into a
  simple slot, the simple source it replaces is skipped.
- `set_source`/`set_part` keep the slot's invert flags (a player's "invert Y" survives a rebind); a listen that
  captures a stick axis sets `invert_x` from the side pushed.
- No fixed array of hits: `assign` walks the context twice (count and holder, then apply).
- `clear_listen_result()` does nothing while listening.

## Stage 5 refinements

- `bindings_io.cpp` writes and saves/loads files; applying a blob is in `bindings_apply.cpp`.
- Part entries carry kind `Button`, or `None` for an emptied part; their flags must be 0.
- The fuzz target creates its set per input (a set registers an input hook, which must not outlive the call); the
  mini-fuzz checks that every set is freed. libFuzzer `fuzz_bindings` is wired (`NNH_BUILD_FUZZERS`) but was not run
  (open item, with the Linux VM checks of T2.7/T5.5).
- The allocation test checks `save(path)`/`load(path)` on engine memory only: the fake platform keeps files in std
  containers.

## Stage 6 results and refinements

- The example logs through its own channel `example` (the engine owns `actions`); its self-check class is
  `ActionsSelfCheck` (the tweens example's `SelfCheck` is linked into the same test program).
- `NNH_SIM_INPUT` holds its controls from the first frame, which FR-13 blocks (a control held when a set is created
  doesn't press), so it can't show actions moving. Screenshots: `NNH_SIM_INPUT`/`ACTIONS_DEMO=controls` for the static
  screens (checked), and the script's `screenshot` commands for motion, the swap and the verdict (checked).
- The bench reads the profiler's `actions` scope (every set's evaluation, so the game's 11 actions are included):
  PC simulator, Debug, avg 0.026 ms, worst 0.055 ms. Its screen stays up with the verdict (B goes back).
- `nnh run actions --pc --no-build --script examples/actions/tests/check.nnhs` (`NNH_PRESET=pc-debug`): PASS
  (77 lines, frames 1–833), twice in a row (the second run starts with the first run's saved file).
- `scripts/check_release_strings.sh` also refuses the FR-9 warning text; the Release `actions.elf` passes, the
  Development one fails as expected; the debug view's strings are absent from Release.

## Stage 8 results

- Header cost (T8.4; `g++ -std=c++20 -fsyntax-only` of a one-line TU, WinLibs GCC 16.2, average of 3): `<nnh/input.hpp>`
  534 preprocessed lines, 253 ms; `<nnh/actions.hpp>` 8 240 lines, 387 ms — the same as `<nnh/tween.hpp>` (8 413
  lines, 379 ms); the cost is `<type_traits>` and `nnh/result.hpp`'s `<new>`/`<utility>`. The header is 287 lines
  (the plan aimed at 250).
- Sizes: Release `actions.exe` text 270 140 (PC), `actions.elf` text 257 812 (3DS); `hello` as in stage 3.
- Not measured in this session: build times (edit → rebuild, clean engine), the set's tag bytes on the 3DS.
- Hand mutation testing: see "Mutation testing" below.

## Mutation testing

32 hand mutants of `src/actions/*` and `core/input_hooks.cpp` (one at a time, rebuild, whole `nnh_tests`, file
restored): **32/32 killed**, no survivor. Covered: stick hysteresis swapped, touch edge inclusive, diagonal not
scaled, dead zone ignored, axis invert ignored, ties taking the later slot (axis, vector), no analog hysteresis, no
blocking on enable, unblocking at the press threshold, repeat delay off by one, counters kept when disabled, no
saturation, contexts left on while listening, releases before presses, swap never giving back, locked bindings
ignored, conflicts across contexts, held-at-listen never forgotten, axis invert on the wrong side, timeout one late,
exclude mask ignored, loading into locked slots, repeated keys applied, no part entries, reserved byte unchecked,
fallback inverted, a scene binding that never ends, no hash-collision check, reset keeping inverts, the frame-loop
pointer left set, the FR-9 warning repeated.

## Console bench refinement

First console measure (Old 3DS, Development build, `examples/actions` bench, events off): **avg 0.224 ms, worst
0.317 ms** for 64 actions × 4 mixed slots (plus the game's 11 actions), ≈ 3.5 µs (≈ 940 cycles) per action, over the
0.1 ms target; behaviour right (the console's sums matched the PC's). The bench runs at rest (nobody touches the
console), so the cost was the per-slot machinery, not the input.

What was hot, read from the ARM code (`arm-none-eabi-objdump` of the 3ds-dev objects, GCC -O2, VFPv2):

- Two out-of-line calls per slot (`eval_slot`, then `combine` in another file), each with a 16-byte `SlotOut` going
  through memory, and a switch on the action kind inside each.
- A VFP divide (`vdiv.f32`, not pipelined on the ARM11) for every percent threshold or dead zone: `press / 100.0f`
  for stick directions, `dead_zone / 100.0f` for every whole-stick and stick-axis slot even at rest, two more per
  axis/vector action for its held hysteresis, one more per blocked slot.
- A float compare followed by `vmrs APSR_nzcv, fpscr` (which waits for the VFP pipeline) for nearly every slot, even
  for a button that is up: `combine` compared the slot's 0 against the action's value.
- Not hot: the `InputView` (two `vsqrt` per set), the repeat's `%` (`__aeabi_uidivmod`, only for held actions with a
  repeat), asserts (none in the path), contexts.

Changes (no public API change, no layout change: `ActionRec` is still 160 B):

- `slot_read.hpp`: the slot readings (`read_slot`, `engaged`) and the FR-7 folds inline; thresholds through a
  `constexpr` table of `p / 100.0f` for every byte (the same bits as the division the code did before);
  `compare_magnitude` (`vabs`) where a magnitude is only compared; `at_least` compares two floats known to be ≥ 0 and
  not NaN (stick pushes, a button action's value) as integers.
- `read_slots.cpp`: one loop per action kind with every slot inline, so per slot there is no call and no `SlotOut` in
  memory. Controls at rest are skipped with integer tests where their reading could not change the result: an up
  button or touch area (button actions), a composite with no part or opposite parts down, a stick axis or direction
  reading ±0, a whole stick of length +0 (vector actions; its vector is then exactly zero). Uncommon source/action
  pairs still go through `read_slot()`, so every pair reads as before.
- `action_eval.cpp`: one `read_slots()` call per action; the held hysteresis and blocking use the table.
- `eval_slot()`, `slot_engaged()` and `combine()` stay (tests, listening), now thin wrappers of the inline functions.

Exactness: `test_action_eval_golden.cpp` hashes every public reading of the bench's set over 2000 frames of
threshold-edge inputs (second context toggled, repeats, other thresholds, inverts); its golden value `4AE2673F` was
taken before the change and holds after it (pc-debug and pc-release); all other tests unchanged.

ARM code of the evaluation path (3ds-dev): `vdiv.f32` at rest 0 (was 1 per stick slot and 2 per axis/vector action:
≈ 190 per bench evaluation); calls per slot 0 (was 2); `vmrs` per slot at rest 0 for buttons, touch, composites and
sticks (was 1–3); per action one `read_slots()` call and, for axis/vector actions, one compare. Divisions left: a moved
stick with an extra dead zone (re-scale and the vector's scale), as the plan expects.

PC microbenchmark (`nnh_actions_bench`, pc-release -O3, the bench's set evaluated 20 000 times, best of 5, old and new
binaries interleaved on the same machine, which was shared with other builds, so absolute numbers vary between runs):

| Workload | Before | After | Gain |
|----------|--------|-------|------|
| At rest (the console bench's case) | 1.75–1.85 µs (3.0–3.1 µs under load) | 1.16–1.30 µs (2.0–2.2 µs under load) | ≈ −34 % |
| A new random input every evaluation (worst case for branch prediction) | 2.15–2.35 µs (3.7 µs under load) | 1.78–1.95 µs (2.9–3.0 µs under load) | ≈ −20 % |

The PC has hardware divide, a short-latency FPU and no `vmrs` stall, so it shows less than the ARM11 should gain: on
the console the removed divides alone were ≈ 190 × ~19 cycles per evaluation, and the removed calls, memory round trips
and VFP flag transfers were the rest of the ≈ 240 cycles per slot. Simulator check script: `nnh: PASS (77 lines)`,
same sums as before.

Next: re-run the console bench (`nnh run actions --script examples/actions/tests/check.nnhs`, HBL → Y) and record
avg/worst here and in `docs/api/actions.md`. If still over the target, the remaining per-action cost is memory (4 of
the 5 cache lines of each 160-byte record are read every frame): a hot/cold split of `ActionRec` would be the next
step, a layout change for the plan.

## Stage 7: console run (prepared)

Old 3DS, nobody touching it after Y:

1. On the 3DS: Homebrew Launcher, press **Y** (netloader waits).
2. On the PC: `build/pc-debug/tools/nnh/nnh run actions --script examples/actions/tests/check.nnhs`

Expected log lines, in order (the sum is the PC's; the console must print the same):

```text
I example actions: no saved bindings: defaults        (or "loaded 2 applied 0 skipped" on a second run)
I example actions: aim uses L/R (no C-Stick)           (an Old 3DS; a New 3DS says "aim uses C-Stick")
I example actions: pause pressed
I example actions: reset all
I example actions: rebind jump -> B (swapped with attack)
I example actions: reset jump
I example actions: rebind jump -> Y
I example actions: saved 40 bytes sum AAB4AA36
I example actions: jump pressed
I example actions: attack pressed
I example actions: reloaded 2 applied 0 skipped sum AAB4AA36
I example actions: bench started, 64 actions x 4 slots
I example actions: bench avg 0.0xx ms worst 0.0xx ms (target < 0.1 ms) PASS
I example actions: PASS 7/7
nnh: PASS (77 lines, ...)
```

Pass = the saved log (`build/logs/actions-<date>-<time>.txt`) ends with `actions: PASS 7/7`; record the bench
avg/worst and the aim line here.

### Console result and new budget (2026-11-22)

Old 3DS XL, Development, after the changes above: functional script unchanged (`saved 40 bytes sum AAB4AA36`, reload
the same), bench **0.142 ms** avg, 0.228 ms worst (was 0.224 / 0.317). The maintainer chose to optimize, then set the
budget from the measurement with margin: AC-9's budget is now **< 0.2 ms** (the example's self-check and bench line
use it). Splitting `ActionRec` into hot and cold parts stays open.
