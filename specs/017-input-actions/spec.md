# Spec 017: Input actions

Status: implemented (console check pending) — approved 2026-11-22 (maintainer pre-approved recommendations: every
open question below is answered with the recommended answer); implemented 2026-11-22 except the console run (T7.2)
Author: NNHGE maintainers · Date: 2026-11-22
Depends on: [spec 001](../001-event-bus-and-scenes/spec.md) (event bus, scenes; scene attachments of
[D30](../../docs/architecture/decisions.md)), [spec 002](../002-core-foundation/spec.md) (memory tags, asserts,
logs, `Result`, `nnh::save`, profiling), [spec 003](../003-full-input/spec.md) (buttons, sticks, touch, the input
sample and its events). Works with [spec 013](../013-console-remote/spec.md)'s remote input and scripts without
change (remote input is merged into the sample before anything reads it, D28). Implements the ROADMAP item "Input
actions" of "Features in the spirit of Godot and Unity". Optional for games (constitution XI, decision D25).

## Problem and goal

Today a game asks for physical controls: the platformer jumps on `pressed(Button::A) || pressed(Button::B)` and
runs on `held(Button::Left)`. The meaning ("jump", "run") is scattered as button names through the code, so
offering a second control for the same thing means another `||` everywhere, letting the player choose their own
buttons is a project of its own (capture the next press, resolve conflicts, save, load, reset), "move" from the
Circle Pad *or* the D-Pad needs hand-written merging, and a menu that closes on B leaks that B into the gameplay
under it. Godot (InputMap, `Input.is_action_pressed`) and Unity (Input System: actions, bindings, action maps,
interactive rebinding) solve this with named actions. After this spec a game declares its actions once in C++
(`jump` is a button, `move` is a 2D vector, `aim` an axis), gives each a few default bindings (buttons, D-Pad, stick
directions or whole sticks, bottom-screen touch areas), groups them into contexts (menu, gameplay) that follow its
scenes, and reads `pressed(Jump)` or `vector(Move)` — or reacts to `ActionPressed` events. A settings screen lets
the player rebind any action with a few calls (listen for the next press, resolve conflicts, reset to defaults), and
the result is saved with `nnh::save` and loaded next time. Everything is evaluated from the same input sample the
game already sees — real, simulated, remote or scripted — so it is the same frame by frame on PC and 3DS and replays
stay exact. A game that reads buttons directly ignores all of it and pays nothing.

## User stories

- As a game developer, I want to declare named actions (`jump`, `attack`, `pause`, `move`, `aim`) once, as buttons,
  axes or 2D vectors, so that game code says what the player means instead of which button was pressed.
- As a game developer, I want each action to have several bindings — two buttons, the D-Pad *and* the Circle Pad, a
  stick direction, a touch area on the bottom screen — so that players can use whichever control they like without
  `||` chains in my code.
- As a game developer, I want `pressed`/`held`/`released`, how long it has been held, an analog value and a vector,
  plus events on press and release, so that I can poll or react as I already do with buttons.
- As a game developer, I want menu and gameplay actions in separate contexts that switch with my scenes, so that the
  B that closes a pause menu never makes the character attack, and pressing A on "Start game" doesn't make it jump.
- As a game developer, I want an API for a "Controls" screen — listen for the next press, show the control's name,
  handle a control that is already used, reset one action or everything to defaults — so that remapping is a small
  menu, not a subsystem.
- As a player, I want my button choices saved on the SD card and restored the next time I play, and a broken or old
  save file never to break the game.
- As a game developer, I want actions bound to New 3DS controls (ZL, ZR, C-Stick) to fall back to other controls on
  an Old 3DS, so that every player can play.
- As a game developer testing on PC, I want the simulator's keyboard and gamepad to drive actions exactly as the
  console's buttons do, and spec 013 scripts to drive actions — including the remap menu — on the console.
- As a game developer who prefers my own code, I want to keep reading buttons directly and lose nothing.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### Declaring actions

- **FR-1 Action sets own actions.** Actions live in an **action set** that the game creates and owns (normally one
  for the whole game, so that remaps apply everywhere; several are allowed). A set has **fixed capacities** given
  when it is created (actions, contexts); its storage is reserved once (memory tag `input`, or a tag the plan names)
  and released when it is destroyed. Declaring actions, binding, evaluating, listening and remapping allocate
  nothing afterwards. Exceeding a capacity asserts in Debug/Development and is ignored with an error log in Release.
- **FR-2 Action identity.** The game identifies an action by a value of **its own enum** (a small integer, dense, used
  for fast queries) and gives it a **name** (`"jump"`: 1–31 characters of `[a-z0-9_]`) used in logs, in saved
  bindings (FR-19) and by the debug overlay. Ids and names are unique per set; a duplicate id, a duplicate name or a
  32-bit name-hash collision asserts at declaration.
- **FR-3 Action kinds.** An action is one of three kinds, chosen at declaration:
  - **button** — digital: held or not, with `pressed`/`released` edges; its `value` is 0–1 (1 for digital sources,
    the analog strength for stick sources, FR-6);
  - **axis** — one `float` in [-1, 1];
  - **vector** — a 2D value (x right, y up, like the sticks) whose length never exceeds 1.
  Every kind also answers `held` (button held, or axis/vector magnitude past the action's press threshold, default
  0.5, released below 0.4) so that "is the player moving?" needs no extra action.
- **FR-4 Contexts.** Each action belongs to one **context** (a game-chosen small id with a name, e.g. `menu`,
  `gameplay`; a set has at most 8, default one context named `default`). Contexts are **enabled** or disabled by the
  game; an action of a disabled context reads as idle (not held, no edges, value 0, no events). Contexts are
  independent: two enabled contexts may use the same control (menu confirm and jump both on A) and both see it.
  There is no priority or "consume" between contexts; scenes (FR-5) and FR-13 cover the cases that need it.
- **FR-5 Contexts follow scenes (optional).** A context may be **bound to a scene**: it is enabled exactly while that
  scene is the top (updating) scene of the stack, so a pushed pause menu disables the level's `gameplay` context and
  enables its own `menu` context, and popping it restores them, with no game code. Binding uses the scene
  attachments of D30 (no allocation, nothing new in `Scene`); the set must outlive the scene or be unbound first
  (asserts). Unbound contexts are switched by hand.

### Bindings

- **FR-6 What an action can be bound to.** Each action has up to **4 binding slots** (the plan may lower this to 3
  if memory demands; documented). A slot holds one **source**:
  - a **button** (`Button::A` … `Button::Right`, ZL/ZR included); the D-Pad buttons count as buttons;
  - a **stick direction** (Circle Pad or C-Stick, up/down/left/right): for button actions, held when pushed past a
    threshold (default 0.5 press / 0.4 release, the D-Pad mirror's hysteresis; per binding adjustable), value =
    the push along that direction; for axis actions, the push along that direction as a positive value;
  - a **whole stick** (vector actions) or **one stick axis** (axis actions), with an optional extra dead zone
    (default 0; the engine's 10% dead zone already applies) re-scaled so the value still starts at 0 and reaches 1;
  - a **button composite**: two buttons for an axis (negative, positive) or four for a vector (up, down, left,
    right); opposite buttons cancel out; a diagonal has length 1, as the simulator's stick keys do;
  - a **touch area** (button actions only): a rectangle of the bottom screen, held while the touch point is inside it
    (sliding in presses, sliding out releases), for on-screen buttons.
  Each slot may also be **inverted** (axis/vector: negated, or only y for vectors — the usual "invert Y" option).
- **FR-7 Combining bindings.** A button action is held while **any** of its slots is held; its value is the largest.
  An axis takes the slot with the **largest absolute** value; a vector the slot with the **longest** vector (the
  simulator's "pushed further wins" rule). Ties take the lowest slot. Two slots of one action on the same control are
  allowed and harmless.
- **FR-8 Default and locked bindings.** The bindings declared in code are the action's **defaults** (FR-18 resets to
  them). A slot may be declared **locked**: it is never changed by remapping, reset or loading (the `Start` that
  always pauses, a touch area that is part of the HUD). Touch areas are always locked (they belong to the layout, not
  to the player).
- **FR-9 Systems without a control.** A slot may be declared **fallback for a capability** (e.g. active only when the
  system lacks `Capability::CStick`, or `Capability::ZButtons`): it is used only on systems without it, and ignored elsewhere.
  Slots whose source the system cannot produce (ZL/ZR, C-Stick on an Old 3DS) are inactive. In Debug/Development, an
  action left with no active slot in an enabled context logs one warning when the context is enabled; a query
  `usable(action)` says whether any slot is active. The PC simulator has every control, so the game checks the Old
  3DS path with the fake platform or a real console.
- **FR-10 Same controls the game already sees.** Actions read the **same per-frame input sample** that
  `<nnh/input.hpp>` queries return — after spec 013's remote input is merged, after the simulator turns keyboard,
  mouse and gamepad into 3DS controls, and with the D-Pad mirror applied when the game turned it on. So the PC
  simulator's mapping (docs/api/input.md) drives actions unchanged, scripts and remote input drive them, and actions
  are a pure function of the input samples and the set's state.

### Reading actions

- **FR-11 Queries.** For an action in this frame: `pressed`, `held`, `released` (one-frame edges, as for buttons),
  `value` (button, axis), `vector` (vector), `held_frames` (frames held so far, 0 when not held) and
  `frames_since_pressed` (saturating; a jump buffer or coyote time without a counter). All queries in one frame agree
  with each other and with the events. A button action already held when its set is created or its context enabled
  does **not** report `pressed` until its sources are released and pressed again (FR-13).
- **FR-12 Repeat for menus (optional per action).** A button action may have a **repeat** (initial delay and interval
  in frames, e.g. 24 and 6): `repeated(action)` is true on the press frame and on each repeat frame while held, so a
  menu cursor scrolls when the D-Pad is held. Frame-counted, deterministic.
- **FR-13 No leaking presses across switches.** When a context becomes enabled (by hand or by FR-5), every action in
  it whose source is held at that moment is **blocked** until that source is released: no `pressed`, no `held`, no
  event — the A that confirmed "Start game" doesn't make the player jump on the next frame. When a context is
  disabled, its held actions simply read idle (no `released` edge, no event); code that needs cleanup does it where
  it disabled the context or in `on_exit`.
- **FR-14 Events.** A set publishes `ActionPressed` and `ActionReleased` (payload: the set and the action id; small,
  trivially copyable) for button actions, and for axis/vector actions when FR-3's `held` changes. They are published
  in the **same frame** as the input that caused them, after that frame's `ButtonPressed`/`ButtonReleased`/touch
  events, all presses before all releases, each in declaration order; inside a handler the queries agree with the
  event. A set can turn its events off (default on). There is no event for value or vector changes (query them).
- **FR-15 When actions are evaluated.** Every live set is evaluated automatically **once per `nnh::frame()`, right
  after the input sample is read** and before events are dispatched and the scene updates, so queries in `update()`
  and events see this frame's input. Scene-bound context changes (FR-5) take effect from the next frame's
  evaluation, consistent with scene changes being applied at the end of the frame. The input module does not depend
  on actions (the plan proposes the allocation-free hook, as D30 did for scenes).

### Remapping

- **FR-16 Listening for the next control.** The game asks a set to **listen** for a new source for one slot of one
  action (`listen(Jump, slot 0)`); for a composite, one part at a time (`listen(Move, slot 1, part Up)`). While
  listening:
  - controls held when listening starts are ignored until released (the A that chose "Jump" in the menu is not
    captured);
  - the first control that goes down is captured: a button; or a stick pushed past 0.5 — as a direction for button
    and axis actions, as the whole stick for vector actions;
  - the game may pass a **cancel control** (default `Start`) that cancels instead of binding, and a mask of
    **excluded controls** that are ignored (e.g. `Select`); touch never binds (FR-8);
  - controls the system lacks can't be captured (they can't be pressed);
  - the listening action and every action of the set read as idle and publish no events, so the press that binds
    doesn't also act;
  - an optional timeout in frames cancels it.
  A query reports the state — listening, bound (with the captured source), cancelled, timed out — and the result
  stays readable until the next `listen` or until the game clears it. The capture is decided on frame edges from the
  input sample, so the same input gives the same result on PC, 3DS and in scripts.
- **FR-17 Conflicts.** A captured source that is already used by another remappable slot **in the same context**
  is a conflict, resolved by the policy the game passes to `listen`:
  - **swap** (default) — the other slot gets this slot's previous source (Jump A ↔ Attack B);
  - **replace** — the other slot is cleared;
  - **allow** — both keep it;
  - **reject** — nothing changes and the result says which action holds it, so the game can ask the player.
  Locked slots are never changed: a source used by a locked slot of the same context is rejected under every policy
  except **allow**. A query lists the conflicts of an action (which action and slot share a source) for display.
  Actions in different contexts never conflict (FR-4).
- **FR-18 Editing and reset.** Besides listening, the game can set or clear a remappable slot directly, set its
  invert flag (FR-6), and **reset** one slot, one action, one context or the whole set to the defaults (FR-8). Each
  change applies from the next evaluation and never touches locked slots.
- **FR-19 Showing controls.** For any source, the engine returns a short English display name in the built-in font's
  character set (`"A"`, `"ZL"`, `"D-Pad Up"`, `"Circle Pad Left"`, `"C-Stick"`, `"Touch"`), and for an action the
  sources of its active slots, so a controls screen lists "Jump: A / B". Translations and button glyphs are the
  game's.

### Saving and loading

- **FR-20 Saving bindings.** A set can write its player bindings with `nnh::save` to a path the game chooses
  (atomic, with backup and checksum, constitution X), or into a caller buffer for games that keep bindings inside
  their own save. The data is compact, has its own **format version**, and stores only what differs from the
  defaults (changed remappable slots and invert flags), keyed by **action name hash, slot and part** — so reordering
  the game's enum, adding actions or changing defaults in a new release keeps a player's own choices and gives them the
  new defaults for the rest. The game may also pass its own **bindings version**; a saved file with another version is
  ignored (the game's way to say "my controls changed too much, reset everyone").
- **FR-21 Loading bindings.** Loading reads that data (from a path or a buffer) and applies it after validation:
  entries for unknown actions, slots or parts, sources of an unknown kind or invalid for the action's kind, and
  entries for locked slots are skipped and counted; a damaged or too-new file changes nothing; a missing file is a
  normal "no remaps". The call returns a `Result` with what was applied and skipped, logs a warning for skipped
  entries, and never asserts on file content. Loaded sources the system lacks stay saved but inactive (FR-9), so an
  SD card moved between an Old and a New 3DS keeps the player's ZL choice. The loader is bounded and fuzz-tested.

### Cost, determinism, opt-in

- **FR-22 Deterministic.** Given the same declarations, the same calls and the same sequence of input samples, a set
  produces the same queries, the same events in the same order and the same listen results, frame for frame, on PC and
  3DS. Actions need no recording of their own: replaying the input samples replays them. Thresholds compare the
  sample's own values with documented, fixed rules (no wall time, no per-platform maths).
- **FR-23 Bounded cost.** Evaluating a set costs time proportional to its enabled actions and their slots, with no
  allocation, no `virtual` per action and no logging in the normal path; each evaluation is a profile scope. Limits
  (actions per set ≤ 64, slots ≤ 4, contexts ≤ 8) are documented and checked (FR-1).
- **FR-24 Debug view.** In Debug/Development, the debug overlay can show a set's enabled contexts and the actions
  held this frame (by name), so "why doesn't jump work?" is visible on the console. Not in Release.
- **FR-25 Optional and independent.** Actions are their own optional module. A game that doesn't create a set links
  no action code and pays nothing per frame (checked on executable size); `<nnh/input.hpp>` and its behaviour are
  unchanged and remain a fully supported way to read controls. The module needs no other optional feature (data
  assets, tweens, UI) and none needs it.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: declarations (kinds, ids, names, duplicates and
  hash collisions assert, capacities); every source kind of FR-6 with its thresholds, hysteresis, dead zone re-scale,
  composites (opposites cancel, diagonal length 1), touch areas (slide in/out) and invert; combining slots (FR-7,
  ties); queries and edges (FR-11, `held_frames`, `frames_since_pressed`, held-at-creation not pressed); repeat
  (FR-12); contexts enable/disable and the block-until-released rule (FR-13); events, their order relative to
  `ButtonPressed`/`TouchStarted` and agreement with queries inside handlers (FR-14); evaluation point (FR-15).
- **AC-2 Same sample, every input path.** Tests feed the fake platform's scripted input and spec 013's remote-input
  merge (remote presses, stick values, touch) and show identical action results for the same merged sample; the D-Pad
  mirror on/off case (FR-10); determinism: the same input sequence run twice gives the same per-frame log of queries,
  events and listen results (FR-22).
- **AC-3 Scenes.** With the scene stack on the fake platform: a scene-bound context is enabled only while its scene is
  on top; pushing a menu scene switches `gameplay` off and `menu` on, popping restores them; the confirm press that
  popped the menu does not reach gameplay (FR-13); destroying a set still bound to a live scene asserts (FR-5).
- **AC-4 Remapping.** Tests for listen: held-at-start ignored, button capture, stick capture as direction or whole
  stick, composite parts, cancel control, excluded controls, timeout, missing controls (ZL on a fake Old 3DS), all
  actions idle while listening; conflicts under swap, replace, allow and reject, and locked slots; set/clear/invert;
  reset of slot, action, context and set (FR-16–FR-18); display names for every source (FR-19).
- **AC-5 Saving.** Round trip through `nnh::save` and through a buffer; only changed slots stored; enum reordered and
  actions added between save and load keep the player's choices; different bindings version ignored; unknown/invalid
  entries skipped and counted; damaged file and missing file; a ZL binding loaded on a fake Old 3DS kept but inactive
  (FR-20, FR-21). The loader is a fuzz target (mini-fuzz in `nnh_tests`, libFuzzer with ASan/UBSan on the Linux build
  VM).
- **AC-6 No allocation.** A test counts allocations: after a set is created and its actions declared, evaluating,
  listening, remapping, resetting, saving and loading allocate nothing; the leak report is clean after the set is
  destroyed.
- **AC-7 Example `examples/actions`** (original art or shapes only): top screen a small character moved by a `move`
  vector (Circle Pad or D-Pad), jumping on `jump`, firing on `attack` with a held-time charge bar, an `aim` axis bound
  to the C-Stick with an L/R fallback on Old 3DS, and an on-screen touch button on the bottom screen; a "Controls"
  scene pushed with `pause` lists every action and its sources, rebinds the selected one (listen, conflict swap shown,
  reset one / reset all) and saves on exit; the bindings are loaded at start. PC screenshots (`NNH_SIM_INPUT` +
  `NNH_EXIT_AFTER_FRAMES` + `NNH_SCREENSHOT`) of the playfield and of the controls screen, checked by reading the PNGs.
- **AC-8 On the console, pressing only Y.** The example logs each action edge and each remap result in a fixed format
  (`actions: jump pressed`, `actions: rebind jump -> B (swapped with attack)`). A spec 013 script for `nnh run actions
  --script` drives the controls screen with ordinary taps (open it, rebind `jump` to another button, check the conflict
  swap line, reset, rebind again, leave so it saves), then presses the new button in play and expects the action
  lines, and finally asks the example (through its menu) to reload the bindings from the file and log a checksum of
  them; the example ends with `actions: PASS` or `actions: FAIL ...`. The script uses only controls every 3DS has. On
  an Old 3DS the maintainer only starts the game (Homebrew Launcher → Y); the verdict is in the saved log, and the
  `aim` fallback line says which path the console took. The same script passes with `--pc`.
- **AC-9 Cost on the console.** Measured on an Old 3DS with the profiler in the example's bench mode: evaluating a set
  of 64 actions × 4 slots (mixed kinds and sources) stays under 0.1 ms per frame (target; the plan confirms it); the
  number goes in the docs.
- **AC-10 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS; `examples/input`,
  `examples/scenes` and `examples/platformer` keep working untouched; spec 003's input tests pass unchanged.
- **AC-11 Docs.** `docs/api/actions.md` (kinds, declaring, bindings and sources, combining, contexts and scenes, the
  no-leak rule, queries and events, repeat, remapping and conflicts, display names, saving and versioning, Old 3DS
  fallbacks, the PC simulator and scripts, capacities and costs), the "Remapping … is a later spec" line in
  `docs/api/input.md` replaced by a link, a decision row in `docs/architecture/decisions.md` (the evaluation hook and
  the saved format), CLAUDE.md layout and ROADMAP updated, `docs/REFERENCES.md` citing Godot InputMap and Unity's
  Input System.

## Non-goals

- Remapping the **PC keyboard or gamepad** to 3DS controls (which key is "A"): that is the simulator's concern, a
  possible follow-up to spec 006; actions bind to the console's controls, which every system's platform provides.
- Chords and modifiers (`L` + `A`), double-tap, hold-to-trigger and other Unity "interactions"; `held_frames`,
  `frames_since_pressed` and repeat cover the common cases. A follow-up if games ask.
- Context priorities and input "consuming" between enabled contexts; several local players or several devices.
- Gyroscope, accelerometer, 3D slider, microphone; touch gestures and touch-drag vectors (a virtual stick).
- Button glyph textures, localized control names, an engine-drawn controls menu (the UI module may add one later).
- Loading default bindings from a data asset (spec 014): bindings are plain data, so a follow-up can add it; this spec
  does not depend on data assets (XI).
- Recording and replaying a session (spec 013's non-goal): FR-22 makes actions replay for free once that exists.
- Threads: sets are used from the game's thread only.

## Constraints

- **I** — one API and the same results on PC and 3DS; the module is portable core code with no platform part; the
  control vocabulary is the engine's (`Button`, sticks, touch), so later systems add sources without changing game
  code, and saved bindings with a source a system doesn't know are skipped (FR-21).
- **II** — fixed capacities reserved once per set; a 64-action set is a few KB; bounded per-frame cost (AC-9).
- **III** — the public header stays light (no `<functional>`, no heavy templates); `examples/hello` unaffected.
- **IV** — query for state (values, vectors, held), events only for press/release moments, delivered at the one
  dispatch point of the frame; one event per meaning (`ActionPressed`, `ActionReleased`), no event per value change.
- **V** — no exceptions, no RTTI, no `virtual` per action, no allocation after creation (AC-6).
- **VI** — pure logic over the input sample: everything unit-tested first on the fake platform; the example is checked
  by screenshot and on the console by script.
- **VII** — declarations, sources, evaluation, contexts, listening/conflicts and the saved format in separate small
  files; one public header.
- **VIII** — header comments, `docs/api/actions.md`, `examples/actions`.
- **X** — the bindings file is untrusted input: bounded, validated, fuzz-tested, never asserting on content; written
  atomically through `nnh::save` inside the game folder.
- **XI, D25** — entirely optional; `<nnh/input.hpp>` stays complete and unchanged; no dependency on data assets,
  tweens or UI.
- **Under pressure:** FR-15 needs the frame loop to evaluate live sets right after the input sample, without the input
  module (or `nnh::frame()`) depending on the actions module and without allocation — the same shape of problem D30
  solved for scenes. The plan proposes the mechanism (e.g. an intrusive list of evaluation hooks registered by sets)
  and a decision row; a game that creates no set must pay at most a null check. FR-5 adds a second user of D30's
  scene attachments. FR-14 adds bus traffic on press frames (bounded by actions per set; turned off per set).

## Prior art

- Godot `InputMap` and `Input` ([InputEvent and actions](https://docs.godotengine.org/en/stable/tutorials/inputs/inputevent.html),
  [InputMap](https://docs.godotengine.org/en/stable/classes/class_inputmap.html)): named actions with several events,
  per-action dead zone, `is_action_pressed`/`just_pressed`/`get_axis`/`get_vector` (FR-3, FR-7, FR-11); remapping is
  left to the game, saved by hand — this spec adds the listen/conflict/save pieces.
- Unity Input System ([Actions](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/Actions.html),
  [Action Maps](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/ActionAssets.html),
  [Interactive rebinding](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/ActionBindings.html#interactive-rebinding)):
  Button/Value actions, composite bindings (2D Vector from four buttons, FR-6), action maps enabled and disabled
  (contexts, FR-4), `PerformInteractiveRebinding` with cancel and excluded controls (FR-16), binding overrides saved as
  differences from defaults (FR-20).
- Common console-game practice: swapping on conflict and "reset to defaults" in controls menus (FR-17, FR-18).

## Open questions

All answered with the recommended answer, pre-approved by the maintainer (2026-11-22).

- [x] **Global input map or game-owned sets?** — **answer: game-owned sets** (FR-1), usually one per game. A hidden
      global map is the singleton anti-pattern and would allocate or cap without the game choosing; several sets cost
      nothing extra. Godot's global `InputMap` is matched by "one set the game keeps for its whole life".
- [x] **How does a game name actions: strings, hashes or its own enum?** — **answer: its own enum for code, a name for
      logs and saves** (FR-2). Enum queries are an array index (no lookup per frame, typos caught by the compiler); the
      name's hash keys the saved file so reordering the enum doesn't break saves.
- [x] **Action kinds** — **answer: button, axis, vector** (FR-3), all three answering `held`. These are Godot's
      `get_action_strength`/`get_axis`/`get_vector` and Unity's Button/Value(Axis/Vector2).
- [x] **Which sources can be bound?** — **answer:** buttons, stick directions (with hysteresis), whole sticks or one
      axis (with optional extra dead zone), button composites, touch areas (button actions, locked) (FR-6). Touch areas
      give on-screen buttons cheaply; touch-drag virtual sticks are a non-goal.
- [x] **Slots per action** — **answer: up to 4** (FR-6), enough for "button + D-Pad + stick + touch"; the plan may lower
      it to 3 if a record must stay small, and documents it.
- [x] **How are several bindings combined?** — **answer:** any-held for buttons; largest magnitude for axes and vectors
      (FR-7), matching the simulator's own merge rule.
- [x] **Contexts: priorities and consuming, or independent?** — **answer: independent, enabled/disabled, optionally
      bound to a scene** (FR-4, FR-5), plus the block-until-released rule on enable (FR-13). With a scene stack, the top
      scene's context is the only one most games need active; priorities and consume add order-dependent behaviour for
      little gain. A follow-up if games ask.
- [x] **Disabling a context while an action is held: emit `released`?** — **answer: no**; it reads idle silently
      (FR-13). A release event from a context that is no longer active would reach gameplay code that should be paused.
- [x] **Who evaluates sets, and when?** — **answer: the engine, right after the input sample in `nnh::frame()`**
      (FR-15), so events arrive in the same frame as `ButtonPressed` and queries are fresh in `update()`. A game-called
      `update()` would deliver action events a frame late and is easy to forget; the hook is the constitution pressure
      noted above.
- [x] **Do actions see remote/scripted input and the D-Pad mirror?** — **answer: yes, they read the same sample as
      `<nnh/input.hpp>`** (FR-10). Scripts, remote input and the simulator then need no change, and replays of input
      replay actions.
- [x] **Listening: what is captured, how is it cancelled?** — **answer:** next control that goes down after releasing
      what was held, sticks past 0.5; game-chosen cancel control (default `Start`), excluded mask, optional timeout;
      every action idle while listening (FR-16). Result via a query, not an event (it concerns only the menu that
      asked).
- [x] **Conflict policy** — **answer: chosen per listen, default swap** (FR-17), with replace, allow and reject
      available; conflicts only within a context; locked slots always win.
- [x] **Saved format: full bindings or overrides? Versioning?** — **answer: overrides only, keyed by name hash + slot +
      part, with the engine's format version and an optional game bindings version** (FR-20); through `nnh::save` or a
      caller buffer. Overrides let new releases change defaults without wiping players' choices; the game version gives
      a clean "reset everyone".
- [x] **Saving automatically?** — **answer: no**; the game calls save and load (FR-20, FR-21), choosing the path and
      the moment (leaving the controls screen). Hidden writes to the SD card are surprising and cost time on the
      console.
- [x] **Old 3DS without ZL/ZR/C-Stick** — **answer: fallback slots declared per capability, missing sources inactive,
      loaded bindings kept but inactive, `usable()` and a development warning** (FR-9, FR-21). Listening can't capture
      what can't be pressed.
- [x] **PC simulator** — **answer: unchanged**; actions bind to 3DS controls, which the simulator's keyboard and
      gamepad already produce (FR-10). Remapping PC keys is a non-goal.
- [x] **Header and module name** — **answer: `<nnh/actions.hpp>`, namespace `nnh::actions`** (the plan may adjust the
      exact type names), a separate optional module; `<nnh/input.hpp>` is not changed except for a doc link.
- [x] **Extras: repeat, held time, input buffer** — **answer: include** `repeat`/`repeated` (FR-12), `held_frames` and
      `frames_since_pressed` (FR-11): cheap, frame-exact, and the reasons games write their own counters (menus,
      charge attacks, jump buffering). Chords and double-tap stay non-goals.
- [x] **Example: new `examples/actions` or convert the platformer?** — **answer: a new `examples/actions`** (AC-7,
      AC-8), which owns the remap menu and the console script; the platformer and `examples/input` stay untouched to
      show XI. Converting the platformer's player to actions can follow as a small change.
- [x] **Default bindings from a data asset** — **answer: not in this spec** (non-goal); defaults are declared in code.
      The saved/default bindings are plain data, so spec 014 can feed them later without changing this API.
