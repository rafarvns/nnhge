# Plan 013: Console remote — input, screenshots and test scripts from the PC

Status: approved (2026-11-21, with every proposed answer)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D28 ([decisions.md](../../docs/architecture/decisions.md), proposed)
Uses spec 002 (logs with frame numbers, build flavors), spec 003 (input sample, queries, events), spec 010 (`nnh run`, the
game's log on TCP 17491, D23), spec 012 (the platformer benchmark whose console gate this runs)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | Remote input is merged in core, before `input_update`, so the game sees it through the normal queries and events on every system; each platform only moves bytes and reads its screens back (one new contract area, `remote`, implemented by pc, 3ds and fake). Games change nothing |
| II 3DS constraint | ⚠️→✅ | Two costs to bound: a screenshot needs a 288 000-byte linear buffer (allocated at the first request, freed when the PC leaves) and ~520 KB on Wi-Fi; a connected game polls its socket once per frame. Both are Development-only and measured on the console in stage 5 |
| III Build speed | ✅ | Six small core files, one contract header (not included by public headers), the rest in `tools/nnh`. Measured before/after (T8.6) |
| IV Event-driven | ✅ | No new events. Remote presses produce the existing `ButtonPressed`/`TouchStarted`/... because they enter before the frame's input is read |
| V No hidden costs | ✅ | Fixed-capacity queue and reader, no per-frame heap; the frame never waits for the PC. The one exception the spec allows: the frame that sends a screenshot blocks while sending, and says so in the log |
| VI Spec + test first | ✅ | Wire codec, reader, schedule, merge and the session (over the fake platform) in core; script parser, runner, stream splitter, image conversion and PNG writer in `tools/nnh`; all pure and tested first. Backends by the scripts themselves on PC and 3DS |
| VII Small files | ✅ | One concept per file; every planned file ≤ 250 lines |
| VIII Documented API | ✅ | No public API change. Docs: nnh tool guide (script format), new guide "testing on the console", debugging docs, simulator page (`NNH_REMOTE`), CLAUDE.md |
| IX Clean and legal | ✅ | Own code only; the PNG writer reuses `zlib_stored` (D23) and core's CRC-32 |
| X Safe by default | ⚠️→✅ | The console now *reads* from the network. Answered by: Development only (no code in Release, checked), the console opens the connection to the PC that launched it (PC: loopback only), fixed-size messages with every field validated, a bounded reader that is fuzzed (mini-fuzz + libFuzzer/ASan on VM 120) |
| XI Opt-in, never in the way | ✅ | A game does nothing to be driven; scripts are optional; without a connected `nnh` nothing runs, and with a log-only `nnh run` the only cost is one non-blocking poll per frame |

## Design

### Overview

```
 console / simulator (Development game)                     PC: nnh run <game> [--pc] [--script f.nnhs]
 ──────────────────────────────────────                     ───────────────────────────────────────────
 platform init: connect to the PC (3DS: 3dslink host:17491,  LogListener accepts (3DS: 0.0.0.0:17491;
   stdout/stderr dup2'd; PC: NNH_REMOTE=127.0.0.1:<port>)      --pc: 127.0.0.1:<any>, port passed in NNH_REMOTE)
                                                     <────  16-byte messages: Hello, Buttons, Stick, Touch,
 each frame, before input is read (core):                     Screenshot, Bye — each stamped with the frame it
   read what arrived (non-blocking, bounded) → validate →     applies to
   queue (64) → apply the ones due → merge into the sample
   → input_update (queries, events as usual)
 log lines "[f1234 ...] I chan text\n"               ────>  shown + saved (build/logs/<game>-<stamp>.txt)
 records "\x1E N R <type> <u32 len> <payload>"       ────>  Hello reply, Tick{frame}, Late, Rejected,
   (only after the PC's Hello)                                Screen{frame, pixels} → PNG in build/logs/<game>-<stamp>/
 end of the frame a screenshot was asked for:                 script runner: schedules inputs by frame, matches
   read both screens back, send them                          log lines, writes PNGs, exits 0 PASS / 1 FAIL / 2 error
```

### Decision D28 — the PC drives a Development game over its own log connection

**Channel.** The game already holds one TCP connection to the PC that started it (3DS: to the 3dslink host, port 17491,
spec 010). It is two-way; the PC simply never sent anything. The remote uses **that connection**: the PC writes
fixed-size messages to it, the console writes small binary records into its log stream. The console still opens the
connection, so only the PC that launched the game can drive it (FR-7), and a PC that never says `Hello` (devkitPro's
`3dslink -s`, `nnh serve`, `nnh run` without commands) sees exactly today's plain-text log (D8 compatibility).

**Console → PC framing: in-band records, not a second connection.** A record is `0x1E 'N' 'R' type u32-length
payload` (ASCII record separator + magic, 8-byte header). Log lines never contain `0x1E` (`NNH_LOG` output is
printable; a raw `printf` of exactly `1E 4E 52 <known type> <valid length>` is the only way to fake one, and the PC
falls back to text on any bad header). One stream keeps **order**: a log line written before a screenshot or a tick
arrives before it, which is what makes `expect ... within N frames` exact. Rejected: a second TCP connection for
binary data (new port and firewall prompt, a second socket in the 3DS's 1 MB SOC buffer, and the order between lines
and records lost); escaping every log byte (costs every line, breaks `3dslink -s`). On the 3DS, stdout is fully
buffered on a socket, so the platform flushes stdout/stderr before writing a record.

**PC → console: fixed 16-byte messages, not the `nnh_link` codec.** `nnh_link` (D24) uses `std::vector`/`std::string`
per message (heap in a per-frame path, constitution V), seals frames with a pairing secret that a game launched by the
Homebrew Launcher doesn't have, and is not a dependency of games. A fixed size has no length field to lie about, cannot
desynchronise, and every field is range-checked; the decoder is ~60 lines and fuzzed. The wire constants live in one
pure file (`engine/core/src/remote/remote_wire.*`, no engine includes) compiled by both core and `tools/nnh`, like
`crc32.cpp` and `map_format.hpp`.

**Repeatable timing: messages carry the frame they apply to.** The script runner (PC) turns the script into messages
stamped with absolute frame numbers; the console applies each one at the start of that frame, before input is read.
Script time starts at the game's first frame; after a matched `expect`, it continues from the frame *after* the
matching line (the frame number is in every log line's `[f<n>` prefix). Nothing depends on network latency: a message
that arrives after its frame is applied at once and reported `Late`, and the runner **fails** the script on it with
the line number and the fix ("add a wait"), instead of silently shifting time. Interactive commands (FR-5) are not
repeatable by nature; they apply "now" (last known frame + 6).

**Merge in core, not in the platforms.** `detail::remote_frame_start(frame, sample)` runs between
`platform::read_input()` and `detail::input_update()`: buttons are OR-ed in, a stick uses whichever of real/remote is
pushed further (the gamepad rule of `input_pc.cpp`), a remote touch wins while it is down. Dead zone, D-Pad mirror,
button edges and events then work unchanged. One merge, tested once on the fake platform, the same on every system.

**Screenshots: read back after the frame, convert on the PC.** At the end of a frame a screenshot was asked for
(right after `platform::end_frame()`), core asks the platform for each screen's pixels and sends them as they are,
tagged with their layout; `nnh` converts to RGB rows and writes the PNG. On the 3DS the source is the citro2d render
target of each screen (what that frame drew, independent of double buffering), copied into one linear buffer with
`C3D_SyncDisplayTransfer` and the target's own transfer flags (tiled RGBA8 → linear BGR8, the framebuffer's rotated
column layout). On PC, `SDL_RenderReadPixels` of each screen's render target (RGBA8 rows). No conversion or
compression on the console: the CPU it would cost is small, but the code would be console code.

**The simulator takes the same path.** `nnh run --pc` listens on `127.0.0.1:<free port>`, starts the simulator with
`NNH_REMOTE=127.0.0.1:<port>` in its environment, and runs the same session code as for the console: same messages,
same records, same frame stamps, screenshots of the same size (FR-4). In the simulator, log lines go to that socket
instead of the terminal while connected, so `nnh` shows and saves them as for the console. Rejected: stdin/stdout
pipes (process plumbing on Windows, binary data through text-mode pipes, and a different code path from the
console's).

**Release has none of it.** Every core call is under `if constexpr (build::kDevToolsEnabled)`; the remote object files
are then never referenced and the linker leaves them out of the static libraries, together with the platform
functions only they call. `scripts/check_release_strings.sh` gains the remote strings and symbol names (AC-4).

Rejected as a whole: a script interpreter on the console (latency-free, but more console code and a text parser
reading network data on the device); time-based (millisecond) scheduling (not repeatable).

### Protocol

All integers little-endian. Version 1.

**PC → console**: exactly 16 bytes per message.

| Bytes | Field |
|-------|-------|
| 0 | magic `'R'` (0x52) |
| 1 | type |
| 2–3 | `seq` (u16, set by the PC, echoed in `Late`/`Rejected`) |
| 4–7 | `frame` (u32): apply at the start of this frame; 0 = at the next frame |
| 8–15 | arguments (below); unused bytes must be 0 |

| Type | Arguments | Effect on the console |
|------|-----------|-----------------------|
| 1 `Hello` | u16 version, u16 flags (0) | attach: reply `Hello`, start ticks. Required first; anything before it is rejected |
| 2 `Buttons` | u32 press mask, u32 release mask (only the 14 `nnh::Button` bits, masks disjoint) | remote buttons held += press, −= release |
| 3 `Stick` | u8 which (0 Circle Pad, 1 C-Stick), u8 0, i16 x, i16 y in thousandths [−1000, 1000] (y up) | remote stick position |
| 4 `Touch` | u8 down (0/1), u8 0, u16 x ≤ 319, u16 y ≤ 239 | remote touch down/move or up |
| 5 `Screenshot` | u16 id, u8 screens (bit 0 top, bit 1 bottom, ≠ 0) | capture at the end of `frame` |
| 6 `Bye` | u8 quit (0/1) | release every remote control, stop ticks, free the screenshot buffer, detach; `quit` = 1 makes `nnh::frame()` return false (simulator runs: `nnh` closes what it started) |

Rules: messages are applied in arrival order; a message whose `frame` is lower than the previous queued one is rejected
(the runner never does that). Any invalid message (magic, type, range, reserved byte, order, before `Hello`) is
dropped, answered with `Rejected{seq, reason}` and logged as a warning (at most one warning line per 60 frames, with a
count); 8 invalid messages in a row detach the remote until the PC reconnects ("remote input off"). The queue holds 64
messages; the console reads only as many bytes per frame as the queue has room for, so reading is bounded and never
waits; a message that finds the queue full is answered `Dropped{seq}` (the runner keeps ≤ 48 unapplied, so this means
a bug).

**Console → PC**: records `0x1E 'N' 'R' type u32-length payload`, only between `Hello` and `Bye`.

| Type | Payload |
|------|---------|
| 1 `Hello` | u16 version, u16 flags, u32 current frame, u32 capabilities (`platform::capabilities()`), char[12] flavor |
| 2 `Tick` | u32 frame `f`: frame `f` started; every log line of earlier frames was sent before this record. Sent right after `Hello` and then every 4th frame (`kTickEvery`, see risks) |
| 3 `Late` | u16 seq, u32 wanted frame, u32 applied frame |
| 4 `Rejected` | u16 seq, u8 reason (magic, type, range, order, not attached, too many) |
| 5 `Dropped` | u16 seq |
| 6 `Screen` | u16 id, u8 screen (0 top, 1 bottom), u8 layout (1 RGBA8 rows top-down: PC; 2 BGR8 columns left→right, each bottom→top: 3DS framebuffer order), u32 frame, u16 width, u16 height, then the pixels (`width × height × 4` or `× 3`) |
| 7 `ScreenFailed` | u16 id, u8 screen, u32 frame (no memory, read-back failed) |

The PC treats this stream as untrusted too: records over 1 MiB, unknown types and bad headers fall back to text; sizes
must match `width × height × bpp` for the two known screen sizes.

### Where each piece lives

| Piece | Place | Why |
|-------|-------|-----|
| Wire format (constants, encode/decode, validation) | `engine/core/src/remote/remote_wire.*` — pure, compiled by core and `tools/nnh` | one definition for both ends |
| Reader (bytes → messages, order, malformed counting), schedule queue, remote input state + merge | `engine/core/src/remote/` — pure, tested, fuzzed | the console-side logic, the same on every system |
| Session (hello, ticks, apply due, bye, quit, disconnect) and screenshots | `engine/core/src/remote/remote_session.cpp`, `remote_screenshot.cpp` | glue over the contract, tested on the fake platform |
| Moving bytes, reading screens back | contract `nnh/platform/remote.hpp`; `remote_*.cpp` + `remote_capture_*.cpp` per platform | the only system-specific parts |
| Script parser, timeline, runner, log matching, stream splitter, image conversion, PNG | `tools/nnh/src/remote/` — pure, tested | all the "test" logic stays on the PC |
| Socket loop, stdin (interactive), files, CLI | `tools/nnh/src/remote/game_session.*`, `stdin_lines.*`, `commands/run*.cpp`, `cli.cpp` | I/O around the pure parts |
| Not used | `link/` (`nnh_link`) | see D28 |

### Platform contract (sketch)

```cpp
// engine/core/contract/nnh/platform/remote.hpp — Debug/Development only (core never calls it in Release).
namespace nnh::platform {

/// Reads bytes the PC sent, never waiting. @return > 0 bytes read, 0 nothing yet, -1 no connection (never had one,
/// or it closed). After -1 core stops calling. Must not allocate.
long remote_receive(void* buffer, std::size_t size);

/// Sends bytes to the PC after everything the program wrote to its log before (3DS: flushes stdout/stderr first).
/// May block until sent (ticks are 12 bytes; screenshots are the only large sends). @return false if the
/// connection is gone.
bool remote_send(const void* data, std::size_t size);

enum class PixelLayout : std::uint8_t { Rgba8Rows = 1, Bgr8ColumnsBottomUp = 2 };
struct ScreenPixels {
    const std::uint8_t* data = nullptr;
    std::uint32_t size = 0;
    std::uint16_t width = 0, height = 0;
    PixelLayout layout = PixelLayout::Rgba8Rows;
};
/// Reads `screen` back as the frame that just ended drew it (called right after end_frame()). The first call
/// allocates the read-back buffer; `out` stays valid until the next call or remote_capture_release().
/// @return false (and last_error()) if it can't.
bool remote_capture(Screen screen, ScreenPixels& out);
/// Frees the read-back buffer (end of a session, shutdown).
void remote_capture_release();

} // namespace nnh::platform
```

Core hooks (`engine/core/src/remote/remote_internal.hpp`), called from `app.cpp` only when `kDevToolsEnabled`:

```cpp
namespace nnh::detail {
/// Start of frame `frame` (= frame_count() during it), between read_input() and input_update(): reads, applies due
/// messages, merges remote input into `sample`, sends a tick when due. @return false when the PC asked to quit.
bool remote_frame_start(std::uint64_t frame, platform::InputSample& sample);
/// Right after platform::end_frame() of `frame`: sends the screenshots due for it.
void remote_frame_ended(std::uint64_t frame);
void remote_reset();     // init
void remote_shutdown();  // frees the capture buffer
}
```

`nnh::frame()` becomes: close the previous frame (+ `remote_frame_ended`) → scenes → poll events → time →
`read_input` → **`remote_frame_start`** → `input_update` → … (the rest unchanged).

### Public API

None for games (FR-1: the game needs no change; XI). The "API" of this spec is the script language and `nnh run`.

### Events introduced

None. Remote input produces the existing `ButtonPressed`, `ButtonReleased`, `TouchStarted`, `TouchEnded`, on the frame
it applies to.

### Script language (`.nnhs`)

One command per line; `#` starts a comment; blank lines are fine; commands and button names are case-insensitive;
text arguments are in double quotes (`\"` and `\\` escapes). The script clock starts at the game's first frame
(frame 1); commands with a duration move it forward.

| Command | Meaning | Clock |
|---------|---------|-------|
| `wait N` | let N frames pass | +N |
| `press B[+B…]` | hold buttons down from now (`A B X Y L R ZL ZR START SELECT UP DOWN LEFT RIGHT`) | — |
| `release B[+B…]` / `release all` | let them go / let every remote control go (buttons, sticks centred, touch up) | — |
| `hold B[+B…] N` | press now, release N frames later | +N |
| `tap B[+B…]` | press now, release on the next frame (two taps in a row are two presses) | +2 |
| `stick circle\|cstick X Y` | set a stick, −1…1 each, y up; `0 0` centres it | — |
| `touch X Y` | touch the bottom screen at X 0–319, Y 0–239 (or move the touch there) | — |
| `drag X Y N` | move the touch from where it is to X Y, one step per frame (needs a touch down) | +N |
| `untouch` | lift the touch | — |
| `screenshot NAME` | both screens of the current frame → `frame-<n>-NAME-{top,bottom}.png` (NAME: `[A-Za-z0-9_-]`, 1–32) | — |
| `expect "TEXT" N` | wait for a log line matching TEXT from now to N frames later; then the clock = the line's frame + 1 | → match |
| `fail-on "TEXT"` | from here to the end, a line matching TEXT fails the script | — |
| `end` | the script passed once everything before it is done (inputs applied, screenshots received); nothing may follow | — |

- **Matching**: TEXT is a pattern over the whole received line (frame prefix included): `*` any run, `?` one
  character, `\*` `\?` literal; it matches anywhere in the line (plain text = "contains"). `fail-on "] E "` fails on
  any error line. A line at level `F` (assert, panic, crash) always fails the script.
- **Failures**: an `expect` that times out, a `fail-on` match, a `Late`/`Rejected`/`Dropped` message, a `ScreenFailed`,
  the game closing before `end`, a control the console doesn't have (ZL/ZR/C-Stick on an Old 3DS, from the `Hello`
  capabilities, checked before any input is sent). The message names the script line.
- **Parser errors** (reported before building, all of them, `file:line: what`): unknown command or button, wrong
  number of arguments, numbers out of range (frames 1–216 000 per command, sticks −1…1, touch within the screen),
  `drag` without a touch, bad name, unterminated quote, anything after `end`; limits: file ≤ 64 KB, ≤ 4096 lines,
  line ≤ 256 characters, TEXT ≤ 128 characters. A missing `end` is the same as `end` at the end of the file.
- **Warning** (not an error): an input or screenshot less than 10 frames after an `expect` will probably arrive late.

Example (`examples/input/tests/remote.nnhs`):

```text
# Smoke test of the console remote: buttons, stick, touch and screenshots.
fail-on "] E "
wait 30
tap A
expect "pressed A" 10
wait 10
hold L+R 20
stick circle 1 0
wait 15
screenshot stick-right
stick circle 0 0
touch 40 40
drag 280 200 30
screenshot dragged
untouch
expect "touch ended" 5
end
```

### `nnh run`

```text
nnh run <game> [--console <ip>] [--pc] [--script <file.nnhs>] [--no-build]
```

- `--script`: parse first (errors → exit 2 before building); refuse `--release` ("Release builds have no remote
  control"); build; send the `.3dsx` (or start the simulator); when the game connects, send `Hello` and wait ≤ 3 s for
  its answer (none → exit 2: "the game didn't answer: a Release build, or one older than nnh"); check capabilities;
  run; print `nnh: PASS (<lines> lines, frames 1–<n>)` or `nnh: FAIL at <file>:<line>: <why>`; `Bye`; exit **0 pass,
  1 fail, 2 could not run** (script error, build failed, no console, no answer). On the console the game keeps
  running; with `--pc` the simulator is closed (`Bye{quit}`).
- **Interactive (FR-5)**: without `--script`, every line typed in the terminal is one script command, run as soon as
  it is typed (the first one sends `Hello`; until then nothing changes on the console). `expect` waits and prints the
  result but never ends the run; `end` detaches and goes back to the plain log; `help` lists the commands. Not
  repeatable: each line starts at the last frame heard + 6.
- `--no-build`: run what is already built (agents re-running a script; the ctest smoke test). Proposed addition, see
  questions.
- **`--pc`**: listens on `127.0.0.1:<free port>`, sets `NNH_REMOTE` in its own environment (inherited by the child),
  starts the simulator on a thread (`run_shell`), and runs the same session. The simulator's log now also goes through
  `nnh` (coloured and saved in `build/logs/` like the console's).
- **Output**: the log as today, plus `nnh:` lines for the script's progress (`nnh: line 14 screenshot stick-right →
  build/logs/input-20261122-101500/frame-000061-stick-right-top.png`), also written to the log file. Screenshots go in
  `build/logs/<game>-<stamp>/` next to `build/logs/<game>-<stamp>.txt` (created at the first screenshot); `<n>` is the
  captured frame, zero-padded to 6 digits so the folder sorts by time.

**Runner (pure, `script_runner.*`)**: inputs — `Hello`, ticks, log lines (frame from the prefix; lines without one get
the last tick's frame), records; outputs — messages to send, PNG jobs, progress text, verdict. It sends messages in
frame order and keeps at most 48 unapplied (frame > last tick) — the console queue holds 64. An `expect` is a barrier:
nothing after it is sent until it matches. `end` passes when a tick ≥ the final clock has arrived and every screenshot
asked for is in.

### Simulator and the platformer's automatic gate

- **pc** (`remote_pc.cpp`): at init (Debug/Development), if `NNH_REMOTE=<ip>:<port>` names a loopback address
  (127.0.0.0/8; anything else is ignored), connect with a 1 s timeout (Winsock / BSD sockets, non-blocking connect);
  `remote_receive` = non-blocking `recv`; `remote_send` = blocking `send`. `log_pc.cpp`: while connected, a line goes
  to the socket instead of stdout (`if constexpr` guarded). `remote_capture_pc.cpp`: `SDL_RenderReadPixels` of the
  screen's render target → an `SDL_Surface` kept until the next capture.
- **3ds** (`remote_3ds.cpp`): the log socket of `app_3ds.cpp` (new accessor `ctr::link_socket()`); `recv(...,
  MSG_DONTWAIT)` (in libctru's `sys/socket.h`); `remote_send` = `fflush(stdout)`, `fflush(stderr)`, then `send` in
  16 KB pieces. `remote_capture_3ds.cpp`: `linearAlloc(240 × 400 × 3)` on the first call; `C3D_SyncDisplayTransfer`
  from the target's `frameBuf.colorBuf` with `GX_BUFFER_DIM(240, 400|320)` in and out and the target's
  `transferFlags` (output format BGR8 as `gfxInitDefault` sets the screens; checked with `gfxGetScreenFormat`); one
  buffer, used for the top screen and then the bottom one (core sends each before asking for the next). Layout and
  colour order are confirmed with a test pattern in stage 5.
- **fake**: in-memory "PC": `fake::remote_connect()`, `remote_push(bytes)`, `remote_disconnect()`,
  `remote_sent()` (what core sent), `set_remote_capture_fails()`; captures are a known RGBA pattern 400×240 / 320×240.
- **`examples/platformer` automatic gate (FR-6)**: in the benchmark, **A** (unused there; or `PLATFORMER_DEMO=gate`)
  starts `gate.cpp`: trial = set counts, skip 30 frames, measure N frames, pass if `missed` = 0. First 4 layers + 300
  sprites for 600 frames, then the limits (layers up with 300 sprites, sprites up by 100 with 4 layers; 300 frames per
  trial; ≤ ~2.5 min in all), each logged (`bench: trial 5 layers + 300 sprites: ok, avg 15.9 ms, worst 16.8 ms, missed
  0/300`); then `bench: limit 6 layers with 300 sprites`, `bench: limit 1100 sprites with 4 layers`, and last
  **`bench: PASS 4 layers + 300 sprites, avg 15.2 ms, worst 16.9 ms, missed 0 of 600 frames`** (or `FAIL`). Lines fit
  the 160-byte log line. Trials count frames, so a slow console takes longer but measures the same thing.
- **`examples/platformer/tests/gate.nnhs` (AC-1)**: bonus level (run right, jump, screenshot) → SELECT bench →
  SELECT flips (wait, screenshot) → SELECT bonus → SELECT bench (screenshot) → `fail-on "bench: FAIL"` → tap A →
  `expect "bench: PASS" 12000` → screenshot of the result → `end`. Flips come *before* the benchmark so their
  screenshot exists even when the gate fails. Screenshots are taken outside the measured trials.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/src/remote/remote_wire.hpp/.cpp` | new | message and record layouts, encode/decode, validation, constants (pure; also compiled by `tools/nnh`) |
| `engine/core/src/remote/remote_reader.hpp/.cpp` | new | byte stream → messages: partial messages, order, `Hello` first, malformed counting and the 8-in-a-row cut (pure; fuzz target) |
| `engine/core/src/remote/remote_schedule.hpp/.cpp` | new | 64-entry FIFO, due messages for a frame, late detection (pure) |
| `engine/core/src/remote/remote_input.hpp/.cpp` | new | remote buttons/sticks/touch, apply a message, merge into an `InputSample`, release all (pure) |
| `engine/core/src/remote/remote_session.cpp`, `remote_internal.hpp` | new | the hooks: poll, attach, ticks, apply, bye/quit, disconnect, warnings (channel `remote`) |
| `engine/core/src/remote/remote_screenshot.cpp` | new | due screenshots → `Screen`/`ScreenFailed` records, the "slow frame" log line |
| `engine/core/contract/nnh/platform/remote.hpp` | new | contract above |
| `engine/core/src/app/app.cpp` | changed | the four hooks, `if constexpr (build::kDevToolsEnabled)` |
| `engine/core/src/log/engine_channels.*` | changed | channel `remote` |
| `engine/core/CMakeLists.txt` | changed | sources |
| `engine/platforms/pc/src/remote_pc.cpp`, `remote_capture_pc.cpp` | new | loopback socket from `NNH_REMOTE`; read-back of the screen targets |
| `engine/platforms/pc/src/log_pc.cpp`, `app_pc.cpp`, `context_pc.hpp`, `CMakeLists.txt` | changed | log to the socket while connected; connect at init / close at shutdown; `ws2_32` on Windows |
| `engine/platforms/3ds/src/remote_3ds.cpp`, `remote_capture_3ds.cpp` | new | non-blocking reads and flushed sends on the log socket; render-target read-back |
| `engine/platforms/3ds/src/app_3ds.cpp`, `context_3ds.hpp`, `CMakeLists.txt` | changed | `ctr::link_socket()` (the shutdown order stays: core frees the capture buffer in `nnh::shutdown()` before `platform::shutdown()` closes the socket) |
| `engine/platforms/fake/src/remote_fake.cpp`, `fake_internal.hpp`, `include/nnh/platform_fake.hpp`, `CMakeLists.txt`, `README.md` | new/changed | scriptable PC, recorded sends, synthetic captures |
| `tests/remote/test_remote_wire.cpp`, `test_remote_reader.cpp`, `test_remote_schedule.cpp`, `test_remote_input.cpp`, `test_remote_session.cpp`, `test_remote_frames.cpp`, `test_remote_screenshot.cpp` | new | unit tests (strategy below) |
| `tests/fuzz/fuzz_targets.*`, `test_fuzz_smoke.cpp`, `libfuzzer_remote.cpp`, `tests/CMakeLists.txt` | changed/new | `remote_target` |
| `tests/platform_conformance/test_contract.cpp` | changed | `remote_receive` is −1 without a connection; no capture without a frame (pc, fake) |
| `tools/nnh/src/remote/game_stream.hpp/.cpp` | new | bytes → log lines and records (bounded; text fallback) |
| `tools/nnh/src/remote/script.hpp`, `script_parser.cpp` | new | `.nnhs` → steps, all errors with line numbers |
| `tools/nnh/src/remote/script_timeline.hpp/.cpp` | new | steps → frame-stamped messages (hold, tap, drag expansion; clock) |
| `tools/nnh/src/remote/script_runner.hpp/.cpp` | new | send window, barriers, expect/fail-on/level F, late/rejected/dropped, end, verdict; interactive mode |
| `tools/nnh/src/remote/log_match.hpp/.cpp` | new | pattern match; frame and level from a line's prefix |
| `tools/nnh/src/remote/screen_image.hpp/.cpp`, `png_writer.hpp/.cpp` | new | layouts → RGB rows; RGB → PNG (stored zlib, CRC-32) |
| `tools/nnh/src/remote/game_session.hpp/.cpp`, `stdin_lines.hpp/.cpp` | new | the socket loop (print/save lines, records → runner, PNG files), the typed-lines thread |
| `tools/nnh/src/commands/run.cpp`, new `commands/run_pc.cpp`, `game_log.*`, `log_listener.*`, `cli.*`, `repo.*` | changed | `--script`, `--no-build`, the simulator path, sessions instead of `read_lines`, screenshot folder |
| `tools/nnh/CMakeLists.txt` | changed | sources (+ core's `remote_wire.cpp`, `crc32.cpp`); tests; ctest `nnh_script_pc_smoke` |
| `tools/nnh/tests/test_game_stream.cpp`, `test_script_parser.cpp`, `test_script_timeline.cpp`, `test_script_runner.cpp`, `test_log_match.cpp`, `test_screen_image.cpp`, `test_png_writer.cpp`, `test_fuzz_tool.cpp`, `test_cli.cpp` | new/changed | tool tests |
| `examples/input/` (`touch_log.cpp`, new `press_log.cpp`), `examples/input/tests/remote.nnhs` | changed/new | log presses and touches (what the smoke script expects) |
| `examples/platformer/gate.hpp/.cpp`, `bench.*`, `main.cpp`, `tests/gate.nnhs` | new/changed | the automatic gate and its script |
| `scripts/check_release_strings.sh` | changed | remote strings and symbols forbidden |
| docs (tasks T8.x) | changed/new | guides, architecture, tech, CLAUDE.md, ROADMAP |

### Data and memory

| Where | What | Size | When |
|-------|------|------|------|
| core (BSS, Development only) | schedule 64 × 16 B; read scratch 64 × 16 B; reader 16 B + counters; remote input ~24 B; session ~64 B | **≈ 2.2 KB** | always in Debug/Development; not linked in Release |
| 3DS `Linear` | read-back buffer `240 × 400 × 3` (top; reused for the bottom) | **288 000 B** | from the first screenshot until `Bye`, disconnect or shutdown |
| PC heap | `SDL_Surface` per capture (RGBA8) | 384 000 B | during one capture |
| Release | — | 0 | never |

**Per frame** (3DS estimates, measured in stage 5): not connected → one flag test; connected, not attached → one
`recv(MSG_DONTWAIT)` (a SOC IPC, est. 20–100 µs); attached → + one 16-byte tick send every 4th frame and a stdout flush.
**Screenshot frame**: transfer ~1 ms, then ~520 KB sent (Wi-Fi, est. 0.5–2 s; loopback on PC: ms); the next frame's
`time::delta()` includes it, and the console logs `remote: frame 1234 sent a screenshot in 812 ms; its time is not
representative` (FR-2).

### Platform work

Summarised in "Simulator and the platformer's automatic gate" above: pc (socket from `NNH_REMOTE`, log to socket,
read-back of targets), 3ds (log socket, flushed sends, `C3D_SyncDisplayTransfer` read-back), fake (scriptable bytes,
recorded sends, synthetic captures, conformance).

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/remote/test_remote_input.cpp` (OR of buttons, further-pushed stick wins, remote touch wins, release all, masks); `test_remote_schedule.cpp` (due at exactly its frame, FIFO, frame 0 = next frame, late reported); `test_remote_frames.cpp` through `nnh::frame()` on the fake (a press stamped for frame 10 is `pressed()` and publishes `ButtonPressed` on frame 10 and not 9; `hold` 5 frames = held 5 frames then `ButtonReleased`; real + remote together; D-Pad mirror from a remote stick; touch events); `tools/nnh/tests/test_script_timeline.cpp` (hold/tap/drag frames, clock); end to end: `examples/input/tests/remote.nnhs` on PC and 3DS |
| FR-2 | `test_remote_screenshot.cpp` (record bytes for both screens of the right frame, only the asked screens, `ScreenFailed`, buffer released at `Bye`/disconnect/shutdown, the log line); `test_screen_image.cpp` (both layouts → same RGB for a known pattern; wrong sizes refused); `test_png_writer.cpp` (decoded back with the test's stored inflate: header, CRCs, pixels); file names in `test_script_runner.cpp`; 3DS test pattern vs PC screenshot (stage 5) |
| FR-3 | `test_script_parser.cpp` (every command, case, comments, quotes and escapes, every error with its line, limits, after-`end`, the expect warning); `test_log_match.cpp` (patterns, escapes, prefix → frame/level, lines without a prefix); `test_script_runner.cpp` against a fake console (expect in window / timeout at the right tick / clock = line frame + 1, fail-on, level F, late/rejected/dropped → fail with line, game closes early, missing capability, `end` waits for ticks and screenshots, ≤ 48 unapplied, barrier); `test_cli.cpp` (`--script`, `--no-build`, `--script` + `--release` refused); exit codes |
| FR-4 | ctest `nnh_script_pc_smoke`: `nnh run input --pc --no-build --script examples/input/tests/remote.nnhs` with `SDL_VIDEO_DRIVER=offscreen` → exit 0 and four PNGs of 400×240 / 320×240; the gate script with `--pc` (AC-2) |
| FR-5 | `test_script_runner.cpp` interactive mode (lazy `Hello`, "now + 6", expect reports without ending, `end` detaches); by hand on PC (type `tap A`) |
| FR-6 | `PLATFORMER_DEMO=gate` in the simulator logs trials, limits and `bench: PASS ...`; gate script on PC and console |
| FR-7 | `check_release_strings.sh` on `3ds-release` and `pc-release` binaries (strings and symbol names); `test_remote_session.cpp` (no `Hello` → nothing sent, no ticks); conformance (no connection → −1); `remote_pc.cpp` ignores non-loopback `NNH_REMOTE` (conformance on pc) |
| FR-8 | `test_remote_wire.cpp` (every field rule, reserved bytes, round trip); `test_remote_reader.cpp` (split messages, garbage, 8 in a row → off, before `Hello`, order); `test_remote_session.cpp` (disconnect → every remote control released and `ButtonReleased` published, warning rate limit, queue full → `Dropped`, reads bounded by room); fuzz `remote_target` (mini-fuzz in `nnh_tests`, `fuzz_remote` libFuzzer with ASan on VM 120); PC side: `test_game_stream.cpp` + mini-fuzz of the stream and the parser in `test_fuzz_tool.cpp` |
| AC-1 | `nnh run platformer --script examples/platformer/tests/gate.nnhs` on an Old 3DS (agent; maintainer presses Y once) |
| AC-2 | the same script with `--pc`; screenshots compared by size and by eye |
| AC-3 | the unit tests and fuzz targets above; ASan/UBSan for `nnh_tests` and `nnh_tool_tests` on VM 120 |
| AC-4 | release strings check (T8.3) |
| AC-5 | docs tasks T8.4–T8.7 |

## Risks and alternatives

- **3DS read-back** (the main unknown): that `C3D_SyncDisplayTransfer` right after `C3D_FrameEnd(0)` waits for the
  frame's rendering, that the target's `transferFlags` give the framebuffer's layout, and the BGR order. Checked first
  in stage 5 with a test pattern compared with the PC screenshot. Fallbacks, in order, all inside
  `remote_capture_3ds.cpp`: `C3D_FrameSync()`/`gspWaitForP3D()` before the transfer; reading the displayed framebuffer
  (`gfxGetFramebuffer` after the swap; double buffering makes "which one" the risk); a CPU Morton de-tile of the RGBA8
  target (pure, testable on PC).
- **Screenshot time on Wi-Fi** (est. 0.5–2 s per pair) and its side effects: the frame blocks, streamed music may
  stutter. Accepted for a Development tool (the spec allows a slow frame). If a pair takes > 2 s: PackBits RLE in
  `Screen` records (layout 3; pure encoder on the console, decoder in the tool, both fuzzed), or sending in pieces over
  several frames.
- **Per-frame poll cost** on 3DS (SOC IPC): measured in stage 5 with a profile scope. If > 0.1 ms: poll every 4th frame
  until attached. Ticks are already every 4th frame (`kTickEvery`); the protocol's meaning doesn't change with it.
- **Benchmark in Development**: the gate's numbers include asserts, profiler scopes and the remote's tick. See question 1.
- **Time-based game logic**: the same inputs on the same frames give the same results only for frame-based logic; a game
  that moves by `time::delta()` can differ slightly between PC and console (documented in the guide; the platformer's
  physics are per frame).
- **Blocking sends**: if the PC stops reading but keeps the connection, a send can wait (as stdout already can today).
  `nnh` always reads; Ctrl+C closes the connection.
- **Lateness**: a script that sends input right after an `expect` fails with "late"; the parser warns and the guide
  explains. Re-anchoring automatically would hide non-repeatable runs (question 4).
- **Rejected alternatives**: `nnh_link` codec, a second connection, pipes for the simulator, a console-side interpreter,
  time-based scheduling, PNG/compression on the console (all in D28 above).

## Questions for the maintainer (answered 2026-11-21: all as proposed)

1. **Gate build flavor.** T3.9 measures a *Release* build, but remote control exists only in Debug/Development (FR-7),
   and a Release build sends no log at all. Proposed: run the gate in **Development** (`RelWithDebInfo`, `-O2`; Release
   is `-O3` without asserts, so a Development pass implies a Release pass; the numbers are reported as Development).
2. **`nnh run --pc` routes the simulator's log through `nnh`** (coloured by `nnh`, saved in `build/logs/` like the
   console's) and **closes the simulator when a script ends**; on the console the game keeps running. Proposed: yes.
3. **Exit codes and `--no-build`**: 0 pass, 1 fail, 2 could not run (only with `--script`; plain `nnh run` keeps 0/1);
   `--no-build` runs what is already built (needed by the ctest smoke test and handy for agents). Proposed: yes.
4. **Strict timing after `expect`**: input less than ~10 frames after an `expect` arrives late and fails the script
   (with a parser warning), rather than being shifted silently. Proposed: strict.
5. **Raw screenshots in v1** (~520 KB per pair, no compression); RLE only if stage 5 measures > 2 s per pair. Proposed: yes.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Merge in core; one contract area with pc, 3ds and fake implementations; same scripts on both (FR-4) |
| II | ✅ (measured) | ≈ 2.2 KB BSS and 288 000 B `Linear` only while screenshots are used, Development only; per-frame cost measured in stage 5 with fallbacks |
| III | ✅ | Small core module, contract header private to core; tool code in `tools/nnh`; measured (T8.6) |
| IV | ✅ | No new events; existing input events for remote input |
| V | ✅ | Fixed capacities, bounded non-blocking reads; the only blocking send is the screenshot frame, logged as such |
| VI | ✅ | Every FR mapped to unit tests (core on the fake platform, tool against a fake console) plus scripts on PC and 3DS |
| VII | ✅ | ≈ 30 files, all planned ≤ 250 lines |
| VIII | ✅ | No public API change; guide pages, simulator page, CLAUDE.md |
| IX | ✅ | Own code |
| X | ✅ | Development only (checked in Release binaries), console-initiated connection (PC: loopback only), fixed-size validated messages, fuzzed reader on both ends |
| XI | ✅ | Nothing for games to do; scripts optional; no cost without a connected `nnh` |

Changed during design: II and X needed the numbers and the fuzzed fixed-size reader above, and a new decision (D28).
