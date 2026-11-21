# NNH Link: build and run from the console

**NNH Link** is a 3DS app that drives your PC: you pick a game on the console, the PC builds it, and
the console runs it. The game's log appears on the PC, and when you close the game you are back in
NNH Link. You don't touch the PC between tries (spec [011](../../specs/011-nnh-link/spec.md)).

```text
 3DS: NNH Link                    PC: nnh serve
 pick the PC, pick "audio"  ───>  builds audio (Development)
 build output, progress     <───  output lines, then audio.3dsx
 runs audio                 ───>  its log in the terminal (and build/logs/)
 close audio → NNH Link again
```

## What you need

- A 3DS with **Luma3DS** and the **Homebrew Launcher** (the usual homebrew setup), on the same network
  as the PC.
- On the PC, this repository built with a PC preset (`cmake --build --preset pc-debug`) and the 3DS
  toolchain set up ([getting started](getting-started.md)).
- **NNH Link on the SD card**: it is built with the games (`scripts/dkp.sh cmake --build --preset
  3ds-release` → `build/3ds-release/apps/nnh-link/nnh-link.3dsx`). Copy it to `sdmc:/3ds/nnh-link.3dsx`
  (it can be anywhere under `sdmc:/3ds/`). `nnh run nnh-link` also works: the Homebrew Launcher's
  netloader saves what it receives under `sdmc:/3ds/`.

## Every day

1. On the PC, in the repository: `build/pc-debug/tools/nnh/nnh serve` (leave it open). The first time,
   **Windows Firewall** asks about `nnh.exe`: allow it on private networks (TCP/UDP 17492, and 17491 for
   the log).
2. On the console: Homebrew Launcher → **NNH Link**. Your PC appears in the list by its name (NNH Link
   asks the network again every 2 seconds, so the order of these two steps doesn't matter). The last PC
   you used is always listed, by its address until it answers with its name, so it can be picked even
   when the search finds nothing.
3. Pick it (**A**). The first time, NNH Link asks for the **6-digit code** that `nnh serve` shows: type
   it with the touch screen (or the D-Pad and **A**). Both sides remember the pairing; next time there
   is no code.
4. Pick a game (**A**). **X** switches between **Development** (the default: the game sends its log to
   the PC) and **Release** builds. The build output scrolls on the top screen, then the game arrives.
5. The game starts. Its log shows in the `nnh serve` terminal, coloured, and is saved in `build/logs/`.
   When you close the game, NNH Link opens again with the same PC and game selected. Edit, save, press
   **A** again.

| Screen | Buttons |
|--------|---------|
| Servers | **A** connect · **X** search again · **Y** forget the pairing with that PC · **START** exit · last row: type an address |
| Code / address | touch the pad, or D-Pad + **A** · **Y** erase · **B** back |
| Games | **A** build and run · **X** Development/Release · **B** other servers |
| Building | **B** cancel (the PC stops the build at its next output line) |
| Message | **A**/**B** back |

The touch screen works everywhere: tap a row, a key, or the button at the bottom.

## When something goes wrong

NNH Link shows every failure with a message, and nothing waits forever.

| You see | What to do |
|---------|------------|
| No servers in the list | Is `nnh serve` running? Allow it in the firewall. Some routers block broadcasts between Wi-Fi and cable: use the last row, **Enter an address**, with the PC's IP (once only: from then on that PC is listed by its address). |
| `can't reach …: is nnh serve running there?` | Same as above; check the address. |
| `the PC refused: …` (code) | Wrong code or more than 2 minutes old: `nnh serve` shows a new one; try again. After 5 wrong codes, pairing is blocked until `nnh serve` restarts. |
| It asks for a code again | The PC forgot the console (`nnh unpair`, or a new user account), or you pressed **Y**: pair again with the code `nnh serve` shows. |
| `the build failed` and error lines | The compiler's last 20 lines, also in the terminal. Fix, then **A** again. |
| `not enough space on the SD card` | Free some space; NNH Link keeps 1 MB free. |
| `the PC stopped answering` / `went away` | The PC or the network dropped; the partial file is deleted. |
| The game crashed | The crash screen, then the Homebrew Launcher (not NNH Link); the log is in `build/logs/` ([debugging](debugging-on-3ds.md)). |
| Release game: no log | Expected: Release builds have no log. |

## Where things go

| Where | What |
|-------|------|
| `sdmc:/3ds/nnh/<game>.3dsx` | the last received copy of each game (the Homebrew Launcher lists them too) |
| `sdmc:/3ds/nnh/link.sav` | this console's id, the PCs it paired with (up to 16, with their secrets), the last PC and game |
| PC: `nnh-pairs.ini` next to `nnh.ini` (`%APPDATA%/nnhge/`, `~/.config/nnhge/`) | the consoles paired with this PC |

Forgetting: **Y** on the servers screen (console side) and `nnh unpair` (every console, PC side).

## Safety

`nnh serve` builds and sends programs, so both sides check each other (decision D24,
[security.md](../architecture/security.md)):

- **Pairing**: a code shown on the PC, valid for 2 minutes and one pairing; 5 wrong codes block pairing.
  Both sides then keep a 256-bit secret; the code is never sent, only an HMAC of it.
- **Every connection**: the PC proves first that it knows the secret, then the console. A console only
  accepts programs from a PC it paired with; a PC only builds for consoles it paired with. Every message
  then carries a MAC and a sequence number: a changed, replayed or injected message ends the connection.
- **The game file** is checked against its SHA-256 and size before it replaces the old copy.
- **Local network only**: both sides refuse addresses outside private networks.
- **Not encrypted**: someone on your network can see the traffic (game names, build output). Someone who
  records the traffic *while you pair* could guess the 6-digit code offline and learn the secret:
  pair on a network you trust, and pair again (**Y**, `nnh unpair`) if in doubt.
- The SD card has no access control: any homebrew can read `link.sav`.

## In the PC simulator

NNH Link also runs in the simulator (`build/pc-debug/apps/nnh-link/nnh-link`): finding, pairing, building
and receiving work; starting the game doesn't (it says where the file went). To see every screen
without a server:

```sh
NNH_LINK_DEMO=games NNH_EXIT_AFTER_FRAMES=20 NNH_SCREENSHOT=link.png build/pc-debug/apps/nnh-link/nnh-link
```

Screens: `servers`, `address`, `code`, `games`, `building`, `receiving`, `failed`, `done`.

## How it works

NNH Link is an NNHGE app (`apps/nnh-link`): one scene, the engine's text and touch input, `nnh::save`,
and the 3DS extensions `<nnh/3ds/network.hpp>` and `<nnh/3ds/launch.hpp>` ([api](../api/3ds-extensions.md)).
It and `nnh serve` share the `link/` library: discovery on UDP 17492, the protocol on TCP 17492, the game's
log on TCP 17491 as with `nnh run`. The game is started through Luma3DS's homebrew loader, as the
Homebrew Launcher does, with `--nnh-return=<NNH Link>` so the engine starts NNH Link again at exit.
Details: [plan 011](../../specs/011-nnh-link/plan.md); the PC side: [the nnh tool](nnh-tool.md#serving-nnh-link-nnh-serve).
