# Plan 003: Full input — sticks, touch, gamepad

Status: in progress (approved 2026-11-18)
Spec: [spec.md](spec.md)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | One API; PC simulates every control; C-Stick absence is a capability query, not an `#ifdef` |
| II 3DS constraint | ✅ | Input state grows by ~40 bytes; `irrst` started only on New 3DS |
| III Build speed | ✅ | No new dependency (SDL3 gamepad is in the SDL already fetched); `input.hpp` stays small |
| IV Event-driven | ✅ | Sticks and touch position are queries; touch start/end are events (FR-5) |
| V No hidden costs | ✅ | Plain structs, no heap, no virtual; one `sqrt` per stick per frame |
| VI Spec + test first | ✅ | Pure logic (dead zone, touch edges, mirror) unit tested before wiring |
| VII Small files | ✅ | Stick math, touch state, mirror, PC gamepad, PC mouse each in their own file |
| VIII Documented API | ✅ | `docs/api/input.md` rewritten, `system.md` capabilities table updated |
| IX Clean and legal | ✅ | Only libctru/SDL3 public APIs; no button art |

## Design

### Public API

```cpp
// <nnh/input.hpp> additions
namespace nnh {

/// Analog stick position: x right, y up, each in [-1, 1], (0, 0) at rest. Dead zone applied.
struct Stick {
    float x = 0;
    float y = 0;
};

/// A point on the bottom screen, in pixels (x 0-319, y 0-239).
struct TouchPoint {
    int x = 0;
    int y = 0;
};

struct TouchStarted { TouchPoint position; };   ///< published on the frame a touch starts
struct TouchEnded   { TouchPoint position; };   ///< published on the frame it ends (last touched point)

namespace input {
Stick circle_pad();
Stick c_stick();                 ///< (0, 0) when !system::has(Capability::CStick)

bool touching();                 ///< bottom screen touched this frame
bool touch_started();            ///< one frame, like pressed()
bool touch_ended();              ///< one frame, like released()
TouchPoint touch_position();     ///< current point, or the last one while not touching

/// Opt-in: the Circle Pad also holds Up/Down/Left/Right (with pressed/released and Button events).
/// Off at init(). Press at |axis| >= 0.5, release below 0.4 (no flicker around the threshold).
void set_circle_pad_mirrors_dpad(bool enabled);
bool circle_pad_mirrors_dpad();
} // namespace input
} // namespace nnh
```

### Platform contract

`read_buttons()` becomes one call that samples everything once per frame (FR-6):

```cpp
// <nnh/platform/input.hpp>
struct InputSample {
    std::uint32_t buttons = 0;          // nnh::Button bits (D-Pad only for Up/Down/Left/Right)
    float circle_x = 0, circle_y = 0;   // raw, scaled to about [-1, 1] by the platform, y up, NO dead zone
    float cstick_x = 0, cstick_y = 0;   // 0 when there is no C-Stick
    bool touching = false;
    std::int16_t touch_x = 0, touch_y = 0; // bottom-screen pixels, valid while touching
};
void read_input(InputSample& out);     // called once per nnh::frame(), after poll_events(); no allocation
```

The platform only scales its raw range to about [-1, 1] (it knows its hardware); the dead zone, clamping,
touch edges and the mirror are core logic, the same on every platform and unit tested.

### Core logic (pure, unit tested)

- **Stick** (`input/stick.hpp/.cpp`): `Stick shape_stick(float x, float y)` — radial dead zone 0.10 with
  rescale (magnitude `m` → `(m - 0.10) / 0.90`, so output starts at 0 and stays continuous), then clamp to
  the unit circle. Non-finite input reads (0, 0).
- **Touch** (`input/touch_state.hpp`, header like `input_state.hpp`): current/previous `touching`, last
  point, `started()` / `ended()`; position clamped to 0–319 / 0–239.
- **Mirror** (`input/dpad_mirror.hpp/.cpp`): from the shaped Circle Pad and the previous mirrored bits,
  returns Up/Down/Left/Right bits with hysteresis (on ≥ 0.5, off < 0.4, per axis). Its bits are OR-ed into
  the held mask **before** edge detection, so `pressed`/`released`/`ButtonPressed` behave like a real
  D-Pad press.

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| `TouchStarted` | input module, in `nnh::frame()` | `TouchPoint position` | touch menus, drag start |
| `TouchEnded` | input module, in `nnh::frame()` | `TouchPoint position` (last point) | buttons on release, drop |

Order within a frame: button presses, button releases (as today), then `TouchStarted`, then `TouchEnded`.
A touch that starts and ends between two samples is not seen (60 Hz sampling, same as buttons).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/input.hpp` | changed | `Stick`, `TouchPoint`, touch events, new queries, mirror switch |
| `engine/core/contract/nnh/platform/input.hpp` | changed | `InputSample`, `read_input()` (replaces `read_buttons()`) |
| `engine/core/src/input/stick.hpp/.cpp` | new | dead zone + clamp |
| `engine/core/src/input/touch_state.hpp` | new | touch edges, last point |
| `engine/core/src/input/dpad_mirror.hpp/.cpp` | new | Circle Pad → D-Pad bits with hysteresis |
| `engine/core/src/input/input.cpp`, `input_internal.hpp` | changed | `input_update(const InputSample&)`, queries, touch events |
| `engine/core/src/app/app.cpp` | changed | `read_input()` instead of `read_buttons()` |
| `engine/platforms/fake/…` (`platform_fake.hpp`, `control_fake.cpp`, `input_fake.cpp`) | changed | `set_circle_pad`, `set_c_stick`, `set_touch`, `release_touch` |
| `engine/platforms/3ds/src/input_3ds.cpp` | changed | `hidCircleRead`, `hidCstickRead`, `hidTouchRead`, `KEY_TOUCH` |
| `engine/platforms/3ds/src/app_3ds.cpp` | changed | `irrstInit`/`irrstExit` on New 3DS |
| `engine/platforms/pc/src/input_pc.cpp` | changed | keyboard (buttons + I/J/K/L, numpad 8/4/2/6), combines sources |
| `engine/platforms/pc/src/gamepad_pc.cpp` | new | first gamepad: open/close on hot-plug, read buttons, sticks, triggers |
| `engine/platforms/pc/src/touch_pc.cpp` | new | left mouse over the bottom screen → touch, window → 3DS pixels |
| `engine/platforms/pc/src/input_override_pc.cpp` | new | `NNH_SIM_INPUT` (dev builds only) for scripted screenshots |
| `engine/platforms/pc/src/app_pc.cpp`, `context_pc.hpp`, `system_pc.cpp` | changed | `SDL_INIT_GAMEPAD`, gamepad events, capabilities |
| `tests/input/test_stick.cpp`, `test_touch_state.cpp`, `test_dpad_mirror.cpp`, `test_input_queries.cpp` | new | FR-1, 2, 4, 6, 12 |
| `tests/events/test_input_events.cpp` | changed | touch events and order (FR-5) |
| `tests/platform_conformance/test_contract.cpp` | changed | idle `read_input` is all zero / not touching |
| `examples/input/` | new | every control on screen |

### Data and memory

`InputState` + `TouchState` + mirror bits + two `Stick`s ≈ 48 bytes of static state. No heap. On 3DS,
`irrst` uses its own service shared memory (New 3DS only). PC: one `SDL_Gamepad*`.

### Platform work

- **3ds:** `hidScanInput()` once (as today); Circle Pad from `hidCircleRead` divided by its full range
  (≈ 156, measured on hardware in T4.3 and set as a named constant), C-Stick from `hidCstickRead` after
  `irrstScanInput()` only when `irrst` started (New 3DS; range measured the same way), touch from
  `KEY_TOUCH` + `hidTouchRead`. `irrstInit()` in `init()` when the model is a New 3DS, `irrstExit()` in
  `shutdown()`; failure to start leaves the C-Stick at (0, 0) and does not fail `init()`.
- **pc:** `SDL_INIT_GAMEPAD`; `SDL_EVENT_GAMEPAD_ADDED/REMOVED` in `poll_events()` keep one open gamepad
  (the first; when it goes, the next one connected is used). Sticks from `SDL_GetGamepadAxis` / 32767 with
  y inverted; triggers > 50% → ZL/ZR; face buttons by position (`EAST` = A, `SOUTH` = B, `NORTH` = X,
  `WEST` = Y). Keyboard sticks: I/J/K/L and numpad 8/4/2/6, full deflection, diagonal scaled to length 1.
  Buttons OR-ed; per stick the source with the larger magnitude wins. Mouse: `SDL_GetMouseState` →
  `SDL_RenderCoordinatesFromWindow` → bottom-screen rect `{40, 240, 320, 240}`; a touch starts only when
  the left button goes down inside it, then follows the mouse clamped to the edges until release (FR-8).
  Capabilities: add `TouchScreen`, `CirclePad`, `CStick`, `ZButtons`.
  `NNH_SIM_INPUT="circle=0.7,0.5;cstick=-1,0;touch=160,120;buttons=A+R"` replaces the sampled input every
  frame (Debug/Development only, ignored in Release), so examples can be screenshotted headless.
- **fake (tests):** setters for every `InputSample` field; `read_input` copies them.

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `test_stick.cpp`: inside dead zone → 0, just outside → small, full → 1, diagonal clamped, NaN → 0 |
| FR-2 | `test_input_queries.cpp`: fake with no C-Stick reads (0, 0); with one, shaped like FR-1 |
| FR-3 | 3DS run (AC-4); PC gamepad triggers (AC-3) |
| FR-4 | `test_touch_state.cpp` + `test_input_queries.cpp`: started/ended one frame, last point kept, clamping |
| FR-5 | `test_input_events.cpp`: `TouchStarted`/`TouchEnded` payload, after button events, same frame |
| FR-6 | `test_input_queries.cpp`: queries and handlers agree within a frame |
| FR-7, FR-8, FR-9 | `NNH_SIM_INPUT` screenshots (T5.2) + maintainer play test (AC-3) |
| FR-10 | the fake setters used by all tests above; conformance test on idle input |
| FR-11 | review: no allocation in `read_input` paths; state size `static_assert` |
| FR-12 | `test_dpad_mirror.cpp`: off by default, on at 0.5, held until < 0.4, events emitted, D-Pad + mirror OR-ed |

## Risks and alternatives

- **Hardware ranges** (Circle Pad ≈ ±156, C-Stick unknown) vary per unit: measure in T4.3 on the
  maintainer's console with the example's raw readout; the dead zone absorbs small differences. If a unit
  can't reach 1.0, a later spec can add calibration.
- **`irrst` on New 3DS**: if `irrstInit` fails, the C-Stick reads (0, 0) and `init()` still succeeds;
  the platform sets `platform::last_error()` and core logs a warning once (platform code never logs).
- **Laptops without a numpad** lack the keyboard C-Stick; acceptable (gamepad or `NNH_SIM_INPUT`).
- Alternative rejected: Circle Pad merged into the D-Pad like libctru's `KEY_UP` — the maintainer chose
  D-Pad only plus the opt-in mirror.

## Constitution Check (after design)

No change: the contract change (`read_buttons` → `read_input`) is internal to engine and platforms; games
only see additions to `<nnh/input.hpp>`. `NNH_SIM_INPUT` is a dev-build tool like `NNH_SCREENSHOT`.
