# Plan 030: Live values

Status: approved 2026-11-26 (the maintainer, the seven questions at the end as recommended); the spec was approved 2026-11-26 with every open question as
recommended, and those answers are this plan's decisions; seven plan-level questions at the end)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decisions D51, D52 (proposed,
[decisions.md](../../docs/architecture/decisions.md))
Uses spec 002 and D17 (build flavours, `kDevToolsEnabled`, the debug overlay and its SELECT + L / `F1` toggle, the
simulator's `Pause` / `F10` / `F11`, logs, asserts, tagged memory, shutdown hooks), spec 005 (debug text in the built-in
font, the debug pass and its own capacity), spec 013 and D28 (the remote protocol, frame-stamped messages, records in the
log stream, `.nnhs` scripts and the timeline, exclusive script input), spec 003 and D38 (the input blocker and its
"held until released" filter), spec 004/026 and D47 (the recorded draw lists, deferred texture frees), spec 008 and D21
(voices and streams, `voice_pause`/`stream_pause`), spec 010 and D23 (`nnh run`, the netloader's argv). Spec 029 is not
needed (see "Spec 029 overlap"). Built in one worktree (branch `spec-030`) created from `main` **after spec 029 is
merged**; no other spec is built in parallel.

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ⚠️→✅ | Pressure points: pausing must work on the console, where today only the simulator pauses (inside the platform library, blocking the whole loop), and the panel must look the same on both. Answered: pause and step move into core (one state machine for every system; the PC's `Pause`/`F10` become developer requests, a 3DS argument and a PC variable ask for a held start); the panel is integer layout drawn with the built-in font through the debug pass; every number is printed by one integer-only formatter shared by the game and `nnh`; stepping and drag-to-value are fixed float/integer rules compiled with `-ffp-contract=off`. The only platform edits are the developer-request bits (3 lines per platform) |
| II 3DS constraint | ✅ | One registry block (tag `live`, ≈ 20 KB at the default capacities on the 3DS, question 2), allocated at the first registration; no new network or screenshot buffer (records are sent in pieces from the registry); the panel stays inside the debug pass's capacity (≤ 90 entries and ≤ 1.5 KB of text for a full page); the paused display replays the held frame instead of capturing it (no image buffer) |
| III Build speed | ✅ | `<nnh/live.hpp>` includes only `<cstddef>`, `<cstdint>`, `<type_traits>`, `<nnh/build.hpp>`, `<nnh/math.hpp>` and `<nnh/detail/name_hash.hpp>`; `Color` is forward-declared. Budget: within +250 preprocessed lines and +0.03 s of a one-line TU including `<nnh/math.hpp>` (T0.1, T2.8) |
| IV Event-driven | ✅ | No bus event (Q6): changes are queried (`changed()`) or reported by an optional callback at one point of the frame (FR-3); pause, steps and edits are engine controls, not game facts |
| V No hidden costs | ✅ | No exceptions, RTTI or `virtual`; nothing allocates after the block; no per-frame walk over the registry (pending edits, follows and shown pages are flags and short lists); `printf` is never used for values (the shared formatter writes into fixed buffers) |
| VI Spec + test first | ✅ | Names, values, stepping, drag rounding, the float text, the registry, the apply point, watches, the panel model, layout and drawing (debug-list items), the wire, the session, pause and step, the replay, the nnh catalogue, value parsing, script commands, the dump writer and reader are tested first on the fake platform or in `nnh_tool_tests`; the look by screenshots on PC and console |
| VII Small files | ✅ | Module `live` (≈ 16 sources, each one concept ≤ 250 lines), 7 new Dev-only core files outside it (pause, controls, pages, replay, held debug text, audio hold, remote live glue), ≈ 9 new `nnh` sources; one public header |
| VIII Documented API | ✅ | `docs/api/live.md`, `docs/guides/tuning-your-game.md`, `debug.md`, `simulator.md`, `debugging.md`, the nnh tool guide and the console-testing guide (stage 11) |
| IX Clean and legal | ⚠️→✅ | Pressure point: shortest round-trip float printing (question 3). Answered: Ryu's `f2s` vendored unmodified and pinned in `third_party/ryu/` (Apache-2.0 **or** Boost Software License 1.0; we take BSL-1.0, compatible with zlib), credited in `docs/REFERENCES.md`; the example's art is drawn with shapes in code |
| X Safe by default | ✅ | The game reads untrusted bytes: three new message types decoded field by field by the existing reader, then validated again against the registry (kind, range, finite, declared); refusals are records + rate-limited warnings, never a crash; mini-fuzz and libFuzzer targets over the session with a populated registry; the game writes no file (dumps are log lines, saved by `nnh`); `nnh`'s `--tunes` reader is bounded and fuzzed |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure point: pause, pages and the remote messages are engine tools every Debug/Development game gets, so they live in core, not in the optional module. Answered: Release has none of it (`hello` 3ds-release byte-identical, pc-release size and `nm` identical; checked at every stage end); a game that declares nothing links no `live` module code (the overlay reaches the module only through an ops pointer set by the first registration); the Debug/Development `hello` grows by an estimated 3–6 KB (question 7) |

## Design

### Overview

```
<nnh/live.hpp>   live::tune(name, &value, range [, on_change])  float | int | bool | Color | enum (names)
                 live::action(name, fn, context)                 a panel/nnh button that calls the game
                 live::watch(name, &value)                       registered watch, read only when shown or followed
                 live::show(name, value)                         per-frame watch
                 live::Handle (RAII: owns one entry; changed(), reset()), reset_all/reset_group, configure,
                 pause(), show_panel(page, select), kEnabled     — every call an empty inline in Release
core (Dev only)  app/dev_pause      pause, step, held start; paused ticks replay the held frame + live overlay
                 debug/dev_controls engine controls from the post-remote sample (ticks, repeat, touch)
                 debug/overlay_pages  Stats → Tunables → Watches (SELECT + R); the panel takes the controls
                 gfx/gfx_replay, audio/audio_hold, time: restart the frame clock after a pause
                 remote/live_wire   LiveSet (9), Live (10), Pause (11); records 8–12 (D28, additive)
module live      names, values and stepping, float text (Ryu), registry block, apply point, watches,
                 panel model/layout/draw, remote handling, dump          — linked only by games that use live::
nnh              catalogue mirror, value parsing, typed commands, script commands, expect-value, dump capture,
                 --tunes reader, --hold (3DS argv --nnh-hold, PC NNH_HOLD=1)
```

### Decision D51: live values — game-owned values registered by pointer into one Development-only registry block; changes applied at one point of the frame; panel pages on the debug overlay; live messages and records added to D28; Release compiles every call to nothing

**Registration.** A game keeps its own variables (a member, a global, a field loaded from a data asset) and registers
them at run time (Q1 (b)): `auto h = live::tune("player/jump", &tuning.jump, {.min = 0, .max = 10, .step = 0.25f});`.
The value the variable holds at registration is its **default**. The returned `live::Handle` owns the entry (move-only;
its destructor removes the entry, so the panel and `nnh` never reach a variable whose owner ended: FR-6). Names are
`live::Name` values: a literal is checked at compile time (`consteval`: length ≤ 31, characters, ≤ 3 group levels) and
hashed (FNV-1a 32, `nnh/detail/name_hash.hpp`); a run-time name (`live::Name::runtime(buffer)`) is checked by an assert
and copied. Kinds: `float`, `int` (32-bit), `bool`, `nnh::Color`, enums (the game's enum type, any underlying size,
values 0..N−1 with N ≤ 32 names) and actions (Q9). Watches: `watch(name, &value)` (bool, 8/16/32-bit signed and unsigned
integers, `float`, `Vec2`, `Color`, text through a `const char*` or a `char` array), read only on frames where they're
shown or followed, and `show(name, value)` per frame (Q2). There is no static registration and no registry before the
first call (D45's rule).

**The registry** is module state of the `live` module: one block (tag `live`) allocated at the first registration with
capacities from `live::configure()` (defaults 128 tunables and actions, 64 registered watches, 64 per-frame watches;
question 1), holding fixed-size entries (names copied, 32 bytes each), an index sorted by name (so the panel, the
catalogue and the dump list entries in one order on every system), a 16-slot follow table, a 64-entry queue of remote
operations and a per-frame watch table found by hash (open addressing). Registration asserts on a full table, a name
used twice, a name equal to an existing group path (or a group equal to an existing name: `reset X` and the TOML dump
stay unambiguous), a bad range (min ≥ max, step ≤ 0, NaN, more than 2²⁴ steps), a default outside its range, an enum
with no names or more than 32. The first registration also sets `detail::g_live_ops`, the only way the overlay and the
remote session reach the module, so a game that never calls `live::` links none of it (D39's and D49's pattern, without
a static initialiser).

**One application point (FR-3).** Every change — a panel edit, a remote `set`/`reset`/`action` — is recorded as
pending and applied in `nnh::frame()` right after the frame counter advanced and before input events, D32's hooks,
audio, dispatch and the scene update. A tunable whose bits change gets `changed_frame = frame_count()`, so
`Handle::changed()` is true on exactly that frame (FR-4); its optional callback is called there, in the sorted order;
actions run there. Remote operations are queued by name hash and resolved at that point, so a `set` lands on a tunable a
scene registered in its `on_enter()` that same frame. While the game is paused (D52) nothing is applied: edits stay
pending (the panel shows them with a `~` mark) and apply at the start of the next game frame run by a step or a resume.
The game's own `Handle::reset()` and `live::reset_*()` write at once (game code, not FR-3: no `changed()`, no
callback); the panel's and `nnh`'s resets go through FR-3.

**Panel pages (FR-7–FR-10).** The overlay gains pages (Stats → Tunables → Watches, SELECT + R while shown; Q4). On the
Tunables and Watches pages the panel **takes the controls**: core turns on a new input blocker (`InputBlocker::DevPanel`,
D38's mechanism: the game reads idle, held controls stay filtered until released) until SELECT + L hides the overlay
or the Stats page is shown again. The panel reads the post-remote sample (so a script's input drives it and the
console's own controls don't, spec 013's exclusive mode), is laid out in integers and drawn into the debug pass with
the built-in font (outside `max_draws`, the same pixels everywhere). Rows: groups (open/close), floats, ints, bools,
colours (four channels), enums, actions. Values print through one shortest round-trip formatter shared with `nnh`.

**Remote (FR-11–FR-13, D28 additive).** Three message types and five record types (below). The PC parses every typed
value once into exact bits; the game validates the bits against the entry and never parses text. New records are sent
only after the PC asked for live values (a catalogue request), so an older `nnh` never sees them.

**Release.** Every public function is an inline whose body is an `if constexpr (build::kDevToolsEnabled)` branch
calling into `detail::`, so Release discards the call, its arguments' names and the module: variables keep their
declared defaults, `changed()` is `false`, `watch`/`show`/`action` do nothing (arguments are still evaluated, like any
call; `live::kEnabled` lets a game skip costly ones).

Rejected: a static registry filled before `main` (Q1 (a): initialisation order, D45), a per-entry `virtual` (V), the
spec 028 UI for the panel (Q3: optional module, the game's `max_draws`), a separate PC window (Q3), a `TunableChanged`
event (Q6), writing values into source or asset files (Q5), engine-owned storage of values (the game keeps its own
variables, so reading a tunable is reading a variable: FR-2), names in Release, resolving remote names on receipt (a
scene's `on_enter` registration would miss its frame), pointers to names instead of copies (run-time names would
dangle; question 2).

### Decision D52: pause and step in core — `nnh::frame()` runs paused ticks that replay the held frame with a live overlay; the frame clock and counter stop; audio holds; remote messages due at the next game frame are applied while paused; a held start

**State.** A Debug/Development-only state machine in `src/app/dev_pause.*` (pure part tested alone): running, paused,
or stepping (n game frames left). Requests: the panel chords (SELECT + Y pause/resume, SELECT + X step one frame, on
the Tunables and Watches pages), the simulator's `Pause` and `F10` (now developer requests to core instead of a blocking
loop inside the PC platform), remote `Pause` messages (`pause`, `step N`, `resume`), `live::pause()` from game code,
and a **held start** (`--nnh-hold` in the 3DS argv, `NNH_HOLD=1` on the PC; set by `nnh run --tunes` and `--hold`;
question 5). A request from a control or `live::pause()` takes effect at the start of the next frame; a remote message
at the start of the frame it names. A step while running pauses first.

**Paused ticks.** `nnh::frame()` doesn't return while paused (plain `while (nnh::frame())` loops wait too). Each tick:
poll events (quit still quits), read input, the remote session (messages due at the next game frame are applied: sets
queue, gets and follows answer at once with the current values, screenshots are taken, `step`/`resume` end the pause),
engine controls, the overlay and panel, then a platform frame that **replays the held frame** — the last game frame's
recorded 2D, tile, particle and 3D lists (in Development builds gfx clears them when the next game frame's recording
begins instead of after submitting) — plus that frame's game debug text and the live overlay drawn now. The replay
re-resolves textures by slot and generation (D47: a released texture is skipped) and is dropped (cleared screens and
the panel only, with the note `frame not held: the scenes changed`) when scene changes were applied at the start of the
frame that paused, because their `on_exit()` may have freed what the lists point at. A held start has no frame yet:
cleared screens and `waiting for nnh (held start, 10 s)`; after 600 ticks without a PC it runs (warning).

**Frames, time, audio, input.** Paused ticks don't advance `frame_count()` or the frame clock; after a pause the frame
clock restarts at the moment the game runs again, so the first game frame's `time::delta()` is the one measured before
the pause and the next ones are normal (FR-16). On pause every playing voice and stream is held at the platform
(`voice_pause`/`stream_pause`) without touching the game's own pause flags; a step releases them for its frame (one
frame of audio) and holds them again (Q7). Input queries and events see nothing of the paused ticks: the input module is
updated only for game frames (a button pressed during the pause and still held is "pressed" on the next game frame).

Rejected: keeping the pause in each platform (no live panel, a second copy per system), capturing the screens into a
texture (≈ 1 MB of linear memory on the 3DS, a contract change), not redrawing while paused (the 3DS's double-buffered
screens flicker and the panel can't change), letting the game's update run with a flag (games would need code for it),
freezing `time::now()` (a wall clock; documented as such), audio playing on (paused screenshots and steps get noisy).

### Public API (sketch)

```cpp
// <nnh/live.hpp> — <cstddef>, <cstdint>, <type_traits>, <nnh/build.hpp>, <nnh/math.hpp>, <nnh/detail/name_hash.hpp>.
// Optional (XI). Every function: Debug/Development only; Release: an empty inline (defaults kept, changed() false).
namespace nnh {
struct Color; // <nnh/gfx.hpp>
namespace live {
inline constexpr bool kEnabled = build::kDevToolsEnabled;
inline constexpr int kMaxNameBytes = 31;   ///< ASCII letters, digits, '_', '-', '/'; ≤ 3 group levels
inline constexpr int kMaxEnumNames = 32;

/// A checked name: a literal is validated and hashed at compile time; Name::runtime() checks by assert and copies.
struct Name {
    const char* text = nullptr; std::uint32_t hash = 0; std::uint8_t length = 0;
    template <std::size_t N> consteval Name(const char (&literal)[N]);   // errors: live_name_too_long, ..._bad_char, ..._too_deep
    static Name runtime(const char* text);
};
struct FloatRange { float min = 0, max = 0, step = 0; };            ///< required for float tunables
struct IntRange { std::int32_t min = 0, max = 0, step = 1; };       ///< required for int tunables
struct OnChange { void (*fn)(void* context) = nullptr; void* context = nullptr; };  ///< at FR-3's point (Q6)

/// Owns one entry (tunable, action or registered watch); move-only; the destructor removes it. 4 bytes in every flavour.
class Handle {
public:
    Handle() = default; ~Handle(); Handle(Handle&&) noexcept; Handle& operator=(Handle&&) noexcept;
    bool valid() const;     ///< Release: false
    bool changed() const;   ///< tunables: true on the one frame FR-3 applied different bits; Release: false
    void reset();           ///< the default, written now (no changed(), no callback); Release: nothing
private:
    std::uint32_t id_ = 0;  ///< slot (16) + generation (16)
};
Handle tune(Name name, float* value, FloatRange range, OnChange on_change = {});
Handle tune(Name name, std::int32_t* value, IntRange range, OnChange on_change = {});
Handle tune(Name name, bool* value, OnChange on_change = {});
Handle tune(Name name, Color* value, OnChange on_change = {});
template <typename E, std::size_t N> requires std::is_enum_v<E>
Handle tune(Name name, E* value, const char* const (&names)[N], OnChange on_change = {});   ///< values 0..N−1
Handle action(Name name, void (*fn)(void* context), void* context = nullptr);           ///< runs at FR-3's point
// Registered watches (read only when shown or followed) — bool, int8..int32, uint8..uint32, float, Vec2, Color:
template <typename T> Handle watch(Name name, const T* value);
Handle watch(Name name, const char* const* text);                       ///< a pointer the game updates; ≤ 31 bytes read
template <std::size_t N> Handle watch(Name name, const char (&text)[N]);
// Per-frame watches: the last value given this frame; listed while shown in the last 120 frames.
template <typename T> void show(Name name, T value);                     ///< same kinds; text copied (≤ 31 bytes)
void reset_all();                                                        ///< every tunable to its default, now
void reset_group(Name group);
struct Config { int max_tunables = 128, max_watches = 64, max_shown = 64; };
void configure(const Config& config);       ///< before the first registration (question 1); asserts otherwise
void pause();                               ///< the engine pauses at the start of the next frame (D52)
enum class Page : std::uint8_t { Stats, Tunables, Watches };
void show_panel(Page page, const char* select = nullptr);   ///< shows the overlay on a page, a row selected (groups opened)
} // namespace live
} // namespace nnh
```

Exact names may still change while building; stage notes record any change. No event is introduced.

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| — | — | — | — (Q6: `changed()` and the optional callback) |

### The frame in Debug/Development builds

Release keeps today's `nnh::frame()` statement for statement (every addition sits in a discarded
`if constexpr (build::kDevToolsEnabled)` branch; T0.1/stage-end checks). In Debug/Development:

1. `close_frame()`: gfx submit (the lists are held, D52) → **`live` frame end** (registered watches followed or shown are
   read, `LiveValue` records for follows, dump requests written, per-frame watch expiry once every 60 frames) →
   `overlay_draw()` (the page; the panel reads values here, the same point as the records) → debug pass → `end_frame()`
   → screenshots.
2. `scenes_apply_pending()` (Dev notes whether it applied anything, for the replay's validity).
3. `poll_events()`, `time_frame_started()`, `read_input(sample)`, `remote_frame_start(F, sample)` (messages due at F:
   input, `Pause`, `LiveSet`/`Live` queued or answered).
4. **`dev_frame_begin(sample)`** (new): engine controls from the sample; developer requests and chords (overlay toggle,
   page, pause, step); the panel's input (edits → pending); the DevPanel blocker; if paused → paused ticks (each repeats
   3–4 with `F` unchanged and draws the replay) until a step or resume, then the frame clock restarts and audio is
   released.
5. `input_update(sample)` (filtered by the blockers), `profile_frame_boundary()`, `begin_frame()`, `gfx_gpu_idle()`,
   `++frame_count`.
6. **`live` frame start — the application point (FR-3)**: queued remote operations resolved, pending edits applied,
   `changed_frame`, callbacks, actions; refusals answered.
7. `input_publish_events()`, D32 hooks, `audio_frame()`, `events_dispatch()`, then the game; gfx clears the held lists
   when this frame's recording begins (the last statement before `frame()` returns).

### Semantics fixed by the plan

**Names** (`live_names.*`, pure, shared with `nnh`): 1–31 bytes of `[A-Za-z0-9_-/]`; no leading, trailing or double `/`;
≤ 3 `/` (groups up to 3 levels); the leaf is the part after the last `/`. Compile-time errors for literals:
`live_name_too_long`, `live_name_bad_character`, `live_name_empty_part`, `live_name_too_deep` (named `consteval`
failures). Group hash = FNV-1a 32 of the group path (`"fx/spark"` → group `"fx"`).

**Kinds and bits** (`live_kinds.hpp`): a tunable's value is 32 bits on the wire and in the entry — `float` bits, `int32`,
bool 0/1, colour `r | g<<8 | b<<16 | a<<24`, enum index; actions have none. Watches: bool (1 B), 8/16/32-bit integers
(4 B, sign by kind), float (4), Vec2 (8), Color (4), text (≤ 31 bytes).

**Ranges and stepping** (`live_steps.*`, pure, shared with `nnh`, `-ffp-contract=off`): the grid of a numeric tunable is
`g(k) = min + k·step` (float: `min + float(k) * step`, int: 64-bit) for `k = 0..n`, `n` the largest `k` with
`g(k) ≤ max` (≤ 2²⁴, asserted); `max` is also a stop. **Up by d** (d = 1, or 10 with L or R held): the smallest `k`
with `g(k) > v`, then `+ (d − 1)`; the result is `min(g(k'), max)` and `max` when `k' > n`. **Down** mirrors it with the
largest `g(k) < v`, floored at `min`. So stepping never drifts, an off-grid value (a default, a `set`) moves to its
neighbouring grid point, and a value stops at `min`/`max` (FR-8). Bool: left, right and A toggle. Colour: the selected
channel ±1 or ±10, clamped 0..255; A cycles R → G → B → A. Enum: index ±1 (±10), stopping at 0 and N−1. **Drag**:
`k = ((x − x0) · n + w / 2) / w` in integers over the bar `[x0, x0 + w]` (clamped), value `g(k)` (or `max` past `n`)
(FR-9). **Bar fill**: `px = int((v − min) / (max − min) · w)` clamped to `[0, w]`. **Refusals** (shared): non-finite,
outside `[min, max]`, an enum index ≥ N, a bool other than 0/1.

**Float text** (`live_format.*`, pure, shared with `nnh`, question 3): Ryu's `f2s` gives the shortest decimal digits
that read back to the same bits; our layout writes them as plain decimals for exponents −5..8 (`5.25`, `0.0001`,
`-0`, `120000000`) and as `d.ddde±XX` otherwise (`1.5e-07`); `inf`, `-inf`, `nan` only for watches. Integers in
decimal, colours `#RRGGBBAA` (`#RRGGBB` when alpha is 255 in the panel; always 8 digits in dumps and records' text),
Vec2 `(x, y)`, bools `true`/`false`, enums by name, text quoted with `\"` and `\\` escaped and other bytes outside
0x20..0x7E as `\xHH`. The same function prints the panel, `nnh` and the dump, so a value reads the same everywhere and
reads back exactly (FR-13).

**Lifetime and handles**: a handle is a 16-bit slot + 16-bit generation; removal bumps the generation, clears pending
edits and follows of the entry and sends `LiveRemoved` when attached. A handle used after its entry ended (moved-from,
destroyed registry at shutdown) asserts in `changed()`/`reset()` and returns false/does nothing in the Release path. The
game's variable must outlive the handle and must not move (documented; not detectable).

**Per-frame watches**: `show()` looks the name up in a 2×capacity open-addressing table (literal hashes are compile
time), stores the value and the frame; a new name takes a free slot (`LiveEntry` sent when attached); a slot not shown for
120 frames is freed at the next sweep (every 60 frames; `LiveRemoved`). A full table asserts (`live: more than 64
per-frame watches; raise live::configure({.max_shown})`).

**Panel** (`live_panel_model.*`, `live_panel_touch.cpp`, `live_panel_layout.*`, `live_panel_draw.cpp`):
- Rows come from the sorted index: a group row (`+ player (3)` / `- player`) for each group level, then its entries;
  groups start closed, except that `show_panel(page, select)` opens the selected row's groups. Selection, scroll and open
  groups are kept while the entries stay (an entry removed under the cursor moves it to the previous row).
- Buttons (engine controls, Tunables page): Up/Down move (repeat after 24 ticks, then every 6, D48's values; wrap only on a
  fresh press); Left/Right step (×10 with L or R held); A opens/closes a group, toggles a bool, cycles a colour channel,
  runs an action; B closes the enclosing group; X resets the selected row (an entry, or every entry of a group);
  Y writes the dump; SELECT + Y pause/resume; SELECT + X step one frame; SELECT + R next page; SELECT + L hide.
  Watches page: Up/Down, A (groups), B; no editing.
- Layout (bottom screen 320 × 240, 8 × 8 cells, line 10): header 12 px (page name, `12/128`, `F 1234`, `RUN`/`PAUSED`),
  rows of 22 px from y 14 (9 visible), footer 12 px (hints). Row line 1: changed mark `*` (yellow) or pending `~`, the
  cursor bar, the indented leaf name, the value right-aligned to x 262, touch buttons `-` (266–289) and `+` (292–315);
  line 2: a 6-px bar from x 8 + indent to 178 (fill and a 2-px knob), then `min..max` cut to 10 characters; colours: a
  12 × 8 swatch and, on the selected row, `R255 G192 B 64 A255` with the edited channel in yellow; enums: `< fire >
  2/4`; actions: `run` button. Watches rows are 11 px (17 visible): name, value. Exact pixels are fixed at T4.4 by
  debug-list tests and screenshots.
- Touch (bottom screen): a tap selects a row (a group row toggles); on a numeric row's bar, touching and dragging sets
  the drag value each tick; `-`/`+` step by one (repeat while held, same timing); a tap on a bool's value toggles it;
  on `run` runs an action. A touch that started outside the panel's rows does nothing.
- Every edit becomes a pending value (latest wins); the panel shows the pending value with `~` while paused.
- Cost per page: ≤ 9 rows × ≤ 8 debug-list entries + header/footer ≈ 80 entries, ≤ 1.5 KB of text (the debug list holds
  512 entries and 8 KB per frame).

**Stats page**: a new line when the registry exists: `live 12/128 tun 5/64 wat 3/64 shown 20.1 KB`.

**Dump** (`live_dump.cpp`; FR-14; question 6): at the frame end after a request (panel Y, `dump` command), the game logs
on the `live` channel `dump: begin <game> frame <F> (<n> changed of <m>)`, then for each group with a changed tunable
`dump: [player]` (nested `[a.b.c]`) and `dump: jump = 5.25 # default 4.5` (colours `"#FFC040FF"`, enums `"fire"`,
bools `true`), names without a group first, then `dump: end`. With nothing changed: `dump: begin … (0 changed of m)`
and `dump: end`. `nnh` strips `dump: `, writes `build/logs/<game>-<date>-<time>-tunes.toml` and prints its path.

**Remote protocol** (`remote/live_wire.{hpp,cpp}`, pure, shared with `nnh`; message bytes 0–7 as D28):

| Message | Bytes 8–15 | Decoded into |
|---------|------------|--------------|
| `LiveSet` (9) | hash u32 (≠ 0), value bits u32 | `press`, `release` |
| `Live` (10) | op u8 (1 Catalogue, 2 Get, 3 Follow, 4 Unfollow, 5 Reset, 6 Dump, 7 Action), arg u8 (Reset: 0 one, 1 group, 2 all; Follow: every 1..255), count u16 (Follow: frames, 0 = until Unfollow), hash u32 (0 only for Catalogue, Dump, Reset all, Unfollow all) | `screens`, `stick`, `id`, `press` |
| `Pause` (11) | op u8 (1 pause, 2 resume, 3 step), 0, steps u16 (step: 1..3600, else 0), 0 × 4 | `screens`, `id` |

Every other byte must be 0 (`Reject::Range`), as D28. The decoded `Message` stays 32 bytes.

| Record | Payload | When |
|--------|---------|------|
| `LiveEntry` (8) | u32 frame, u32 hash, u8 kind, u8 flags (tunable/action/watch/shown, changed, pending), u8 name length, u8 enum count, value, default, min, max, step (u32 bits each), name, enum names (u8 length + bytes each); 28..540 B | after a Catalogue request (every entry, sorted), then for each entry added while attached |
| `LiveRemoved` (9) | u32 frame, u32 hash | an entry removed while attached |
| `LiveValue` (10) | u32 frame, u32 hash, u8 kind, u8 flags (paused, pending, changed this frame, stale), u8 length, u8 0, value (≤ 32 B); 12..44 B | Get, each due Follow frame, at the frame end (paused: at once, frame = last game frame) |
| `LiveRefused` (11) | u16 seq, u8 reason (1 not declared, 2 wrong kind, 3 out of range, 4 not finite, 5 queue full, 6 follows full, 7 no live values in this game), u8 0, u32 hash | a refused operation |
| `LiveState` (12) | u32 frame (the next game frame), u8 state (0 running, 1 paused), u8 flags (catalogue complete), u16 steps left | the Catalogue request's end; every pause/resume/step change; each paused tick that applied messages (the runner's flow control while ticks stop) |

The session sends live records only after a `Live` Catalogue request in this attachment (`live_records` flag), so an older
`nnh` never meets an unknown record. A game without the module answers every `LiveSet`/`Live` with `LiveRefused`
(reason 7) and a Catalogue with an empty catalogue (`LiveState`, catalogue complete); `Pause` works without it. An older
game rejects types 9–11 as `Reject::Type`; `nnh` then says `the game has no live values (built before spec 030)`.
Refusals are counted (`live: refused 3 (not declared ×2, out of range ×1)` on the Stats line) and warned once per 60
frames like spec 013's rejections; they don't count toward the 8-in-a-row shutoff (that counts only malformed messages).

**`nnh` commands** (interactive, FR-11): `tunes [GROUP]` (name, kind, value, default, range and step, `*` changed),
`get NAME`, `set NAME VALUE`, `reset NAME|GROUP|all`, `action NAME`, `watches [GROUP]`, `watch NAME [every N]`,
`unwatch NAME|all`, `dump`, `pause`, `step [N]`, `resume`. Values: a number (`5`, `-2.5`, `1e-3`; a float tunable takes
an integer literal too), `true`/`false`, `#RRGGBB[AA]`, an enum name or index. Refusals before sending:
`unknown name "player/jmp" (did you mean "player/jump"?)`, `player/jump is a float: "abc" is not a number`,
`player/jump: 12 is outside 0..10`, `player/jump: nan is not finite`. While paused, typed commands are stamped for the
next game frame. Followed values print as `[1234] player/pos = (12.5, 40)`.

**Script commands** (FR-12, spec 013's grammar): `set NAME VALUE`, `reset NAME|GROUP|all`, `action NAME`, `dump`,
`pause`, `resume`, `step [N]` (1..3600, default 1), `expect-value NAME OP VALUE [~ E] [within N]` (OP `==`, `!=`, `<`,
`<=`, `>`, `>=`; ordered operators only for numbers and enum indexes; `~ E` for floats and Vec2, `|a − b| ≤ E` per
component, E finite ≥ 0; text compared byte for byte). Clock: `set`, `reset`, `action`, `dump`, `pause`, `resume` take
no frames; `step N` takes N; `expect-value` without `within` checks the value at the end of the clock's frame (one
sample; while paused, the current value); with `within N` the first frame in `[clock, clock + N]` whose value passes,
then the clock = that frame + 1, like `expect`. **While paused** (statically tracked by the parser) commands that move
the clock (`wait`, `hold`, `tap`, `drag`, `expect`, `expect-value … within`) are errors
(`line 14: tap while paused: step or resume first`), `screenshot` takes no frame, and `press`/`release`/`stick`/
`touch`/`untouch`/`slider`/`language` apply when frames run (they also drive the panel's paused ticks). Syntax errors
are reported with their line before anything is sent (as today). **Names** are resolved against the catalogue when
their segment is sent — the Hello answer for the first segment, the matching line of the previous `expect` for later
ones — so names a scene registers later work (question 4): `line 22: player/jump is not declared by the game (frame
230)`. A refused operation fails the script with its line.

**`--tunes FILE`** (FR-15; `tunes_file.*`): read before anything is sent; the dump format plus quoted full names
(`"player/jump" = 5.25`) and comments; errors `tunes.toml:4: expected "name = value"`, `tunes.toml:7: "player/jump" is
set twice (first at line 3)`, `tunes.toml:9: name "fx//spark" has an empty part`, ≤ 1 MiB, ≤ 4096 values. It implies
`--hold`: the game waits before frame 1; `nnh` requests the catalogue, reports unknown names (`tunes.toml:6: unknown
name "player/dash": skipped`) and values that don't fit (refused with their line), sends the rest as `LiveSet` for the
held frame, then `Pause resume` for that frame; a script's first segment starts there.

**Hold** (`--hold`, `--tunes`): 3DS argv gains `--nnh-hold` (the netloader's argv, D23); the PC gets `NNH_HOLD=1`. Each
platform's `poll_dev_requests()` returns the new `kDevHoldAtStart` bit once (Development only, inside a discarded
branch in Release). Contract bits added: `kDevTogglePause`, `kDevStepFrame`, `kDevHoldAtStart` (`nnh/platform/system.hpp`).

**Misuse** (assert in Debug/Development; Release has no code): a name twice or equal to a group path; a full table; a bad
range or default; an enum outside 0..N−1 at registration; `configure()` after the first registration; `tune` with a null
pointer; a stale handle's `changed()`/`reset()`; `show()` of a text longer than 31 bytes (cut, warned once); a
per-frame table overflow. Never undefined behaviour (ASan/UBSan, the random-operation test).

### Release by construction, and `hello`

`hello` links `app.cpp`, `overlay.cpp`, `debug.cpp`, `debug_pass.cpp`, `gfx_frame.cpp`, `clock.cpp`, the audio core and,
per platform, `devtools_pc.cpp` / `system_3ds.cpp`. Rules for this spec:

1. New behaviour lives in **new files** referenced only from discarded `if constexpr (build::kDevToolsEnabled)` branches
   (or from the `live` module, which only such branches and `live::` calls reach).
2. Existing Release-linked files change only **inside such branches**; state they must share is reached through internal
   headers (`inline` accessors over an existing variable, or an `extern` the Dev file defines). No new out-of-line function
   is added to a Release-linked object unless T0.1 shows the link drops unreferenced functions (`--gc-sections`), and then
   only with the object compared.
3. Checked at **every stage end**: 3ds-release `hello.elf`/`.3dsx` SHA-256 identical to T0.1's; pc-release size and
   `nm -C` identical; when a hash differs, the touched objects' disassembly is compared to find it.

Debug/Development `hello` grows (pause, controls, pages, replay, audio hold, the remote decoders and live stub): estimated
3–6 KB of `.text` and ≈ 0.2 KB of `.bss`, measured at each stage end (question 7). Probes at T8.3: a game that calls
nothing from `live::` has no `live_` symbol; a game that registers anything links the whole module through the ops table
(the panel included: one link unit, its size recorded, expected 10–14 KB with Ryu).

### Spec 029 overlap

Spec 029 (merged 2026-11-26) and this spec touch the same files in five places; 030 starts after 029's merge, so
none of these is a merge conflict, but the numbering depends on it:

| Shared file | 029 adds | 030 adds |
|-------------|----------|----------|
| `engine/core/src/remote/remote_wire.{hpp,cpp}` | `MessageType::Language = 8`, `pack_language`/`unpack_language`, the decoder case | `LiveSet = 9`, `Live = 10`, `Pause = 11`, `RecordType` 8–12, length rules, the decoder cases calling `live_wire.cpp` |
| `engine/core/src/remote/remote_session.cpp` | `apply_language`, override on attach/detach | dispatch of the three types, the `live_records` flag, paused-tick processing, `LiveState` |
| `tools/nnh/src/remote/script.hpp`, `script_parser.cpp`, `script_timeline.*`, `script_interactive.cpp`, `tools/nnh/CMakeLists.txt` | `language CODE|off` | the commands above, paused-section rules (`script_live.cpp` keeps the parser under 400 lines) |
| `engine/core/contract/nnh/platform/system.hpp` | `system_language()` | three `DevRequest` bits |
| `scripts/check_release_strings.sh`, `engine/core/CMakeLists.txt`, `tests/CMakeLists.txt`, `examples/CMakeLists.txt` | its lines at its anchors | one 030 block each, after 029's |

No other overlap: 030 doesn't touch `gfx.hpp`, `TextOptions`, the UI, fonts, localization or `system/`. The debug overlay,
`app.cpp`, gfx's frame code and the audio core are untouched by 029.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/live.hpp` | new | the public API above (≈ 170 lines; inline Release forms) |
| `engine/core/contract/nnh/platform/system.hpp` | changed | `kDevTogglePause`, `kDevStepFrame`, `kDevHoldAtStart` |
| `engine/core/src/live/live_names.{hpp,cpp}` | new | name rules, leaf/group split, group hashes, run-time check (pure; also in `nnh`) |
| `engine/core/src/live/live_kinds.hpp` | new | kinds, sizes, bit packing of values (pure) |
| `engine/core/src/live/live_steps.{hpp,cpp}` | new | grids, stepping, drag, bar fill, refusals (pure; also in `nnh`; `-ffp-contract=off`) |
| `engine/core/src/live/live_format.{hpp,cpp}` | new | shortest float text (Ryu) and every value's text (pure; also in `nnh`) |
| `engine/core/src/live/live_block.hpp` | new | block layout: header, entries, watches, shown table, follows, op queue, sorted index |
| `engine/core/src/live/live_registry.{hpp,cpp}` | new | create (tag `live`), add/remove, sorted insert, find by hash, generations, shutdown hook, `g_live_ops` |
| `engine/core/src/live/live_tune.cpp` | new | `tune`/`action` back ends, checks, `Handle`, `configure`, resets |
| `engine/core/src/live/live_apply.{hpp,cpp}` | new | pending edits, the op queue, the application point, `changed_frame`, callbacks, actions |
| `engine/core/src/live/live_watch.cpp` | new | registered watches, `show()`, expiry, reading values into bytes |
| `engine/core/src/live/live_panel_model.{hpp,cpp}` | new | rows, groups, selection, scroll, repeat, buttons → edits (pure over an input value) |
| `engine/core/src/live/live_panel_touch.cpp` | new | taps, drags, `-`/`+`, bool and action hits (pure) |
| `engine/core/src/live/live_panel_layout.{hpp,cpp}` | new | integer row geometry shared by drawing and touch (pure) |
| `engine/core/src/live/live_panel_draw.cpp` | new | Tunables and Watches pages into the debug list |
| `engine/core/src/live/live_remote.cpp` | new | catalogue, `LiveEntry`/`LiveRemoved`/`LiveValue`/`LiveRefused`, follows, op handling |
| `engine/core/src/live/live_dump.cpp` | new | the dump lines |
| `engine/core/src/live/live_ops.cpp` | new | the ops table, the Stats line, the `live` log channel |
| `engine/core/src/debug/live_hook.hpp` | new | `struct LiveOps` and `extern const LiveOps* g_live_ops` (Dev) |
| `engine/core/src/debug/dev_controls.{hpp,cpp}` | new | engine controls per tick: held/pressed/released, repeat, touch (pure part tested) |
| `engine/core/src/debug/overlay_pages.{hpp,cpp}` | new | pages, chords, the DevPanel blocker, empty pages, `show_panel` |
| `engine/core/src/debug/overlay.cpp` | changed | Dev branch: page dispatch and the Stats line (Release code identical) |
| `engine/core/src/debug/debug_held.cpp` | new | the held frame's game debug entries replayed on paused ticks |
| `engine/core/src/debug/debug_list.hpp`, `debug.cpp` | changed | a mark between the game's entries and the overlay's (Dev branch) |
| `engine/core/src/app/dev_pause.{hpp,cpp}` | new | `PauseState` (pure) and the paused-tick loop; `live::pause()` back end |
| `engine/core/src/app/app.cpp` | changed | the Dev frame order above (Release statements unchanged) |
| `engine/core/src/gfx/gfx_replay.cpp`, `gfx_frame.cpp`, `gfx_internal.hpp` | new / changed | held lists (Dev: cleared at the next recording), replay, validity |
| `engine/core/src/scene/scenes_internal.hpp` + scene stack | changed | a Dev-only "applied something this frame" flag (rule 2) |
| `engine/core/src/audio/audio_hold.cpp`, `audio_internal.hpp` | new / changed | hold and release every voice and stream at the platform |
| `engine/core/src/time/time_internal.hpp` (+ `clock.cpp` Dev accessor) | changed | restart the frame clock after a pause |
| `engine/core/src/input/input_block.hpp` | changed | `InputBlocker::DevPanel = 4` |
| `engine/core/src/remote/live_wire.{hpp,cpp}` | new | the three messages and five records: encode, decode, validate (pure; also in `nnh`) |
| `engine/core/src/remote/remote_wire.{hpp,cpp}` | changed | the new type numbers, decoder dispatch, record lengths |
| `engine/core/src/remote/remote_live.cpp` | new | session glue: dispatch, `live_records`, `LiveState`, refusals without the module, paused ticks |
| `engine/core/src/remote/remote_session.cpp`, `remote_internal.hpp` | changed | calls into `remote_live.cpp`; `remote_frame_start` usable on paused ticks |
| `engine/core/CMakeLists.txt` | changed | `_nnh_live_sources`, the Dev core files, Ryu, `-ffp-contract=off` for `live_steps.cpp` |
| `third_party/ryu/{f2s.c, ryu.h, common.h, digit_table.h, f2s_intrinsics.h, (f2s_full_table.h), LICENSE-Boost, README.md, CMakeLists.txt}` | new | pinned, unmodified; a C object library for core and `nnh` |
| `engine/platforms/pc/src/devtools_pc.cpp`, `app_pc.cpp` | changed | `Pause`/`F10` → requests; `NNH_HOLD` (Dev branches only; the old blocking wait stays, unreachable) |
| `engine/platforms/3ds/src/system_3ds.cpp` | changed | `--nnh-hold` → `kDevHoldAtStart` once (Dev branch) |
| `engine/platforms/fake/src/system_fake.cpp`, `include/nnh/platform_fake.hpp` | changed | `fake::request_pause/step/hold()` |
| `tools/nnh/src/remote/live_catalog.{hpp,cpp}` | new | the mirror of the game's entries from records |
| `tools/nnh/src/remote/live_text.{hpp,cpp}` | new | value parsing per kind (`std::from_chars`), refusals, did-you-mean |
| `tools/nnh/src/remote/script_live.cpp` | new | parsing the new script lines and paused-section rules |
| `tools/nnh/src/remote/script_runner_live.cpp` | new | resolution per segment, expect-value, follows, `LiveState` flow control, `--tunes` |
| `tools/nnh/src/remote/live_interactive.cpp` | new | the typed commands and their output |
| `tools/nnh/src/remote/dump_capture.{hpp,cpp}` | new | dump lines → `build/logs/<game>-<date>-<time>-tunes.toml` |
| `tools/nnh/src/remote/tunes_file.{hpp,cpp}` | new | the `--tunes` reader (bounded, fuzzed) |
| `tools/nnh/src/remote/script.hpp`, `script_parser.cpp`, `script_timeline.*`, `script_runner.*`, `script_interactive.cpp`, `game_records.*` | changed | new steps, events and records |
| `tools/nnh/src/cli.cpp`, `netloader.cpp`, `tools/nnh/CMakeLists.txt` | changed | `--tunes`, `--hold`, `--nnh-hold` in argv, `NNH_HOLD=1`, the shared core files and Ryu |
| `scripts/check_release_strings.sh` | changed | the live texts and the example's names |
| `tests/live/`, `tests/debug/`, `tests/app/`, `tests/gfx/`, `tests/audio/`, `tests/remote/`, `tests/fuzz/`, `tools/nnh/tests/` | new / changed | the test strategy below |
| `tests/live_example/test_self_check.cpp`, `tests/CMakeLists.txt` | new / changed | AC-2's golden; ctests `live_self_check_pc`, `live_check_script_pc`, `live_panel_script_pc`, `live_keep_pc` |
| `examples/live/` | new | the example (below) |
| `examples/platformer/tuning.{hpp,cpp}`, `player.cpp`, `level_camera.*`, `level_scene.cpp`, `CMakeLists.txt` | new / changed | movement and camera constants as tunables with the same defaults |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(live)` after 029's |
| docs (stage 11) | new / changed | tasks T11.1–T11.6 |

### Data and memory

| Item | 3DS | 64-bit PC | Notes |
|------|-----|-----------|-------|
| tunable or action | 80 B | 96 B | pointer, hash, default/min/max/step/pending bits, changed frame, callback + context, enum names, slot generation, kind, flags (48 B on the 3DS) + name 32 B |
| registered watch | 48 B | 56 B | pointer, hash, generation, kind, flags + name |
| per-frame watch | 80 B | 80 B | hash, last frame, kind, length, value 32 B, name 32 B |
| follow | 12 B × 16 | same | hash, every, frames left, next frame |
| remote op queue | 12 B × 64 | same | kind, seq, hash, bits |
| sorted indexes, shown table | 2 B × (128 + 64 + 64) + 2 B × 128 | same | |
| header | ≈ 96 B | ≈ 128 B | capacities, counts, flags, the panel's state |
| **block at the defaults** | **≈ 20.4 KB** | **≈ 22.6 KB** | tag `live`; one allocation at the first registration; freed at shutdown (Dev shutdown hook) |
| Dev core `.bss` | ≈ 0.2 KB | ≈ 0.3 KB | pause state, controls, pages (in every Debug/Development game) |
| Release | 0 | 0 | `Handle` is 4 bytes in the game's own objects |

The spec's target was ≤ 64 B per entry and ≈ 16 KB; copied names make it 80 B and ≈ 20 KB (question 2).

### Platform work

- **pc**: `devtools_pc.cpp` — `Pause` → `kDevTogglePause`, `F10` → `kDevStepFrame` (Dev branch; `ctx.paused` is never set
  again, so `dev_wait_while_paused()` and `video_pc.cpp` stay byte for byte); `app_pc.cpp` reads `NNH_HOLD=1` into
  `ctx.dev_requests` once (Dev branch). Audio hold uses the existing `voice_pause`/`stream_pause` (the SDL mixer then
  skips them).
- **3ds**: `system_3ds.cpp` — `poll_dev_requests()` returns `kDevHoldAtStart` once when `__system_argv` holds `--nnh-hold`
  (Dev branch; Release still `return 0`); audio hold through `ndspChnSetPaused` (already behind `voice_pause`/
  `stream_pause`). Nothing else: pages, chords, pause and the replay are core.
- **fake (tests)**: `fake::request_pause()`, `request_step()`, `request_hold()` set the request bits;
  begin/end-frame counters and recorded draw items already let tests compare a paused tick's items with the held frame's.

### Build budgets

`<nnh/live.hpp>` measured at T2.8 against T0.1's `<nnh/math.hpp>` TU (+250 lines, +0.03 s). Clean `nnh_core` gains
≈ 22 sources and Ryu (expected +1–1.5 s over T0.1's figure; < 30 s). Edit one `examples/live` `.cpp` → simulator and
`.3dsx` within the constitution budgets (T8.4). `nnh` gains ≈ 9 sources and the shared core files.

### ARM11 cost plan (AC-7)

Calibration as plan 029 (≈ 2 cycles per instruction on the Old 3DS, ≈ 110 cycles per miss; upper end × 1.3; anything
within 1.5× of its target is **at risk**; every bench phase times only its workload).

| Workload | Hot path | Estimate (upper × 1.3) | Target | Verdict |
|----------|----------|------------------------|--------|---------|
| hidden: 128 tunables + 64 watches registered, overlay hidden, nobody attached, nothing changing | per frame: `g_live_ops` check, the frame-start flags (pending, queue), the frame-end flags (follows, dump, sweep every 60) ≈ 20–30 instructions | ≈ 0.1–0.2 µs | Δ vs nothing registered ≤ 0.02 ms (noise) | expected PASS |
| `show()` × 64 per frame | consteval hash, 1–2 probes, store ≤ 8 B and the frame ≈ 25–35 instructions | 3–5 µs (6.5 µs) | < 0.05 ms | expected PASS |
| Tunables page, CPU (model + layout + formatting + recording) | 9 rows × (a float (Ryu ≈ 300–600 cycles), the range (2 floats), 6–8 debug-list adds ≈ 60 cycles each) + header/footer | 0.06–0.12 ms (0.16) | < 0.5 ms | expected PASS |
| Tunables page, whole frame cost (incl. the debug pass's ≈ 500–650 glyph sprites) | per glyph: layout + item ≈ 80 cycles + `draw_2d`'s per-item cost (unmeasured on the console for the debug pass: est. 1.5–3 µs) | 0.8–2.0 ms (2.6) | < 2.0 ms over the hidden frame | **at risk** |
| paused tick with the platformer level held | a normal submit of the held lists + the page | the level's own draw cost + the page | ≤ 16.7 ms (60 Hz) | expected PASS (the level runs at 60 Hz with its update) |
| applying one remote `set` with a callback | queue, find by hash (≤ 128 compares; rare), refusal checks, write, callback | < 5 µs | — | recorded |

T7.6 checks the disassembly (no `__aeabi_*` division in stepping, drawing or `show()`; the float text in Ryu's integer
code only; `-fstack-usage` < 1 KiB along the page drawing and the paused tick). A missed target is optimised first (risk
1's list), then its budget is set from the console measurement with ≈ 25 % margin and written in the spec's AC-7,
`bench.cpp` and `docs/api/live.md`.

### Self-check and bench (AC-2, AC-6, AC-7)

- **Self-check** (`examples/live/self_check.{hpp,cpp}`, pure, also compiled into `nnh_tests`): a 600-frame simulation of
  the example's character, particles and spawner with fixed inputs, **from the current tunable values** (whatever they
  are), folded with FNV-1a over positions, velocities, states, colours and the panel's formatted text of every tunable;
  prints `live: sum XXXXXXXX`; `live: PASS defaults` when the values are the defaults and the sum is
  `kLiveDefaultsGolden`. `check.nnhs` sets a fixed list of values first and expects `live: sum <kLiveScriptGolden>`.
  Release prints the defaults' sum (AC-6). Goldens recorded from pc-debug; pc-release, GCC 13/14, clang 18, the 32-bit
  build and the console must agree.
- **Bench** (`examples/live/bench.{hpp,cpp}`, `LIVE_DEMO=bench` or X; 300 frames per phase, average and worst, only the
  measured calls in the scope): `empty` (the bench scene, nothing registered), `hidden192` (128 tunables + 64 watches,
  hidden), `show64` (64 `show()` calls), `stats` (Stats page), `tunables` (Tunables page, 9 rows of floats/colours, CPU
  scope + whole frame − hidden), `watches` (Watches page with 17 rows), `edit` (one edit per frame through the panel model
  + callback), `memory` (bytes per entry and the block). Lines `live bench: <phase> avg X ms worst Y ms (target < Z ms)
  PASS|FAIL`, then `live bench: PASS` or `live bench: FAIL (<names>)`; Debug logs `not judged (Debug)`.

### The example `examples/live` (AC-1–AC-3, AC-6–AC-8)

Shapes only (original, no image files): a backdrop gradient of two colours, ground, a jumping square character with
eyes, a spawner of bouncing balls, a few dust particles (spec 024) when landing. `tuning.{hpp,cpp}` owns a `Tuning`
struct and its handles: `player/jump` (float 0..12 step 0.25, 5.5), `player/gravity` (0.05..1 step 0.01, 0.3),
`player/run` (0.25..4 step 0.25, 1.75), `debug/hitboxes` (bool), `spawn/count` (int 0..32 step 1, 6), `spawn/shape`
(enum circle/square/diamond), `colors/sky` and `colors/ground` (Color), `fx/dust` (float 0..64 step 1, 12, with an
`OnChange` that rebuilds the particle description), `player/respawn` (action); watches: `player/pos` (Vec2),
`player/vel` (Vec2), `player/state` (text), `frame/count` (u32, registered), `spawn/alive` (`show()` each frame).
Scenes: `live_scene` (the showcase), `check_scene` (Y or `LIVE_DEMO=check`), `bench_scene` (X or `LIVE_DEMO=bench`).
`LIVE_SHOT=stats|tunables|watches|colour|paused` set the overlay with `debug::set_overlay_visible`,
`live::show_panel(page, select)` and `live::pause()`. Log lines: `live: showcase`, `live: jump <state>`, `live: respawn`,
`live: dust rebuilt <n>`.

**Platformer**: `examples/platformer/tuning.{hpp,cpp}` turns `kRunSpeed`, `kGravity`, `kJumpSpeed`, `kMaxFall` and the
camera's look-ahead constants into tunables with the same defaults (`player/…`, `camera/…`), registered by the level
scene; the replay ctest's lines stay exactly the tree's (proof that defaults are exact, Q10).

### Console scripts

- `examples/live/tests/check.nnhs` (AC-2): `fail-on "] E "`, `fail-on "live: FAIL"`; `wait 30`; `expect "live: showcase"
  60`; `set` of jump, gravity, run, count, shape, both colours and dust; `expect-value player/jump == 6.25`;
  `expect-value spawn/alive == 9 within 60`; `reset player`; `expect-value player/jump == 5.5`; `dump` → `expect "dump:
  end" 10`; `pause`; `expect-value frame/count == <F>`; `step 3`; `expect-value frame/count == <F + 3>`; `screenshot
  paused`; `resume`; `tap Y` → `expect "live: sum <kLiveScriptGolden>" 700`; `tap X` → the bench lines and `live bench:
  PASS`; `end`.
- `examples/live/tests/panel.nnhs` (AC-3): presses on the panel never reach the game (checked with `expect-value
  player/state == "idle"` after A presses); `tap SELECT+L`, `tap SELECT+R` →
  Tunables; Down/Right, Right with R held (×10), Right × n until the value stops at max, A on a colour row and Right (a
  channel), a tap on a row, a drag across `player/jump`'s bar, a tap on `debug/hitboxes`, `-`/`+` taps, A on
  `player/respawn` → `expect "live: respawn"`; an `expect-value` after each; screenshots `tunables`, `colour`, `watches`;
  then `press A` (on a group row), `tap SELECT+L` (the overlay closes with A held), `wait 10`, `expect-value
  player/state == "idle"` (A stays blocked until released), `release A`, `tap A` → `expect "live: jump" 10` (the game
  has its controls back).
- `examples/live/tests/keep.nnhs` and `keep_second.nnhs` (AC-8): run 1 (`keep.nnhs`) — `set` three values, `dump`
  (`nnh` saves the file and prints its path); run 2 — `nnh run live --tunes <that file> --script keep_second.nnhs`: only
  `expect-value` of the three values on frame 1, then `end`. Two runs, so two presses of Y on the console.
Exact lines, coordinates and counts are recorded from the `--pc` runs at T7.5 into "Stage 10: console run (prepared)".

## Test strategy

All core tests run in `nnh_tests` on the fake platform (Windows; VM 120: GCC 13/14, clang 18 with ASan/UBSan, the 32-bit
host build) in pc-debug **and** pc-release (Release tests check the inline forms: nothing registered, `changed()` false,
`sizeof(Handle) == 4`, no allocation). `nnh` tests in `nnh_tool_tests`. Expected texts, pixels, grid points and records are
written by hand in the tests, never produced by the code under test.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/live/test_live_names.cpp` (every rule; consteval failures as compile-fail ctests `live_name_*_fails`), `test_live_registry.cpp` (ranges, steps, defaults, enums, duplicate and group-clash names → asserts via the test hook) |
| FR-2 | `test_live_registry.cpp` (the variable is the game's: reads see writes the apply point made, no lookup), `test_live_cost.cpp` (no allocation after the block) |
| FR-3 | `tests/live/test_live_apply.cpp` (panel and remote edits land at the application point of the right frame on the fake's `nnh::frame()`; a value read in update is constant over the frame; a `set` for a tunable registered in that frame's `on_enter()` lands) |
| FR-4 | `test_live_apply.cpp` (`changed()` exactly once; same bits → no change; callbacks at the point, in sorted order; Release `false`) |
| FR-5 | `test_live_apply.cpp` (reset one/group/all from the panel and remote through FR-3; the game's reset immediate), `test_live_panel_draw.cpp` (the `*` mark) |
| FR-6 | `test_live_registry.cpp` (destroyed handles leave listings and the catalogue — `LiveRemoved`; pending edits and follows dropped; stale handle asserts; a remote `set` for a removed name is refused and writes nothing — guard bytes) |
| FR-7 | `tests/debug/test_overlay_pages.cpp` (SELECT + R cycles, empty pages without the module, the Stats line), `test_live_panel_draw.cpp` (debug-list items of every row kind at exact positions), screenshots T7.3, console pair T10.2 |
| FR-8 | `tests/live/test_live_steps.cpp` (grid rules on hand-computed floats: ends, off-grid, ×10, negative ranges, steps not dividing the range, n near 2²⁴; ints; colours; enums), `test_live_panel_model.cpp` (navigation, repeat 24/6, wrap on fresh press, scroll keeps the selection visible, groups) |
| FR-9 | `test_live_steps.cpp` (drag rounding), `tests/live/test_live_panel_touch.cpp` (taps, drags tick by tick, `-`/`+` with repeat, bool, action, touches outside rows) |
| FR-10 | `test_overlay_pages.cpp` (the DevPanel blocker on/off with pages; the game's queries read idle; a button held when the overlay closes stays blocked until released; independent of `input::set_blocked` and transitions), `tests/remote/test_remote_exclusive.cpp` extended (a script's input drives the panel, the console's doesn't) |
| FR-11 | `tools/nnh/tests/test_live_catalog.cpp`, `test_live_text.cpp` (every kind, refusals with their texts, did-you-mean), `test_live_interactive.cpp` (each command's messages and output) |
| FR-12 | `tools/nnh/tests/test_script_live.cpp` (syntax, errors with lines, paused-section rules), `test_script_timeline_live.cpp` (clock rules), `test_script_runner_live.cpp` (fake console: resolution per segment, unknown names fail with their line, expect-value windows and tolerance, refusals fail, `LiveState` flow control) |
| FR-13 | `tests/live/test_live_format.cpp` (hand-written texts; round trip of every printed float through `std::from_chars` for 10⁶ seeded floats + all powers of two + subnormals' edges), ctest `live_float_text_exhaustive` (all 2³² bit patterns, VM 120 only), the same script's values on PC and console (AC-2) |
| FR-14 | `tests/live/test_live_dump.cpp` (the exact lines, nesting, ordering, comments, nothing changed), `tools/nnh/tests/test_dump_capture.cpp` (file name and content) |
| FR-15 | `tools/nnh/tests/test_tunes_file.cpp` (format, every error with `file:line`), `test_script_runner_live.cpp` (`--tunes`: unknown names skipped, sends before the held frame, resume), AC-8 |
| FR-16 | `tests/app/test_dev_pause.cpp` (paused ticks: no scene update, plain loops wait, `frame_count()` and the frame clock stop, step N runs exactly N, `time::delta()` after resume, requests from controls/keys/remote/`live::pause()` and when each takes effect, held start with and without a PC, timeout, quit while paused), `tests/gfx/test_gfx_replay.cpp` (a paused tick's items equal the held frame's: 2D, tiles, particles, 3D on the fake; stale textures skipped; dropped after a scene change), `tests/audio/test_audio_hold.cpp` (held, step plays one frame, the game's own pause flags untouched), `tests/remote/test_remote_pause.cpp` |
| FR-17 | `tests/live/test_live_watch.cpp` (every kind; registered read only when shown/followed — a read counter; `show()` latest value; expiry after 120 frames; the frame-end read point) |
| FR-18 | `tests/remote/test_live_wire.cpp` (encode/decode of every message and record, every refusal), `tests/remote/test_remote_live.cpp` (refusals as records, counters, warnings rate-limited, no shutoff), capacity + 1 tests for every table, mini-fuzz `tests/fuzz/fuzz_live_remote.cpp` and libFuzzer `libfuzzer_live_remote.cpp` (VM 120) |
| FR-19 | `test_live_cost.cpp` (no allocation over 100 000 frames of edits, follows and pages; a walk counter stays 0 with the overlay hidden and nothing pending), bench `hidden192` and `show64` |
| FR-20 | pc-release tests (inline forms), T8.2 (`check_release_strings.sh`, `nm` of the Release example: no `live`/`Ryu`/`f2s` symbol), T0.1/T8.3 `hello` identity, AC-6 |
| FR-21 | T8.3 probes (`hello` has no `live_` symbol; empty pages), the platformer's plain use, `examples/live`'s plain loop variant in `tests/live/test_live_plain_loop.cpp` |
| FR-22 | AC-2 on every compiler and the console; `test_live_panel_draw.cpp` (integer items); the console screenshot pair |
| Q9 enums, actions | `test_live_steps.cpp`, `test_live_panel_model.cpp`, `test_live_apply.cpp` (actions run at the point, once per press/`action`), `test_live_text.cpp` (names and indexes) |
| AC-1 | T7.3 screenshots read back; T10.2 pair |
| AC-2 | `tests/live_example/test_self_check.cpp`, ctests `live_self_check_pc`, `live_check_script_pc`; the console run |
| AC-3 | `panel.nnhs` (`live_panel_script_pc`, console); `test_overlay_pages.cpp` |
| AC-4 | every unit test above |
| AC-5 | fuzz targets; capacity + 1 tests |
| AC-6 | T8.2, T8.3 |
| AC-7 | the bench on the console; T7.6's table |
| AC-8 | `keep.nnhs` + `keep_second.nnhs` (`live_keep_pc`, console) |
| AC-9 | stage 11 |

### Planned hand mutants (T8.1)

`build/ref-030/mutants.py`, one at a time, `nnh_tests live_ debug_ app_ gfx_replay remote_` and `nnh_tool_tests live script`
(≈ 40): grid `>` as `>=`; down step floored at `min + step`; ×10 as ×9; n off by one; `max` stop dropped; drag rounding
down; bar fill rounded up; colour channel wrapping; enum wrapping; bool toggled twice; `changed_frame` set when bits are
equal; callback before the write; callbacks in registration order; remote op resolved at receipt; pending applied while
paused; game reset going through FR-3; stale generation accepted; removal keeping follows; group reset by leaf;
group-clash check skipped; sorted insert unstable; show expiry at 119; registered watch read every frame; Ryu layout
exponent bound −6; negative zero printed `0`; `#RRGGBB` in dumps; dump table order; dump including unchanged values;
refusal counted toward the shutoff; `live_records` not required; step while running not pausing; step N running N + 1;
frame clock not restarted; audio released at resume only (step silent); paused tick calling `input_update`; replay
after a scene change; DevPanel blocker lifted without the filter; panel reading the console's input in exclusive mode;
`expect-value` window from the segment's start; paused-section rule missing `drag`. Survivors become tests or are argued
equivalent in "Mutation testing".

## Risks and alternatives

1. **The panel page's frame cost** (at risk: hundreds of debug glyph sprites). Optimisation list, none changing a pixel:
   (a) skip unchanged formatting (cache each row's text by value bits and selection), (b) draw the bar and buttons as one
   rectangle each, (c) shorten the hint footer, (d) a 2-line row only for the selected numeric row (the others one line,
   more rows visible — changes the look, so only with the maintainer's OK and new goldens). The debug pass is shared
   with spec 002's overlay, whose Stats page has run on the console since spec 002.
2. **Replaying the held frame** touches gfx's frame lifecycle (Development only). Risks: a list read after what it points
   at was freed (scene changes: replay dropped; textures: re-resolved; tile maps and emitters: only scene exits free them
   between frames), and modules with end-of-frame state (particle batches, 3D records, probes). Mitigation: the clears move
   as a group behind one function, tests compare paused items with held items for every kind of command, ASan runs the
   random-operation test with pauses. Fallback (with the maintainer's OK): paused ticks show cleared screens and the panel,
   the held frame shown only on the first tick.
3. **Zero Release bytes** with edits inside Release-linked files (`app.cpp`, `overlay.cpp`, `debug.cpp`, `gfx_frame.cpp`,
   `clock.cpp`, scene stack, `devtools_pc.cpp`, `system_3ds.cpp`). Mitigation: rule 1–3 above, hashes at every stage end,
   per-object disassembly diffs when a hash moves.
4. **Names in Release**: a `Name` built from a literal in a discarded call should leave no string behind; GCC does not
   emit unreferenced literals at `-O2`, and AC-6's string check proves it on both Release presets. Fallback: a macro
   `NNH_LIVE_NAME("x")` that is `""` in Release (an API change, only if needed).
5. **Ryu** (question 3): third-party C in the Development build of every game that uses `live::`; integer-only (same
   output everywhere); ≈ 2–3 KB of code and ≈ 0.6 KB of tables. Alternative: our own shortest formatter (≈ 200 lines)
   proved by the exhaustive ctest against `std::to_chars` on the host.
6. **Scripts and pauses**: frame stamps stop while paused; the runner's flow control and `expect` windows rely on ticks.
   Mitigation: `LiveState` records carry the paused frame and "applied" events; the static paused-section rules forbid
   clock-moving commands; interactive commands are stamped for the next game frame.
7. **Name resolution per segment** (question 4) differs from the spec's "fails at the Hello answer" for names registered
   after the first segment; the first segment behaves exactly as the spec says.
8. **Held start on the console** depends on the netloader passing argv (D23 sends argv[0] today; the format allows more).
   If a launcher drops it, `--tunes` still works with values landing a few frames late (logged with the frame).
9. **Merging**: after 029's merge; insertions after 029's blocks; rebased before stage 8 and the merge.

## Decisions taken (to review)

The spec's open questions are decided as it recommends. Plan-level decisions:

1. **Names**: module `live` (`<nnh/live.hpp>`, `src/live/`), tag and log channel `live` (the channel defined in the module,
   not in `engine_channels.cpp`), example `examples/live` (`LIVE_DEMO`, `LIVE_SHOT`).
2. **Registration by pointer with RAII handles**; the default is the variable's value at registration; ranges required,
   checked by assert; names checked at compile time for literals.
3. **Capacities through `live::configure()`**, not an `AppConfig` field (question 1).
4. **Names copied** into 32-byte fields: 80 B per tunable, ≈ 20 KB at the defaults (question 2).
5. **Grid stepping** for floats and ints (no drift; off-grid values move to a neighbouring grid point).
6. **Shortest round-trip float text by Ryu's `f2s`**, shared by the game and `nnh` (question 3).
7. **Edits while paused stay pending** and apply at the next game frame; the game's own resets are immediate.
8. **Pause and step in core** (D52), the held frame replayed; the PC's keys become developer requests.
9. **Remote**: messages 9–11, records 8–12, live records only after a catalogue request; names resolved per segment
   (question 4); `expect-value` as above; `action NAME` for actions.
10. **Held start** for `--tunes`/`--hold` (question 5).
11. **Dump as TOML tables per group** (question 6).
12. **`live::pause()` and `live::show_panel()`** in the public API (debug pause from code; the example's screenshots).

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | One pause and one panel in core for every system; integer layout; one formatter and one stepping rule shared with `nnh`; only three request bits per platform |
| II | ✅ | ≈ 20 KB block only for games that register; no new image or network buffer; the debug pass's capacity holds a page |
| III | ✅ | Light header measured; Ryu is one C file; nothing on the "edit one game `.cpp`" path changes |
| IV | ✅ | Queries and an optional callback; no event |
| V | ✅ | No allocation after the block, no `virtual`, RTTI, exceptions or `printf` of values; no per-frame registry walk |
| VI | ✅ | Tests first for every rule; goldens; fuzzing; mutants |
| VII | ✅ | ≈ 22 new core sources and ≈ 9 `nnh` sources, each one concept ≤ 250 lines; `remote_wire.cpp` and `script_parser.cpp` kept under 400 by new files |
| VIII | ✅ | API page, guide, the affected pages and two examples |
| IX | ✅ | Ryu (BSL-1.0) vendored unmodified, pinned and credited; original shapes for art |
| X | ✅ | Validated and fuzzed messages; no file written by the game; the `--tunes` reader bounded and fuzzed |
| XI | ✅ | Release byte-identical `hello`; the module links only with `live::` calls; the Development cost of the engine tools is measured and stated |

No constitution exception is requested. D51 and D52 are new (status: proposed, for review); D28 gains three message types
and five record types (additive in version 1, sent only after a catalogue request); D38's input block gains the
`DevPanel` blocker; D17's overlay gains pages; the PC's pause moves from the platform library into core (D52). Flagged
for the maintainer: questions 1–7 below and the panel's frame cost at risk (risk 1).

## Questions for the maintainer (answered 2026-11-26: each as recommended)

1. **Capacities: `live::configure()` instead of an `AppConfig` field?** Recommended: yes. `AppConfig` is copied in
   `nnh::init()`, so a new field changes the Release `hello` bytes, which this spec must leave identical; `configure()`
   before the first registration does the same job, linked only by games that call it. Alternative: an `AppConfig::live`
   field, accepting a few bytes of change in every Release game.
2. **Entry size: names copied (80 B per tunable, ≈ 20 KB at the defaults) instead of the spec's ≤ 64 B (≈ 16 KB)?**
   Recommended: accept. Copies make run-time names (`goblin/speed` built from data) safe and keep one code path; pointing
   at literals would save ≈ 4 KB but needs a second storage for run-time names. Alternative: 64-byte entries with names
   cut to 23 bytes.
3. **Float text: vendor Ryu's `f2s` (BSL-1.0, unmodified, Development builds only)?** Recommended: yes — proven shortest
   round-trip printing in integer code, the same text on PC and 3DS, credited and pinned like `stb_vorbis`; we add an
   exhaustive check of all 2³² floats on VM 120. Alternative: our own ≈ 200-line formatter with the same exhaustive check
   (no third-party code, more to own).
4. **Script names checked when their segment is sent** (the Hello answer for the first segment, after each `expect` for
   later ones) instead of all at the Hello answer? Recommended: yes — with registration at run time (Q1 (b)) a scene's
   tunables don't exist at the Hello; the first segment keeps the spec's behaviour, and a missing name still fails with
   its line before anything of its segment is sent. Alternative: every name at the Hello (scripts must start where all
   tunables are registered).
5. **Held start** (`nnh run --tunes` and `--hold` start the game paused before frame 1 — 3DS argv `--nnh-hold`, PC
   `NNH_HOLD=1`, a timeout of 10 s) so reapplied values land before the first update (AC-8's frame-1 check)?
   Recommended: yes; it costs one request bit per platform. Alternative: values land when the PC attaches (a few frames
   late on the console; AC-8 would check frame N instead of 1).
6. **Dump format: TOML tables per group** (`[player]` then `jump = 5.25 # default 4.5`) instead of the spec's example
   `player/jump = 5.25`, which isn't valid TOML (a bare key can't contain `/`)? Recommended: tables — valid TOML,
   pasteable into a data asset section, and `--tunes` also accepts `"player/jump" = 5.25`. Alternative: quoted full names
   only.
7. **Development `hello` growth**: pause, engine controls, pages, the replay, audio hold and the remote decoders are in
   every Debug/Development game: estimated 3–6 KB of `.text` (Release unchanged). Accept (recommended), or ask for a
   smaller budget (then the replay — the largest part — becomes the fallback of risk 2).

## Implementation notes

Built in the worktree `spec-030` from `main` at `dfd2867` (spec 029 merged). Results per stage below; deviations from
the plan are listed under "Implementation refinements".

### Implementation refinements

- **R1 (T1.1) Name rules in the public header.** The constexpr rule checker (`detail::live_name::check`) lives in
  `<nnh/live.hpp>` (≈ 25 lines), because `Name`'s `consteval` constructor needs it and games can't see `src/`;
  `src/live/live_names.{hpp,cpp}` reuses it and adds the leaf/group split, group hashes and the run-time check. An empty
  name is `live_name_empty_part`.
- **R2 (T1.5) LiveEntry bounds.** The fixed part is 32 bytes (u32 frame, u32 hash, u8 kind, u8 flags, u8 name length,
  u8 enum count, 5 × u32), so a record is 33..575 bytes (not 28..540): names 1..31 bytes, enum names 1..15 bytes each
  (`remote::kMaxEnumNameBytes`, a limit the plan left open; `tune()` asserts it), at most 32 of them.
- **R3 (T1.5) Kinds.** One `live::Kind` enum covers tunables and watches (`Float 1, Int32 2, Bool 3, Color 4, Enum 5,
  Action 6, Int8 7, Int16 8, UInt8 9, UInt16 10, UInt32 11, Vec2 12, Text 13`): the `kind` byte of LiveEntry and
  LiveValue records.
- **R4 (T1.3) Ryu configuration.** `RYU_FLOAT_FULL_TABLE` (the 3 KB float table; the double tables aren't vendored) and
  `NDEBUG` are set on the object library's command line; the files are unmodified. `f2s_buffered_n()` prints `d.dddE±x`;
  `live_format.cpp` splits its digits and exponent and lays them out.
- **R5 (T1.4) The exhaustive ctest** `live_float_text_exhaustive` is registered with label `slow` and `DISABLED` unless
  `-DNNH_SLOW_TESTS=ON` (minutes long; the build VM runs it at T9.1).
- **R6 (T1.5) Older tests.** `remote_wire_rejects_unknown_types` and `remote_slider_message_refuses_bad_values` used
  type 9 as "the first unknown type"; they now use 12.
- **R7 (T1.5) `nnh` side.** `game_records` keeps a live record's payload (`Record::payload`) and the script runner routes
  live records to `on_live_record()` (`script_runner_live.cpp`, filled at T6.3).

### Stage 0 results

- T0.0: `remote_wire.hpp` on `main` at `dfd2867`: messages 1–8 (`Language = 8`), records 1–7. Spec 030 takes messages
  9–11 and records 8–12 as planned.
- T0.1 (baselines in `build/ref-030/t0/`, script `build/ref-030/hello_id.sh`): `hello`
  - 3ds-release `hello.elf`: text 202204, data 28360, bss 107368, file 2 509 112 B; SHA-256 `2423db7941cf922f…`
    (`hello.3dsx` `ec4a65bad1d226a4…`);
  - 3ds-dev: text 220920, data 32872, bss 109992;
  - pc-release `hello.exe`: text 134880, data 22052, bss 212400, file 285 885 B (the PE timestamp changes per link, so
    pc-release is compared by `size` and the `nm -C` symbol list, as the plan says);
  - pc-debug: text 209596, data 26820, bss 215152.
  - 3ds-release `platformer.elf` `.text` 452672.
  - Header cost (GCC 15, MinGW): a one-line TU with `<nnh/math.hpp>` preprocesses to 63 lines (math.hpp includes nothing);
    `<cstdint>` + `<cstddef>` alone add ≈ 550 lines, `<type_traits>` ≈ 3 200. See "Stage 2 results" for the header.
- T0.2/T0.3: the reference screenshots and the full per-object disassembly were not taken: every stage end compares
  the Release `hello` bytes (3ds-release SHA-256, pc-release size and symbols), which is the property they protect; the
  goldens are checked by the ctests of both PC presets at each stage end (368 pc-debug / 337 pc-release ctests at
  stage 1). `kMaxTests` has room (see stage 2).

### Stage 1 results

- Ryu vendored from <https://github.com/ulfjack/ryu> at commit `4c0618b0e44f7ef027ebae05d2cc7812048f7c8f`
  (2026-02-09), BSL-1.0 chosen (`LICENSE-Boost`); the files and their SHA-256 are in `third_party/ryu/README.md`
  (`ryu/f2s.c` `0a98f4fce6be4c59…`).
- New tests: `test_live_names` (14), `test_live_steps` (23), `test_live_format` (11, including 10⁶ seeded floats and
  every power of two read back through `std::from_chars`), `test_live_wire` (14), `test_fuzz_live_wire` (1); compile-fail
  ctests `live_name_{too_long,bad_character,empty_part,too_deep}_{fails,explains}` and `live_name_good_compiles`.
- Stage-end guards: ctest pc-debug 368/368, pc-release 337/337; 3ds-release and 3ds-dev build with 0 warnings; 3ds-release
  `hello.elf` SHA-256 identical, pc-release `hello` size and symbols identical. Debug/Development `hello` (the remote
  decoder now links `live_wire.cpp` and the name check): 3ds-dev text +488 B, pc-debug text +5 976 B.
- **R8 (T2.2) Integers by size and signedness.** `std::int32_t` is `long` on the 3DS and `int` on the PC, so
  `tune(name, int*)` would compile on one and not the other (constitution I). The int overload is a template accepting
  `int` and 32-bit `long`; watches and `show()` map any integer type (not `char`, not `bool`) to its kind by size and
  signedness (`detail::live_api::kind_of<T>()`). The header uses `__is_same`/`__is_enum` instead of `<type_traits>`
  (GCC, Clang and MSVC builtins), keeping it light.
- **R9 (T2.8) The Release `hello` check with debug info.** 3ds-release builds with `-g`: editing `app.cpp` (even only
  inside discarded `if constexpr` branches) moves its line table, so `hello.elf`'s SHA-256 changes by its DWARF alone.
  The guard therefore compares `hello.3dsx` (what ships) byte for byte and `hello.elf` with its debug info stripped
  (`arm-none-eabi-strip --strip-debug`) against T0.1's, built again from `dfd2867`'s `app.cpp` to confirm (both
  identical at stage 2); the ELF's section sizes and symbols are compared too. pc-release: section sizes and `nm -C`.
- **R10 (T2.3) Remote operations over panel edits.** At the application point the queue is resolved first, then the
  pending values are written, so a remote `set` overrides a panel edit made earlier in the same frame
  (`live_apply_latest_edit_wins`).
- **R11 (T2.5) The dump's first line** is `dump: begin frame <F> (<n> changed of <m>)`: the game's name is left out
  (core doesn't keep the title in Release-linked state, and `nnh` names the file); `m` counts tunables without actions.
- **R12 (T2.6) Plain loops** are tested in `test_live_cost.cpp` (`live_cost_a_plain_loop_game_uses_everything`) instead of
  a separate file.

### Stage 2 results

- The module (`src/live/`): `live_block.hpp`, `live_registry`, `live_tune.cpp`, `live_apply`, `live_watch`, `live_dump`,
  `live_ops`, `live_control.cpp` (pause/show_panel back ends, filled at stages 3–4), `debug/live_hook.{hpp,cpp}` (the
  ops pointer). `app.cpp` calls `frame_start` after `++frame_count` and `frame_end` after the 2D submit, both in
  Development branches.
- Entry sizes (pc-debug, 64-bit): `Entry` 104 B, `WatchEntry` 56 B, `ShownEntry` 76 B, `Follow` 12 B, `RemoteOp` 12 B;
  the 3DS's 32-bit pointers give `Entry` ≤ 84 B (checked by `live_registry_block_size_follows_the_capacities` when
  pointers are 4 bytes; measured on the console run). The block at the defaults: `block_bytes(128, 64, 64)`.
- Tests: `test_live_registry` (9), `test_live_tune` (10), `test_live_release` (1), `test_live_apply` (12),
  `test_live_watch` (7), `test_live_dump` (3), `test_live_cost` (3), `test_live_random_ops` (2, 100 000 seeded
  operations with guard bytes). `nnh_tests live_`: 132 pass in pc-debug and pc-release; the whole `nnh_tests`: 3917.
- Header: a one-line TU with `<nnh/live.hpp>` preprocesses to 1 020 lines against 611 for `<cstdint>` + `<cstddef>` +
  `<nnh/math.hpp>` (+409, over the plan's +250: the header's own API and docs; no `<type_traits>`), `-fsyntax-only`
  ≈ 0.39 s for both (no measurable difference, within the +0.03 s budget).
- Stage-end guards: ctest pc-debug 368/368, pc-release 337/337; 3ds-release and 3ds-dev 0 warnings (two Release-only
  unused warnings fixed after the run); `hello.3dsx` identical, `hello.elf` without debug info identical (R9);
  pc-release `hello` sections and symbols identical. Development `hello`: 3ds-dev text 221 456 (+536 B over T0.1).
- **R13 (T3.1) The pause and the overlay's toggle read the engine controls in core.** `overlay_update()` keeps the
  frame-time history only; SELECT + L, F1 and the pages' chords are taken by `overlay_pages_input()` from the dev
  controls of each tick (so they work on paused ticks). A new engine starts with the overlay hidden on the Stats page
  (`overlay_reset()`'s Development branch): an existing test (`visible_overlay_draws_fps_and_memory_before_the_frame_ends`)
  left it shown for whichever test ran next, which the new test order exposed.
- **R14 (T3.4) The held frame.** In Development `gfx_submit()` keeps the 2D, tile and particle lists (flag
  `g_gfx_held`, defined in the Development-only `gfx_replay.cpp`); `gfx_begin_recording()` clears them right after the next
  game frame's `begin_frame()`, before any game code. A frame that dropped draws or tiles is cleared at once as before
  (its assert must not fire again when `shutdown()` submits the open frame). The debug text the game recorded is counted
  in `debug.cpp`'s Development branch and replayed from `debug_list_last_frame()`. 3D draws are not replayed (the 3D
  module's records end with its own submit); a paused 3D game shows its 2D layers and the overlay.
- **R15 (T3.4, rule 2) Shared state** between Release-linked files and the new Development files goes through `extern`
  variables the Development files define (`g_gfx_held`, `g_debug_recorded`, `g_debug_game_mark`, `g_time_keep_delta`,
  `g_dev_scenes_changed`, `g_dev_frame_requests`), used only inside discarded branches in Release.
- **R16 (T3.1) Fake platform.** `platform_fake::queue_dev_requests({...})` returns one mask per `poll_dev_requests()` (so a
  test drives a pause and its ticks inside one `nnh::frame()`), and `quit_after_polls(n)`.
- **R17 (T4.2) Rows on demand.** The panel's rows are generated from the sorted index when asked (`row_count`, `row_at`),
  nothing stored; groups open by hash (16 at most).
- **R18 (stages 3–4) One commit** for stages 3 and 4: they share `overlay.cpp`, `dev_pause.cpp`, `live_ops.cpp` and the
  CMake lists, and one stage-end run covered both.

### Stages 3 and 4 results

- Pause and step in core (`app/dev_pause.*`, `app/pause_state.cpp`), engine controls (`debug/dev_controls.*`), the
  held frame (`gfx/gfx_replay.*`, `debug/debug_held.cpp`), audio hold (`audio/audio_hold.cpp`), the frame clock restart
  (`clock.cpp`'s Development branch); the simulator's Pause/F10 and `NNH_HOLD=1`, the 3DS's `--nnh-hold`, the fake's
  requests. The overlay's pages (`debug/overlay_pages.*`), the DevPanel blocker, the panel's model, touch, layout and
  drawing (`live/live_panel_*`).
- Tests: `test_dev_pause_state` (11), `test_dev_pause` (7), `test_dev_controls` (5), `test_gfx_replay` (4),
  `test_audio_hold` (2), `test_overlay_pages` (6), `test_live_panel_model` (10), `test_live_panel_touch` (5),
  `test_live_panel_draw` (3). The whole `nnh_tests`: 3970 pass.
- Stage-end guards (run `s34`): ctest pc-debug 368/368, pc-release 337/337; 3ds-release and 3ds-dev one warning each
  (a `-Wformat-truncation` in `live_panel_draw.cpp`, fixed after the run: 0); `hello.3dsx` identical, `hello.elf` without
  debug info identical, sections and symbols identical; pc-release `hello` sections and symbols identical.
  Development `hello`: 3ds-dev text 225 976 (+5 056 B over T0.1: pause, controls, pages, replay, audio hold, remote
  decoders — inside question 7's 3–6 KB), pc-debug text 223 988.
- Not done in this pass: T3.7's by-hand simulator check (Pause/F10 by keyboard) and T4.5's screenshot comparison
  script; the pages' look is checked by the example's screenshots (stage 7).

### Stage 5 results

- Session glue `remote/remote_live.cpp` (LiveSet/Live to the module or refused with reason 7 and an empty catalogue
  without it; Pause to core's pause; LiveState on each pause change, after a catalogue, and on each paused tick that
  applied messages), `remote_session.cpp` (dispatch, detach). The module's side `live/live_remote.cpp` (catalogue in
  name order in pieces from the registry, LiveEntry/LiveRemoved while the PC listens, Get/Follow answered by LiveValue at
  the frame end — at once on paused ticks, flagged paused —, refusals as LiveRefused).
- Fuzzing: `tests/fuzz/fuzz_live_remote.cpp` (the target: a registry of every kind with guard bytes, pauses bounded by
  the fake's `quit_after_polls`), the mini-fuzz `live_remote_target_survives_mutated_sessions` (1 500 mutants) and
  `libfuzzer_live_remote.cpp` for the build VM (T9.1).
- Tests: `test_remote_live` (7 + `remote_pause_step_and_resume_by_messages`); `nnh_tests`: 3979 pass.
- **R19 (T5.1) Stage-end run.** Stage 5 was committed with the whole `nnh_tests` green; its ctest/3DS/`hello` guards ran
  with stage 6's (see "Stage 6 results").
