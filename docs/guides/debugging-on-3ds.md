# Debugging on the 3DS

Tools from the simplest to the most powerful. Build with a `3ds-dev` (or `3ds-debug`) preset: logs,
asserts and the overlay are compiled out of Release builds.

```sh
scripts/dkp.sh cmake --preset 3ds-dev && scripts/dkp.sh cmake --build --preset 3ds-dev
```

## 1. Logs on your PC (3dslink)

1. On the console: Homebrew Launcher → **Y** (netloader).
2. On the PC, send the game **with `-s`**, so 3dslink stays connected and prints what the game logs:
   ```sh
   /c/devkitPro/tools/bin/3dslink.exe -s build/3ds-dev/examples/foundation/foundation.3dsx -a <console-ip>
   ```
   (PowerShell: `C:\devkitPro\tools\bin\3dslink.exe -s build\3ds-dev\examples\foundation\foundation.3dsx -a <console-ip>`)
3. Every `NNH_LOG_*` line appears in the terminal. The same lines are written to
   `sdmc:/3ds/<data folder>/log.txt` (the previous run: `log.prev.txt`).

## 2. On-screen tools

- **Debug overlay**: hold **SELECT** and press **L** — FPS, frame-time graph, memory per region, slowest
  profile scopes ([api/debug.md](../api/debug.md), [api/profile.md](../api/profile.md)).
- **Debug text**: `nnh::debug::print(...)` anywhere in your frame.

## 3. Crashes

A failed `NNH_ASSERT`/`NNH_VERIFY` or an `NNH_PANIC` shows the **crash screen** on both screens and
writes `sdmc:/3ds/<data folder>/crash.txt` (previous one: `crash.prev.txt`) with the message, location,
registers (for CPU faults) and the last 64 log lines. Press START to return to the Homebrew Launcher.
Read the report on the PC by taking the SD card out, or over FTP (e.g. ftpd).

**CPU faults** (bad pointer, misaligned access, undefined instruction) reach the engine's crash screen
only when the console reports them to the game. In Luma3DS: hold **SELECT** while powering on →
enable **"Set developer UNITINFO"** (development only; turn it off for normal use). Without it, Luma3DS
shows its own exception screen and saves a dump in `sdmc:/luma/dumps/arm11/` — open it on the PC with
Luma3DS's `luma3ds_exception_dump_parser`. Try it with the foundation example: hold L + R and press A
(an assert), or add a deliberate null write to test CPU faults.

To turn a crash address into a source line: `arm-none-eabi-addr2line -e <game>.elf -f -C <pc>`
(from `C:\devkitPro\devkitARM\bin`; use the `.elf` next to the `.3dsx`).

## 4. GDB on the console (step through code)

Luma3DS includes a GDB stub.

1. Build `3ds-debug` (no optimization, full symbols).
2. On the console: open the Rosalina menu (**L + Down + SELECT**) → **Debugger options** → **Enable
   debugger**, then start the game from the Homebrew Launcher. Rosalina shows the port to use for the
   game's process.
3. On the PC:
   ```sh
   /c/devkitPro/devkitARM/bin/arm-none-eabi-gdb build/3ds-debug/examples/foundation/foundation.elf
   (gdb) target remote <console-ip>:<port>
   (gdb) break main
   (gdb) continue
   ```
4. Use `bt` for the call stack, `info registers`, `print <expr>`, `next`/`step`. Disconnect with
   `detach`; disable the debugger in Rosalina when done (it slows the system and keeps a port open).

The ARM11 is slow and the link is Wi-Fi: breakpoints work well, single-stepping through hot loops is slow.

## 5. Reproduce on the PC first

Most bugs reproduce in the simulator, where any debugger works and the tools are richer (pause with
**Pause**, step with **F10**, slow motion **F11**, screenshots **F12**). Console-only bugs usually come from
memory (linear/VRAM budgets — watch the overlay), texture sizes, uninitialized memory or timing.

## Further reading

- [docs/architecture/debugging.md](../architecture/debugging.md), [docs/architecture/errors.md](../architecture/errors.md)
- [Luma3DS](https://github.com/LumaTeam/Luma3DS) (Rosalina, GDB stub, exception dumps)
