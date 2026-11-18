# Spec 009: Packaging — icon, metadata and installable `.cia`

Status: in progress (approved 2026-11-18)
Author: NNHGE maintainers · Date: 2026-11-18
Depends on: [spec 007](../007-assets-and-romfs/spec.md) (romfs), [spec 008](../008-audio/spec.md) (WAV checks).
Implements ROADMAP M5.

## Problem and goal

A game built with NNHGE today is a `.3dsx`: it runs from the Homebrew Launcher, with the title and
author from `nnh_add_game()` and, unless the game passes `ICON`, devkitPro's generic icon. Players who
want it on the HOME Menu like any other app need a `.cia`, which the engine can't produce: it takes two
tools devkitPro doesn't ship (`makerom`, `bannertool`), an RSF description, a unique ID, a banner image
and a banner sound, and several rules that are easy to get wrong. After this spec a game declares its
identity once in `nnh_add_game()` and the build produces a `.3dsx` with a proper icon and, when asked, an
installable `.cia` with a banner and a short jingle, both from the same code and assets.

## User stories

- As a game developer, I want my game's name, author, version and icon declared once and used everywhere.
- As a game developer, I want a `.cia` my players install with FBI and start from the HOME Menu, with my
  banner and sound, without learning makerom's RSF format.
- As a game developer, I want a mistake in the package metadata (an ID outside the homebrew range, a
  banner of the wrong size, a title too long) to fail the build with a message naming the field.
- As a game developer, I want my game to behave the same as `.3dsx` and as `.cia`: assets, saves, exit.
- As a contributor, I want builds that don't make a `.cia` to stay as fast as today and never download
  anything.

## Functional requirements

- **FR-1 One identity.** `nnh_add_game()` takes `TITLE`, `DESCRIPTION`, `AUTHOR`, `ICON` (exists) and a
  new `VERSION` (`major.minor.micro`); the `.3dsx` SMDH, the `.cia` and the PC simulator use them.
- **FR-2 Icon.** `ICON` is a 48×48 PNG, checked at build time (the build fails naming the file
  otherwise). Without `ICON`, the game gets a default NNHGE icon (original art shipped with the engine)
  instead of devkitPro's.
- **FR-3 `.cia` on request.** With `CIA` options in `nnh_add_game()` — at least a `UNIQUE_ID` — a target
  `<game>_cia` builds `<game>.cia` from the same ELF and romfs: SMDH from FR-1, a banner from a 256×128
  PNG (`BANNER`) and a sound (`BANNER_AUDIO`, WAV up to 3 s), both with NNHGE defaults when omitted,
  product code and title version from the metadata.
- **FR-4 Same behaviour.** As `.cia`, the game reads its assets from its romfs, keeps its data folder at
  `sdmc:/3ds/<data_folder>/`, and exits back to the HOME Menu, exactly like the `.3dsx`.
- **FR-5 Checked metadata.** The build fails, naming the field, for: a unique ID outside the homebrew
  range (`0x00300`–`0xF7FFF`), a malformed product code or version, a title/description/author longer
  than the SMDH allows, a banner that isn't a 256×128 PNG, a banner sound that isn't a WAV of at most
  3 s. The ID and title are printed by the build so they can be checked against other homebrew.
- **FR-6 Tools.** `makerom` and `bannertool` are obtained by the build only when a `.cia` is requested,
  at pinned versions with checksums, for Windows and Linux; a clear error explains how to provide them
  otherwise (macOS, offline). Nothing is downloaded for builds without `.cia`.
- **FR-7 Outputs in one place.** Each game's distributable files (`.3dsx`, `.cia`) are gathered in
  `build/<preset>/dist/<game>/`.

## Acceptance criteria

- **AC-1** Every example gets the default NNHGE icon (or its own); `hello` on the 3DS shows it in the
  Homebrew Launcher (maintainer).
- **AC-2** One example (`examples/audio`: it uses assets, music and saves nothing — or another the plan
  picks) builds a `.cia` with its own icon, banner and jingle; the maintainer installs it with FBI, sees
  the banner and hears the jingle on the HOME Menu, plays it (assets load, sound works) and exits to the
  HOME Menu; uninstalling works.
- **AC-3** Script tests (ctest) for every metadata check of FR-2 and FR-5, with fixtures.
- **AC-4** Configuring and building without `CIA` downloads nothing and takes the same time as before
  (measured); `hello.3dsx` keeps its size apart from the icon.
- **AC-5** New guide `docs/guides/packaging.md` ("Distributing your game": `.3dsx` vs `.cia`, choosing an
  ID, installing with FBI); `cmake.md`, `devkitpro-3ds.md`, `REFERENCES.md`, ROADMAP M5 updated.

## Non-goals

- Signing or encryption for retail consoles, eShop/cartridge (`.3ds`) images, update or DLC titles,
  e-manuals, 3D (CGFX) banners — homebrew `.cia` only, installed on consoles with custom firmware.
- Other consoles (Switch `.nro`/`.nsp`, Wii U) — later milestones reuse the identity of FR-1.
- An `nnh package` command-line tool (see open questions).
- Uploading or publishing releases.

## Constraints

- Constitution I: game code doesn't change between `.3dsx` and `.cia`.
- Constitution III: builds without `.cia` unchanged; the `.cia` step only reruns when its inputs change.
- Constitution IX: makerom and bannertool are MIT-licensed build tools, not shipped in games; the default
  icon, banner and jingle are original NNHGE assets; no Nintendo SDK or keys.
- Constitution X: downloaded tools pinned by URL and SHA-256, never run unpinned.

## Open questions

Answered by the maintainer (2026-11-18): the proposals, all accepted.

- [x] **How to get the tools** — proposal: download the official prebuilt release binaries (makerom
  v0.19.0 from 3DSGuy/Project_CTR; bannertool 1.2.3 from carstene1ns/3ds-bannertool, the maintained fork)
  pinned by SHA-256 into the build folder, only when a `.cia` is requested. Alternatives: build them from
  source at configure time (slower, needs their dependencies), or require them on `PATH`.
- [x] **Which builds make a `.cia`** — proposal: only games with `CIA UNIQUE_ID ...`, and only through
  the `<game>_cia` target (not on every build), so day-to-day builds stay fast.
- [x] **Default banner and jingle** — proposal: a fixed NNHGE banner (256×128, the engine's logo, no
  title) and a short original jingle; a game overrides them with `BANNER`/`BANNER_AUDIO`.
- [x] **New 3DS / memory mode** — proposal: an optional `CIA ... SYSTEM_MODE` (e.g. more app memory on
  Old 3DS, the New 3DS's 804 MHz/L2 cache) exposed later; the first version uses the defaults.
- [x] **`nnh package`** — proposal: not now. The CMake target plus `dist/` covers it; the CLI comes with
  NNH Link (M6), which needs it anyway.
