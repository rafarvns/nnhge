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
