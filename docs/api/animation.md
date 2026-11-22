# animation — sprite sheets, clips, markers and state machines

Header: `<nnh/animation.hpp>` · Spec: [016](../../specs/016-sprite-animation/spec.md) ·
Plan: [016](../../specs/016-sprite-animation/plan.md) · Decision: D31 ([decisions](../architecture/decisions.md)) ·
Example: [`examples/animation`](../../examples/animation/main.cpp), and the player of
[`examples/platformer`](../../examples/platformer/player_sprite.cpp)

Describe a sprite sheet once — its frames, named clips with their timing, markers on frames ("step", "hit") and a
small state machine (idle, run, jump, fall, attack) — in code or by exporting it from Aseprite; give every moving thing
a small **animator**; call `update()` once per frame and draw its current frame with one `gfx::draw`. Animation
advances by game frames on the frame clock of [time.md](time.md), so the same calls give the **same frames and
notices on PC and 3DS**, frame for frame. Nothing allocates while playing.

The module is optional: a game that doesn't include the header links none of it. Frames work without clips, clips
without the state machine, code without Aseprite, and hand-written frame counters stay fully supported.

```cpp
#include <nnh/animation.hpp>
#include <nnh/gfx.hpp>

auto lib = nnh::SpriteLibrary::create({.frames = 4, .clips = 4, .clip_frames = 5, .markers = 2, .states = 4,
                                       .pivot = nnh::SpritePivot::BottomCenter}).value();
(void)lib.add_grid({.frame_width = 16, .frame_height = 16, .count = 4, .image_width = 64, .image_height = 16});
const nnh::SpriteClipId run = lib.add_clip({.name = "run", .first = 1, .count = 2, .frame_time = nnh::Frames{8}}).value();
(void)lib.add_marker(run, 0, "step");
const nnh::SpriteStateId s_run = lib.add_state("run", run).value();
// … idle, jump, fall the same way
nnh::SpriteAnimator anim{lib, s_idle};

// every frame, after the physics:
anim.request(on_ground ? (moving ? s_run : s_idle) : (vy < 0 ? s_jump : s_fall));
anim.set_flip(facing_left);
for (const nnh::SpriteNotice& n : anim.update())
    if (n.marker == nnh::sprite_name("step")) play_footstep();
nnh::gfx::draw(nnh::Screen::Top, sheet, feet, anim.draw_options());
```

The platformer's [`player_sprite.cpp`](../../examples/platformer/player_sprite.cpp) is this, complete.

## Sprite libraries

A `SpriteLibrary` holds frames, clips, markers and a state machine in **one block** reserved at creation (memory tag
`animation`, in the leak report and the memory overlay); adding allocates nothing. Every capacity is required (no
hidden default); adding beyond one returns `LimitReached`. Data errors are **returned** (`Result`), never asserted, and
a refused call adds nothing.

| Capacity (`SpriteLibraryDesc`) | Range | Bytes each |
|---|---|---|
| `frames` | 1..4096 | 16 |
| `clips` | 0..256 | 52 |
| `clip_frames` (the frames of all clips) | 0..8192 | 12 |
| `markers` | 0..1024 | 8 |
| `states` | 0..32 | 40 |
| `transitions` | 0..128 | 4 |

Plus a 64-byte header: the platformer's player is 572 bytes, the example's knight about 1.2 KB, the ceilings 187 KB.
`memory_used()` says the block's size.

**Lifetime.** A library is read-only while animators use it, and any number of animators share one. It must outlive
every animator using it: Debug and Development builds assert at an animator's next use after its library was destroyed;
Release doesn't check. Moving a library keeps its block, so animators stay valid. Keep the library in the object that
owns the animators (a scene, the game), destroy it before `nnh::shutdown()`.

### Frames (FR-1, FR-2, FR-11)

- `add_grid({.frame_width, .frame_height, .count, .columns or .image_width, .image_height, .margin, .spacing})` adds
  equal cells read row by row; with the image size, frames that don't fit are refused, naming the frame.
- `add_frame({.region, .pivot, .offset_x/y, .source_width/height})` adds one region; for trimmed frames the offset says
  where the region sits in the untrimmed frame of `source_width × source_height`.
- Pivots are in the **untrimmed** frame's pixels, kept to 1/16 pixel (bottom-centre of an odd width is exact). A
  negative pivot means the library's default: `TopLeft` (as `gfx::draw`), `Center` or `BottomCenter`.
- Frames alone are a static atlas: `frame(i)` and `draw_options(i, flip_x, flip_y)` need no clip or animator.

### Clips and their sequences (FR-3, FR-4)

`add_clip({.name, .first/.count or .frames, .frame_time, .durations, .mode, .repeat, .pingpong_passes, .direction})`.
Names are 1..31 bytes of UTF-8, unique (by their FNV-1a hash). A clip has 1..256 frames (a frame may repeat), a default
duration (`nnh::Frames{6}` or a `Duration`, rounded once to the frame clock) and optional per-frame durations. One play
lasts at most 1 hour. What `0 1 2 3` shows:

| Mode | Shows, then | `repeat` |
|---|---|---|
| `Loop` | `0 1 2 3 0 1 …` forever, `Looped` at every wrap | — |
| `Once` | `0 1 2 3`, holds 3 | — |
| `Repeat` | `0 1 2 3` × n, holds 3 | plays, 1..65535 |
| `PingPong` forever | `0 1 2 3 2 1 0 1 …` (turning frames never twice), `Looped` per round trip | 0 |
| `PingPong` n round trips | `0 1 2 3 2 1 0` (n = 1), holds the first frame | round trips |
| `PingPong` passes | `pingpong_passes = 3`: `0 1 2 3 2 1 0 1 2 3`, holds 3 | one-way passes (Aseprite's count) |
| `Reverse` direction | the same counts, mirrored: `3 2 1 0 …` / ping-pong `3 2 1 0 1 2 3 …` | |

A one-frame clip shows its frame; in once/repeat it finishes after its duration (ping-pong counts one per pass).
`clip_duration()` is one play (a round trip for ping-pong). `find_clip("run")` is a scan of ≤ 256 hashes: keep the id.

### Markers (FR-8)

`add_marker(clip, position, "step", value)` puts a marker on a frame of a clip; up to 8 per frame, fired in the order
they were added. Names are hashed only: compare with `nnh::sprite_name("step")`, which is `constexpr` (a `case` label).

## The animator (FR-6, FR-7)

`nnh::SpriteAnimator` is a plain value of **64 bytes on 3DS** (80 on PC; `static_assert`ed), one per sprite, with no
heap and no `virtual`. It plays clips of one library and advances only in `update()` (one 60 Hz frame) or
`advance(FrameTime)` (any span: `time::FixedStep`, a skip), both × `set_speed()` (0..64, rounded once to 1/4096: 0.5
and 1.5 exact; 0 freezes). The time left at the end of a frame carries into the next frame, loop and queued clip, so
nothing drifts: four 6-frame frames loop every 24 updates and end loop 10 on update 240.

| Command | Does |
|---|---|
| `play(clip)` / `play("name")` | starts it at frame 0; the same clip still playing is left alone unless `restart` |
| `play_from(clip, position)` | starts at a frame |
| `queue(clip)` | plays it when the current clip finishes or ends its loop (≤ 4 waiting; `LimitReached` keeps the queue); at once when nothing plays |
| `stop()`, `pause()`, `resume()`, `set_frame(position)`, `clear_queue()` | |
| `set_flip(x, y)`, `on_notice(callback)` | |
| `request(state)`, `force(state)` | the state machine (below) |

`play()` clears the queue; `play()` and `queue()` leave the state machine. Queries: `playing()`, `paused()`,
`finished()` (a finite clip holds its last frame), `clip()`, `clip_name()`, `position()` (in the clip), `frame()`
(sheet index), `region()`, `pivot()`, `offset()`, `time_in_frame()`, `progress()` (0..1 of the finite clip or of the
current loop; a scan of the clip), `loops()`, `state()`, `previous_state()`, `time_in_state()`.

Misuse — a command on an animator without library, an unknown id, a negative speed, an invalid time — asserts in
Debug/Development and is ignored with an error log in Release.

### Notices (FR-8, FR-9)

`update()`/`advance()` return the update's notices — `Marker` (with `marker`, `value`, `position`), `Looped`,
`Finished`, `StateChanged` (with `state`, `previous`) — and pass each to the `on_notice` callback in the same order.

- **Commands never notify.** A frame entered by `play`, `set_frame`, `request` or `force` fires its markers (and
  `StateChanged`) at the next update, before time advances.
- Per crossing: `Looped`/`Finished` of the old frame, then `StateChanged` if the end switched state, then the markers of
  the entered frame. Several frames crossed in one call each fire once, in order. A paused animator fires nothing.
- The returned view points at one module-wide buffer: valid until the next `update()`/`advance()` of **any** animator.
- The callback is inline (≤ 16 bytes of trivially copyable captures, like event handlers; bigger captures don't
  compile). It may call any command of its animator or of others; a command that changes the clip applies at once,
  drops the old clip's remaining notices and the rest of this update's time, and the new clip's entry fires in the same
  update. A chain of callbacks changing clips stops at depth 8 (`kSpriteMaxCallbackDepth`) with one error log; the last
  clip's entry fires at the next update. Calling `update()` from a callback is misuse.
- The engine publishes **no** bus event: publish your own from the callback (`events::publish(Footstep{id_})`).
- **Bounds:** one call crosses at most 32 frames and returns at most 32 notices. Beyond them Debug/Development assert;
  Release logs once, skips whole loops at once and lands on the right frame without the skipped notices. Keep frames
  longer than 1/32 of what one call covers (a 1 ms Aseprite frame is 1/16 of an update at speed 1).

## Drawing (FR-10)

`draw_options()` fills `region`, `pivot` and `flip_x/flip_y` of a `DrawOptions`; add the layer, tint, opacity or
scale and pass it to `gfx::draw` with your texture (the library owns no texture). The pivot is the frame's pivot minus
the trim offset, mirrored inside the region when flipped, so a trimmed frame, its untrimmed original and both flipped
land on the same pixels. With no clip it returns an empty region (the whole texture): don't draw then.

## State machine (FR-12, FR-13)

States live in the library: `add_state("run", clip)`, `set_next_on_finish(jump, fall)` (a once/repeat/finite
ping-pong clip that ends moves on by itself) and rules `add_transition({.from, .to, .when, .keep_time})`, where
`from`/`to` may be `SpriteStateId::any()`. An animator made with `SpriteAnimator{lib, start}` plays the start state
(no `StateChanged` for it). The game **requests** a state every frame from its own conditions:

| `request(s)` when… | Happens |
|---|---|
| `s` is the current state | nothing (and a waiting request is cancelled) |
| the rule is `Now` (also with no rule) | switches now; `keep_time` keeps the frame position and time (run ↔ run_shoot) |
| the rule is `AtEnd` | waits for the end of the clip or of its loop (a later request replaces it), then switches carrying the leftover time; at once if the clip already finished |
| the rule is `Never` | ignored |

The most specific rule wins: from→to, then from→any, then any→to, then any→any. At the end of a finite clip a
waiting request wins over next-on-finish, which wins over holding. `force(s)` switches now ignoring the rules (and
clears a waiting request; forcing the current state does nothing else). With no current state (after `play()`),
`request` acts as `force`. There are no engine-side parameters: the game's `if` is the condition.

## Aseprite and `load_sprite_library` (FR-16..FR-18)

Export the sprite sheet from Aseprite as `<name>.png` with its JSON data as `<name>.ase.json` beside it in the
assets folder; the build converts the JSON with `nnh-assetc sprite` into `<name>.nnhsprite` (D31), and the game loads
both by the same path:

```cpp
auto texture = nnh::load_texture("art/hero");
auto hero = nnh::load_sprite_library("art/hero", {.extra_states = 6, .extra_transitions = 3});
```

Tags become clips, `@name`/`@name=value` tags become markers, a `pivot` slice gives the pivot: the export settings,
what is read and the errors are in [assets.md](assets.md#aseprite-sprite-sheets). Loading validates the whole file
first (`Corrupted`, `Unsupported` for a newer format, `LimitReached`), then copies it into a block sized exactly for its
contents plus the extras asked for, so the game can add markers, states and rules in code (Aseprite has no state
machines). A loaded library and the same library built in code behave identically. The validator is the converter's
own and is fuzzed.

## Cost

Updating one animator is a pending-flag test, an add and a compare in the common case; a crossing reads one 12-byte
record and the frame's markers. Measured with the example's bench (512 animators: half on clips with markers, half on
a state machine with a request every frame): **0.12 ms** per frame in a pc-debug build, 0.04 ms in pc-dev. On an Old 3DS
(Development) the first console run measured 0.892 ms avg, 1.389 ms worst; the update path was then made call-free in
the common case and the fields it touches moved into the animator's first 32 bytes (one ARM11 cache line when the
animator is 32-byte aligned): **0.626 ms** avg, 1.309 ms worst ([plan 016](../../specs/016-sprite-animation/plan.md),
"Console bench refinement"). The console budget is < 0.8 ms for those 512 animators (about 1.2 µs each); a game with
50 animators spends about 0.06 ms. Module statics:
520 bytes of notice buffer, plus a 2 KB table of live library serials in Debug/Development.

## Limits and pitfalls

- Animators are single-threaded, like the rest of the game code.
- Keep the notices view only until the next update of any animator; copy what you need.
- Speed is per animator; there is no global animation clock to pause: pause the scene or stop calling `update()`.
- Marker names are hashed; two different marker names with the same hash can't be told apart (the converter refuses
  them; `add_marker` can't check, since names aren't stored).
