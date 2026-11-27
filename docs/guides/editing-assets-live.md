# Editing assets while the game runs

> Spec 031 (implemented 2026-11-27): images, Tiled maps, data files, string tables, fonts, sounds, music, meshes and
> sprite libraries reload, and tunables on reloaded data follow the file. Checked in the simulator, on Linux and in a
> first console run; a second console run (sprite libraries, music, font, timing) is pending.

Run a Debug or Development build with `nnh run`, change a file in the game's assets folder, save, and see the change in
the running game — in the simulator or on the console — without a restart. This page is the walk-through; the reference
is [assets.md](../api/assets.md#hot-reload) (what changes behind your handles, the queries, refusals, memory) and
[nnh-tool.md](nnh-tool.md#hot-reload-of-assets) (what `nnh` prints, typed commands).

## The loop

```sh
cmake --build --preset pc-debug --target sprites                          # once
NNH_PRESET=pc-debug build/pc-debug/tools/nnh/nnh run sprites --pc --no-build
# nnh: hot reload on: saving a file in .../examples/sprites/assets reloads it
```

Open `examples/sprites/assets/art/hero.png` in an image editor, change a few pixels, save. `nnh` waits until nothing
changed for 250 ms, rebuilds only the game's asset target (the same converters as a normal build), sends the changed
converted files as one batch and prints:

```
nnh: reload 1: 1 files, 0.3 KB (build 0.88 s)
nnh: reload 1 applied at frame 1234, 1.03 s after the save: art/hero
```

On the console it's the same: `nnh run sprites` (Homebrew Launcher → Y). The files travel over the log connection and
are kept in `.nnh_reload/` of the game's data folder on the SD card; the romfs never changes.

## What reloads by itself

Handles keep pointing at the same slot, so these need no code: textures (and a tilemap's tilesets, UI images and mesh
textures that use them), fonts, sounds (voices playing them stop with `SoundFinished`), music (it goes on at the same
sample), meshes from `load_mesh`, sprite libraries from `load_sprite_library` (each animator goes on with the clip of the
same name), and the current language's strings (`loc::serial()` changes; UI texts resolve again). A tunable on a field
you reload from a data file shows the file's value ([live.md](../api/live.md#tunables)).

## What you reload yourself

Your own copies — structs filled by `data::load`, a `Tilemap`, bytes from `assets::load` — stay as they are. Reload them
when the batch is announced, in a scene's `update()` or a subscription to `nnh::AssetsReloaded`:

```cpp
if (nnh::assets::reloaded("levels/room")) map = std::move(nnh::load_tilemap("levels/room").value());
if (nnh::assets::reloaded("enemies/goblin")) (void)nnh::data::load("enemies/goblin", goblin);
```

The script `examples/data/tests/reload.nnhs` swaps `enemies/goblin.enemy` for a fixture and checks that the batch is
applied (`expect-reload enemies/goblin`), then restores it.

## Errors and refusals

- A conversion error (a broken map, an image that isn't a power of two) prints the converter's `file:line: reason` and
  `reload refused: conversion failed`; the game logs one warning and keeps what it had.
- A batch is all or nothing: if one file fails its loader's check, or the new content doesn't fit in memory, nothing of
  it applies (`W assets reload refused (frame N): <path>: <reason> (old content kept)`).
- A change that needs code — a `DATA_SCHEMA` struct, a language list — is refused with "rebuild and restart".

## Limits

- Debug and Development builds only; Release has no reload code and the queries are constants. Game logic must not
  depend on reloads.
- A game links the reload code only if it loads something reloadable (a texture, sound, music, font, mesh, sprite
  library, map, data asset or string table).
- During a batch, the old and new content of its assets are in memory at once.
- `nnh run --no-reload` turns watching off; scripts never watch, but can swap files themselves (`swap-asset`,
  `expect-reload`: [testing-on-the-console.md](testing-on-the-console.md#hot-reload-in-scripts-spec-031)). If `nnh` was
  killed during a script's swap, `nnh restore-assets <game>` puts the originals back.
