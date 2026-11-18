# The `nnh` tool

`nnh` builds a game, runs it on the console with its log on your screen, and installs `.cia` files —
one command each (spec [010](../../specs/010-nnh-cli-run/spec.md)). It is built with the PC presets:
after `cmake --build --preset pc-debug` it is `build/pc-debug/tools/nnh/nnh(.exe)`. Run it from anywhere
inside the repository.

```sh
nnh run hello                 # build for the 3DS, send to the console, show the log
nnh run hello --pc            # the same game in the simulator
nnh build audio --release     # just build (Release)
nnh install audio             # build the .cia and install it with FBI
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
- Ctrl+C stops waiting for the log; the game keeps running on the console.
- The first time, **Windows Firewall** may ask about `nnh.exe`: allow it on private networks. The game's
  log comes back on TCP port 17491 and the console's discovery answer on UDP 17491.

## Installing a `.cia`

1. On the 3DS: FBI → **Remote Install** → **Receive URLs over the network**.
2. On the PC: `nnh install <game>` (the game needs `CIA UNIQUE_ID ...` in its `nnh_add_game()`:
   [packaging.md](packaging.md)).

`nnh` builds the `.cia` (Release), serves it from your PC on a free port, gives FBI its address, and stops
when FBI is done. Accept the installation on the console. FBI has no discovery: `nnh` uses `--console` or
the last console used.

## Safety

`nnh` only talks to local network addresses (10.x, 172.16–31.x, 192.168.x, link-local) and refuses others.
It listens only while a run or an install needs it, and its file server answers only the file it is
installing.

## How it works

`nnh` speaks the Homebrew Launcher netloader's protocol (the one devkitPro's `3dslink` uses) and FBI's
URL protocol with its own small code: no compression library and no devkitPro tool on the PATH needed
(decision D23). Details: [plan 010](../../specs/010-nnh-cli-run/plan.md); code: `tools/nnh/`. On Windows,
3DS builds run through devkitPro's MSYS2 shell (`DEVKITPRO_MSYS2` overrides its location), like
`scripts/dkp.sh`.
