# Spec 003: Full input — sticks, touch, gamepad

Status: approved (2026-11-18, maintainer)
Author: NNHGE maintainers · Date: 2026-11-17
Depends on: [spec 001 — event bus and scenes](../001-event-bus-and-scenes/spec.md) (input events) and
[spec 002 — core foundation](../002-core-foundation/spec.md) (platform contract, fake platform) — both
implemented.

## Problem and goal

Today a game can only read the digital buttons and the D-Pad (`<nnh/input.hpp>`). It cannot read the
**Circle Pad** (the main way to move on a 3DS), the **C-Stick** of the New 3DS, or the **touch screen**, so
it cannot make anything that needs analog movement or a touch menu. On PC, the simulator only knows the
keyboard: there is no way to play with a **gamepad**, and nothing stands in for the touch screen. After
this spec a game reads every 3DS control with the same query style it uses for buttons (plus events for
touch moments), and the simulator maps them to keyboard, gamepad and mouse, so the same game code plays
the same on PC and 3DS.

## User stories

- As a game developer, I want to read the Circle Pad as a normalized analog value, so that my character
  walks slowly or fast depending on how far the pad is pushed, on PC and 3DS alike.
- As a game developer, I want to read the C-Stick and know it is simply centered on an Old 3DS, so that I
  can use it for a camera without special-casing the model.
- As a game developer, I want to know where and when the bottom screen is touched, so that I can build
  touch menus and drag interactions.
- As a game developer testing on PC, I want to plug in a gamepad and click the bottom screen with the
  mouse, so that I can play-test analog movement and touch without sending the game to a console.

## Functional requirements

- **FR-1 Circle Pad.** A query returns the Circle Pad position as two values normalized to [-1, 1], with
  a dead zone applied (values inside it read exactly 0) and the result clamped to the unit circle. The
  same physical push gives the same value range on PC and 3DS.
- **FR-2 C-Stick.** A query returns the C-Stick position with the same normalization as FR-1. On systems
  without one (Old 3DS) it always reads (0, 0); `system::has(Capability::CStick)` (already in
  `<nnh/system.hpp>`) tells whether the system has one. The PC simulator reports the Circle Pad, C-Stick,
  ZL/ZR and touch capabilities, since it simulates them all.
- **FR-3 ZL/ZR.** `Button::ZL` and `Button::ZR` (already in the enum) are read on New 3DS and reported as
  never held on Old 3DS (already the contract; this spec verifies it on hardware and maps them on PC
  gamepads).
- **FR-4 Touch state.** Queries return whether the bottom screen is touched this frame and the touch
  position in bottom-screen pixels (x 0–319, y 0–239), plus "touch started this frame" and "touch ended
  this frame" with the same one-frame semantics as `pressed`/`released`. While not touched, the position
  is the last touched point.
- **FR-5 Touch events.** The input module publishes `TouchStarted` and `TouchEnded` events, each with the
  position, on the frames FR-4 reports them, delivered like `ButtonPressed` (same frame, before the game's
  update). Continuous state (stick positions, touch moves) is query-only (constitution IV: query for
  state, events for changes).
- **FR-6 One sample per frame.** Sticks and touch are sampled once per `nnh::frame()` together with the
  buttons, so every query and event in a frame agrees.
- **FR-7 PC keyboard.** The simulator maps keys to the Circle Pad and the C-Stick (digital: full
  deflection, diagonals clamped to the unit circle) without changing the existing button keys.
- **FR-8 PC mouse = touch.** Holding the left mouse button over the bottom screen of the simulator window
  touches it; the position is converted to 3DS bottom-screen pixels whatever the window scale. Dragging
  outside the bottom screen keeps touching at the clamped edge position; releasing the button ends the
  touch.
- **FR-9 PC gamepad.** The simulator uses the first connected gamepad (SDL3 gamepad API), connected
  before or during the run: left stick = Circle Pad, right stick = C-Stick, face buttons by **position**
  (the 3DS layout: right = A, bottom = B, top = X, left = Y), shoulders = L/R, triggers = ZL/ZR,
  D-Pad = D-Pad, Start/Back = Start/Select. Keyboard and gamepad work at the same time (buttons are
  combined; for a stick the input with the larger deflection wins). Unplugging releases everything it
  held.
- **FR-10 Testable without hardware.** The platform contract for input carries sticks and touch, and the
  fake platform can script them, so FR-1, FR-2, FR-4, FR-5 and FR-6 are covered by unit tests.
- **FR-11 Cost.** No heap allocation and no RTTI per frame; the input state stays a few dozen bytes.
- **FR-12 Circle Pad → D-Pad mirror (opt-in).** `Button::Up/Down/Left/Right` stay D-Pad only. A game can
  turn on a mirror so that pushing the Circle Pad far enough also holds the matching D-Pad button (with
  `pressed`/`released` and `ButtonPressed`/`ButtonReleased` as for a real press), e.g. for menus. Off by
  default; small movements around the threshold must not make the button flicker.

## Acceptance criteria

- **AC-1** Unit tests (PC, fake platform) cover normalization, dead zone and clamping (FR-1/FR-2), the
  C-Stick-less case (FR-2), the D-Pad mirror and its threshold (FR-12), touch state and its one-frame
  flags (FR-4), touch events and their ordering relative to button events (FR-5/FR-6).
- **AC-2** A new example `examples/input/` shows every control: buttons, both sticks (a dot inside a
  circle), and the touch point. Simulator screenshots show the sticks deflected and a touch, driven by
  environment variables like the other examples.
- **AC-3** On PC, the maintainer plays `examples/input` with keyboard, mouse and a gamepad (hot-plugged
  during the run) and sees each control respond.
- **AC-4** On a New 3DS, `examples/input` shows Circle Pad, C-Stick, ZL/ZR and touch responding; on an Old
  3DS (or with the C-Stick query false) the C-Stick stays centered.
- **AC-5** `hello`, `foundation` and `scenes` still work unchanged; `docs/api/input.md` documents all new
  queries, events and the PC mapping (keyboard, mouse, gamepad); the "planned for M1" note is removed.
- **AC-6** Build budgets still met (constitution III).

## Non-goals

- Configurable bindings or an action-mapping layer ("Jump" → A or Space) — a later spec (Command pattern).
- Gyroscope, accelerometer, 3D slider, volume slider, microphone, cameras, software keyboard (`swkbd`).
- Circle Pad Pro on Old 3DS, multiple gamepads, rumble, gamepad remapping UI.
- Touch gestures (swipe, long press); the 3DS has single touch only, so no multi-touch.
- A `TouchMoved` event (see open questions).

## Constraints

- Constitution I: no platform `#ifdef` in game code or public headers; one API for PC and 3DS.
- Constitution II and V: input state fixed size, no per-frame heap, no RTTI; C-Stick support must not
  cost anything when unused on Old 3DS.
- Constitution III: build budgets (edit one game `.cpp` → simulator < 2 s, `.3dsx` < 5 s).
- Constitution IV: query for state, events for changes (FR-5).
- Constitution VII: stick normalization, touch tracking and the PC gamepad/mouse code in separate files.

## Open questions

Resolved at approval (2026-11-18, maintainer):

- [x] **Stick value type and axes** — `float` in [-1, 1]; up and right are positive.
- [x] **Dead zone** — fixed engine default (about 10% of the range), not configurable.
- [x] **Circle Pad as D-Pad** — `Button::Up` etc. stay D-Pad only, plus an opt-in mirror function (FR-12).
- [x] **PC keys for the sticks** — maintainer's choice delegated: Circle Pad = I/J/K/L, C-Stick = numpad
  8/4/2/6; the existing button keys stay as they are.
- [x] **`TouchMoved`** — no; games poll the position while touched.
- [x] **Gamepad face buttons** — by position (the 3DS layout on any pad).
