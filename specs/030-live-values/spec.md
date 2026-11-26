# Spec 030: Live values

Status: implemented 2026-11-26; Linux checks passed; console correctness passed, console run 2 (bench budgets) pending; approved 2026-11-26 (the maintainer, every open question as recommended)
Author: NNHGE maintainers · Date: 2026-11-26
Depends on: [spec 002](../002-core-foundation/spec.md) (build flavors and `kDevToolsEnabled`, the debug overlay and its
SELECT + L / `F1` toggle, the simulator's `Pause` / `F10` / `F11` keys, logs, asserts, tagged memory),
[spec 005](../005-text-and-builtin-font/spec.md) (debug text in the built-in font, the same pixels everywhere),
[spec 013](../013-console-remote/spec.md) (the log connection to `nnh run`, frame-stamped messages, records in the log
stream, `.nnhs` scripts, exclusive script input), [spec 003](../003-full-input/spec.md) (buttons, touch, the engine's own
input blocker that never blocks engine controls). Works with [spec 014](../014-data-assets/spec.md) (data asset text
format; values loaded from data assets can be tuned) and [spec 001](../001-event-bus-and-scenes/spec.md)/D30 (scene
lifetime) without needing either. Does not need [spec 028](../028-ui-widgets/spec.md) (UI widgets) or
[spec 029](../029-game-fonts-and-localization/spec.md) (game fonts, merged 2026-11-26): the panel is
drawn in debug text (open question Q3). Implements the ROADMAP item "Live values in the simulator" (Features in the
spirit of Godot and Unity), the third of the last four features before NNHGE 1.0. A development tool: Debug and
Development builds only; optional for games (constitution XI).

## Problem and goal

Tuning a game is mostly changing numbers: jump height, gravity, an enemy's speed, a camera's dead zone, a particle
colour. In NNHGE today each change is an edit, a rebuild and a restart, and on the console a resend, and then the
developer plays back to the same spot to feel the difference. Watching a value means a `debug::printf` that is added,
read and removed again. Unity has the Inspector and Godot the remote scene tree for this: change a property while the
game runs, see it at once, and the change is gone when the game stops ([Godot debugging
tools](https://docs.godotengine.org/en/stable/tutorials/scripting/debug/overview_of_debugging_tools.html)). Console
teams have long done the same with "tweakables" on an in-game debug menu (Shawn Hargreaves' [MotoGP
tweakables](https://shawnhargreaves.com/blog/motogp-tweakables.html)). NNHGE has no editor (constitution XI), so after
this spec a Debug or Development game **declares tunables** (floats, ints, bools, colours) and **watches** in C++; a
**live panel** on the debug overlay lists them by group and lets the developer change tunables with buttons or touch
**while the game runs**, on the PC simulator **and on the 3DS**; the **`nnh` tool** reads and sets the same values over
spec 013's connection (typed commands and `.nnhs` scripts); the game can be **paused and stepped one frame** on both
systems; and the changed values can be **dumped as text** to copy into code or a data asset, or reapplied next run. A
Release build has none of it: not one byte of code, data or names, and every tunable is its declared default.

## User stories

- As a game developer, I want to mark my player's jump speed and gravity as tunables with a range, then nudge them
  with the D-Pad on the console while I jump around, so that I find the right feel in one session instead of twenty
  rebuilds.
- As a game developer, I want to watch the player's position, velocity and state, and the number of live enemies, in
  a tidy list on the overlay, so that I stop adding and removing `debug::printf` lines.
- As a game developer, I want to tweak a colour by its red, green, blue and alpha channels and see the sprite change
  the same frame, so that I can pick colours on the real screen.
- As a game developer, I want to pause the game on the console, step it one frame at a time and change a value while
  paused, so that I can study a collision or an animation frame by frame where it actually runs.
- As a game developer, I want to type `set player/jump 5.25` in `nnh`'s terminal, or put it in a `.nnhs` script with
  `expect-value player/state == 2`, so that tuning and tests use the same names and work on the PC and the console.
- As a game developer, I want to dump the values I changed as text lines when I'm happy, and to reapply that dump at
  the next start, so that a tuning session is never lost and I decide where the numbers finally live.
- As a game developer, I want my Release build to contain none of this, with every tunable at its declared default,
  so that players get exactly the game I tested without the tool.
- As a game developer who doesn't want any of it, I want to ignore it and pay nothing.

## Functional requirements

### Tunables

- **FR-1 Declaring a tunable.** A game declares a tunable with a **name**, a **kind**, a **default** and, for numbers,
  a **range** (min, max) and a **step**. Kinds: `float`, `int` (32-bit signed), `bool` and `nnh::Color` (RGBA, 8 bits
  per channel). Names are ASCII paths such as `player/jump`; the parts before the last `/` are **groups**. The
  declaration form (a macro over a value, or registering the game's own variable by pointer, or both) is open
  question Q1; either way the public API shape is part of this spec's product and the plan fixes it. A default outside
  its range, an empty range, a step ≤ 0, a name too long or not unique is a compile-time error where the compiler can
  see it, otherwise an assert in Debug/Development (never a silent clamp).
- **FR-2 Reading a tunable costs what reading a variable costs.** The game reads a tunable as a value of its kind,
  anywhere, every frame, with no lookup by name and no allocation. The game does not write tunables in normal code
  (changes come from the panel and from `nnh`, FR-7 and FR-11; `reset` is the exception, FR-5).
- **FR-3 Changes apply at one point in the frame.** A change made by the panel or by `nnh` takes effect at a fixed
  point of `nnh::frame()` (with spec 013's remote messages, before the frame's input and scene update), never in the
  middle of the game's update, so a value is constant for the whole of one game frame and a script's change lands on
  the frame it names.
- **FR-4 Knowing that a value changed.** For each tunable the game can ask whether it changed this frame (true on
  exactly the one frame where FR-3 applied a different value), so a game can rebuild what depends on it (a particle
  description, a cached table). An optional per-tunable callback, called at FR-3's point, is open question Q6. In
  Release this query is a constant `false`.
- **FR-5 Defaults and reset.** Each tunable remembers its default. The panel marks every value that differs from its
  default, and the panel and `nnh` can reset one value, one group, or everything to the defaults. The game may also
  call reset (for example on a scene's `on_exit`).
- **FR-6 Lifetime.** A tunable that belongs to something with a shorter life than the game (a scene, a loaded enemy
  type) is removed from the panel and from `nnh` when that thing ends, so the panel never reads or writes freed
  memory; using a tunable after its owner ended is caught by an assert, never a silent write. How this is expressed
  (RAII handle, D30 scene binding, or namespace-scope tunables only) follows from Q1.

### The live panel (PC and 3DS)

- **FR-7 Pages on the debug overlay.** The debug overlay (spec 002) gains pages: the existing **Stats** page, a
  **Tunables** page and a **Watches** page, switched by an engine control (open question Q4). The panel is drawn with
  the built-in font and overlay rectangles (spec 005's debug text), outside the game's `max_draws`, on the bottom
  screen by default, and looks **pixel for pixel the same** in the simulator and on the 3DS.
- **FR-8 Navigating and editing by buttons.** On the Tunables page the D-Pad moves between rows and opens or closes a
  group; left/right changes the selected value by one step (a bool toggles; a colour edits the selected channel, A
  cycles R → G → B → A), and holding L or R while pressing left/right changes it by ten steps. A held direction
  repeats after a delay. A value never leaves its range (it stops at min and max). Each row shows the name, the value
  (floats with enough digits to tell neighbouring steps apart), the range for numbers, a swatch for colours and the
  changed mark (FR-5). A page longer than the screen scrolls to keep the selected row visible.
- **FR-9 Editing by touch.** On the bottom screen a tap selects a row; dragging across a numeric row's bar sets the
  value at that point of its range, rounded to a whole number of steps from min; tapping a bool toggles it; buttons on
  the row step it down and up. The simulator's mouse is the touch screen, as everywhere.
- **FR-10 The game doesn't see panel input.** While the panel has the controls (open question Q4), the buttons,
  sticks and touch it uses are blocked for the game through the engine's input blocker (spec 003: queries read idle,
  held controls stay blocked until released), separate from the game's own blocker and from transitions'. Closing the
  overlay gives the controls back. While a `.nnhs` script runs, its input is exclusive (spec 013 amendment): the
  console's controls neither reach the game nor drive the panel; the script uses `set` instead (FR-12).

### `nnh` over the log connection (PC and 3DS)

- **FR-11 Typed commands.** In `nnh run <game> [--pc]` (spec 013's interactive mode) the developer can: list the
  tunables (`tunes [group]`: name, kind, value, default, range, step); read one (`get NAME`); set one (`set NAME
  VALUE`: a number, `true`/`false`, or `#RRGGBB[AA]`); reset (`reset NAME|GROUP|all`); list and read watches; follow a
  watch (`watch NAME [every N frames]`, `unwatch NAME`), whose values arrive as frame-stamped lines in the log; and
  `dump` (FR-14). An unknown name, a value of the wrong kind or out of range is refused **by `nnh`** with a message,
  before anything is sent; the game validates again what it receives (FR-18).
- **FR-12 Script commands.** `.nnhs` scripts (spec 013 FR-3) gain `set NAME VALUE`, `reset NAME|GROUP|all`,
  `expect-value NAME OP VALUE [within N frames]` (OP one of `==`, `!=`, `<`, `<=`, `>`, `>=`; floats compared exactly,
  or with an explicit tolerance `~ E`) and the pause commands of FR-16. Script errors (unknown command, bad value) are
  reported with their line before anything is sent, as today; a name the game doesn't declare fails the script at the
  `Hello` answer, naming the line.
- **FR-13 Exact values.** A value typed on the PC is parsed **once, on the PC**, and sent as its exact bits; the game
  never parses text for live values. A float shown by the panel or by `nnh` is printed so that it reads back to the
  same bits. The same script sets the same bits in the simulator and on the 3DS.

### Keeping what was tuned

- **FR-14 Dump.** `dump` (panel button or `nnh` command) writes every tunable that differs from its default, one per
  line, as `name = value` in spec 014's TOML subset (`player/jump = 5.25`, `fx/spark = "#FFC040FF"`), grouped by
  group, with the default in a trailing comment. In the log it appears as plain lines; `nnh` also saves it next to the
  log as `build/logs/<game>-<date>-<time>-tunes.toml`. The game writes no file for this.
- **FR-15 Reapplying a dump.** `nnh run <game> [--pc] --tunes <file>` sends the values of a dump file right after the
  game connects (before its first scene update when possible), so a session's values come back next run in the
  simulator and on the console. Unknown names in the file are reported by `nnh` and skipped; a malformed file is
  refused with `file:line: reason` before anything is sent. Whether the engine ever writes values back into source
  files (C++ or data assets) is open question Q5; this spec's recommendation is that it doesn't.

### Pause and step on both systems

- **FR-16 Pause, step, resume.** The simulator's existing `Pause` / `F10` keys keep working; the 3DS gains the same
  through the panel (an engine control, open question Q4) and both gain `nnh` commands `pause`, `step [N]` and
  `resume`, usable in scripts. While paused, `nnh::frame()` does not return to the game (scenes are not updated, plain
  `while (nnh::frame())` loops wait too), the game's last frame stays on screen, the overlay and panel stay live and
  can change tunables, and `step N` runs exactly N whole game frames and pauses again. The frame clock and
  `frame_count()` advance only for game frames run, and the first frame after a pause or step reports a normal
  `time::delta()`, not the length of the pause. Audio during a pause is open question Q7.

### Watches

- **FR-17 Watches.** A game declares watches: named, grouped values the panel and `nnh` show but never change. Kinds:
  `bool`, signed and unsigned integers up to 32 bits, `float`, `nnh::Vec2`, `nnh::Color` and short text. Two forms are
  proposed (open question Q2): a **registered watch** that names a value the engine reads only when it is shown or
  followed (same lifetime rules as FR-6), and a **per-frame watch** the game reports each frame with its current value
  (no lifetime to manage; like `debug::printf`, but listed in the panel and readable by `nnh`). A watched value is read
  at one point of the frame (after the scene update), so the panel, the log and `expect-value` see the same value for a
  given frame.

### Limits, safety, cost, opt-in

- **FR-18 Bounded and safe.** The registry has fixed capacities (tunables, watches, name length, group depth; proposed
  values in open question Q8), sized for the Old 3DS, reported when exceeded by an assert naming the limit. Every live
  value message the game receives from the PC is bounded and validated (kind, range, known name, size); a bad message
  is ignored, logged and counted like spec 013's rejected messages, never a crash; the reader is a fuzz target.
- **FR-19 No per-frame cost when unused, no per-frame heap ever.** With the overlay hidden, nobody connected and no
  value changing, live values cost the game no measurable frame time (no per-frame walk over the registry). Nothing
  allocates per frame; the registry is one block allocated once, under its own memory tag, shown on the Stats page.
- **FR-20 Release has none of it.** In a Release build: tunables are their declared defaults (FR-4's query is
  `false`), watch and registration calls compile to nothing, and the binary contains no live-values code, no
  registry, no names, no panel and no remote commands for them; `examples/hello`'s Release `.text` is unchanged.
  Debug and Development builds have all of it.
- **FR-21 Optional.** A game that declares no tunable or watch builds and runs as today (in Development the overlay
  shows empty pages; its size cost is bounded and documented). Live values need no data assets, no UI widgets, no
  entities and no scenes; a game written with a plain `while (nnh::frame())` loop can use all of it.
- **FR-22 Same results on PC and 3DS.** The same declarations, the same panel presses or the same script give the same
  values bit for bit, the same changed-frames, the same watch lines and the same panel pixels in the simulator and on
  the 3DS.

## Acceptance criteria

- **AC-1 Example** `examples/live` (name confirmed by the plan; original art): a small scene with a jumping character,
  a few particles and a coloured backdrop, its jump, gravity, run speed, a "show hitboxes" bool, a spawn count and two
  colours as tunables, and position, velocity, state and live-object count as watches. PC screenshots of the Stats,
  Tunables (one value changed, one colour row open) and Watches pages (`LIVE_SHOT=tunables|watches|colour|paused`,
  `NNH_EXIT_AFTER_FRAMES` + `NNH_SCREENSHOT`); on an Old 3DS XL the same pages look the same (screenshot pair through
  `nnh run --script`, compared with the PC's).
- **AC-2 One script, PC and console.** `examples/live/tests/check.nnhs` sets tunables, checks watches with
  `expect-value`, resets a group, dumps, pauses, steps 3 frames (the frame counter watch advances by exactly 3) and
  resumes; it passes with `--pc` and on the Old 3DS XL, and the example's self-check line (a checksum of the
  simulation after the scripted changes) is identical on both.
- **AC-3 Panel by buttons and touch.** A script drives the panel with buttons (select, step, ×10 step, stop at max,
  colour channel) and with touch (tap a row, drag a bar, tap a bool), and `expect-value` checks every resulting value
  — on PC and console. A test shows the game's input queries read idle while the panel has the controls and that a
  button held when the panel closes stays blocked until released.
- **AC-4 Unit tests** (written first, constitution VI) on the fake platform: declaration checks (range, step, default,
  duplicate and too-long names), stepping and clamping for every kind (including floats at the ends of their range and
  ten-step jumps), drag-to-value rounding, changed-this-frame exactly once, reset of one/group/all, lifetime (a tunable
  removed with its owner disappears from listings; use after removal asserts), FR-3's application point, the watch
  forms, float text round trip (FR-13), the dump writer and the dump reader (`nnh` side, with `file:line` errors), the
  new script commands in the `.nnhs` parser, pause/step frame accounting and `time::delta()` after resume.
- **AC-5 Bounded and fuzzed.** The game-side reader of live value messages is in the mini-fuzz of `nnh_tests` and a
  libFuzzer target, run under ASan/UBSan on the Linux build VM without findings; tests feed each capacity plus one.
- **AC-6 Release is clean.** `scripts/check_release_strings.sh` finds no tunable or watch name and no live-values
  string in the Release `examples/live`; a symbol check finds no live-values code; `examples/hello`'s Release `.text`
  is unchanged on PC and 3DS; the Release `examples/live` self-check prints the defaults' checksum.
- **AC-7 Cost.** On the Old 3DS XL, measured and written into the budgets: frame time with the overlay hidden and 128
  tunables + 64 watches declared (no change from a build without them, within noise), and the panel's cost when shown
  (a target proposed by the plan, e.g. < 1 ms per frame); the registry's memory per entry is documented.
- **AC-8 Keep a session.** In a scripted run, values changed by `set` are dumped, the dump file is saved by `nnh`, and a
  second run with `--tunes <file>` starts with those values (`expect-value` on frame 1), on PC and console.
- **AC-9 Docs.** A `docs/api/` page for the module, `docs/api/debug.md` (overlay pages), `docs/api/simulator.md` and the
  debugging architecture page (pause/step on the console), the nnh tool guide (commands, script commands, `--tunes`),
  a guide section "tuning your game", a decision row in `docs/architecture/decisions.md`, CLAUDE.md and ROADMAP
  updated, `docs/REFERENCES.md` citing the prior art below.

## Non-goals

- An editor or an inspector of arbitrary game objects: only values the game declares are shown (no reflection, no
  RTTI, no browsing of entities or components). Browsing spec 020's worlds is a possible follow-up.
- A separate PC window, a GUI in `nnh`, or a web page (open question Q3 explains why the overlay is the one surface).
- Writing values into the game's C++ sources or data asset text files automatically (open question Q5).
- Tuning strings, enums, `Vec2`, arrays or whole structs (open question Q9 for the next candidates), and free-form
  expressions in watches.
- Graphs of watched values over time (the ROADMAP's later "Profiler overlay").
- Hot reload of assets or game code: separate ROADMAP items; FR-15 and spec 014's "load again into the same struct"
  leave room for them.
- Live values in Release builds, over the internet, or from a PC other than the one `nnh run` connected to (spec 013
  FR-7 applies unchanged); NNH Link-started games (spec 011) like spec 013.
- Rewinding, recording or replaying a session.

## Constraints

- **I — Same game code everywhere.** The same declarations on PC and 3DS; no platform `#ifdef` in game code or public
  headers. Compiling out in Release follows the build flavor (`kDevToolsEnabled`), as the overlay and debug text
  already do, not the platform.
- **II — The 3DS is the constraint.** Fixed capacities and one registry block sized for the Old 3DS; the panel drawn
  with debug text inside the overlay's existing capacity; no new screenshot or network buffers beyond spec 013's.
- **III — Build speed.** The public header stays light (only `<nnh/math.hpp>`-level types); declaring a tunable must not
  pull heavy templates into every game file. Measured before and after on `examples/live` and the platformer.
- **IV — Event-driven.** No bus events are required: changes are queried (FR-4) and applied at one point of the frame
  (FR-3). Whether a `TunableChanged` event is worth adding is part of open question Q6.
- **V — No hidden costs.** No exceptions, no RTTI (kinds by templates), no `virtual`, no per-frame heap; FR-19's
  no-cost-when-unused is measured.
- **VI — Spec first, test first.** Registry, stepping, panel logic, wire messages, dump reader/writer and script
  commands are unit-tested first on the fake platform; the panel's look is checked by screenshots on PC and console.
- **VII — Small files**, one public header for the module.
- **VIII — Documented API** (AC-9).
- **X — Safe by default.** The game reads untrusted bytes from the PC: bounded, validated, fuzzed (FR-18); the game
  writes no file for live values (FR-14, FR-15; dumps are saved by `nnh` on the PC); Debug/Development only.
- **XI — Opt-in, never in the way; no editor.** The panel is a debug view of values the game names in code, not an
  editor; nothing is required to use it (FR-21).
- **Spec 013 compatibility.** New messages and records extend D28's wire format without changing existing ones;
  scripts written for spec 013 run unchanged.

## Prior art

- Godot's remote scene tree: change node properties in the running game from the editor's Inspector; changes are lost
  when the game stops ([debugging tools](https://docs.godotengine.org/en/stable/tutorials/scripting/debug/overview_of_debugging_tools.html)).
  NNHGE keeps the "change while running, nothing saved unless you ask" idea without an editor.
- Unity's Inspector in Play Mode: same idea, same "changes are reverted when Play Mode ends".
- Console-era tweakables: Shawn Hargreaves, [MotoGP tweakables](https://shawnhargreaves.com/blog/motogp-tweakables.html)
  (named `TweakVariable`s with paths like `sun/show`, edited on an in-game menu; thousands by ship time); Wikipedia,
  [Debug menu](https://en.wikipedia.org/wiki/Debug_menu).
- Tweakable constants re-read from source files: GameDev.net,
  [Tweakable Constants](https://gamedev.net/tutorials/programming/general-and-gameplay-programming/tweakable-constants-r2731/);
  cbloom, [Tweakable Vars](http://cbloomrants.blogspot.com/2011/02/02-23-11-some-little-coder-things.html); Magnum's
  [`Corrade::Utility::Tweakable`](https://doc.magnum.graphics/corrade/classCorrade_1_1Utility_1_1Tweakable.html). These
  need the source tree at run time, which a console doesn't have (why Q5 recommends a dump instead).
- Runtime tweak registries in C++: [tunable-cpp](https://github.com/miloszmaki/tunable-cpp) (a command line over
  captured variables); GameDev.net, [Configuration and Tweaking](https://gamedev.net/tutorials/programming/general-and-gameplay-programming/configuration-and-tweaking-r3154)
  (AntTweakBar panels bound to game variables).

## Open questions

Answered 2026-11-26: the maintainer approved every recommendation below (Q1 (b) by pointer, Q2 both, Q3 overlay +
`nnh`, Q4 as proposed, Q5 (a) dump and reapply only, Q6 `changed()` + optional callback, Q7 audio holds while paused,
Q8 the capacities, Q9 enums and actions added, Q10 both examples, Q11 `<nnh/live.hpp>`).

- [x] **Q1 How does a game declare a tunable?** Options: (a) a namespace-scope macro
      `NNH_TUNABLE(float, jump_speed, "player/jump", 4.5f, .min = 0, .max = 10, .step = 0.25f)` that, in
      Debug/Development, defines a small object registered at static initialisation into a fixed table, and in Release
      a plain `const` variable; (b) registering a value the game owns by pointer at run time,
      `auto t = nnh::live::tune("player/jump", &cfg.jump, {.min = 0, .max = 10, .step = 0.25f});`, returning an RAII
      handle (or bound to a scene by D30), with every call an empty inline function in Release; (c) both. —
      **Recommendation: (b) first, (a) only if the plan shows it stays allocation-free and initialisation-order safe.**
      (b) needs no static initialisers (the project avoided a static registry for saves, D45), works for values loaded
      from data assets (`tune("goblin/speed", &enemy.speed, …)`) and for scene members, and makes lifetime explicit
      (FR-6). (a) is the shortest to write for true constants, but it needs a registry filled before `main`, and its
      Release form (`const`, not `constexpr`) differs from its Debug form in subtle ways. The constitution pressure is
      noted below.
- [x] **Q2 Watches: registered, per-frame, or both?** — **Recommendation: both.** A per-frame watch
      (`nnh::live::show("enemies/alive", count)`, collected in a fixed buffer like debug text) has no lifetime to
      manage and suits computed values; a registered watch (`watch("player/pos", &player.pos)`, RAII like Q1) costs
      nothing on frames it isn't shown and suits values the game keeps anyway. Only the per-frame form, if the maintainer
      prefers one.
- [x] **Q3 Where does the developer see and change values: overlay panel, separate PC window, or the `nnh` CLI?** —
      **Recommendation: the overlay panel (PC and 3DS) plus `nnh` commands; no separate window.** The panel is the only
      surface that works on the console in hand and looks the same in the simulator; `nnh` gives exact typing, scripts
      and dumps on both; a separate SDL window would be simulator-only (it can't help the console), adds a second
      renderer path and a GUI to maintain. Drawing the panel with spec 028's widgets was considered and not
      recommended: it would make a development tool depend on an optional feature (XI), spend the game's `max_draws`
      and change look when a game restyles its UI; debug text and overlay rectangles have their own capacity.
- [x] **Q4 Engine controls on the console.** The overlay is SELECT + L. Proposed: with the overlay shown, SELECT + R
      cycles its pages (Stats → Tunables → Watches); on the Tunables and Watches pages the panel **takes the controls**
      (FR-10) until SELECT + L closes the overlay or the Stats page is shown again; on the Tunables page SELECT + Y
      pauses/resumes and SELECT + X steps one frame. In the simulator the same chords plus the existing `Pause`/`F10`,
      and `F1` for the overlay. — **Recommendation: as proposed.** Alternative: the panel never takes the controls and
      is edited only by touch, which leaves the game playable while tuning but makes the D-Pad flow of the roadmap
      ("sliders driven by buttons") impossible; a "hold SELECT to edit" mode is another option the maintainer may prefer.
- [x] **Q5 Writing values back to data assets or source files?** Options: (a) never: dump text (FR-14) and reapply it
      (FR-15), the developer copies lines where they belong; (b) `nnh` (on the PC, never the game) patches spec 014
      data asset text files in the project when a tunable is bound to an asset field, keeping comments and layout;
      (c) also patching C++ default values. — **Recommendation: (a) in this spec.** The game must not write outside its
      folder (X) and the console has no source tree; (b) needs a binding from tunables to asset fields and a
      comment-preserving TOML writer, and fits better with the next roadmap item (hot reload of assets), where editing a
      data file reloads it in the running game; (c) is fragile (cbloom-style source parsing). The dump's `name = value`
      lines are already in spec 014's format, so a tunable named like a data field can be pasted into a data file.
- [x] **Q6 How does the game react to a change?** — **Recommendation: the `changed()` query (FR-4) plus an optional
      callback at registration, no bus event.** A query is the project's rule for state; a callback at FR-3's point
      suits "rebuild the particle description"; a `TunableChanged` event would put dev-only facts on the gameplay bus,
      and in Release it would never fire, inviting Debug-only behaviour.
- [x] **Q7 Audio while paused.** — **Recommendation: voices and music hold while paused and continue on resume; a step
      plays one frame of audio.** Alternative: audio keeps playing (today's simulator behaviour must be checked by the
      plan), which is harmless but makes paused screenshots and steps noisier.
- [x] **Q8 Capacities and names.** — **Recommendation:** 128 tunables, 64 registered watches, 64 per-frame watches per
      frame, names ≤ 31 bytes (ASCII letters, digits, `_`, `-`, `/`), groups up to 3 levels, fixed in Debug/Development
      (an `AppConfig` field to raise them if a game needs more); Release 0. The plan confirms the memory per entry
      (target ≤ 64 bytes, so ≈ 16 KB in all) against the Old 3DS Development budget.
- [x] **Q9 More kinds now?** Candidates: enums shown by name (a list of names at declaration), `Vec2`, `uint8_t`
      ranges, and "actions" (a named button that calls a game function: "respawn", "give 99 coins", as debug menus
      often have). — **Recommendation: add enums and actions** (cheap, high value, same panel rows), keep `Vec2` and
      strings out (two floats or a watch cover them).
- [x] **Q10 Example: new `examples/live`, the platformer, or both?** — **Recommendation: both.** `examples/live` shows
      every kind, the pages, pause/step and the self-check; the platformer turns its movement constants into tunables
      with the same defaults, so its replay lines stay unchanged (proof that defaults are exact) and the real use —
      tuning a jump — is shown where it matters.
- [x] **Q11 Module name.** `<nnh/live.hpp>`, namespace `nnh::live` — **Recommendation: yes** (short, matches the
      ROADMAP item); `<nnh/tune.hpp>` is the alternative.

## Constitution pressure

- **V and D45's "no static registry":** Q1 (a) would need registration before `main`; the recommendation avoids it.
- **XI "there is no editor":** a panel that edits values comes close; it stays a debug view of values the game names in
  C++, with nothing saved by the engine (Q5).
- **I "same game code everywhere" vs Release:** tunables are mutable in Debug/Development and fixed in Release; game
  logic must not depend on a tunable changing (the docs will say so, and FR-4 is `false` in Release).
