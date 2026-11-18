# Plan 006: Simulator window — layouts, scale, fullscreen

Status: approved (2026-11-18)
Spec: [spec.md](spec.md)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | Everything lives in the PC platform library; games and core unchanged |
| II 3DS constraint | ✅ | Screens keep their native sizes; integer scales, nearest filtering (D4) |
| III Build speed | ✅ | A few small PC files; no new dependency |
| IV Event-driven | ✅ | Window keys handled in the PC event loop like the developer keys |
| V No hidden costs | ✅ | Two small render targets created at init; layout math is a few multiplications per frame |
| VI Spec + test first | ✅ | Layout math and settings parsing are pure and tested first |
| VII Small files | ✅ | layout math, settings, window control, composition in separate files |
| VIII Documented API | ✅ | `docs/api/simulator.md`, `docs/tech/sdl3.md` |
| IX Clean and legal | ✅ | — |
| X Safe by default | ✅ | Settings file bounded and validated, outside the game's data folder |

## Design

### Rendering: one target per screen, composed at the end of the frame

Today everything is drawn into one 400×480 logical canvas (`SDL_SetRenderLogicalPresentation`) with
each screen at a fixed offset. New:

- Two render-target textures at native size: top 400×240, bottom 320×240 (nearest scale mode).
- `begin_frame` clears each target to its color; `draw_2d` and the crash-screen text draw into the
  screen's target in **native screen pixels** (no offset, no clip: the target is the clip).
- `end_frame` sets the window as target, fills the background, and copies each visible screen to its
  rectangle from the layout (integer scale), then screenshots/presents as today.
- No logical presentation any more: the layout works in render-output pixels (high-DPI aware via
  `SDL_GetCurrentRenderOutputSize`).

Drawing code gets simpler (no offsets/clips), and layouts are only the final copy.

### Pure code (in `engine/platforms/pc/src/`, SDL-free, compiled into `nnh_tests` too)

```cpp
// sim_layout.hpp
enum class SimLayout : std::uint8_t { Stacked, SideBySide, LargeTop, TopOnly, BottomOnly };
struct Placement { bool visible; int x, y, scale; };                // window pixels, integer scale
struct LayoutResult { Placement top, bottom; int width, height; };   // content size
Size natural_size(SimLayout, int scale);           // stacked 400s×480s, side 720s×240s, large top
                                                   // 400s+320b × 240s (b = max(1, s/2)), single screens
int fit_scale(SimLayout, int window_w, int window_h);              // largest s ≥ 1 that fits (1 if none)
LayoutResult compute_layout(SimLayout, int scale, int window_w, int window_h);   // centered
bool window_to_bottom(const LayoutResult&, float x, float y, float& bx, float& by, bool clamp);
const char* layout_name(SimLayout); SimLayout next_layout(SimLayout);

// sim_settings.hpp
struct SimSettings { SimLayout layout; int scale; bool fit; bool fullscreen; int x, y, w, h; bool has_window; };
bool parse_settings(const char* text, std::size_t size, SimSettings& out);   // key=value lines, bounded
int format_settings(const SimSettings&, char* out, std::size_t size);
```

- Stacked keeps today's geometry (bottom centered under the top); side by side aligns both to the top;
  large top puts the small bottom screen to the right of the top screen, bottom-aligned.
- Scale 1–6 or fit; fullscreen forces fit.
- Settings: `layout=side`, `scale=3`, `fit=0`, `fullscreen=0`, `window=x,y,w,h`; unknown keys ignored,
  values clamped/validated, files over 1 KB rejected.

### Window control (`window_pc.cpp`)

- State in the PC context: layout, scale, fit, fullscreen, notice timer.
- Keys (all builds): F2 next layout, F3/F4 scale −/+, F5 fit, F9 or Alt+Enter fullscreen. F1/F10/F11/F12/
  Pause unchanged (Debug/Development).
- Changing scale/layout outside fullscreen and fit resizes the window to the natural size; fit
  recomputes every frame from the current window size.
- Title notice: "<title> — Side by side 3×" for 2 seconds, then the plain title.
- Settings file: `SDL_GetPrefPath("NNHGE simulator", <data folder>)/window.ini`, loaded at init and saved
  at shutdown — **skipped** for headless/scripted runs (`NNH_EXIT_AFTER_FRAMES`, `NNH_SCREENSHOT`,
  offscreen driver) and when `NNH_SIM_LAYOUT`/`NNH_SIM_SCALE` are set.
- Environment (screenshots, CI): `NNH_SIM_LAYOUT=stacked|side|large|top|bottom`,
  `NNH_SIM_SCALE=1..6|fit`. Default for scripted runs: stacked at `AppConfig::pc_window_scale`.
- A saved window position outside every display is ignored.

### Touch

`touch_pc.cpp` maps the mouse (window coordinates → render-output pixels) through
`window_to_bottom()`: a touch starts only over the visible bottom screen and, while held, follows the
mouse clamped to its edges; no bottom screen (top only) = no touch.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `pc/src/sim_layout.{hpp,cpp}` | new | layout math (pure) |
| `pc/src/sim_settings.{hpp,cpp}` | new | settings text format (pure) |
| `pc/src/window_pc.cpp` | new | keys, window size/fullscreen, title notice, settings file, env vars |
| `pc/src/video_pc.cpp` | changed | screen targets, compose in `end_frame` |
| `pc/src/draw2d_pc.cpp`, `debug_text_pc.cpp` | changed | draw into the screen's target, native pixels |
| `pc/src/touch_pc.cpp` | changed | mouse through the layout |
| `pc/src/app_pc.cpp`, `context_pc.hpp`, `devtools_pc.cpp` | changed | targets at init, no logical presentation, key routing |
| `tests/simulator/test_sim_layout.cpp`, `test_sim_settings.cpp` | new | AC-1 |

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1, FR-2 | `test_sim_layout.cpp`: rectangles per layout and scale, fit, centering, small windows |
| FR-3 | maintainer (AC-3) |
| FR-4 | `test_sim_layout.cpp`: window → bottom per layout, clamping, top only = none |
| FR-5 | all existing tests and example screenshots unchanged |
| FR-6 | `test_sim_settings.cpp` (valid, missing keys, garbage, oversized, out-of-range) + AC-3 |
| FR-7 | default headless screenshots byte-identical; `NNH_SIM_LAYOUT` screenshots per layout |
| FR-8 | docs + AC-3 |

## Risks and alternatives

- **Screenshots may change** if SDL's old logical presentation rasterized differently from the new
  native targets (e.g. rotated sprites); if so, the new images become the references (documented).
- **High-DPI displays**: mouse and window coordinates differ from render pixels; handled through
  SDL's render-output size and coordinate conversion, checked by the maintainer.
- Alternative rejected: scaling every draw call per screen (more code in every drawing path).

## Constitution Check (after design)

No change; the PC backend gets simpler drawing code (no offsets/clips).
