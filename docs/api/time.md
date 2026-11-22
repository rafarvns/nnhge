# time — clock, frame delta, fixed timestep

Header: `<nnh/time.hpp>`

## Duration

`nnh::Duration` is a span of time in integer nanoseconds (no float drift in long sessions).

```cpp
nnh::Duration cooldown = nnh::Duration::from_ms(250);
if (nnh::time::now() - last_shot > cooldown) shoot();
```

| Member | |
|--------|-|
| `from_seconds(s)`, `from_ms(ms)`, `from_us(us)` | build from integer units |
| `.ns`, `.ms()`, `.seconds()` | read as nanoseconds, milliseconds (integer), seconds (double) |
| `+ - * += -=`, `== != < > <= >=` | arithmetic and comparison |

## Frames and FrameTime (the frame clock)

The deterministic clock of tweens and timers ([spec 015](../../specs/015-tweens-and-timers/spec.md)) counts game
frames, not wall time: one update is exactly one 60 Hz frame on every machine, so replays and console checks give
the same numbers on PC and 3DS.

```cpp
nnh::FrameTime a = nnh::Duration::from_ms(500); // 30 frames
nnh::FrameTime b = nnh::Frames{12};             // 12 frames
```

| Member | |
|--------|-|
| `nnh::Frames{n}` | a whole number of 60 Hz frames |
| `FrameTime(Duration)`, `FrameTime(Frames)` | implicit; ticks of 1/4096 frame. A `Duration` is rounded to the nearest tick (1 s = 245 760 ticks = 60 frames exactly, 16.67 ms = 4096); frames are exact |
| `.ticks`, `.valid()`, `.duration()`, `.frames()` | the tick count; false for the invalid marker; back to nanoseconds (rounded); fractional frames |
| `FrameTime::from_ticks(t)` | a raw tick count |
| `nnh::kMaxFrameTime` | 1 hour (884 736 000 ticks). Negative or longer times become the invalid marker (`ticks == FrameTime::kInvalid`), which functions taking a `FrameTime` reject |

Plain values: no state, `constexpr`, no allocation. [Tweens and timers](tween.md) (`<nnh/tween.hpp>`) take them
for durations, delays and periods; `nnh::Timer` is a plain timer value on the same clock.

## Clock

| Function | Returns |
|----------|---------|
| `nnh::time::now()` | time since `nnh::init()`, monotonic (0 before init) |
| `nnh::time::delta()` | time between the start of the previous frame and this one (~16.7 ms at 60 Hz); 0 on the first frame |

Sources: 3DS `svcGetSystemTick` (ARM11 clock, ~268 MHz); PC `SDL_GetTicksNS`; tests a manual clock.

## FixedStep

Runs game logic at a fixed rate, independent of frame hiccups.

```cpp
nnh::time::FixedStep logic(60);          // 60 logic steps per second
while (nnh::frame()) {
    while (logic.next()) update_physics(logic.step());
    draw(logic.alpha());                 // optional: interpolate between steps
}
```

- `next()` adds the new frame's `delta()` once per frame, then returns true once per step due.
- After a long stall (loading, debugger) it runs at most `max_steps_per_frame` steps (default 4) and
  drops the rest: the game slows down briefly instead of freezing to catch up.
- `step()` is the step length; `alpha()` the fraction of a step pending, for smooth drawing.

Per-frame safe, no allocation. Keep game logic in fixed steps when it must be deterministic
(physics, replays); variable `delta()` is fine for animations and effects.
