# Spec 007: Assets — build pipeline and romfs

Status: draft
Author: NNHGE maintainers · Date: 2026-11-18
Depends on: [spec 002](../002-core-foundation/spec.md) (`nnh::fs`, memory, `Result`),
[spec 004](../004-2d-graphics/spec.md) (textures, `IMAGES`). Implements ROADMAP M3 and decision D6.

## Problem and goal

Today a game's images are embedded in the executable (`nnh_add_game(... IMAGES ...)`, decision D19):
every image is loaded into memory with the program, the `.3dsx` grows with each picture, and a game can
only ship images — no level files, dialogue, tables or other data it reads at run time. After this spec
a game keeps its content in an **assets folder** next to its code. The build converts each file once
into the platform's format (only what changed), packs everything into the **romfs** inside the `.3dsx`
on the 3DS and into a folder next to the executable on PC, and the game loads textures and reads data
files **by path, on demand**, with the same code on every system.

## User stories

- As a game developer, I want to drop PNGs and data files into `assets/` and use them from the game,
  without listing each file or embedding it in the program.
- As a game developer, I want `load_texture("characters/hero")` to work the same in the simulator and
  on the console, and to load only when I ask.
- As a game developer, I want to read my own files (levels, text, tables) as bytes, so that I can store
  game data outside the code.
- As a game developer, I want a mistake in an asset (bad image size, wrong letter case in a path) to
  fail on PC the same way it would on the console.
- As a contributor, I want editing one asset to rebuild only that asset.

## Functional requirements

- **FR-1 Assets folder.** A game declares one folder (e.g. `nnh_add_game(... ASSETS assets)`); every
  file under it becomes an asset named by its path relative to the folder, with `/` separators. Files
  added or removed in the folder are picked up by the next build.
- **FR-2 Conversion per type.** PNG images become platform textures (3DS: tex3ds `.t3x`; PC: the PNG),
  checked against the texture rules at build time (spec 004 FR-3, the build fails naming the file).
  Other files are copied unchanged. Converting only re-runs for assets whose content changed.
- **FR-3 Packaging.** 3DS: all converted assets go into the romfs of the `.3dsx` (one file to send, read
  from the SD card on demand, not loaded with the program). PC: a folder next to the executable.
- **FR-4 Textures from assets.** `nnh::load_texture("characters/hero")` loads `assets/characters/hero.png`
  on demand. Existing games using `IMAGES` keep working (see open questions for the transition).
- **FR-5 Reading data files.** The game can get the size of an asset and read it into its own buffer,
  or load it into engine-allocated memory released automatically (RAII), with `Result` errors:
  missing asset, buffer too small, read failure.
- **FR-6 Paths are strict and portable.** Asset paths are relative, `/`-separated, without `..`, and
  **case-sensitive on every system** (romfs is; the simulator must not hide a case mistake that works
  only on a case-insensitive PC file system). Invalid paths are rejected without touching the disk.
- **FR-7 Read-only and separate from saves.** Assets can't be written at run time; `nnh::fs` (the game's
  data folder, saves) is unchanged and never mixed with assets.
- **FR-8 Testable without hardware.** The asset source is part of the platform contract, so the fake
  platform serves in-memory assets for unit tests of the core logic (path rules, lookup, loading,
  errors); the build steps have script tests like the image check.

## Acceptance criteria

- **AC-1** Unit tests cover asset path validation (including case), lookup and reads through the fake
  platform, texture loading from assets, and every error (missing, too small, invalid path).
- **AC-2** `examples/sprites` moves its images to `assets/` and adds a data file it reads (e.g. a level
  layout); its screenshots stay identical; `hello` still has no assets and is unchanged.
- **AC-3** On the 3DS, `examples/sprites` loads its textures and data from the romfs (maintainer); the
  `.3dsx` no longer embeds the images in the program.
- **AC-4** Editing one asset rebuilds only that asset and the package; build budgets still met.
- **AC-5** `docs/api/assets.md` (new), `texture.md`, `cmake.md`, `devkitpro-3ds.md`, decision D19 updated
  (superseded or narrowed); ROADMAP M3.

## Non-goals

- Audio (M4), game fonts (follow-up of spec 005), texture atlases/packing, mipmaps, compression formats
  beyond what tex3ds does today.
- Hot reload of assets in the simulator (later), mods/patch folders, streaming large files.
- `.cia` packaging (M5) — the romfs layout should not block it.
- A content-addressed cache shared between build presets (see open questions).

## Constraints

- Constitution I: same API and paths everywhere; no `#ifdef` in games.
- Constitution II / D4: the simulator enforces romfs rules (case-sensitive paths, texture rules).
- Constitution III: incremental conversion; a game without assets builds as fast as today.
- Constitution V: no per-frame allocation; loading is a load-time operation.
- Constitution X: asset paths confined and validated; read-only.

## Open questions

- [ ] **Build tool** — keep the CMake-script pipeline grown from `IMAGES` (one build step per asset,
  incremental by file timestamps, no new tool to compile; proposed for now), or write the C++ `assetc`
  host tool of decision D10 now (content-hash cache, room for heavier conversions later)?
- [ ] **`IMAGES` after this spec** — keep it as a small-games option (embedded, no romfs), or deprecate it
  and convert the examples (proposed: keep it working, documented as "embedded images", and make
  `ASSETS` the recommended way)?
- [ ] **Texture names** — `load_texture` takes the asset path without extension (`"characters/hero"`,
  proposed), with or without the extension, and should `IMAGES` names (file name only) stay valid?
- [ ] **Data API** — `assets::read(path, buffer, capacity)` + `assets::size(path)` only (caller's memory),
  or also `assets::load(path)` returning an RAII buffer from the `assets` memory tag (proposed: both)?
- [ ] **PC packaging** — a plain folder next to the executable (proposed: easy to inspect and fast to
  update) or one pack file like the romfs?
