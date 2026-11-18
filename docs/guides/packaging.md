# Distributing your game

How to give players a 3DS game made with NNHGE (spec [009](../../specs/009-packaging/spec.md)). Build
details: [cmake.md](../tech/cmake.md#common-changes).

## `.3dsx` or `.cia`?

| | `.3dsx` | `.cia` |
|-|---------|--------|
| Runs from | the Homebrew Launcher | the HOME Menu, like any app |
| Installing | copy it to `sdmc:/3ds/` | install it with FBI (custom firmware) |
| Banner and sound on the HOME Menu | no (icon in the launcher only) | yes |
| Your game's code and assets | the same | the same |

Both come from the same build, with the same code, assets (romfs) and saves (`sdmc:/3ds/<data folder>/`).
Offer both: the `.3dsx` works everywhere the Homebrew Launcher does, the `.cia` is nicer to keep.

## Your game's identity

Declare it once in `nnh_add_game()`:

```cmake
nnh_add_game(mygame
    SOURCES main.cpp
    ASSETS assets
    TITLE "My Game"                     # at most 64 characters; also the PC window title
    DESCRIPTION "A short line about it" # at most 128
    AUTHOR "You"                        # at most 64
    VERSION 1.0.0                       # major.minor.micro, at most 63.63.15
    ICON icon.png                       # 48x48 PNG (the NNHGE icon if omitted)
    CIA UNIQUE_ID 0x4F2B1               # turns the .cia on (see below)
        BANNER banner.png               # 256x128 PNG (NNHGE banner if omitted)
        BANNER_AUDIO banner.wav)        # 16-bit WAV, at most 3 s (NNHGE jingle if omitted)
```

Mistakes fail the build and name the field: `TITLE is 70 characters: at most 64`,
`banner.png is 200x120: the banner must be a 256x128 PNG`.

### Choosing a unique ID

The unique ID tells the console which installed title is yours: two `.cia` files with the same ID
replace each other. Pick a **random** number in the homebrew range `0x00300`–`0xF7FFF` (for example
with `python -c "import random; print(hex(random.randint(0x300, 0xF7FFF)))"`), keep it for the life of
your game, and check community lists of homebrew title IDs before you publish. The build prints the ID
and the product code (default `CTR-H-` + your target's first 4 letters; set `PRODUCT_CODE` to change it).

## Building

```sh
scripts/dkp.sh cmake --build --preset 3ds-release --target mygame_cia   # just the .cia
scripts/dkp.sh cmake --build --preset 3ds-release --target dist         # every game's .3dsx and .cia
```

The files land in `build/3ds-release/dist/<game>/`. The first `.cia` build downloads makerom and
bannertool (pinned, a few MB, into `build/<preset>/_tools/`); normal builds never do. Without network, or
on macOS, build or download them yourself and configure with
`-DNNH_MAKEROM=<path> -DNNH_BANNERTOOL=<path>`.

Use a **Release** build for players (`3ds-release`): Development builds print logs and wait for 3dslink.

## Installing

**Over the network, with FBI** (no SD card swapping):

1. On the console: FBI → **Remote Install** → **Receive URLs over the network**. It shows the console's
   address, like `192.168.10.52:5000`.
2. On the PC (same network):
   ```sh
   python -I scripts/fbi_install.py 192.168.10.52 build/3ds-release/dist/mygame/mygame.cia
   ```
   The script serves the file from the PC, tells FBI where to download it, and stops when FBI is done.
3. Confirm the install on the console, return to the HOME Menu.

**From the SD card**: copy the `.cia` to the card (e.g. `sdmc:/cia/`) and install it with FBI.

`.3dsx`: copy it to `sdmc:/3ds/` and start it from the Homebrew Launcher; during development, send it
with 3dslink instead ([getting-started.md](getting-started.md)).

Uninstall a `.cia` with FBI (Titles) or System Settings → Data Management. Saves in
`sdmc:/3ds/<data folder>/` stay on the card.

## What the `.cia` contains

The same ELF as the `.3dsx`; the SMDH (title, description, author, icon); a banner made by bannertool from
`BANNER` and `BANNER_AUDIO`; your assets folder as the romfs; and NNHGE's ROM spec
(`cmake/3ds/nnhge.rsf`): 64 MB of application memory, 268 MHz, the system services the engine uses. It
is a homebrew `.cia`: unencrypted, signed with makerom's test keys, for consoles with custom firmware.
