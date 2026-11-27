# The `nnh` tool

`nnh` builds a game, runs it on the console with its log on your screen, and installs `.cia` files —
one command each (spec [010](../../specs/010-nnh-cli-run/spec.md)). It can also drive a running game and
run test scripts on it ([below](#driving-the-game-from-the-pc), spec [013](../../specs/013-console-remote/spec.md);
tutorial: [Testing on the console](testing-on-the-console.md)). `nnh serve` lets the console drive
instead, through the NNH Link app (spec [011](../../specs/011-nnh-link/spec.md)). It is built with the PC presets:
after `cmake --build --preset pc-debug` it is `build/pc-debug/tools/nnh/nnh(.exe)`. Run it from anywhere
inside the repository.

```sh
nnh run hello                 # build for the 3DS, send to the console, show the log
nnh run hello --pc            # the same game in the simulator
nnh run input --pc --script examples/input/tests/remote.nnhs   # a test script in the simulator: PASS/FAIL
nnh run input --script examples/input/tests/remote.nnhs        # the same script on the console
nnh build audio --release     # just build (Release)
nnh install audio             # build the .cia and install it with FBI
nnh serve                     # wait for NNH Link on the console (it picks, builds and starts games)
nnh unpair                    # forget the consoles paired with nnh serve
nnh help
```

`<game>` is the name given to `nnh_add_game()` (`hello`, `audio`, …). If you mistype it, `nnh` lists the
games it knows.

## Running on the console

1. On the 3DS: open the **Homebrew Launcher** and press **Y** (the netloader waits for a game).
2. On the PC: `nnh run <game>`.

`nnh` builds the game (Development by default, so it sends its log; `--release` for a Release build,
which runs without a log), finds the console on the network, sends the `.3dsx` and prints the game's log
until the game exits:

```text
nnh: found the console at 192.168.10.52
nnh: sending hello.3dsx (260 KB) to 192.168.10.52
nnh: hello is running; its log follows (also in build/logs/hello-20261118-170133.txt)
[f0        0.048s] I app    started "Hello NNHGE" (Development build), data in sdmc:/3ds/hello-nnhge/
[f421      8.996s] I app    shutdown after 421 frames
nnh: hello closed
```

- The level letter and the channel are coloured (errors in red) when the terminal supports it; set
  `NO_COLOR=1` to turn colours off. The same lines, without colours, are saved in
  `build/logs/<game>-<date>-<time>.txt` — useful after a crash ([debugging-on-3ds.md](debugging-on-3ds.md)).
- **Finding the console**: `nnh` asks the last console used, every local network's broadcast address and
  the global broadcast. If nothing answers it tries the last console anyway. `--console <ip>` sets it;
  the address is remembered per user (`%APPDATA%/nnhge/nnh.ini`, `~/.config/nnhge/nnh.ini`).
- Ctrl+C stops waiting for the log; the game keeps running on the console (any control `nnh` was holding
  is released).
- The first time, **Windows Firewall** may ask about `nnh.exe`: allow it on private networks. The game's
  log comes back on TCP port 17491 and the console's discovery answer on UDP 17491 (`nnh serve` also uses
  TCP and UDP 17492).
- `--pc` runs the simulator instead. A Debug/Development simulator connects back to `nnh` on the loopback
  interface (`NNH_REMOTE`, set by `nnh`), so its log is shown and saved by `nnh` too, exactly like the
  console's.

## Driving the game from the PC

`nnh run` can also press buttons, move the sticks, touch the screen and take screenshots of a running
game, and run **test scripts** that do this and check the game's log (spec
[013](../../specs/013-console-remote/spec.md)). It works with Debug and Development builds, on the console
and in the simulator, and the game needs no change. A Release build has no code for it. For a step-by-step
introduction, read [Testing on the console](testing-on-the-console.md); this section is the reference.

```text
nnh run <game> [--console <ip>] [--pc] [--script <file.nnhs>] [--keep-open] [--no-build]
```

| Option | Effect |
|--------|--------|
| `--script <file.nnhs>` | Run a test script and end with PASS or FAIL. The script is checked **before anything is built**: every error is printed as `file:line: what` and `nnh` exits with 2. Refused with `--release`. At its end, pass or fail, the game closes: the console goes back to the Homebrew Launcher |
| `--keep-open` | With `--script`: leave the game running after the script (controls given back), e.g. to look at a failure |
| `--pc` | Use the simulator. `nnh` listens on `127.0.0.1` and starts the simulator with `NNH_REMOTE=127.0.0.1:<port>`, so a Debug/Development simulator connects back: its log goes through `nnh` (shown and saved like the console's) and scripts drive it the same way. At the end of a script, `nnh` closes the simulator |
| `--no-build` | Run what is already built (for agents and ctest). If nothing is built, `nnh` stops (exit 2 with a script, 1 without) |
| `NNH_PRESET=<preset>` (environment) | Build and run from another preset of the same system, e.g. `pc-debug` instead of `pc-dev`, `3ds-debug` instead of `3ds-dev`. Letters, digits and `-` only; anything else is ignored |

The game is driven over the connection it already opens for its log (port 17491 on the console,
loopback with `--pc`), so only the PC that started it can drive it. `nnh` says `Hello` first; a game that
doesn't answer within 10 s is a Release build or one built before this feature (the wait starts again with every
line the game logs, so a game that loads for a while before its first frame is waited for). When `nnh` stops (end of
the script, `end`, Ctrl+C) the game gets every control back: remote buttons are released, sticks centred,
the touch lifted. A script's end then closes the game (unless `--keep-open`); after `end` or Ctrl+C the game keeps
running. With typed commands real buttons keep working the whole time (remote and real input are merged); a
**script** owns the controls instead: from `Hello` to its end the game reads only the script's input and the
console's (or simulator's) own controls are ignored, so a resting Circle Pad past the dead zone can't change a
result. A control that isn't at rest when the script attaches is reported once, by the console (`W remote Circle Pad
not at rest (0.064, 0.090): ignored while the script runs`) and as an `nnh:` note. HOME on the 3DS, closing the
simulator's window and Ctrl+C always work (spec 013, amendment 2026-11-24).

### Script format

A script is a plain text file, by convention `<game folder>/tests/<name>.nnhs`. Examples:
`examples/input/tests/remote.nnhs` (buttons, stick, touch, screenshots) and
`examples/platformer/tests/gate.nnhs` (a benchmark that reports PASS or FAIL) and
`examples/platformer/tests/level.nnhs` (levels, then a replay whose checksums must be the PC's).

```text
# Comments start with # (anywhere outside quotes). Blank lines are fine.
fail-on "] E "                 # any error line fails the script
wait 30                        # let 30 frames pass
tap A
expect "pressed A" 10          # a log line containing "pressed A", at the latest 10 frames from here
wait 10
hold L+R 20
screenshot both-held
end
```

- One command per line. Commands and button names are case-insensitive.
- TEXT arguments are in double quotes. Inside them, `\"` is a quote and `\\` a backslash; any other `\x`
  is kept as written (so `\*` and `\?` reach the pattern).
- Buttons: `A B X Y L R ZL ZR START SELECT UP DOWN LEFT RIGHT`; join several with `+` (`L+R`).

| Command | Meaning | Clock |
|---------|---------|-------|
| `wait N` | let N frames pass | +N |
| `press B[+B...]` | hold the buttons down from now on | — |
| `release B[+B...]` | let them go | — |
| `release all` | let every remote control go: buttons up, both sticks centred, touch lifted | — |
| `hold B[+B...] N` | press now, release N frames later | +N |
| `tap B[+B...]` | press now, release on the next frame | +2 |
| `stick circle X Y`, `stick cstick X Y` | set the Circle Pad or the C-Stick; X and Y from -1 to 1 (at most 3 decimals), y up; `0 0` centres it | — |
| `touch X Y` | touch the bottom screen at X 0–319, Y 0–239, or move the touch there | — |
| `drag X Y N` | move the touch from where it is to X Y, one step per frame; needs a touch down | +N+1 |
| `untouch` | lift the touch | — |
| `screenshot NAME` | save both screens at the end of the current frame; NAME is 1–32 of `A-Z a-z 0-9 _ -` | +1 |
| `screenshot NAME eyes` | the same plus the top screen's right eye (`-top-right.png`; stereo 3D, spec 026) | +1 |
| `slider V`, `slider off` | set the 3D slider the game reads, 0 to 1 (at most 3 decimals), from this frame; `off` gives it back (0 while a script runs). See [Testing on the console](testing-on-the-console.md#the-3d-slider) | — |
| `expect "TEXT" N` | wait for a log line that matches TEXT, at the latest N frames from now | the line's frame + 1 |
| `fail-on "TEXT"` | from here on, a log line that matches TEXT fails the script | — |
| `end` | the script passes once everything before it is done; nothing may follow. Optional: the end of the file is the same | — |

N is a whole number of frames, 1 to 216 000 (one hour at 60 Hz).

**Limits**: a script is at most 64 KB and 4096 lines, a line at most 256 characters, a TEXT at most 128.
Parser errors: unknown command or button, wrong number of arguments, a value out of range, `drag` without
a touch down, a bad NAME, an unterminated quote, TEXT without quotes (or quotes around anything else), an
empty TEXT, anything after `end`.

### Timing rules

Scripts count **frames**, not milliseconds, so a run is repeatable.

- The script clock starts at **frame 1**, the game's first frame. Each command happens at the current
  clock; only the commands with a "Clock" entry above move it.
- Everything on the same clock applies **at the start of that frame, before the game reads its input**, so
  the game sees remote input exactly like real input: `pressed()`, `ButtonPressed`, `TouchStarted`, ... on
  that frame. It is the only input the game sees: nothing pressed on the console reaches it during a script.
- `tap A` presses on frame *t* and releases on *t*+1; the next command is on *t*+2, so two taps in a row
  are two presses.
- `hold A N` presses on *t* and releases on *t*+N; the next command is on *t*+N too.
- `screenshot` shows the **end** of frame *t* and moves the clock one frame, so the commands after it are
  not in the picture.
- `drag X Y N` moves the touch on frames *t*+1 to *t*+N (the last step is exactly X Y) and holds it there
  one more frame; a `screenshot` or `untouch` right after it sees the touch at X Y.
- `expect` is a **barrier**: nothing after it is sent until a matching line arrives. Its window starts at
  the first frame of its part of the script (frame 1, or the frame after the previous `expect`'s line),
  because a line can come before "now" (`tap A` moves the clock past the press). It ends N frames after the
  clock at the `expect`. After a match, the clock is **the matching line's frame + 1**.
- **Late input fails the script.** `nnh` sends each command ahead of its frame, but after an `expect` it
  can only send once the line has arrived, and the message needs time to reach the console (over Wi-Fi,
  a few frames). A message that reaches the console after its frame is applied at once and reported, and
  the script fails with "late" and the line number instead of silently shifting time. So put `wait 10` or
  more between an `expect` and the next input or screenshot; the parser warns
  (`file:line: warning: input N frames after the expect on line M ...`) when there is less.
- A control the console doesn't have (ZL, ZR or the C-Stick on an Old 3DS) fails the script before any
  input is sent. The simulator has them all.

### Matching log lines

TEXT is a pattern matched anywhere in the whole line as received, frame prefix included:

```text
[f1234    20.571s] I demo   pressed A
```

`*` matches any run of characters, `?` exactly one character; `\*`, `\?` and `\\` are those characters.
Plain text therefore means "contains". Useful patterns: `"] E "` matches every error line, `"] W "` every
warning. A line at level **F** (failed assert, panic, crash) always fails a script. The frame of a line
comes from its `[f<n>` prefix; text without one (a raw `printf`) counts as the last frame the console
reported.

### Output and exit codes

`nnh` prints the game's log as usual plus its own `nnh:` lines, which are also saved in the log file
(here `examples/input/tests/remote.nnhs` in the simulator, shortened):

```text
nnh: input is running; its log follows (also in E:/nnhge/build/logs/input-20261121-212121.txt)
[f0        0.002s] I remote PC attached: remote control on
[f31       0.492s] I demo   pressed A
nnh: line 7: expect "pressed A" matched at frame 31
nnh: line 12 screenshot stick-right -> E:/nnhge/build/logs/input-20261121-212121/frame-000077-stick-right-top.png
nnh: line 12 screenshot stick-right -> E:/nnhge/build/logs/input-20261121-212121/frame-000077-stick-right-bottom.png
[f77       1.302s] I remote frame 77 sent a screenshot in 27 ms; its time is not representative
[f110      1.826s] I demo   touch ended at 280, 200
nnh: line 18: expect "touch ended at 280, 200" matched at frame 110
nnh: PASS (19 lines, frames 1-111)
```

A failure names the script line and the reason, e.g. `nnh: FAIL at tests/smoke.nnhs:7: expect "pressed A":
no matching line in frames 1-42`. After the verdict, `nnh` still shows the log for up to 1 s (console) or
10 s (simulator, until it closes).

| Exit code (with `--script`) | Meaning |
|-----------------------------|---------|
| 0 | PASS |
| 1 | FAIL: an `expect` timed out, a `fail-on` matched, a level-F line (unless the current `expect` names it and nothing of the script follows: a crash test), an input that arrived late, a message the console rejected or dropped, a screenshot that failed, a control the console doesn't have, or the game closed before `end` |
| 2 | could not run: a bad command line or script, `--release`, the build failed, nothing built (`--no-build`), no console found, no log connection within 10 s, a simulator that closed without connecting, no answer to `Hello` within 10 s of the game's last log line (a Release or older build), another protocol version |

Without `--script`, `nnh run` exits as before: 0 when the game ran (on the console) or the simulator's own
exit code (`--pc`), 1 when it couldn't build or send the game, 2 for a bad command line.

### Screenshots

Each `screenshot NAME` saves two PNG files, top (400×240) and bottom (320×240), the same size on the
console and in the simulator:

```text
build/logs/<game>-<date>-<time>.txt                                 the log
build/logs/<game>-<date>-<time>/frame-<n>-<NAME>-top.png            the screenshots of that run
build/logs/<game>-<date>-<time>/frame-<n>-<NAME>-bottom.png
build/logs/<game>-<date>-<time>/frame-<n>-<NAME>-top-right.png     with `eyes`: the right eye (400×240)
```

`<n>` is the captured frame, six digits, so the folder sorts by time. On the console a screenshot pair
takes about 0.6–0.8 s over Wi-Fi (Old 3DS XL): that frame is slow, and the game logs
`remote: frame <n> sent a screenshot in <ms> ms; its time is not representative`. Take screenshots
outside anything you measure.

### Typed commands (interactive mode)

Without `--script`, every line you type in `nnh`'s terminal while the game runs is one script command, run
as soon as you press Enter. Nothing is sent to the game before the first command, so a plain `nnh run`
behaves as before until you type.

- `help` lists the commands. Mistakes are reported as `nnh: typed:<n>: ...`.
- A typed command starts about 6 frames (100 ms) after the last frame `nnh` heard, or after the commands
  still waiting (a command typed during a long `hold` waits for its release). Typed timing is not
  repeatable: use a script for that.
- `expect "TEXT" N` reports `matched at frame <n>` or `no matching line in frames ...` and never ends the
  run; `fail-on` reports matches.
- `end` gives the game its controls back (the next command attaches again). In the simulator it doesn't
  close the game.

## Installing a `.cia`

1. On the 3DS: FBI → **Remote Install** → **Receive URLs over the network**.
2. On the PC: `nnh install <game>` (the game needs `CIA UNIQUE_ID ...` in its `nnh_add_game()`:
   [packaging.md](packaging.md)).

`nnh` builds the `.cia` (Release), serves it from your PC on a free port, gives FBI its address, and stops
when FBI is done. Accept the installation on the console. FBI has no discovery: `nnh` uses `--console` or
the last console used.

## Serving NNH Link: `nnh serve`

`nnh serve [--release]` turns the PC into a build server for **NNH Link**, the console app of spec 011
(`apps/nnh-link`, built with the others: `build/3ds-*/apps/nnh-link/nnh-link.3dsx`; copy it to
`sdmc:/3ds/` and open it from the Homebrew Launcher; guide: [NNH Link](nnh-link.md)). Run it in
the repository and leave it open:

```text
nnh: serving E:/RANDOM/nnhge as "RAFA" on port 17492 (Development builds); Ctrl+C stops
nnh: pairing code 726717 (valid for 2 minutes; a new one appears when a new console connects)
nnh: console 5c5c5c5c paired
nnh: the console asked for the games (7)
nnh: building hello_3dsx (3ds-dev)
ninja: no work to do.
nnh: sending hello.3dsx to the console
nnh: the console is starting hello
```

- It answers NNH Link's search on the local network (UDP 17492), serves one console at a time (TCP
  17492), and shows the log of the game the console starts, like `nnh run` (TCP 17491, saved in
  `build/logs/`).
- **Pairing**: a console's first connection needs the 6-digit code shown here. Codes last 2 minutes and
  work once; when a console tries to pair with no valid code, `nnh serve` shows a new one to type. Five
  wrong codes block pairing until `nnh serve` restarts. Paired consoles are kept in `nnh-pairs.ini` next
  to `nnh.ini` (secrets: owner-only file; never shown in the terminal). `nnh unpair` forgets them all.
- Builds stream their output to the console; if the build fails, the console shows its last 20 lines.
  Cancelling on the console (B) stops reading the build, which then fails on its next output line.
- The server's id is kept in `nnh.ini` (`server_id`), so the console recognises it after a restart.

## Safety

`nnh` only talks to local network addresses (10.x, 172.16–31.x, 192.168.x, link-local) and refuses others.
It listens only while a run, an install or `nnh serve` needs it, and its file server answers only the file
it is installing. `nnh serve` builds and sends programs only to an authenticated, paired console, and the
console accepts them only from the PC it paired with ([security.md](../architecture/security.md)).
Scripts and typed commands reach only the Debug/Development game that opened its log connection to this
`nnh` (the simulator only on the loopback interface); the game checks every message and ignores bad ones.

## How it works

`nnh` speaks the Homebrew Launcher netloader's protocol (the one devkitPro's `3dslink` uses) and FBI's
URL protocol with its own small code: no compression library and no devkitPro tool on the PATH needed
(decision D23). `nnh serve` uses NNH Link's own protocol from the `link/` library (decision D24, [plan
011](../../specs/011-nnh-link/plan.md)). Scripts and typed commands use the game's log connection (decision
D28, [plan 013](../../specs/013-console-remote/plan.md); code: `tools/nnh/src/remote/`). Details: [plan
010](../../specs/010-nnh-cli-run/plan.md); code: `tools/nnh/`. On Windows,
3DS builds run through devkitPro's MSYS2 shell (`DEVKITPRO_MSYS2` overrides its location), like
`scripts/dkp.sh`.

## Live values (spec 030)

`nnh run <game> [--pc]` speaks to a Debug/Development game's live values ([api/live.md](../api/live.md)):

- typed: `tunes [GROUP]` (name, value, default, range, step, `*` when changed), `watches [GROUP]`, `get NAME`,
  `watch NAME [every N]` (values printed as `[frame] name = value`), `unwatch NAME|all`, and the script commands `set`,
  `reset`, `action`, `dump`, `pause`, `resume`, `step [N]`, `expect-value` (reported, never ending the session);
- a value is refused before anything is sent, in plain words (`player/jump: 12 is outside 0..10`,
  `unknown name "player/jmp" (did you mean "player/jump"?)`);
- a dump (`dump`, or Y on the Tunables page) is saved next to the log as `build/logs/<game>-<date>-<time>-tunes.toml`;
- `--tunes FILE` reads a dump (or `"full/name" = value` lines) before anything is built — errors as `file:line: reason`
  —, starts the game held before frame 1 (`NNH_HOLD=1` in the simulator, `--nnh-hold` in the console's argv), sends the
  values (names the game doesn't declare are reported and skipped) and resumes it; `--hold` only holds;
- an older game answers `the game has no live values (built before spec 030)`.

## Hot reload of assets

> Status: in progress (spec 031): interactive watching, typed `reload`, the script commands (`swap-asset`,
> `restore-asset`, `expect-reload`; [testing-on-the-console.md](testing-on-the-console.md#hot-reload-in-scripts-spec-031))
> and `nnh restore-assets <game>` work; see [assets.md](../api/assets.md#hot-reload) for the kinds that reload.

While `nnh run <game> [--pc]` follows a Debug or Development game, it watches the game's assets folder (`nnh: hot reload
on: ...`). Saving a file there — an image, a Tiled map, a data file — starts a reload once nothing changed for 250 ms (so
an editor's save through a temporary file, or Tiled saving a map and its tileset, is one reload):

1. `nnh` builds the game's asset target (Ninja directly in the preset's folder on the PC, `cmake --build --preset <p>
   --target <game>_assets` through devkitPro's shell for the 3DS: the same converters, checks
   and output bytes as a normal build; nothing is compiled or linked, the `.3dsx` isn't repacked) and shows its output.
   A conversion error prints the converter's `file:line: reason`, and the game logs one warning and keeps its old content.
2. For a changed file outside the assets folder (a script's `swap-asset` of a `DATA_SCHEMA` source, for instance), a Ninja
   dry run of the game's program tells whether the change needs a rebuild: `nnh: reload refused: <command>: rebuild and
   restart (nnh run)`. Saves inside the assets folder skip it (≈ 0.4 s).
3. The converted files whose bytes changed go to the game as one batch over the log connection (the simulator's loopback
   too): `nnh: reload 1: 2 files, 6.2 KB (build 0.21 s)`, then `nnh: reload 1 applied at frame 1234, 0.58 s after the
   save: art/hero, levels/room`.

Typed commands: `reload` (build and send now, without waiting for a save), `reload <output path>` (send that converted
file again, e.g. `reload art/hero.png`). `nnh run --no-reload` turns watching off; scripts (`--script`) never watch.
A game that loads nothing reloadable (no texture, sound, font, map, data asset or string table) links no reload code and
answers like an older game. Release builds and games built before spec 031 don't reload (`the game has no hot reload (built before spec 031):
watching off`). What reloads by itself and what a game reloads on its own: [assets.md](../api/assets.md#hot-reload).
