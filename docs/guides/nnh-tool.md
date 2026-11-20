# The `nnh` tool

`nnh` builds a game, runs it on the console with its log on your screen, and installs `.cia` files —
one command each (spec [010](../../specs/010-nnh-cli-run/spec.md)). `nnh serve` lets the console drive
instead, through the NNH Link app (spec [011](../../specs/011-nnh-link/spec.md), in progress). It is built with the PC presets:
after `cmake --build --preset pc-debug` it is `build/pc-debug/tools/nnh/nnh(.exe)`. Run it from anywhere
inside the repository.

```sh
nnh run hello                 # build for the 3DS, send to the console, show the log
nnh run hello --pc            # the same game in the simulator
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
- Ctrl+C stops waiting for the log; the game keeps running on the console.
- The first time, **Windows Firewall** may ask about `nnh.exe`: allow it on private networks. The game's
  log comes back on TCP port 17491 and the console's discovery answer on UDP 17491 (`nnh serve` also uses
  TCP and UDP 17492).

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

## How it works

`nnh` speaks the Homebrew Launcher netloader's protocol (the one devkitPro's `3dslink` uses) and FBI's
URL protocol with its own small code: no compression library and no devkitPro tool on the PATH needed
(decision D23). `nnh serve` uses NNH Link's own protocol from the `link/` library (decision D24, [plan
011](../../specs/011-nnh-link/plan.md)). Details: [plan 010](../../specs/010-nnh-cli-run/plan.md); code: `tools/nnh/`. On Windows,
3DS builds run through devkitPro's MSYS2 shell (`DEVKITPRO_MSYS2` overrides its location), like
`scripts/dkp.sh`.
