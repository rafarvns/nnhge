# Testing on the console

A **test script** lets the PC play your game for you: it presses buttons, moves the sticks, touches the
screen, takes screenshots of both screens and checks the game's log, then says PASS or FAIL. The same
script runs in the simulator and on a 3DS. On the console, the only thing a person does is start the game
(Homebrew Launcher → **Y**); nobody needs to hold the console, read numbers or report them.

This guide walks through it. The full list of commands and rules is in the
[nnh tool guide](nnh-tool.md#driving-the-game-from-the-pc). The design is in
[spec 013](../../specs/013-console-remote/spec.md).

## What you need

- A **Debug or Development** build. `nnh run` builds Development by default; Release builds have no remote
  control at all.
- For the console: a 3DS with the Homebrew Launcher, on the same local network as the PC (see
  [Getting started](getting-started.md#4-run-on-a-3ds)).
- Your game needs no change. Remote input reaches it exactly like real input: `nnh::input::pressed()`,
  `ButtonPressed`, `TouchStarted` and so on, on the frame the script says. During a script it is the *only*
  input: the console's own controls are ignored ([step 4](#4-run-it-on-the-console)).

## 1. Make the game say what happened

Scripts check the game through its **log**. Log what a test needs to see with `NNH_LOG_*`
([logging](../architecture/logging.md)). For example, `examples/input` logs every press and touch
(`press_log.cpp`, `touch_log.cpp`):

```cpp
void PressLog::on_pressed(const nnh::ButtonPressed& e) { NNH_LOG_INFO(demo, "pressed %s", button_name(e.button)); }
```

Every log line starts with its frame and level, so a script knows *when* it happened:

```text
[f31       0.492s] I demo   pressed A
```

## 2. Write a script

Put it next to the game, in `tests/<name>.nnhs`. This is `examples/input/tests/remote.nnhs`:

```text
fail-on "] E "                     # any error line fails the test
wait 30                            # the game's first 30 frames
tap A
expect "pressed A" 10              # the game must log it
wait 10                            # give the next input time to travel (see "Timing")
hold L+R 20
stick circle 1 0
wait 15
screenshot stick-right             # both screens, saved as PNG
stick circle 0 0
touch 40 40
drag 280 200 30                    # 30 frames to get there
screenshot dragged
untouch
expect "touch ended at 280, 200" 5
end
```

`nnh` checks the whole script before it builds anything and lists every mistake as `file:line: what`.

## 3. Run it in the simulator

```sh
build/pc-debug/tools/nnh/nnh run input --pc --script examples/input/tests/remote.nnhs
```

The simulator opens, the script plays, and `nnh` prints the game's log with its own `nnh:` lines:

```text
nnh: line 7: expect "pressed A" matched at frame 31
nnh: line 12 screenshot stick-right -> E:/nnhge/build/logs/input-20261121-212121/frame-000077-stick-right-top.png
...
nnh: PASS (19 lines, frames 1-111)
```

At the end `nnh` closes the simulator (`--keep-open` leaves it running). Exit code: **0** PASS, **1** FAIL, **2** could not run (a script
error, a failed build, no answer from the game, ...). Write and fix scripts here first: it is fast and the
results are the same as on the console.

## 4. Run it on the console

1. On the 3DS: open the **Homebrew Launcher** and press **Y**.
2. On the PC:

   ```sh
   build/pc-debug/tools/nnh/nnh run input --script examples/input/tests/remote.nnhs
   ```

`nnh` builds the `.3dsx` (Development), sends it, waits for the game to connect and runs the script. While it
runs, **only the script's input counts**: the console's own buttons, sticks and screen are ignored (in the
simulator too), so a Circle Pad that doesn't quite centre can't move things. If a control is busy when the script
attaches (a stick past the dead zone, a button held, the screen touched), the console says so once and `nnh`
repeats it:

```text
[f0        0.002s] W remote Circle Pad not at rest (0.064, 0.090): ignored while the script runs
nnh: the console's own controls are ignored while the script runs: Circle Pad not at rest (0.064, 0.090)
```

That's a hint, not a failure; but if the same game misbehaves when *you* play it, that pad is the reason (some Old
3DS pads rest just past the 0.10 dead zone). To leave in the middle, press HOME or stop `nnh` (Ctrl+C): the
connection closes and the console has its controls back. When the script ends (PASS or FAIL), the game
closes and the console is back in the Homebrew Launcher, ready for the next run (press **Y** again). Add
`--keep-open` to leave the game running with its controls given back, e.g. to look at a failure on the screens.

If the game doesn't answer (`nnh: the game didn't answer: a Release build, or one older than nnh`), it is a
Release build or was built before this feature. If nothing connects at all, see the firewall note in the
[nnh tool guide](nnh-tool.md#running-on-the-console).

## Timing: frames, not seconds

A script counts **frames** of the game. The clock starts at frame 1 and only `wait`, `hold`, `tap`, `drag`,
`screenshot` and `expect` move it. `nnh` stamps every input with its frame, and the game applies it at the
start of that frame, before reading input. Network delay therefore doesn't change *when* an input happens,
so the same script gives the same frames every time, on the PC and on the console.

The one thing to remember: **after an `expect`, wait before the next input.** `nnh` can only send what
follows an `expect` once the matching line has arrived, and the input needs a few frames to reach the
console over Wi-Fi. If it arrives after its frame, the script **fails** with "late" and the line number
(it never shifts time silently). Use `wait 10` or more; the parser warns when there is less. Even in the
simulator a short wait fails now and then on a busy PC (`prefabs`' script did, one frame late, until its waits
were raised). The one exception in the tree is `examples/entities/tests/check.nnhs`: its touches hit moving
targets, so its waits keep the frames the expected lines were recorded with (and its warnings).

Other rules worth knowing:

- An `expect` window starts at the frame after the previous `expect`'s line (or frame 1), so a line logged
  during the commands before the `expect` counts. After a match, the clock continues from the line's
  frame + 1.
- `tap` takes 2 frames (press, release); `hold B N` takes N.
- A `screenshot` shows the end of its frame and takes one frame: what follows is not in the picture.
- A `drag X Y N` takes N + 1 frames: it holds the touch at X Y for one frame before what follows.

All the rules: [nnh tool guide, timing rules](nnh-tool.md#timing-rules).

## What is repeatable

Inputs land on the same frames every run. The game's response is the same when its logic is the same for
the same inputs on the same frames:

- **Frame-based logic** (move 2 pixels per frame, count frames) repeats exactly, on the PC and on the
  console.
- Logic that uses **`nnh::time::delta()`** or the clock can differ slightly between runs and between the
  PC and the console, because frame times differ.
- **Random numbers** repeat only with a fixed seed.
- **Screenshots** cost time on the console (next section), and a measured frame time is never exactly the
  same twice: test thresholds ("no frame over 16.7 ms"), not exact times.
- A script can't press ZL, ZR or move the C-Stick on an Old 3DS: such a script fails before any input is
  sent. The simulator has every control.

## Reporting a result from the game

Some checks are easier inside the game: a benchmark, a self-test, a long sequence. Let the game decide and
**log the verdict** as one line; the script waits for it and fails on the opposite. `examples/platformer`
does this for its tile benchmark ([gfx](../api/gfx.md), `examples/platformer/gate.cpp`): A in the benchmark
starts the automatic gate, which logs one line per trial and ends with

```text
I platformer bench: PASS 4 layers + 300 sprites, avg 16.7 ms, worst 17.4 ms, missed 0 of 600 frames
```

or `bench: FAIL ...` (a warning, so it is also kept in Release logs). Its script,
`examples/platformer/tests/gate.nnhs`, ends with:

```text
fail-on "bench: FAIL"
tap A
expect "bench: gate started" 10
expect "bench: PASS" 12000
wait 15
screenshot gate-result
end
```

On an Old 3DS XL this ran for about a minute with nobody touching the console and passed (spec 012's
tile gate, 2026-11-21). Tips: keep result lines short (log lines are at most 159 characters), put the
numbers in them so the log is the report, and show the verdict on screen too, so a screenshot shows it.
A script can `expect` only one line per frame (the next window starts after the matched line), so put a
final checksum and a verdict logged on the same frame in one line.

**Testing a crash.** A fatal (`F`) line fails a script, except when the current `expect` names it and the script
has nothing after it: the game is stopped on its crash screen, so that expect is the last line.
`examples/foundation/tests/cpu_fault.nnhs` presses L + R + B (a null-pointer write) and expects the engine's
"The CPU stopped on an invalid operation" line; on a 3DS this needs Luma3DS developer UNITINFO
([debugging on 3DS](debugging-on-3ds.md)).

## Checking that the console plays like the PC

Frame-based logic repeats exactly, so a game can prove it: play a scripted input **inside the game** (no
buttons), log a checksum of its state every so often, and let the script `expect` the exact lines the PC
printed. `examples/platformer` does this with its replay (X in a level; `replay.hpp`): level 1 then the
bonus level, a checksum of the player and the camera every 60 frames, and an in-game 60 Hz verdict on the
last line. `examples/platformer/tests/level.nnhs` ends with:

```text
tap X
expect "replay: started" 10
expect "replay: 60 level1 x 121 y 242 coins 1 sum 0CF7E4CE" 120
...
expect "replay: 840 bonus x 437 y 162 coins 0 sum C3E8734D" 120
expect "replay: done 900 frames sum 26B21D57: 60 Hz PASS" 120
```

The lines come from a PC run, and the ctest `platformer_replay_matches_script` replays it in the simulator
on every test run and fails if they no longer match, so the script never goes stale.

## Screenshots

`screenshot NAME` saves both screens as PNG in a folder named like the run's log:

```text
build/logs/<game>-<date>-<time>/frame-<n>-<NAME>-top.png       400×240
build/logs/<game>-<date>-<time>/frame-<n>-<NAME>-bottom.png    320×240
```

They show exactly what the screens show, at the console's own size, on the PC and on the console. Agents
can read the PNG files to check a result without a person.

`screenshot NAME eyes` adds the top screen's **right eye** (spec 026, stereo 3D):
`frame-<n>-<NAME>-top-right.png` (400×240) beside the top (left eye) and the bottom. After a frame drawn with one
picture for both eyes (slider at 0, no 3D draws) the right eye is the top screen again.

## The 3D slider

`slider V` (0 to 1, at most 3 decimals) sets the 3D slider the game reads (`nnh::system::stereo_slider()`) from that
frame on, on the console and in the simulator; `slider off` puts it back. While a script runs the slider is the
script's, like the buttons and sticks: **0 from its start** until its first `slider` command, 0 again after `slider
off`, and the console's own slider is ignored, so stereo checks need nobody's hand on the console. Typed by hand
(`nnh run` without `--script`), `slider V` overrides the console's slider until `slider off`. When the script ends the
console's own slider counts again. The models example's `tests/stereo.nnhs` uses both:

```text
slider 1
expect "eyes: 2" 10        # the game logs how many pictures it drew
wait 10
screenshot stereo eyes     # stereo-top.png, stereo-top-right.png, stereo-bottom.png
slider off
```

A game built before the slider existed rejects the command (`the console rejected the message (unknown type)`).

## The system language

`language CODE` (a well-formed tag: `ja`, `pt-BR`, `zh-Hant`) sets what `nnh::system::language()` returns from that
frame on (spec 029), on the console and in the simulator; `language off` gives the system's back. Like the slider, a
script owns it: **`en` from its start** until its first `language` command, so results don't depend on the console's
settings. A game that follows the system language ([localization](../api/localization.md)) switches at the next frame
and logs `language ja (system)`. The localization example's `tests/check.nnhs` walks pt-BR, de, ru and ja this way.

On the console, a screenshot pair takes about **0.6–0.8 s** over Wi-Fi (Old 3DS XL): the game stops for
that time, and its log says `remote: frame <n> sent a screenshot in <ms> ms; its time is not
representative`. Take screenshots outside anything you measure; in the simulator they cost a few
milliseconds.

## Trying commands by hand

Without `--script`, type commands in `nnh`'s terminal while the game runs: `tap A`, `touch 160 120`,
`screenshot here`, `slider 0.5`, `expect "pressed" 30`, `help` for the list, `end` to give the game its controls back.
Handy for reproducing a bug step by step; for anything you want to repeat, write a script. Typed commands share the
controls: what you press on the console still counts, merged with them.

## For agents and CI

- `--no-build` runs what is already built; `NNH_PRESET=<preset>` picks another preset of the same system:

  ```sh
  NNH_PRESET=pc-debug build/pc-debug/tools/nnh/nnh run input --pc --no-build --script examples/input/tests/remote.nnhs
  ```

- In the simulator without a display, set `SDL_VIDEO_DRIVER=offscreen`. The ctest `nnh_script_pc_smoke`
  (`tools/nnh/tests/script_pc_smoke.cmake`) runs the input script this way in every Debug/Development PC
  preset and checks the exit code and the four PNG files. `hierarchy_check_script_resting_pad_pc` plays the
  hierarchy script with `NNH_SIM_INPUT="circle=0.0641025641,0.0897435897"` (an Old 3DS pad resting at (10, 14) of
  156, past the dead zone): it must still pass, with the "not at rest" note.
- On the console, an agent runs the same command as a person; the maintainer only opens the Homebrew
  Launcher and presses Y. The log and the screenshots in `build/logs/` are the report.

## How it works

`nnh` uses the connection the game already opens for its log: it sends small fixed-size messages
stamped with a frame, and the game answers inside its log stream (decision D28,
[debugging](../architecture/debugging.md#driving-a-game-from-the-pc)). Only the PC the game connected to
can drive it, and only Debug/Development builds have the code
([security](../architecture/security.md)).

## Live values in scripts (spec 030)

`set NAME VALUE` (a colour quoted: `"#FFC040"`), `reset NAME|GROUP|all`, `action NAME`, `dump`, `pause`, `resume`,
`step [N]` and `expect-value NAME OP VALUE [~ E] [within N]` ([api/live.md](../api/live.md#nnh-and-scripts)). A script
that uses them asks for the game's catalogue first and starts its clock when it arrives; a name is checked when its
part of the script is sent. Between `pause` (or `step`) and `resume`, commands that move the clock are errors and a
screenshot takes no frame. Examples: `examples/live/tests/check.nnhs`, `panel.nnhs`, `keep.nnhs` + `keep_second.nnhs`
(run with `--tunes`).

## Hot reload in scripts (spec 031)

A script can swap an asset's source for a test fixture and check the reload, in the simulator and on the console:

```
fail-on "] E "
wait 30
swap-asset assets/art/hero.png fixtures/hero_swapped.png   # SOURCE: the game's folder; FIXTURE: the script's
expect-reload art/hero                                     # applied, and the batch has art/hero (path or load name)
screenshot after
restore-asset assets/art/hero.png                          # optional: every swap is put back at the end anyway
end
```

`swap-asset` (held) pauses the game at the script's frame F, builds the asset target, sends the batch with its Apply and a
Resume both stamped F, and the script goes on at F + 1: the batch lands on the same frame on every system. `swap-asset ...
unheld` only copies and sends (for timings); `expect-reload [PATH] [refused] [within N]` checks the last swap — `refused`
expects a refusal, `within N` waits up to N frames for an unheld one. A swapped source outside the assets folder (e.g. a
`DATA_SCHEMA` source) gets Ninja's dry run: a change that needs a rebuild is refused. None of them may be used while the
script has the game paused. Every swapped source is put back byte for byte at the end (pass, fail or Ctrl+C) and the
asset target rebuilt; an interrupted run leaves a journal in `build/logs/reload-backup/<game>/`, and `nnh run` then
refuses to start until `nnh restore-assets <game>` puts the originals back. Examples:
[`examples/data/tests/reload.nnhs`](../../examples/data/tests/reload.nnhs),
[`examples/sprites/tests/reload.nnhs`](../../examples/sprites/tests/reload.nnhs).
