# Plan 031: Hot reload of assets

Status: approved 2026-11-26 (the maintainer, the eleven questions at the end as recommended); in progress (see "Implementation notes"); the spec was approved 2026-11-26 with every open question as recommended, and those answers
are this plan's decisions; eleven plan-level questions at the end)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decisions D53, D54 (proposed,
[decisions.md](../../docs/architecture/decisions.md))
Uses spec 007 (the assets folder, one build step per file, `.nnh_assets`, `<game>_assets`, romfs and the folder next to
the PC program), spec 010 and D23 (`nnh run`, the build through `cmake --build --preset`, the MSYS shell for the 3DS),
spec 013 and D28 (frame-stamped messages, in-band records, `.nnhs` scripts, segments and the timeline), spec 030 and
D51/D52 (the application point after `++frame_count`, pause and step in core, paused ticks that still read the remote,
`LiveState` records, the paused-section script rules), spec 002 (flavours, `kDevToolsEnabled`, `nnh::fs` confined to the
data folder, tagged memory, shutdown hooks), D47 (GPU-deferred frees), D27/D29/D31/D42/D49/D50 (the converters, the data
fingerprint, the sprite block and its serials, the mesh and font tables, the string chain). Built in one worktree (branch
`spec-031`) created from `main` **after spec 030 is merged**; no other spec is built in parallel.

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ⚠️→✅ | Pressure point: the spec (FR-5) lets the simulator read new outputs from its assets folder while the console receives them over the connection — two delivery paths. Answered (question 1): **one path on every system** — `nnh` sends every batch over D28's connection (the simulator's loopback socket too) and the game keeps received files in a Development-only folder of its data folder; the simulator then runs exactly the console's code (FR-21 by construction, the receiver exercised by every `--pc` script). The only platform work: two Development-only contract functions (a data-folder file as an asset stream, an asset stream another program may replace) in new files per platform |
| II 3DS constraint | ✅ | No RAM copy of a batch: chunks go from a 16 KB receive buffer straight to the SD card; ≈ 22 KB of Development-only memory (tag `reload`) allocated at the first batch; per-object path hashes in side tables (4 bytes per texture/sound/mesh/font/music slot, 16 bytes per loaded sprite library); peak = old + new content of the batch's assets only, refused cleanly when it doesn't fit (FR-16) |
| III Build speed | ⚠️→✅ | Pressure point: the edit loop's speed is the feature (AC-8: < 1 s simulator, < 3 s console), and a 3DS build goes through devkitPro's MSYS shell. Answered: `nnh` builds only `<game>_assets` (no compile, no link, no `.3dsx` repack), measures the pipeline at T0.2 and keeps one MSYS shell open for the session if its start-up costs more than 0.3 s (risk 2); public headers gain only `<nnh/build.hpp>` (`<nnh/assets.hpp>`, `<nnh/texture.hpp>`), measured at T2.9 |
| IV Event-driven | ✅ | One bus event, `AssetsReloaded` (Q5), published at the batch's frame and dispatched in the same frame; FR-13's state is queried; per-kind swaps are engine commands at one point of the frame; no per-asset callbacks |
| V No hidden costs | ✅ | No exceptions, RTTI or `virtual` (a kind is a struct of function pointers); allocations only at the first batch (receive buffer, overlay table) and in the frame that applies a batch; an idle frame costs one flag check (no walk over loaded assets) |
| VI Spec + test first | ✅ | The wire, the reader's payload mode, the receiver, the overlay, name matching, the batch engine and every kind's prepare/commit, the queries, the session glue, the apply point, the `nnh` snapshot, watcher, build runner, sender, controller and script commands are tested first on the fake platform or in `nnh_tool_tests`; the visible result by screenshots and scripts on PC and console |
| VII Small files | ✅ | Core: a `reload/` folder (≈ 11 sources ≤ 250 lines) and one `<kind>_reload.cpp` per kind in its own module; `nnh`: a `reload/` folder (≈ 7 sources) and two script files; no public header beyond `<nnh/assets.hpp>` (Q13) |
| VIII Documented API | ✅ | `docs/api/assets.md`, the reloaded kinds' API pages, a guide "editing assets while the game runs", the nnh tool and console-testing guides (stage 11) |
| IX Clean and legal | ✅ | Original art and sounds drawn or generated for the example; the example's font is the OFL subset spec 029 already ships (copied with its licence); no new third-party code |
| X Safe by default | ⚠️→✅ | Pressure point: the console **writes files received from the PC** — new for the engine. Answered: files only under `<data folder>/.nnh_reload/`, named by the game (`<n>.bin`, never by a received path), through `nnh::fs`'s validated paths; every transfer message decoded and bounded (sizes, order, CRC-32), every received file validated by its loader's own validator before anything changes; mini-fuzz and libFuzzer targets over the reader and receiver; Release has no code that reads or writes the folder |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure point: engine-owned content changes in a Debug/Development game without the game asking (spec's "Constitution pressure"). Answered: only while `nnh run` watches (`--no-reload` turns it off; scripts never watch, they swap explicitly), Release byte-identical (`hello` checked at every stage end), kinds linked only by games that load that kind, a game that ignores FR-13 keeps its copies |

## Design

### Overview

```
nnh (PC)       watch the source assets folder (polling, quiet period) ─► build <game>_assets ─► dry run of <game>:
               anything to compile? → refused "rebuild and restart"   ─► diff the outputs against what the game has
               ─► one batch: ReloadBegin, ReloadFile + chunks ×n, ReloadApply (frame-stamped)  [D28 connection]
game (Dev)     remote reader (payload mode) ─► receiver ─► .nnh_reload/<n>.bin on the data folder (SD / PC folder)
               ReloadApply due ─► at the application point of that frame (before 030's live point):
                 prepare every file (the kinds' loaders, validators, allocations; nothing visible changes)
                 ─► all prepared: commit (swap behind handles, new index and overlay, retire old content: D47)
                 ─► AssetsReloaded, reload_count()+1, reloaded(path), log line, ReloadApplied record
                 any failure: abandon every prepared item, ReloadRefused, one warning
later loads    assets:: and every loader read through the overlay first, then romfs / the PC assets folder
Release        none of it: reload_count() == 0, reloaded() false, no reload code, data, messages or strings
```

### Decision D53: hot reload — `nnh` watches and builds, batches travel over D28 into a Development-only overlay, the game prepares then commits a batch whole at one point of a frame

**Who does what.** `nnh` owns the source folder (Q2): it polls the game's assets folder, waits for a quiet period,
builds the game's asset target (Q3), asks Ninja whether the game's program itself would now rebuild (FR-14), diffs the
outputs against the content the game has (a per-path content hash mirror), and sends the changed outputs as one batch.
The game only receives, validates and applies. One delivery path on every system (question 1): the simulator receives
its batches over its loopback connection exactly as the console over Wi-Fi.

**Where received files live (Q4 (a)).** `<data folder>/.nnh_reload/<n>.bin`, `n` a per-session counter chosen by the
game (never a received name: asset paths allow characters, lengths and depths `nnh::fs` refuses). An in-memory
**overlay** maps an asset path to its current file. The folder is wiped at the session's first batch (leftovers of a
crashed run) and by a shutdown hook (Development only); nothing is ever read from a file the overlay doesn't name. The
asset index of the newest batch (or the original) answers `exists`/`size`/lookups; `assets::read/load`, `load_texture`
and every loader read an overlay path from its file, any other path from the romfs / PC folder as today.

**Two phases (FR-4, FR-16).** A **kind** is a struct of function pointers (`claims`, `validate`, `prepare`, `commit`,
`abandon`) registered by a module the first time it loads something in a Development build, so a game that never loads
fonts links no font reloader. At the batch's frame: every file is offered to every registered kind; a kind with loaded
objects from that path (found by path hash, Q12 (a)) **prepares** new content (reads the file, runs its loader's
validator, allocates new memory under its own tag, creates GPU resources); a kind without loaded objects runs only its
validator when it has one (maps, data, sprites, fonts, meshes, strings, indexes). If anything fails, every prepared item
is **abandoned** (freed) and the batch refused; otherwise every item is **committed** (pointer swaps that cannot fail),
the index and overlay switch, and old content is released (textures and meshes through D47; sound samples after their
voices stopped; the rest at once). Paths files of the batch name but no linked kind claims are plain files: they reach
the game through `assets::` reads.

**Same handle, new content (FR-7–FR-10).** Engine-owned objects are reached by their slot or table entry, so the swap
happens behind every handle: texture slots (and `Texture::width()/height()`, which read the slot in
Debug/Development: question 6), mesh slots, the 32-entry font table, sound slots, playing music slots, the localisation
chain. Sprite libraries are game-held blocks, so they gain a Development-only indirection through D31's serials (below).
Game-interpreted content (data structs, tilemaps, `assets::load` bytes) is announced (FR-11, Q6).

**Frames (FR-6, Q10).** Transfer messages are handled when they arrive; only `ReloadApply` is frame-stamped and goes
through D28's schedule. The batch is applied at the application point of the frame it names (or the next frame when it
says 0): after `++frame_count`, **before** spec 030's live point (so an action that loads an asset sees the new
content), before input events, D32's hooks, audio and dispatch. While paused (D52) the apply waits for the next game
frame (step or resume); files still arrive on paused ticks. Scripts make it exact with D52 (question 2): `swap-asset`
pauses the game at the script clock's frame, builds and sends, then stamps `ReloadApply` and `Pause resume` for that
same frame.

**Release.** Every addition sits in new files reached only from discarded `if constexpr (build::kDevToolsEnabled)`
branches; `<nnh/assets.hpp>`'s queries are inline constants in Release; the receiver, overlay, kinds and wire are not
linked. 030's three rules apply unchanged (plan 030, "Release by construction").

Rejected: the game watching its own folder (Q2: simulator only, races with the build), calling `nnh-assetc` directly (Q3:
the build's routing would drift), a RAM overlay (Q4 (b): competes with the Old 3DS's memory, can't stream music), an HTTP
fetch (Q4 (c)), reloading tilemaps in place (Q6), letting voices finish on old samples (Q7), coupling with tunables (Q9),
applying while paused (Q10), an overlay build folder for scripts (Q11 (b)), a global registry of loaded objects (Q12 (b)),
in-place reads of the PC's build folder (question 1), OS file-notification APIs (question 3).

### Decision D54: payload messages in D28 — a message may announce raw bytes that follow it; files travel as chunks with CRC-32, handled at receipt; only `ReloadApply` is frame-stamped

D28's messages are 16 fixed bytes, which can't carry files. Additive in version 1: a message type may declare a
**payload length** (decided by the decoder from its own fields, bounded per type); after decoding such a message the
reader (`RemoteReader`, still pure and fixed-size) hands the next `length` bytes through unchanged as **payload pieces**
pointing into the caller's buffer, split across reads as they arrive, then returns to decoding messages. A malformed
header can't announce a payload (its length is never trusted), so the stream stays in sync or falls into D28's
8-in-a-row shutoff. Transfer messages must carry frame 0 and are handled at receipt by the reload receiver, never
scheduled; protocol refusals (order, sizes, CRC, limits) drop the batch with a `ReloadRefused` record and don't count
toward the shutoff (like spec 030's refusals); decode failures do. Records 13–15 are sent only after the PC sent a
`ReloadBegin` or `ReloadNotice` in this attachment, so an older `nnh` never meets them; an older game answers message 12
with `Rejected{Type}` and `nnh` says `the game has no hot reload (built before spec 031): watching off`.

### Public API (sketch)

```cpp
// <nnh/assets.hpp> — gains <cstdint> and <nnh/build.hpp> (light). Everything below: Debug/Development only;
// Release: inline constants (FR-13, FR-19).
namespace nnh {

/// A batch of reloaded assets was applied at the start of this frame (spec 031, Debug/Development only: never
/// published in Release). Delivered in the same frame, before the scene's update. Game logic must not depend on it.
struct AssetsReloaded {
    std::uint32_t count = 0;     ///< reload_count() after this batch
    std::uint16_t changed = 0;   ///< paths added or replaced
    std::uint16_t removed = 0;   ///< paths removed
};

namespace assets {
inline constexpr bool kReloadEnabled = build::kDevToolsEnabled;

/// A view of this frame's reloaded asset paths (exact index paths, e.g. "art/hero.png"; removed paths included).
struct ReloadedPaths {
    const char* const* paths = nullptr;
    int count = 0;
    const char* const* begin() const { return paths; }
    const char* const* end() const { return paths + count; }
};

/// Applied batches since the start (+1 per batch). Release: 0.
std::uint32_t reload_count();
/// True when `path_or_name` was added, replaced or removed by the batch applied this frame: an exact asset path
/// ("levels/level1.nnhmap") or a load name — the path without its last extension ("levels/level1", "art/hero").
/// Release: false.
bool reloaded(const char* path_or_name);
/// This frame's paths (empty on frames without a batch). Valid until the end of the frame. Release: empty.
ReloadedPaths reloaded_paths();
} // namespace assets
} // namespace nnh

// <nnh/texture.hpp> — Debug/Development: width()/height() read the texture's slot (a reload may change the size);
// Release: the cached fields, as today. Texture's layout is unchanged.
int width() const { if constexpr (build::kDevToolsEnabled) return detail::texture_width_dev(*this); else return width_; }
```

Release forms are `inline` functions in the header whose bodies are the constants (no out-of-line symbol). Exact names
may still change while building; stage notes record any change.

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| `AssetsReloaded` | the reload apply point (Debug/Development only) | `count` u32, `changed` u16, `removed` u16 (8 bytes) | a level scene that reloads its map or data (`assets::reloaded("levels/level1")`), a cache of measured text |

### The frame in Debug/Development builds (after spec 030)

Release keeps today's `nnh::frame()` statement for statement. In Debug/Development, 030's order plus:

1. `remote_frame_start(F, sample)`: the reader in payload mode feeds the receiver (chunks appended to the SD file, CRC
   updated, `ReloadStaged` when the batch is complete); `ReloadApply` messages enter the schedule; a due one marks the
   staged batch "apply at F" (this also happens on paused ticks; nothing is applied there).
2. 030's `dev_frame_begin` (pause, paused ticks), `input_update`, `begin_frame`, `gfx_gpu_idle` (D47 frees, the reload's
   retired textures and meshes included), `gfx_begin_recording`, `++frame_count`.
3. **`reload_frame_start(F)`** (new, one null check when nothing is pending): prepare → commit or abandon; the
   localisation chain and data index swapped; `AssetsReloaded` published; the log line; `ReloadApplied`/`ReloadRefused`.
4. 030's live point (FR-23: when a batch was applied this frame, live snapshots edited tunables here), then input
   events, D32 hooks (the language switch reads the reloaded chain), `audio_frame` (voices stopped by a reload end here
   with `SoundFinished`), `events_dispatch` (`AssetsReloaded` delivered), the game.
5. Frame end: 030's live frame end compares the snapshotted tunables (FR-23's warning).

### Semantics fixed by the plan

**Paths and names** (`reload/reload_names.*`, pure, shared with `nnh`). Asset paths follow spec 007's rules
(`valid_asset_path`, ≤ 255 bytes). A **load name** is a path without its last extension (`art/hero.png` → `art/hero`,
`enemies/goblin.enemy` → `enemies/goblin`, `strings/pt-BR.nnhstr` → `strings/pt-BR`); `reloaded(x)` is true when `x`
equals a reloaded path or its load name (question 7). Index files (`.nnh_assets`, `.nnh_data`, `.nnh_loc`) are in the
list too. The log line prints load names in batch order, cut to fit 160 characters with `, ... (+N)`:
`I assets reloaded 3 (frame 1234): art/hero, levels/level1, enemies/goblin`.

**Batches.** ≤ 256 files, each ≤ 16 MiB, ≤ 32 MiB per batch (data + paths); a file is a **put** (added or replaced)
or a **remove**. Consistency (checked in prepare): when the batch carries `.nnh_assets`, every put must be in the new
index and every remove absent from it; without it, puts must be in the current index and there are no removes. Files
are prepared in batch order (`nnh` sends indexes last, then dependents after their sources), kinds in registration
order. A batch with a refused file applies nothing (FR-4).

**Kinds and what each does** (claims by output path; ✔ = engine-owned, swapped; ✉ = announced):

| Kind | Claims | Prepare | Commit | Notes |
|------|--------|---------|--------|-------|
| asset index | `.nnh_assets` | parse into a new block | swap the current index | the original index stays (romfs truth); paths removed by the batch become `NotFound` |
| texture ✔ | `*.png` | read, `platform::texture_create`, size rules | slot data swapped (handle, w, h, bytes); old handle retired (D47) | linear memory refusal names the asset and the bytes; tilemap tilesets, mesh textures and UI images follow |
| sprite library ✔ | `*.nnhsprite` | read, `sprite_validate`, new block | the serial's current block swapped (below) | animators rebind (FR-8) |
| sound ✔ | `*.wav`, `*.ogg` | read and decode into new samples | voices of that sound stopped (`SoundFinished`), slot swapped, old samples freed after the platform stopped them | Q7 |
| music ✔ | `*.wav`, `*.ogg` | open the new file as a stream, read its header | the slot's stream reopened at the same sample (loop start when past the new end); a crossfade keeps its timing | FR-9 |
| font ✔ | `*.nnhfont` | read, `font_validate`, block + page textures | the table entry swapped (same id, same `FontRef`); old pages retired (D47) | a full texture table refuses |
| mesh ✔ | `*.nnhmesh` | read, `mesh_validate`, record + platform mesh + its textures | the slot's record swapped; old record retired (D47) | |
| strings ✔ | `strings/*.nnhstr`, `.nnh_loc` | when the file is in the current chain (or the manifest): load the chain into a new block (or the same block when it fits) | block swapped, serial bumped (`text_changed()`, spec 028 widgets) | a manifest with another language list or base refuses (FR-14); the Debug pseudo-language follows the base |
| data ✔/✉ | `.nnh_data` and data files the index lists | validate each data file against the compiled schema (D29's fingerprint: a mismatch refuses with "rebuild and restart"); a new index into a new block | the index swapped (`count`/`name`/`find`/`path` describe the new set) | structs the game filled are announced (FR-11) |
| tilemap ✉ | `*.nnhmap` | `map_validate` (spec 012's validator) | nothing | Q6 |
| plain ✉ | anything else | nothing | nothing | `assets::load/read` see it |

**Sprite libraries (FR-8, question 5).** Development builds keep a table of up to 64 libraries loaded from a path
(`{serial, path hash, identity block, current block}`, 16 bytes each on the 3DS). A reload builds the new content in a
**new block**; the game's `SpriteLibrary` keeps pointing at its **identity block** (the one it loaded), whose header is
marked forwarded, and every Development-build library query resolves the current block through the table (Release:
untouched, the block itself). The previous current block (not the identity) is freed at commit: animators compare their
`block_` with the table's current block **by pointer, never dereferencing it**, and on a mismatch rebind before any
other work: the clip of the same name (FNV-1a of the stored name) at `min(step, new length − 1)` with the new durations
and markers; a missing clip → the library's first clip and one warning `animation: clip "walk" no longer exists in
hero: playing "idle"`; a library whose state or transition sections differ byte for byte → the entry state and one
warning. The identity block is freed with the library. Memory: a reloaded library costs its identity block plus its
current block until it is destroyed (Development only).

**Textures (question 6).** Each texture slot gets a Development-only path hash (side array sized by
`max_textures`, allocated at `gfx_init` in Development builds); `load_texture` records it. `Texture::width()/height()`
read the slot in Development builds (`detail::texture_width_dev`: a table lookup) so a game sees the new size; Release
keeps the cached fields. A stale handle (released texture) reads its cached values, as today.

**Sounds and music.** A reloaded sound stops its voices at the apply point through the normal stop path (`SoundFinished`
in that frame's dispatch); its old samples are freed at the next frame (the platform may still hold the buffer for the
frame that stopped it; the 3DS's `ndspChnWaveBufClear` is asynchronous). Music: the slot's stream position in samples is
kept; the new stream seeks there (`ogg_seek`, WAV offset) or to the loop start when past the new end; fades and
crossfades keep their frame counters. Streams opened from the PC's build folder use `asset_open_shared` (below) so the
build can rewrite a file the simulator streams (AC-9).

**Strings (FR-10).** The chain's block is reallocated when the new chain is larger (D50's block sized by the largest
chain), else rewritten in place after the prepare checked it; `tr` pointers obtained before the apply point are invalid
after it (as D50's switch); removed keys show their names in Debug/Development (D50's unknown-key rule).

**Removals (FR-12).** A removed path disappears from the index (`NotFound` for new loads); objects loaded from it keep
their content; one warning per path: `assets: "art/old.png" was removed: what was loaded from it keeps its content`.

**Refusals (FR-3, FR-14, FR-16, FR-18)** are one warning line each and a `ReloadRefused` record:
`assets: reload refused (frame 1234): art/huge.png: no linear memory for 4194304 bytes (old content kept)`,
`... enemies/goblin.enemy: made for another Goblin struct: rebuild and restart`, `... levels/room.nnhmap: corrupted
(layer 2 out of range)`, `... transfer: chunk out of order`, `... levels/room.tmj: conversion failed (see nnh)` (a
`ReloadNotice`), `... needs a rebuild and restart: data_types.cpp changed` (a `ReloadNotice` from the dry run).

**FR-23.** When a batch was applied, 030's live point (if the module is linked: `g_live_ops`) snapshots up to 16
tunables whose value differs from their default (hash + bits); 030's frame end of the same frame compares: a tunable
whose bits changed meanwhile logs once `live: player/speed was set by a reload (panel value 5.25, file value 4.5)`.
`changed()` is untouched (it belongs to FR-3's apply point). No tunable is written by the reload module.

### Remote protocol (`reload/reload_wire.{hpp,cpp}`, pure, shared with `nnh`; bytes 0–7 as D28)

| Message | frame | Bytes 8–15 | Payload after it |
|---------|-------|------------|------------------|
| `ReloadBegin` (12) | 0 | batch u16 (≠ 0), files u16 (1..256), total u32 (data + path bytes, ≤ 32 MiB) | — |
| `ReloadFile` (13) | 0 | index u16 (the next expected), op u8 (1 put, 2 remove), path length u8 (1..255), size u32 (put ≤ 16 MiB; remove 0) | the path, then (put) its CRC-32 u32 |
| `ReloadChunk` (14) | 0 | index u16 (the open file), length u16 (1..16384), offset u32 (= bytes of this file so far) | `length` bytes |
| `ReloadApply` (15) | the frame to apply (0 = next) | batch u16, files u16, 0 × 4 | — |
| `ReloadCancel` (16) | 0 | batch u16 (0 = whatever is open or staged), 0 × 6 | — |
| `ReloadNotice` (17) | 0 | reason u8 (1 conversion failed, 2 needs a restart), path length u8 (1..255), others u16 (more sources), 0 × 4 | the source path (relative to the game's folder) |

Every other byte must be 0 (`Reject::Range`). The decoded `Message` stays 32 bytes (batch/index in `id`, op/reason in
`screens`, lengths in `x`, sizes and offsets in `press`), as 029 and 030 did; `payload_length(message)` is a pure
function of the decoded fields. Receiver rules (`reload_receiver.*`): one batch at a time (a `Begin` while one is open or
staged → refused, "busy"); files in index order; a put's chunks contiguous from offset 0 to its size; path bytes valid
asset paths; the sum of sizes and paths equal to `total`; a put's CRC-32 checked after its last chunk; `Apply` only for
the staged batch with the same file count; `Cancel` and a disconnect drop the open or staged batch and remove its files.

| Record | Payload | When |
|--------|---------|------|
| `ReloadStaged` (13) | u16 batch, u16 files, u32 frame | the last file of a batch received and checked |
| `ReloadApplied` (14) | u16 batch, u16 files, u32 frame, u32 reload count, u32 apply time (µs, prepare + commit) | at the apply point |
| `ReloadRefused` (15) | u16 batch, u8 stage (1 transfer, 2 validate, 3 memory, 4 restart, 5 cancelled, 6 busy, 7 notice), u8 reason, u16 file index (0xFFFF none), u16 0, u32 frame, u32 bytes missing | a refused batch or a notice logged |

**Reading pace.** Messages are read as today (bounded by the schedule's room); payload bytes are read straight into the
receive buffer, **at most 16 KB per frame**, and appended to the open file with `fs_append` (one call per frame at most).
Over the 3DS's Wi-Fi this is the bottleneck by design (≈ 1 MB/s at 60 Hz, more than Wi-Fi gives); measured at T7.6 and
on the console.

### `nnh` side (`tools/nnh/src/reload/`)

- **Where the assets are.** `nnh_add_assets()` writes, at configure time, `<binary dir>/nnh_reload_<game>.txt`: the
  source assets folder and the game's source folder (absolute, native). No build step changes.
- **Mirror** (`output_snapshot.*`): at session start, the content hash (FNV-1a 64) and size of every file under
  `<binary dir>/assets/`; after a reconversion, files whose size or mtime moved are hashed again; the diff against the
  mirror is the batch. The mirror moves only when the game answers `ReloadApplied` (a refused batch leaves it, so the
  next save sends the difference again).
- **Watching** (`source_watcher.*`, question 3): polls the source folder every 100 ms (size + mtime of every file,
  temporary files included); a change starts a **quiet period** of 250 ms that restarts on every further change; then
  the controller builds. Pure core with an injected clock and lister (tested with temporary-file-and-rename sequences).
- **Building** (`asset_build.*`, question 4): `cmake --build --preset <p> --target <game>_assets` (on the 3DS through the
  MSYS shell; one persistent shell per session if T0.2 measures its start-up above 0.3 s); its output is printed as a
  normal build prints it (`file:line: reason`); a failure sends `ReloadNotice(conversion failed, <first changed source>)`.
  Then a **dry run** of the game's program target (`--target <game> -- -n`, Ninja's dry run): any command left means the
  change needs the program rebuilt (a schema source, `IMAGES`, a CMake declaration) → `nnh: reload refused: <the first
  command's description>: rebuild and restart (nnh run <game>)` and a `ReloadNotice(needs a restart)`. The dry run runs
  while the batch is sent; `ReloadApply` waits for its answer (`ReloadCancel` otherwise).
- **Sending** (`batch_sender.*`): non-blocking writes interleaved with reading the game's stream (the game blocks on its
  log sends too: a blocking sender could deadlock both), chunks of 16 KB, at most 256 KB beyond what the game
  acknowledged (`ReloadStaged`, ticks); paths in batch order with indexes last.
- **Controller** (`reload_controller.*`): idle → quiet → building → sending → waiting for `Applied`/`Refused`; saves
  during a cycle are coalesced into the next one; prints `nnh: reload 1: 3 files, 6.2 KB (build 0.21 s)` and
  `nnh: reload 1 applied at frame 1234, 0.58 s after the save: art/hero, levels/level1` (FR-15), refusals with the
  game's reason. Off with `--no-reload`, while a script runs (scripts swap explicitly), for Release builds (no Hello),
  and for a game that rejects message 12.
- **Typed commands**: `reload` (build now, skip the quiet period, send what changed), `reload PATH` (send that output
  again even if the game has it).
- **Script commands** (`remote/script_reload.cpp`, `script_runner_reload.cpp`, FR-22, Q11):
  `swap-asset SOURCE FIXTURE [unheld]` — SOURCE relative to the game's folder (inside it; `assets/art/hero.png`,
  `data_types.cpp`), FIXTURE relative to the script; the original is backed up first (`build/logs/reload-backup/<game>/`
  + a journal), the fixture copied over it. **Held** (default): `Pause pause` for the clock's frame F, wait for
  `LiveState` paused (the runner requests 030's catalogue when a script swaps, as it does for `pause`), build, send,
  then `ReloadApply` and `Pause resume` both for F: the batch lands exactly at F on every system; the command takes no
  frame. **Unheld**: the copy only; the watcher (on for this command) notices it like an editor save, for AC-8's timings.
  `restore-asset SOURCE` puts the original back (and reloads it, held). `expect-reload [PATH] [refused] [within N]` —
  checks the last swap's outcome: applied (or refused), the batch containing PATH (path or load name); `within N` for
  unheld swaps (the clock moves to the applied frame + 1, as `expect`). At the script's end, failure or Ctrl-C every
  backup is restored byte for byte and the journal removed; at start `nnh run` refuses while a journal exists
  (`nnh: assets of <game> were left swapped by an interrupted script: run "nnh restore-assets <game>"`).
- **Older peers**: a game that rejects `ReloadBegin` as `Type` → `the game has no hot reload (built before spec 031):
  watching off`; scripts with reload commands fail at that line.

### Spec 030 overlap

Spec 030 (merged before this spec starts) and this spec touch the same files. None is a merge conflict when 031 starts
from 030's merge; numbers come after 030's:

| Shared file | 030 adds | 031 adds |
|-------------|----------|----------|
| `engine/core/src/remote/remote_wire.{hpp,cpp}` | `LiveSet = 9`, `Live = 10`, `Pause = 11`, records 8–12 | `ReloadBegin = 12` … `ReloadNotice = 17`, records 13–15, payload lengths, decoder dispatch to `reload_wire.cpp` |
| `engine/core/src/remote/remote_reader.{hpp,cpp}` | — | payload mode (pieces), `bytes_wanted` in payload mode |
| `engine/core/src/remote/remote_session.cpp`, `remote_internal.hpp` | live dispatch, paused-tick processing, `LiveState` | payload reads into the receive buffer, transfer messages at receipt, `ReloadApply` from the schedule, detach → cancel (calls into `remote_reload.cpp`) |
| `engine/core/src/app/app.cpp` | the Dev frame order, live frame start/end | `reload_frame_start(F)` right before 030's live call |
| `engine/core/src/debug/live_hook.hpp`, `src/live/` | `LiveOps`, the module | `LiveOps::reload_snapshot`/`reload_compare` (FR-23), `live/live_reload.cpp` |
| `tools/nnh/src/remote/script.hpp`, `script_parser.cpp`, `script_timeline.*`, `script_runner.*`, `script_interactive.cpp`, `game_records.*`, `game_session.cpp` | live commands, paused-section rules, records 8–12, catalogue flow | `swap-asset`/`restore-asset`/`expect-reload` (in `script_reload.cpp`), the held-swap flow (in `script_runner_reload.cpp`), records 13–15, `reload` typed command, the non-blocking sender and controller tick in the session loop |
| `tools/nnh/src/cli.cpp`, `commands/run.cpp`, `run_pc.cpp`, `tools/nnh/CMakeLists.txt` | `--tunes`, `--hold` | `--no-reload`, `nnh restore-assets`, the controller's start |
| `scripts/check_release_strings.sh`, `engine/core/CMakeLists.txt`, `tests/CMakeLists.txt`, `examples/CMakeLists.txt`, `engine/platforms/fake/include/nnh/platform_fake.hpp` | 030's blocks | one 031 block each, after 030's |

The paused-section rules of 030's parser allow `swap-asset`/`restore-asset` (zero-frame commands) and `expect-reload`
without `within`; `unheld` swaps and `expect-reload … within` are clock-moving and refused while paused.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/assets.hpp` | changed | `AssetsReloaded`, `kReloadEnabled`, `ReloadedPaths`, `reload_count`, `reloaded`, `reloaded_paths` (Release inline) |
| `engine/core/include/nnh/texture.hpp` | changed | Dev `width()/height()` through the slot |
| `engine/core/contract/nnh/platform/reload.hpp` | new | Dev-only: `reload_stream_open(data_path, AssetStream&)`, `asset_open_shared(path, AssetStream&)` |
| `engine/core/src/reload/reload_wire.{hpp,cpp}` | new | messages 12–17, records 13–15, payload lengths (pure; also in `nnh`) |
| `engine/core/src/reload/reload_receiver.{hpp,cpp}` | new | the transfer state machine over a sink (pure; fuzzed) |
| `engine/core/src/reload/reload_names.{hpp,cpp}` | new | load names, path matching, the log line's list (pure; also in `nnh`) |
| `engine/core/src/reload/reload_overlay.{hpp,cpp}` | new | the current index and the path → file table; `g_reload_assets` (the extern `assets.cpp` consults) |
| `engine/core/src/reload/reload_files.{hpp,cpp}` | new | `.nnh_reload/<n>.bin`: append, finish, remove, wipe; the shutdown hook; the receive buffer |
| `engine/core/src/reload/reload_kind.hpp` | new | `struct ReloadKind`, registration, `ReloadFile` (path, hash, file, size) |
| `engine/core/src/reload/reload_batch.{hpp,cpp}` | new | prepare / commit / abandon over the kinds; consistency; refusals |
| `engine/core/src/reload/reload_apply.cpp` | new | the apply point, pending apply, counters, FR-13 back ends, the event, the log line, records |
| `engine/core/src/reload/reload_index.cpp` | new | the asset-index kind |
| `engine/core/src/remote/remote_reload.cpp` | new | session glue: payload reads, transfer messages, `Apply`, `Cancel`, `Notice`, detach |
| `engine/core/src/remote/remote_wire.{hpp,cpp}`, `remote_reader.{hpp,cpp}`, `remote_session.cpp`, `remote_internal.hpp` | changed | see "Spec 030 overlap" |
| `engine/core/src/assets/assets.cpp`, `assets_internal.hpp` | changed | Dev branches: the overlay's index and files for `exists`/`size`/`read`/lookups; `detail::asset_stream_open/read_at/close` wrappers |
| `engine/core/src/audio/music.cpp`, `data/data_index.cpp` | changed | streams through the wrappers (Release: the same platform calls) |
| `engine/core/src/app/app.cpp` | changed | `reload_frame_start(F)` (Dev branch) |
| `engine/core/src/gfx/texture_reload.cpp`, `textures.cpp`, `gfx_state.hpp`, `gfx_frame.cpp` | new / changed | the texture kind, path hashes, `texture_width_dev`, retiring raw platform handles (D47) — Dev branches only in the existing files |
| `engine/core/src/animation/sprite_reload.cpp`, `animator_rebind.cpp`, `sprite_library_internal.hpp`, `sprite_load.cpp`, `animator*.cpp` | new / changed | the library table, the kind, the rebind; Dev-only resolution of the current block |
| `engine/core/src/audio/sound_reload.cpp`, `music_reload.cpp`, `sounds.cpp`, `audio_internal.hpp` | new / changed | the sound and music kinds; path hashes; deferred sample frees |
| `engine/core/src/font/font_reload.cpp`, `font_load.cpp`, `font_table.*` | new / changed | the font kind; path hashes in the table |
| `engine/core/src/mesh/mesh_reload.cpp`, `load_mesh.cpp`, `mesh_table.cpp` | new / changed | the mesh kind |
| `engine/core/src/loc/loc_reload.cpp`, `loc_state.*` | new / changed | the strings kind: chain reload, serial, block growth, manifest check |
| `engine/core/src/data/data_reload.cpp`, `data_index.cpp` | new / changed | the data kind: file validation, fingerprint refusal, index swap |
| `engine/core/src/tilemap/map_reload.cpp` | new | the tilemap kind (validate only) |
| `engine/core/src/ui/` (the update/draw file) | changed | Dev branch: relayout when `assets::reload_count()` changed |
| `engine/core/src/live/live_reload.cpp`, `debug/live_hook.hpp` | new / changed | FR-23's snapshot and warning |
| `engine/core/CMakeLists.txt` | changed | `_nnh_reload_sources` (Dev core), the kind files in their modules |
| `engine/platforms/{pc,3ds,fake}/src/reload_<system>.cpp` | new | the two contract functions (PC: Windows `CreateFileW` with `FILE_SHARE_READ \| WRITE \| DELETE` behind `SDL_OpenIO`, else `SDL_IOFromFile`; 3DS: `fopen` on `sdmc:`/`romfs:`; fake: its file store) |
| `engine/platforms/fake/include/nnh/platform_fake.hpp`, `src/assets_fake.cpp` | changed | replace an asset's bytes while "open", count shared opens |
| `cmake/nnh_assets.cmake` | changed | `nnh_reload_<game>.txt` at configure time |
| `tools/nnh/src/reload/{output_snapshot,source_watcher,asset_build,batch_sender,reload_controller,asset_swap}.{hpp,cpp}` | new | the `nnh` side above |
| `tools/nnh/src/remote/script_reload.cpp`, `script_runner_reload.cpp` | new | script commands and the held/unheld flows |
| `tools/nnh/src/remote/*` (see overlap), `cli.cpp`, `commands/run.cpp`, `run_pc.cpp`, `commands/restore_assets.cpp`, `tools/nnh/CMakeLists.txt` | new / changed | wiring, `--no-reload`, `nnh restore-assets`, the shared core files |
| `scripts/check_release_strings.sh` | changed | the reload texts and the example's names |
| `tests/reload/`, `tests/remote/`, `tests/assets/`, `tests/gfx/`, `tests/animation/`, `tests/audio/`, `tests/font/`, `tests/mesh/`, `tests/loc/`, `tests/data/`, `tests/ui/`, `tests/live/`, `tests/fuzz/`, `tools/nnh/tests/`, `tests/platform_conformance/` | new / changed | the test strategy below |
| `tests/reload_example/test_self_check.cpp`, `tests/CMakeLists.txt` | new / changed | AC-1's goldens; ctests `reload_check_script_pc`, `reload_errors_script_pc`, `reload_open_file_pc` (Windows), `sprites_reload_script_pc` |
| `examples/reload/` | new | the example (below) |
| `examples/sprites/tests/reload.nnhs`, `tests/fixtures/` | new | AC-4b (the example's code unchanged) |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(reload)` after 030's |
| docs (stage 11) | new / changed | tasks T11.1–T11.6 |

### Data and memory

| Item | 3DS | 64-bit PC | When | Notes |
|------|-----|-----------|------|-------|
| reload state (BSS) | ≈ 0.3 KB | ≈ 0.4 KB | every Debug/Development game | receiver state, pending apply, counters, this frame's list (64 pointers are in the block) |
| receive buffer | 16 KB | 16 KB | first batch, kept to shutdown | tag `reload`; chunks go from here to the SD file |
| overlay table | 4 KB | 4 KB | first batch | 512 × {hash u32, file u16, flags u16}; ≤ 256 reloaded paths per session (more → "restart" refusal) |
| current index | its text + 4 B per entry | + 8 B per entry | a batch carrying `.nnh_assets` | the original stays |
| batch block | 24 B per file + paths | 32 B per file + paths | per batch, until the next | ≤ 256 files; typical < 2 KB |
| path hashes | 4 B × `max_textures` (512 B at the default 128), 4 B × `max_sounds` (512 B), 4 B × mesh slots, 4 B × 32 fonts, 4 B × music slots | same | the kind's first load in Dev | side arrays, tag `reload` |
| sprite library table | 16 B × 64 = 1 KB | 24 B × 64 | first `load_sprite_library` in Dev | |
| FR-23 snapshot | 8 B × 16 | same | inside the `live` block | only games with live values |
| **total at the defaults** | **≈ 22.5 KB** + the index copy | ≈ 23 KB | | Development only |
| during a batch | old + new content of the batch's assets | same | the apply frame | refused when it doesn't fit (FR-16) |
| Release | 0 | 0 | | `Texture`, `Sound`, `SpriteLibrary` layouts unchanged |

### Platform work

- **pc**: `reload_pc.cpp` — `reload_stream_open` (a data-folder file through `fs_root()` as an `SDL_IOStream`) and
  `asset_open_shared` (Windows: `CreateFileW` with all three share flags behind an `SDL_IOStreamInterface` given to
  `SDL_OpenIO`, checked in the installed SDL 3 header, so `asset_read_at`/`asset_close` keep working; elsewhere the plain
  `SDL_IOFromFile` — POSIX lets a writer replace an open file). No change to `assets_pc.cpp` (Release bytes). The data folder is already `<exe dir>/data/<folder>/` in Debug/Development.
- **3ds**: `reload_3ds.cpp` — both functions `fopen` (`sdmc:/3ds/<folder>/…` and `romfs:/…`; the romfs is read-only, so
  "shared" is the plain open). SD writes go through the shared stdio `fs_append`.
- **fake (tests)**: `reload_fake.cpp` + `platform_fake.hpp`: `fake::replace_asset(path, bytes)`, counters of shared and
  overlay opens, the fs store's existing failure injection reused for SD write failures.

### Build budgets

`<nnh/assets.hpp>` and `<nnh/texture.hpp>` gain `<cstdint>`/`<nnh/build.hpp>`: measured at T2.9 against T0.1 (expected
+20 preprocessed lines, < +0.01 s). Clean `nnh_core` gains ≈ 22 sources (expected +1 s, < 30 s). Edit one game `.cpp` →
unchanged (T8.4). The edit loop: T0.2 measures the asset target's no-op and one-file builds; AC-8's save → frame targets
are measured at T7.5 (PC) and T10.2 (console).

### ARM11 cost plan (AC-8)

Calibration as plans 029–030 (≈ 2 cycles per instruction on the Old 3DS, ≈ 110 cycles per miss; upper end × 1.3;
within 1.5× of a target = **at risk**).

| Workload | Hot path | Estimate | Target | Verdict |
|----------|----------|----------|--------|---------|
| idle frame (nothing received, nothing pending) | the session's existing poll + one flag check at the apply point | < 0.1 µs over today | Δ ≤ noise (0.02 ms) | expected PASS |
| a transfer frame | read ≤ 16 KB from the socket + CRC-32 (table, ≈ 8 cycles/byte) + one `fs_append` of 16 KB on the SD | 0.5 ms CRC + SD write 2–6 ms (unmeasured) | recorded; ≤ 8 ms | **at risk** (the SD) |
| applying a 64×64 PNG | read from SD + `Tex3DS_TextureImport` + swap | 1–3 ms | recorded | — |
| applying a map / a data file / a string file | read + validator (+ chain load) | 0.5–4 ms | recorded | — |
| applying a font / mesh / sprite library / sound | read + validator + pages / record / decode | 2–20 ms (a decoded Ogg sound is the worst) | recorded | — |
| asset reads with an overlay of 256 paths | one hash probe before the index lookup | < 1 µs per read | load time only | expected PASS |

T7.6 checks the disassembly (the idle check inline, CRC table-driven without `__aeabi_*`, `-fstack-usage` < 1 KiB in the
receiver and the batch engine). Apply-frame lengths and transfer rates are recorded, not judged, except AC-8's save →
frame targets; budgets are set from the console measurement with ≈ 25 % margin (spec AC-8, `docs/api/assets.md`).

### Self-check and bench (AC-1, AC-8)

- **Self-check** (`examples/reload/self_check.{hpp,cpp}`, pure over what the scene sees, also in `nnh_tests`): after the
  start and after each `AssetsReloaded` the scene reloads its map and data and folds with FNV-1a: every texture's
  width/height, the animator's clip name hash, frame position, state and next marker, the enemies' values, the map's
  tile layer bytes and object positions, `loc::tr` of three keys and their measured widths in the game font, the font's
  metrics, the sound's duration in samples, the music's position bucket, the mesh's vertex count and bounds,
  `reload_count()`; prints `reload: sum XXXXXXXX after N`. Goldens per step recorded from pc-debug into `check.nnhs`;
  pc-release (no reload: only `after 0`), GCC 13/14, clang 18, the 32-bit build and the console must agree.
- **Bench** (`RELOAD_DEMO=bench` or X): `idle` (300 frames of the showcase workload, before any batch and after ten
  batches: the averages within noise), `reads` (1000 `assets::exists` + `size` with 256 overlay paths vs none).
  Apply times and transfer rates come from `ReloadApplied` records and `nnh`'s timing lines (`timing.nnhs`).

### The example `examples/reload` (AC-1–AC-3, AC-8)

Original assets (drawn or generated for it): `art/backdrop.png` (128×128), `hero.ase.json` + `hero.png` (idle and walk
clips, a marker, a two-state machine), `levels/room.tmj` + `levels/tiles.tsj` + `levels/tiles.png` (a 20×15 room, a
platform object layer), `enemies/goblin.enemy`, `enemies/bat.enemy` (`data_types.cpp`: an `Enemy` struct), `sfx/blip.wav`,
`music/loop.ogg` (generated tones, with loop points), `strings/en.strings`, `strings/pt-BR.strings` (`LANGUAGES en
pt-BR`), `fonts/body.font` over spec 029's OFL subset (copied with its licence), `models/gem.gltf` (a hand-written
octahedron, `max_draws_3d = 8`). Scenes: `reload_scene` (the showcase: backdrop, the walking hero, the room, two enemies
moving at their data speeds, A plays the blip, music, a label `tr("hello")`, the spinning gem, a corner line with
`reload_count()` and the last batch's names), `check_scene` (`RELOAD_DEMO=check`: the self-check lines), `bench_scene`.
`RELOAD_SHOT=start|hero_big|room_moved|strings|font|gem` with `NNH_EXIT_AFTER_FRAMES` + `NNH_SCREENSHOT`. Fixtures in
`examples/reload/tests/fixtures/`: `hero_big.png` (another size) and `hero_renamed.ase.json` + PNG (a clip renamed),
`room_moved.tmj`, `room_broken.tmj`, `goblin_fast.enemy`, `pt-BR_new_char.strings` (a character the font lacked, so the
font is rebaked in the same batch), `blip2.wav`, `loop2.ogg`, `body_big.font`, `gem2.gltf`, `huge.png` (1024×1024),
`data_types_changed.cpp` (a field added).

### Console scripts

- `examples/reload/tests/check.nnhs` (AC-2): `fail-on "] E "`; `wait 30`; `expect "reload: sum <start> after 0" 60`; for
  each kind `swap-asset <source> <fixture>`, `expect-reload <name>`, `expect "reload: sum <golden> after N" 5`,
  `screenshot <kind>`; voices: `tap A` before the sound swap, `expect "] I audio: SoundFinished"`-style line from the
  scene; music position line; then `restore-asset` of two sources (the original sums again); `end`.
- `examples/reload/tests/errors.nnhs` (AC-3): `swap-asset assets/levels/room.tmj fixtures/room_broken.tmj` →
  `expect-reload refused`, `expect "reload refused" 5`, a screenshot equal to the one before; `swap-asset … room_moved.tmj`
  → applied; `swap-asset data_types.cpp fixtures/data_types_changed.cpp` → `expect-reload refused` ("rebuild and
  restart"); on the console only (a `RELOAD_DEMO=tight` scene holding linear memory until < 3 MB are free):
  `swap-asset assets/art/backdrop.png fixtures/huge.png` → `expect-reload refused` and the old backdrop on screen.
- `examples/reload/tests/timing.nnhs` (AC-8): `unheld` swaps of a 64×64 PNG, a `.tmj` and a data file, each
  `expect-reload <name> within 600`; `nnh` prints the save → frame times; then the bench.
- `examples/sprites/tests/reload.nnhs` (AC-4b): one held swap of its PNG, a screenshot showing the new pixels; the
  example's code is not touched.

Exact lines, sums and coordinates are recorded from the `--pc` runs at T7.5 into "Stage 10: console run (prepared)".

## Test strategy

All core tests run in `nnh_tests` on the fake platform (Windows; VM 120: GCC 13/14, clang 18 with ASan/UBSan, the 32-bit
host build) in pc-debug **and** pc-release (Release: the queries are constants, no reload symbol, `Texture` reads its
fields). `nnh` tests in `nnh_tool_tests`. Expected bytes, names, records and refusals are written by hand in the tests.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tools/nnh/tests/test_source_watcher.cpp` (save, add, remove, rename, temporary file + rename, a Tiled map + tileset burst → one cycle after the quiet period; `--no-reload`), `test_reload_controller.cpp` (typed `reload`, coalescing) |
| FR-2 | `test_asset_build.cpp` (the command line per preset, the MSYS form; output passthrough), `test_output_snapshot.cpp` (unchanged outputs not in the batch; dependents present because the build rewrote them: fixture build folders), ctest `reload_check_script_pc` (tileset → map, strings → font, data → `.nnh_data`, an added file → `.nnh_assets`) |
| FR-3 | `test_reload_controller.cpp` (a failed build sends a notice, never a batch; the converter's lines printed), `tests/reload/test_reload_notice.cpp` (the game's one warning), `errors.nnhs` |
| FR-4 | `tests/reload/test_reload_batch.cpp` (fake kinds: one failing prepare abandons every prepared item; unchanged outputs never sent; the batch order; consistency rules with and without `.nnh_assets`) |
| FR-5 | `tests/reload/test_reload_overlay.cpp`, `tests/assets/test_assets_overlay.cpp` (reads, sizes, streams and `exists` through the overlay; later loads get the new bytes; the romfs file untouched; a path reloaded twice uses its newest file and the older is removed) |
| FR-6 | `tests/reload/test_reload_apply.cpp` (applied at the stamped frame's point on the fake's `nnh::frame()`; before 030's live point and the input events; a late `Apply` → `Late` + the next frame; paused → waits for step/resume, files still received on paused ticks) |
| FR-7 | `tests/gfx/test_texture_reload.cpp` (same handle, new size through `width()`, draws use the new texture, the old freed after D47's frames, tileset/mesh textures follow), `tests/mesh/test_mesh_reload.cpp`, `tests/font/test_font_reload.cpp` (same `FontRef`, new metrics, pages retired), `tests/audio/test_sound_reload.cpp`, `tests/animation/test_sprite_reload.cpp`, `tests/ui/test_ui_reload.cpp` (relayout of an image and a label) |
| FR-8 | `tests/animation/test_animator_rebind.cpp` (same clip, clamped frame, new durations and markers, removed clip → first clip + warning, changed state machine → entry state + warning, an animator never dereferences a freed block — ASan, a moved/destroyed library) |
| FR-9 | `test_sound_reload.cpp` (voices stopped with `SoundFinished` that frame, the next `play` uses new samples, old samples freed a frame later), `tests/audio/test_music_reload.cpp` (same position, loop start when past the end, crossfade timing kept) |
| FR-10 | `tests/loc/test_loc_reload.cpp` (current and fallback files, serial bump, `text_changed()`, block growth, removed key shows its name, manifest change refused, pseudo-language) |
| FR-11 | `tests/data/test_data_reload.cpp` (structs untouched; `count`/`name`/`find`/`path` after add, remove, rename), `tests/tilemap/test_map_reload.cpp` (validated, nothing changed, a bad map refuses) |
| FR-12 | `test_assets_overlay.cpp` (removed → `NotFound`, loaded objects keep content, one warning per path; rename = remove + add) |
| FR-13 | `tests/reload/test_reload_queries.cpp` (counter, `reloaded` by path and by load name, the list this frame only, `AssetsReloaded` delivered that frame), `tests/reload/test_reload_release.cpp` (pc-release: constants, no event published), `tests/reload/test_reload_names.cpp` |
| FR-14 | `test_data_reload.cpp` (fingerprint mismatch), `test_loc_reload.cpp` (language list), `test_asset_build.cpp` (a dry run with commands → refused with the first description; empty → accepted), `errors.nnhs` |
| FR-15 | `test_reload_apply.cpp` (the exact log lines, truncation), `test_reload_controller.cpp` (`nnh`'s lines with times) |
| FR-16 | `test_texture_reload.cpp` (linear memory exhausted → refused with bytes, old kept), `test_font_reload.cpp` (texture table full), `test_reload_batch.cpp` (an allocation failure injected at every prepare step: nothing changed, nothing leaked), `test_loc_reload.cpp` / `test_data_reload.cpp` (blocks grow) |
| FR-17 | `tests/reload/test_reload_cost.cpp` (no allocation and a zero walk counter over 100 000 frames without a batch; allocations only in apply frames and at the first batch) |
| FR-18 | `tests/reload/test_reload_wire.cpp`, `tests/remote/test_remote_reader_payload.cpp`, `tests/reload/test_reload_receiver.cpp` (every refusal: order, sizes, totals, CRC, busy, cancel, disconnect; every limit and limit + 1; files only under `.nnh_reload/`), `tests/reload/test_reload_files.cpp` (names, wipe, SD write failure), mini-fuzz `tests/fuzz/fuzz_reload_receiver.cpp`, libFuzzer `libfuzzer_reload_receiver.cpp` |
| FR-19 | pc-release tests, T8.2 (`check_release_strings.sh`, `nm` of the Release example: no `reload`), `hello` identity at every stage end |
| FR-20 | AC-4b (`sprites_reload_script_pc`), `tests/reload/test_reload_plain_loop.cpp` (a `while (nnh::frame())` game gets FR-7), T8.3 probes |
| FR-21 | the self-check's sums equal on PC and console (T10.2), pixels within tolerance |
| FR-22 | `tools/nnh/tests/test_script_reload.cpp` (syntax and line errors, paused-section rules), `test_script_runner_reload.cpp` (held flow: pause, build, send, apply and resume at F; unheld; refused; restore on end, failure and interrupt; the journal and `restore-assets`), `test_asset_swap.cpp` |
| FR-23 | `tests/live/test_live_reload.cpp` (a tunable on a data-loaded field shows the file's value after the game reloads it; `changed()` untouched; the one warning when it differed from its default) |
| AC-1 | T7.3 screenshots; `tests/reload_example/test_self_check.cpp` |
| AC-2 | `check.nnhs` (`reload_check_script_pc`, console); `git status` clean afterwards (checked by the ctest) |
| AC-3 | `errors.nnhs` (`reload_errors_script_pc`, console with the linear-memory step) |
| AC-4 | every unit test above |
| AC-4b | `examples/sprites/tests/reload.nnhs` |
| AC-5 | the `nnh` tests above |
| AC-6 | the fuzz targets, ASan/UBSan at T9.1 |
| AC-7 | T8.2, T8.3, every stage end |
| AC-8 | `timing.nnhs` on PC (T7.5) and console (T10.2); the bench |
| AC-9 | ctest `reload_open_file_pc` (Windows: music streaming and a large `assets::load` in progress while the asset is swapped; `tests/platform_conformance/test_reload_shared_open.cpp` on the PC platform) |
| AC-10 | stage 11 |

### Planned hand mutants (T8.1)

`build/ref-031/mutants.py`, one at a time, `nnh_tests reload_ assets_ remote_ texture_reload sprite_reload animator_rebind
sound_reload music_reload font_reload mesh_reload loc_reload data_reload map_reload live_reload` and `nnh_tool_tests
reload script` (≈ 40): payload length off by one; payload pieces not split at the read boundary; frame ≠ 0 accepted on a
transfer message; a refusal counted toward the shutoff; chunk offset not checked; CRC skipped; total not checked; `Begin`
accepted while busy; `Apply` for another batch accepted; cancel keeping files; files named by the received path; commit
before every prepare succeeded; abandon leaking one item; unchanged files applied; consistency check skipped for
removes; overlay consulted after the original index; removed path still found; apply while paused; apply after 030's live
point; event published in the next frame; `reloaded()` without load-name matching; the list kept past the frame; Release
counter 1; texture `width()` from the cached field in Dev; old texture freed at once (D47); rebind on equal pointers;
clamped frame `= length`; missing clip without warning; state machine compared by count only; voices left playing; music
restarted at 0; loc serial not bumped; data fingerprint ignored; map not validated; `nnh` mirror moved on a refusal; quiet
period not restarted by a later change; dry run ignored; `Apply` sent before the dry run answered; swap not restored on
failure; journal ignored at start. Survivors become tests or are argued equivalent in "Mutation testing".

## Risks and alternatives

1. **Windows file sharing (AC-9).** SDL's Windows file streams are expected to open with `FILE_SHARE_READ` only (the
   prebuilt SDL has no source in the tree: T0.2 proves it by copying over a streamed file), so the build couldn't rewrite
   a file the simulator streams. Mitigation: `asset_open_shared` for streams in Development builds (all three share flags); whole-
   file reads are short (a collision makes the build step fail once: `nnh` retries the build once after 200 ms, the game
   sees a loader error for a half-written file only for a path that is about to be reloaded). A streamed file rewritten
   in place may glitch until its batch applies (Ogg pages carry a CRC and are skipped; WAV may click). Alternative: copy
   the PC's assets folder per run (rejected: start-up cost, disk).
2. **The 3DS edit loop's latency.** `cmake --build` through MSYS bash may cost 0.5–1.5 s of shell start-up per cycle on
   Windows. Mitigation: T0.2 measures it; above 0.3 s `nnh` keeps one login shell per session and sends it commands
   (a sentinel line marks each end). Fallback: calling Ninja through that shell directly.
3. **The dry run's false positives.** Adding a file reruns CMake's configure (`CONFIGURE_DEPENDS`); if that regenerates
   a source the game compiles (embedded images, the schema), the dry run would refuse a plain addition. T0.2 checks the
   four cases (asset edit, addition, schema edit, `IMAGES` edit); a false positive is fixed in the generator (write only
   if different), not by skipping the check.
4. **SD write cost per transfer frame** (at risk). `fs_append` opens and closes the file each call. Mitigation: one call
   per frame (≤ 16 KB); measured at T10.2. Fallback: a Development-only "keep the file open" pair in the reload contract.
5. **Zero Release bytes** with Dev branches in Release-linked files (`app.cpp`, `assets.cpp`, `textures.cpp`,
   `gfx_frame.cpp`, `sounds.cpp`, `music.cpp`, the sprite loader and animator, the font and mesh tables, `texture.hpp`).
   Mitigation: plan 030's rules 1–3, hashes at every stage end, per-object disassembly diffs when one moves.
6. **Sprite libraries' indirection** (question 5) touches every animator command in Development builds (one compare).
   Mitigation: the compare sits beside D31's existing serial check; ASan runs the random-operation test with reloads.
   Alternative: refuse a sprite reload whose block grows (simpler, but FR-7 asks for more frames).
7. **Asynchronous frees on the console** (textures through D47, `ndsp` buffers a frame later). Mitigation: tests on the
   fake model both delays; the console script reloads each kind while drawing and playing.
8. **Spec 030 dependency.** Held swaps need D52's pause and `LiveState` (sent only after a catalogue request: the runner
   asks for it, as 030's runner does for `pause`). If 030's merge changes those names, T0.0 updates this plan.
9. **Big edits.** A tileset used by many maps can exceed 256 files or 32 MiB: `nnh` refuses locally with "too many
   changes for one reload: restart" (the game never sees a partial batch).
10. **Path hash collisions** (FNV-1a 32 over asset paths): `nnh` warns at session start when two outputs share a hash
    (both would reload together; nothing breaks).
11. **Deadlock between two blocking senders** (`nnh` sending chunks, the game sending log lines). Mitigation: `nnh`'s
    non-blocking sender interleaved with reads (tested over loopback with a game that logs heavily during a transfer).

## Decisions taken (to review)

The spec's open questions are decided as it recommends. Plan-level decisions:

1. **One delivery path** (question 1): batches over the connection on every system, files in `.nnh_reload/` of the data
   folder.
2. **Held script swaps** through D52's pause (question 2), `unheld` for timings.
3. **Polling watcher** with a 250 ms quiet period (question 3).
4. **Restart detection by a Ninja dry run** of the game's program after the asset build (question 4).
5. **Sprite libraries keep an identity block** and resolve the current block through D31's serial table in Development
   builds (question 5).
6. **`Texture::width()/height()` read the slot** in Development builds (question 6).
7. **FR-13's list holds exact paths; `reloaded()` takes a path or a load name** (question 7).
8. **One spec, staged; no 031a/031b split** (question 8; Q1 allowed it).
9. **Two-phase batches** (prepare, then commit that cannot fail) with kinds registered by the modules that load them.
10. **Payload messages in D28** (D54), 16 KB per frame, files on the SD by `fs_append` (question 10).
11. **FR-23's warning on the game side** through two `LiveOps` entries (question 11).

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | One delivery and apply path on every system; the only platform functions are two Development-only stream opens in new files |
| II | ✅ | ≈ 22.5 KB Development-only memory after the first batch; no RAM copy of batches; old + new content only for the batch's assets, refused cleanly when short |
| III | ✅ | Only the asset target is built; headers gain `<nnh/build.hpp>`; the edit-one-`.cpp` path untouched; the MSYS shell cost measured and avoided |
| IV | ✅ | One development-only event at one point of the frame, queries for state, engine commands for swaps |
| V | ✅ | No allocation in idle frames, no walk without a batch, function-pointer kinds, no exceptions/RTTI/`virtual` |
| VI | ✅ | Tests first for every piece, fuzzing, mutants, goldens on every compiler and the console |
| VII | ✅ | ≈ 22 new core sources and ≈ 9 `nnh` sources, each one concept ≤ 250 lines; `remote_session.cpp` and `script_parser.cpp` kept under 400 by new files |
| VIII | ✅ | API pages, a guide, the tool guides, the example |
| IX | ✅ | Original assets; the OFL font reused with its licence |
| X | ✅ | Game-named files inside the data folder, bounded and validated messages, loaders' validators before use, fuzzed; Release reads and writes nothing of it |
| XI | ✅ | Off with `--no-reload` and in scripts unless swapped; kinds linked only by their users; Release byte-identical |

No constitution exception is requested. D53 and D54 are new (status: proposed, for review). Existing decisions touched
additively: D28 gains payload messages 12–17 and records 13–15 (D54); D31's Development-only serial table gains the
current block per library; D47 also retires raw platform handles replaced by a reload; D52's pause is used by script
swaps; D50's chain is reloadable at the apply point. Flagged for the maintainer: questions 1–11 below and risks 1, 2 and 4.

## Questions for the maintainer (answered 2026-11-26: each as recommended)

1. **One delivery path: the simulator receives batches over its connection too, instead of reading the build folder
   (FR-5's wording for the simulator)?** Recommended: yes — the simulator then runs the console's exact code (FR-21 by
   construction; the receiver and the SD-file path are exercised by every `--pc` script and ctest), later loads see only
   accepted batches on both systems, and a loopback copy of a few KB costs nothing. Alternative: the spec's wording (the
   simulator reads its build folder; a refused batch's files are then already visible to later loads there, and the
   console path is only tested on the console).
2. **Script swaps held with D52's pause** (`swap-asset` pauses at the clock's frame, builds and sends, then applies and
   resumes at that same frame), with `unheld` for timing runs? Recommended: yes — the only way a reload "lands on the
   frame it names" when a build takes seconds; it reuses 030's pause, so scripts that swap need a game built after 030.
   Alternative: unheld only (`expect-reload … within N`; frames differ between PC and console runs).
3. **Polling the source folder every 100 ms with a 250 ms quiet period**, instead of OS notifications
   (`ReadDirectoryChangesW`, inotify)? Recommended: yes — portable, one code path, immune to the rename-save problem the
   spec cites, ≈ 1 ms per poll for a few hundred files (measured at T0.2). Alternative: OS APIs with polling as fallback.
4. **FR-14's "needs a restart" detected by a Ninja dry run** of the game's program after the asset build? Recommended:
   yes — one general rule for schema sources, `IMAGES`, CMake declarations and anything else compiled in, using the
   build's own knowledge, ≈ 0.1–0.2 s and run while the batch is sent. Alternative: per-case checks (the data
   fingerprint and the language list on the game side only; `IMAGES` and other cases would then go unnoticed).
5. **Sprite libraries keep their first block as their identity** after a reload that changes their size (Development:
   identity + current block until the library is destroyed), with a pointer compare in every animator command?
   Recommended: yes — FR-7 and FR-8 without changing `SpriteLibrary`/`SpriteAnimator` layouts or Release code.
   Alternative: reload only when the new content fits the old block, refusing larger libraries.
6. **`Texture::width()/height()` read the slot in Debug/Development builds** (a table lookup instead of a field)?
   Recommended: yes — otherwise a game sees the old size after a reload of another size (FR-7); Release keeps the
   fields. Alternative: keep the cached size and document that `width()` is the load-time size.
7. **FR-13's list holds exact asset paths (`art/hero.png`) while `reloaded()` accepts a path or a load name
   (`art/hero`)**, instead of a list of load names? Recommended: yes — a plain file's name *is* its path, a data asset's
   extension is its type key, and the game can always strip; exact paths are unambiguous. Alternative: load names in the
   list (paths that differ only by extension would then collide).
8. **One spec, not split into 031a/031b** (Q1 allowed a split)? Recommended: one spec — with one delivery path the
   console needs only two tiny platform functions and its stage (10) is a verification stage; the cut line if 1.0 can't
   wait is stage 10 recorded as pending (simulator-only release), then meshes, then fonts (as Q8 says). Alternative: 031a
   (stages 0–9) and 031b (stage 10 and console budgets) as two specs.
9. **Debug/Development `hello` growth**: the payload reader, receiver, overlay, batch engine and the asset-index kind
   are in every Debug/Development game (the session links them): estimated 6–9 KB of `.text`, ≈ 0.3 KB of `.bss`
   (Release unchanged). Accept (recommended), or link the reload core only through a hook set by the first kind
   registration (smaller, but a game loading only plain files would then not reload).
10. **Transfer pacing: up to 16 KB of payload read per frame and one `fs_append` per frame**, with a 16 KB receive buffer
    allocated at the first batch? Recommended: yes — bounded frame cost and memory; ≈ 1 MB/s at 60 Hz, above Old 3DS
    Wi-Fi. Alternative: 4 KB per frame (smaller frame cost, slower big files) or a kept-open writer contract (risk 4's
    fallback).
11. **FR-23's warning computed on the game side** (030's live module snapshots edited tunables at a batch's apply point
    and compares at that frame's end, through two new `LiveOps` entries; ≤ 16 tunables tracked)? Recommended: yes — `nnh`
    can't see a struct the game reloads, the game can, and nothing is written to tunables (Q9 (a)). Alternative: drop the
    warning and document the "file wins" rule only (FR-23's text would change).

## Implementation notes

Built in the worktree `.claude/worktrees/spec-031` (branch `spec-031`, from `main` at `cf15822`, after spec 030's merge).

### Implementation refinements

- **R1 (T0.0) Numbers confirmed.** The merged `remote_wire.hpp` ends at `Pause = 11` and `LiveState = 12`; spec 030's names
  are as this plan uses them (`dev_pause_state()`, `PauseOp::{Pause,Resume,Step}`, `LiveState` records, `LiveOps`
  through `g_live_ops`, the live point right after `++g_frame_count` in `app.cpp`). Messages 12–17 and records 13–15 as
  planned; no change to the plan.
- **R2 (T1.1) Field mapping.** Begin/Apply keep `files` in `Message::x` (u16 fits an `int16_t` up to 256), Notice keeps
  `others` in `Message::release`; the rest as planned. Every reload message but Apply must carry frame 0. `ReloadReason`
  (the record's reason byte) is one enum: transfer reasons 1–9, validate/memory/restart 20–25, notices 30–31.
- **R3 (T1.2) Transfer messages skip the frame order.** A transfer message resolves to the frame being read and does not
  move the reader's "last frame": a script that scheduled input 100 frames ahead can still send a batch now (Apply keeps
  the order, like every scheduled message). A refused but decoded message (NotAttached, Order) still has its payload
  skipped; a decode failure never announces one.
- **R4 (T1.2) Feeds until every byte is handled.** A read of `room × 16` bytes can now yield more than `room` items (a
  payload piece is an item too), so the session (and the remote fuzz target) feed the rest of the bytes after handling
  the items, instead of assuming one feed takes everything. `bytes_wanted(0)` is 0 in payload mode too.
- **R5 (T1.5) Overlay entries are 16 bytes on the 3DS (24 on a 64-bit PC)**: {hash, file, used, size, path pointer}, so
  the table is 8 KB on the 3DS instead of the planned 4 KB (the exact compare needs the path). **Overlay tombstones.** A removed path keeps its slot (a tombstone); a path added again takes a new slot, so
  `kOverlayPaths` counts slots taken in the session (removals included).
- **R6 (stage 1) A latent padding compare.** `map_coins_equal_the_hand_written_coins` (platformer prefabs, spec 022)
  compared `Barrel {int, bool}` component bytes including padding; the new tests changed the test order and the stale
  heap bytes differed. The test now zeroes both Barrels after comparing their members, as it already did for `Turret`.

- **R7 (T2.1) Files through the platform contract.** `reload_files.cpp` writes `.nnh_reload/<n>.bin` with the
  platform's `fs_append`/`fs_remove` directly (the names are the game's own and pass `nnh::fs::validate_path`: tested),
  not through `nnh::fs`'s public calls, so the receive buffer's in-place reads need no copy. A chunk is read straight into
  the free part of the 16 KB receive buffer (`ReloadFiles::window`), and the sink appends it without copying.
- **R8 (T2.1) The batch block at Begin.** The block listing a batch's files (entries + paths, bounded by the Begin's
  total) is allocated when the batch begins, not at the apply point: a transfer frame allocates once per batch.
- **R9 (T2.3) Reads during prepare.** While a batch is prepared, every asset read (`assets::`, music, the data index)
  first looks at the batch's own files (puts found, removes gone), then the current index and the overlay: the kinds call
  their normal loaders and see the new bytes, while nothing visible has changed yet. `assets.cpp`, `music.cpp` and
  `data_index.cpp` consult the reload layer only in `if constexpr (build::kDevToolsEnabled)` branches
  (`g_reload_assets_active`, set at the first applied batch).
- **R10 (T2.4) The asset index is the engine's, not a registered kind.** The batch engine parses a batch's
  `.nnh_assets` first (consistency is checked against it), commits it before the kinds, and removes its received file
  once parsed. Kinds are prepared file by file in batch order, kinds in registration order.
- **R11 (T1.5, T2.8) Tombstones are reused.** A path removed and added again reuses its tombstone, so `kOverlayPaths`
  bounds live paths, not paths ever seen (the random test adds and removes paths thousands of times). R5 is superseded.
- **R12 (T2.5) Reading pace.** A frame reads messages once (as before, one socket read when idle); while a batch's
  payload arrives it goes on reading, up to 16 KB of payload per frame (`kReloadReadPerFrame`), then flushes the buffer
  with at most one `fs_append`. Records: `ReloadStaged`/`Applied`/`Refused` only after a `ReloadBegin`, `ReloadNotice`
  or `ReloadApply` in the attachment. A transfer refusal is one warning and one record; the rest of that batch is ignored
  quietly until the next Begin or Cancel; refusals never count toward the 8-in-a-row shutoff.
- **R13 (T2.7) Data index state shared.** `data_index.cpp`'s state moved to `data/data_index_state.hpp`
  (`g_data_index_state`, `data_index_read`, `data_index_match_types` made non-static, same code), so the data kind's
  prepare/commit live in `data/data_reload.cpp` and Release links none of it. Data reload tests are in
  `tests/data_assets/test_data_reload.cpp` (beside the data module's other tests and fixtures) instead of `tests/data/`.

- **R14 (T3.4) Pacing instead of a non-blocking sender.** `net::Socket` (in `link/`, shared with the console) has no
  non-blocking send, so the controller paces a batch by the game's ticks: 64 KB at once, then 48 KB per Tick record
  (ticks come every 4 frames; the game reads 16 KB a frame), handed to the session's ordinary write after the frame
  messages. The game never has more than about a second of payload waiting, so neither side can block the other for
  long; the loopback stress test with a game logging heavily during a 4 MB transfer is not written (Not done).
- **R15 (T3.5) The dry run before the batch.** The build call does the asset build and the dry run back to back (about
  0.1 s more) before the batch is encoded, instead of the dry run running while the batch is sent; the Apply then simply
  follows the batch's bytes.
- **R16 (T3.3) Dry-run lines that aren't the game's.** A Ninja dry run always lists CMake's CONFIGURE_DEPENDS check
  (`Re-checking globbed directories...`) and, because it can't know that check finds nothing, CMake's re-run
  (`Re-running CMake...`); both are ignored (the asset build has already re-run CMake when a declaration changed, and
  what that changes in the game shows as compiles and links). Found by the end-to-end check (plan risk 3).
- **R17 (T2.1, found end to end) Received files made with fs_write.** On the PC `fs_append` doesn't create folders, so
  the first put of a session failed ("could not write the file"); a put now starts with `fs_write(name, nothing)` (makes
  the folder, replaces a leftover), then appends.
- **R18 (T3.5) Interactive runs attach at once.** With hot reload on, interactive `nnh run` sends its (non-exclusive)
  Hello when the game connects instead of at the first typed command, so a save can reach the game; the console's own
  input still counts.
- **R19 (T4.1) Raw texture retirement.** A reload's replaced platform textures go to a Development-only list in
  `gfx/texture_reload.cpp` freed by a hook `gfx_gpu_idle()` calls in a Development branch (D47), so `GfxState`'s layout
  (and the Release bytes) don't change. The path-hash side table holds {hash, generation} per slot (8 bytes).
- **R20 (T2.1) Log wording.** The applied line is `I assets reloaded N (frame F): ...` (channel `assets`, message
  `reloaded ...`), as planned; a refusal is `W assets reload refused (frame F): <path>: <reason> (old content kept)`.

- **R21 (T3.6) Held swaps go on at F + 1.** `swap-asset` (held) sends Pause for the clock's frame F; when `LiveState`
  says paused, the controller builds and sends the batch, its Apply for F and a Resume for F (one ordered stream), and the
  script continues at F + 1 (the batch applied at F; anything after the swap reaches the game for F + 1). `expect-reload`
  takes no frame after a held swap; after an unheld one it waits `within N` and the script goes on at the applied frame
  + 1. Reload lines are refused while the script has the game paused (a held swap pauses and resumes itself); an unheld
  swap sends the batch at once (the watcher's quiet period is skipped: the script knows the file changed).
- **R22 (T3.3) No persistent shell; Ninja directly, and the dry run only when needed.** Measured (pc-debug, `data`): a no-op
  `cmake --build --target data_assets` 0.54 s (of which CMake's glob re-check 0.28 s), after touching a data file 0.84 s
  via `cmake --build` and 0.82 s via Ninja directly; the dry run 0.39 s. On the PC `nnh` now calls the preset's Ninja
  (`CMAKE_MAKE_PROGRAM` from `CMakeCache.txt`) directly (−0.15 s); the 3DS keeps devkitPro's shell (a persistent MSYS
  shell was not built: the 3DS edit loop is dominated by Wi-Fi, see stage 10). The dry run runs only for a changed source
  outside the assets folder (a script's swap of a schema source); saves inside the folder skip it (−0.4 s): the edge case
  left is an `IMAGES` file kept inside the assets folder, which would reload as an asset without the restart notice.
  Result: build 0.88 s and save → frame 1.03 s for a data asset (three converter steps), from 1.42 s / 2.2 s — about at
  AC-8's proposed 1 s; a PNG is quicker (one copy step).
- **R23 (dev growth) Opt-in by linking.** Every Release-linked file and the session reach the reload core only through
  `reload/reload_hooks.{hpp,cpp}` (pointers, null until `reload::install()`, which `register_kind()` calls): a
  Debug/Development game that loads nothing reloadable links no reload core and answers `ReloadBegin`/`ReloadApply` with
  `Rejected{Type}` (`nnh`: "built before spec 031: watching off"). Biggest contributors before (3ds-dev `hello`):
  `apply_batch` 2.0 KB, the receiver ≈ 2 KB, `apply_now` 0.8 KB, `format_list` 0.8 KB, `index_prepare` 0.5 KB, the
  reason texts, the 4 KB `g_batch` BSS of the remote decoder's table unchanged. **3ds-dev `hello` text: 227 088 (T0.1) →
  242 648 (stage 2) → 229 416 now (+2 328 B: the reader's payload mode, the wire's reload messages and the hooks)**, inside
  question 9's approved 6–9 KB; **for the maintainer to confirm**: a game that only reads plain files (`assets::load`)
  doesn't reload (plan question 9's alternative, now taken).
- **R24 (T5.2) Old samples freed at the next batch.** The samples a sound reload replaced wait in a Development-only list
  freed at the next batch's prepare or at shutdown (instead of "the next frame"): no per-frame hook in `audio_frame`, and the
  3DS has stopped the channel long before.
- **R25 (T4.2) A new chain block every time.** The strings kind always builds the chain in a new block (old and new held
  for the apply frame) instead of rewriting in place when it fits; the first string file of a batch prepares the whole
  chain (the batch's other string files and manifest are read through the staging). The manifest is always re-read through
  the batch and compared (language codes, base, pseudo-language).
- **R26 (T4.3) The UI relayouts in batch frames.** `ui_step` (Dev branch) forgets resolved texts when
  `assets::reloaded_paths()` is not empty: a UI updated in the batch's frame resolves and measures again (`forget_text`,
  now shared with `Ui::text_changed`). A texture's new size doesn't change an image widget (its region is explicit).
- **R27 (T5.4) Fonts need no side table.** The font table already keys entries by the hash of the load path; the kind
  finds them directly and swaps the block in the same entry (`font_table_replace`), keeping the id and fallbacks.
- **R28 (T3.7, found) A data game's asset target builds the data schema program, which links core**: editing engine
  sources while a reload builds compiles them. Harmless in game work; it made one full ctest run fail while sources were
  being edited (re-run green).

### Stage 0 results

- T0.0: see R1.
- T0.1 (partial): baselines recorded in `build/ref-031/base/` (not committed): 3ds-release `hello.3dsx` SHA-256
  `8c687f3dec3df9fa…`, stripped `hello.elf` `5cd5d9505dd67b73…`; pc-release `hello.exe` size `text 134880 data 22052 bss
  212400` and its `nm -C` list (3001 lines); 3ds-dev `hello.elf` `text 227088 data 32872 bss 110096`; pc-debug `hello.exe`
  `text 225744`. The per-object disassembly, the edit-one-`.cpp` timings and the header measurements were not taken
  (see "Not done").
- T0.2 and T0.3: not done (see "Not done").

### Stage 1 results

- New pure files: `reload/reload_wire.{hpp,cpp}`, `reload_receiver`, `reload_names`, `reload_overlay` (also in `nnh`:
  the wire and names); `remote_wire` dispatches messages 12–17 and records 13–15; the reader has its payload mode.
- New tests: `test_reload_wire` (17), `test_remote_reader_payload` (10), `test_reload_receiver` (20), `test_reload_names`
  (5), `test_reload_overlay` (6), mini-fuzz `test_fuzz_reload` (2: 20 000 random streams, 20 000 mutated batches);
  libFuzzer target `fuzz_reload_receiver` (`NNH_BUILD_FUZZERS`, for the VM at T9.1). T1.6 stays unticked: its 60 s
  libFuzzer run needs clang's libFuzzer, which the Windows MinGW toolchain doesn't have (the mini-fuzz runs everywhere). Changed: `test_remote_wire`,
  `test_remote_slider`, `test_live_wire` (type 18 is now the first unknown type), the remote fuzz target (R4).
- Stage-end guards: `nnh_tests` 4051/4051, `nnh_tool_tests` 197/197; ctest pc-debug 373/373, pc-release 337/337;
  3ds-release and 3ds-dev build with 0 warnings; 3ds-release `hello.3dsx` and stripped `hello.elf` SHA-256 identical to
  T0.1; pc-release `hello` sizes and `nm -C` identical. 3ds-dev `hello` text +1 712 B (the reader's payload mode and
  `reload_wire.cpp`, linked by the remote decoder), bss +8 B.

### Stage 2 results

- Game side: `reload_files` (received files, the receive buffer, the wipe), `reload_assets` (reads through the staged
  batch, the current index and the overlay; `reload_asset_open` for music and the data index), `reload_kind.hpp`,
  `reload_batch` (prepare/commit/abandon, consistency, the overlay's room), `reload_index` (the asset-index kind),
  `reload_apply` (the pending batch, `g_reload_apply` called by `app.cpp` right before the live point, FR-13's back ends,
  `AssetsReloaded`, the log lines, records), `remote/remote_reload.cpp` (the session's glue; `remote_session.cpp` routes
  transfer messages at receipt, `ReloadApply` from the schedule, payload pieces, detach). Kinds so far: data
  (`data/data_reload.cpp`) and tilemap (`tilemap/map_reload.cpp`, validate only). The platform contract
  `nnh/platform/reload.hpp` with `reload_{pc,3ds,fake}.cpp` (PC: `CreateFileW` with every share flag behind
  `SDL_OpenIO`); the fake gained `replace_asset`, `fail_next_appends` and stream counters.
- Public API: `<nnh/assets.hpp>` gained `AssetsReloaded` (8 bytes), `kReloadEnabled`, `ReloadedPaths`, `reload_count()`,
  `reloaded()`, `reloaded_paths()` (Release: inline constants).
- New tests: `test_reload_files` (7), `test_assets_overlay` (6), `test_reload_apply` (10), `test_reload_batch` (8),
  `test_reload_queries` (3), `test_reload_release` (1), `test_data_reload` (5), `test_map_reload` (2), `test_reload_cost`
  (3, 100 000 idle frames), `test_reload_random` (1: 10 000 seeded batches against a model since stage 4; 2 000 at stage 2), `test_reload_overlay` +1.
- Stage-end guards: `nnh_tests` 4097/4097 (pc-debug), 4035/4035 (pc-release); `nnh_tool_tests` 197/197; ctest pc-release
  337/337; 3ds-release and 3ds-dev built (3ds-release had two unused-parameter warnings in `reload_apply.cpp`, Release
  only: fixed with `[[maybe_unused]]`); 3ds-release `hello.3dsx` and stripped `hello.elf` SHA-256 identical to T0.1;
  pc-release `hello` sizes and `nm -C` identical. Debug/Development `hello` growth: 3ds-dev text 227 088 → 242 648
  (+15 560 B, over question 9's 6–9 KB estimate: the receiver, the files, the overlay, the batch engine, the asset-index
  kind, the apply point and their log texts are all reachable from the session), bss +88 B; pc-debug text +33 224 B.
- Open in stage 2 (unticked): T2.2's conformance test on the PC platform (the shared open surviving a rewrite) is not
  written (the fake's shared open is tested); T2.8's random test runs 2 000 batches, not 10 000, without guard bytes
  (ASan on the VM covers the bounds); T2.9's header measurement is recorded after stage 4.

### Stage 3 results (partial)

- `nnh` side: `cmake/nnh_assets.cmake` writes `nnh_reload_<game>.txt`; `tools/nnh/src/reload/`: `output_snapshot` (the
  mirror), `source_watcher`, `asset_build` (commands per preset, the MSYS form, one retry, dry-run parsing),
  `batch_sender` (exact bytes, local limits), `reload_controller` (the cycle, pacing, notices, older games, the printed
  lines), `reload_setup` (the paths file, the run's session); `script_runner_reload.cpp` (typed `reload [PATH]`, records,
  paced bytes), `game_session.cpp` (polls the controller, writes its bytes after the frame messages),
  `commands/run_reload.cpp` (`start_reload` after the build), `cli` (`--no-reload`).
- New tests (`nnh_tool_tests`, 217 in all): `test_reload_tool` (12: mirror, watcher, build, dry run, batch bytes, limits,
  paths file, `--no-reload`), `test_reload_controller` (8: save, batch, applied; failed build, notice; needs a restart;
  refused keeps the mirror; nothing changed; older game; pacing; typed reload of one path).
- End to end by hand (T3.7, `build/ref-031/e2e_data.py`, not committed): `nnh run data --pc --no-build`, `goblin.enemy`
  saved with `speed = 2.5` gave `I assets reloaded 1 (frame 303): enemies/goblin` and `nnh: reload 1 applied at frame
  303, 2.20 s after the save`; then `speed = "fast"` gave the converter's `assets/enemies/goblin.enemy:4: speed: expected
  a number, found a string`, `nnh: reload refused: conversion failed`, the game's warning, old content kept; then the
  file restored gave `reload 2 applied ... 2.01 s after the save`. Save to frame is about 2.0-2.2 s on Windows, of which
  the asset build is about 1.6 s (the CONFIGURE_DEPENDS glob check and the data steps): over AC-8's proposed 1 s; T0.2's
  measurements and the persistent-shell / Ninja-direct options are open.

### Stage 4 results (partial: T4.1)

- Textures: `gfx/texture_reload.{hpp,cpp}` (the kind, the path-hash side table, raw retirement, `texture_width_dev`),
  `textures.cpp` (records the path of an asset-loaded texture, Development branch), `gfx_frame.cpp` (the retire hook in a
  Development branch), `texture_table.hpp` (`mutable_get`), `<nnh/texture.hpp>` (`width()`/`height()` read the slot in
  Debug/Development; `cached_width()`/`cached_height()`).
- New tests: `test_texture_reload` (5: the same handle with the new size and the new platform texture in draws; the old
  one freed at the next GPU idle; two textures from one path follow and another doesn't; a bad image refuses the batch;
  released textures read their cached size).
- T4.4's preview by hand (a throwaway driver): `nnh run sprites --pc --no-build`, `art/tiles.png` copied over
  `art/hero.png` while the game runs → `nnh: reload 1: 1 files, 0.6 KB (build 1.62 s)`, `I assets reloaded 1 (frame
  241): art/hero`, `nnh: reload 1 applied at frame 241, 2.16 s after the save`; the exit screenshot shows the hero sprite
  drawn from the tiles image (the example's code untouched: AC-4b's behaviour). The PNG was restored afterwards.

### Stage-end guards after stages 3 (partial) and 4 (T4.1)

Header cost (T2.9's check, by `g++ -E` of a one-line TU): `<nnh/assets.hpp>` 7 643 → 7 698 lines (+55, most of them
the new Doxygen comments, which `-E` keeps as blank lines; the code adds `<nnh/build.hpp>`), `<nnh/texture.hpp>` 7 505
→ 7 526 (+21); `-fsyntax-only` time unchanged within noise.

`nnh_tests` 4102/4102 (pc-debug), 4040/4040 (pc-release); `nnh_tool_tests` 217/217; ctest pc-release 337/337 and pc-debug
(recorded in the stage commit); 3ds-release and 3ds-dev built with 0 warnings; 3ds-release `hello.3dsx` and stripped
`hello.elf` SHA-256 identical to T0.1; pc-release `hello` sizes and `nm -C` identical. 3ds-dev `hello`: text 242 624,
bss 110 456.

### Stages 3–5 results (second session)

- T3.3/T3.6/T3.7: `reload/asset_swap.*` (backups, journal, restore, `swap_backup_dir`, `journal_game_dir`),
  `remote/script_reload.*` (the three lines, paused rules), `script_runner_swap.cpp` (held/unheld flows, expect-reload),
  `commands/run_reload.cpp` (scripts get swaps, SIGINT restores, `finish_reload` rebuilds after restoring,
  `nnh restore-assets`), Ninja directly and the conditional dry run (R22). Scripts `examples/data/tests/reload.nnhs`
  (+ fixture `goblin_fast.enemy`) and `examples/sprites/tests/reload.nnhs` (+ `hero_swapped.png`), ctests
  `reload_data_smoke_pc` and `sprites_reload_script_pc` (each through `tests/reload_example/script_clean.cmake`, which
  also checks `git status` of the game's folder). Smoke: `reload 1 applied at frame 31, 1.03 s after the save`,
  `reload 2 applied at frame 42`; sprites: the `after` screenshot shows the hero drawn from the tile atlas.
- T4.2/T4.3: `loc/loc_reload.cpp` (R25), `loc_state.*` (`load_chain_into`, `chain_block_bytes`), UI (R26).
- T5.2/T5.4: `audio/sound_reload.cpp` (R24), `sounds.cpp` (`decode_sound` split out), `font/font_reload.cpp` (R27).
- Opt-in by linking (R23).
- New tests: `nnh_tool_tests` `test_script_reload` (8), `test_reload_tool` (the Ninja command lines); `nnh_tests` `test_loc_reload` (5),
  `test_ui_reload` (1), `test_sound_reload` (2), `test_font_reload` (2). Totals: `nnh_tests` 4112 (pc-debug),
  `nnh_tool_tests` 225.

- Stage-end guards (stages 3–5): `nnh_tests` 4112/4112 (pc-debug, three runs), 4050/4050 (pc-release); `nnh_tool_tests`
  225/225; ctest pc-release 337/337, pc-debug 375/375 (the two new script ctests failed once in the full run on their
  `git status` check, which counted the staged new script files: the check now compares the working tree with the index;
  re-run green); 3ds-release and 3ds-dev built with 0 warnings; 3ds-release `hello.3dsx` and stripped `hello.elf` SHA-256
  identical to T0.1; pc-release `hello` sizes identical; `check_release_strings.sh` (with the reload texts, T8.2) passes on
  the 3ds-release `hello`, `sprites` and `data` `.3dsx` (the PC `.exe` fail on `NNH_REMOTE`, as before this spec: the
  script is for console binaries).

- T6.1 meshes: `mesh/mesh_reload.cpp` (side table of path hashes per mesh slot, a new record with its textures at
  prepare, `mesh_replace` at commit; the old record freed at once: the apply point comes after the frame's GPU wait and
  before any 3D draw is recorded, and paused frames never apply: R29), `load_mesh.cpp` (`mesh_record_from_file` split out),
  tests `test_mesh_reload` (2).
- **R29 (T6.1) Meshes freed at commit.** A replaced mesh record is destroyed at the apply point itself: the GPU waited at
  this frame's start and nothing of this frame is recorded yet, and a paused frame never applies a batch. Textures keep
  their D47 retire list (written first; both are safe).

- T8.1 mutation pass (18 hand mutants, `nnh_tests`/`nnh_tool_tests` with the module's filter, each reverted): 17 killed,
  1 equivalent. Killed: the payload length off by one, the transfer-frame check, the receiver's offset/CRC/total checks,
  the batch's consistency check and `abandon_all`, the list-frame check of the queries, the name's extension rule, the
  texture retire, the data kind's stale-schema refusal, the strings serial bump, the sounds' `SoundFinished`, the reader's
  receipt-time frame of transfers (by `remote_reader_transfer_messages_skip_the_frame_order`: the first filter missed it),
  the watcher's quiet-period restart, the controller's mirror accept (it survived the first pass: new test
  `reload_controller_an_applied_batch_moves_the_mirror`), the journal written before a swap. Equivalent: the overlay's
  probe `return nullptr` on an empty slot → `continue` (a full scan instead of an early end: same answers, 112 reload tests
  green; only the lookup's cost changes).

- T7.6 ARM pre-check (3ds-dev objects; `build/ref-031/armcheck.py`: objdump per function, then each file recompiled
  with its `compile_commands.json` line plus `-fstack-usage -fno-lto`): no VFP instruction in any of the reload core's
  files (receiver, batch, overlay, apply, wire, names, assets, files, `remote_reader`, `remote_reload`, `crc32_update`);
  one `__aeabi_ldivmod`, in `apply_now` (the apply time in microseconds for the `ReloadApplied` record: once per batch,
  never on an idle frame); `crc32_update` is a 22-instruction table loop (no libgcc call). Largest functions:
  `apply_batch` 504 instructions, `apply_now` 204, `format_list` 208, `on_payload` 132, `RemoteReader::feed` 126. Stack:
  `reload_shutdown` 328 B, `remote_reload_message` 296 B, `apply_now` 288 B, `apply_batch` 136 B, the receiver ≤ 40 B,
  the reader 96 B: all under the 1 KiB target. The idle frame's cost is the session's existing poll plus one null check
  of `g_reload_apply` (R23).

### Stage 7 results (third session)

- `examples/reload` (T7.1, T7.2, T7.4, part of T7.5): assets generated by `art/make_assets.py` (backdrop, hero, a
  20×15 room with a platform object, tiles, `sfx/blip.wav`, `models/gem.glb`: a `.glb` because glTF buffers beside a
  `.gltf` are not packed and a swap of one file must carry its buffer), `enemies/goblin|bat.enemy`, `strings/en|pt-BR`;
  fixtures `hero_big.png`, `room_moved.tmj`, `room_broken.tmj`, `goblin_fast.enemy`, `blip2.wav`, `en_new.strings`,
  `gem2.glb`, `data_types_changed.cpp`. `world.*` (the loads, `refresh()` after `AssetsReloaded`, `snapshot()`),
  `self_check.*` (FNV-1a over 16 fields; `tests/reload_example/test_self_check.cpp`, 3 tests, golden `5C9FEDE9` for a
  fixed snapshot), `reload_scene.*`, `bench_scene.*`, `main.cpp`. Screenshot `build/reload.png` checked (backdrop, room,
  platform, hero, enemies, the gem, the greeting).
- **R30 (T7.2) The self-check line waits for the script.** A script's `expect` only sees lines after the PC attached (the
  simulator attaches at frame ≈ 6, after the first frame's lines), so the start line is logged at frame 60, and each
  batch's line on the frame after the batch (an `expect` after `expect-reload` starts its window on the next frame).
- Scripts: `tests/check.nnhs` (hero, room, goblin, blip, strings, gem, then two restores; sums `2B7C6114` start,
  `1E0979A5`, `B431FED6`, `2C7F4726`, `9EBF4F3F`, `454252F5`, `5FA80812`, `1C2468A3`, `70C46B1C` after 1–8; PASS, 45
  lines, frames 1–183; save → frame 1.07–1.38 s per swap in the simulator, builds 1.0–1.3 s), `tests/errors.nnhs` (a broken
  map refused by the build, the old room kept; a valid map applied; a changed schema refused by the game: "made for another
  struct: rebuild and restart"; PASS, 17 lines). Ctests `reload_check_script_pc` and `reload_errors_script_pc`.
  Screenshots read back: `hero_big` (the green 64×64 hero), `room_moved`, `strings` ("Hello again, new words"), `gem`
  (the tetrahedron).
- Two `nnh` bugs found by the example and fixed test first: (1) a held swap refused by the build (no batch) let the
  next step's messages overtake its queued Resume, which the game then rejected (frame order): the runner now goes on
  only once the notice and the Resume have been taken (`reload_flushed()`, phase 4; test
  `script_reload_a_held_swap_refused_by_the_build_resumes_before_the_next_step`); (2) the controller ignored every
  `ReloadRefused` with the Restart stage as the echo of its own notice, so a batch the game refused for a restart (a
  changed schema) never ended the cycle and the script hung: echoes are told by batch 0 now (test
  `reload_controller_a_batch_refused_for_a_restart_ends_the_cycle`).
- Findings left as they are: the dry run did not see the schema change (the game's schema check refused it instead,
  FR-14's safety net; the Ninja dry run of the game target should list `data_types.cpp.obj`: to look at); the game's
  notice names `assets` instead of the swapped source in scripts (the watcher's list is empty there).
- Bench (pc-debug, Windows): idle frame avg 0.022 ms, worst 0.116 ms over 300 frames; reads 37.5 µs per
  `exists` + `size` (the PC's file system; the console run gives the real numbers).
- Not built from plan.md's list: the sprite library, music and font assets and fixtures (those kinds or the font copy
  aren't part of this run), `RELOAD_SHOT`, the `tight` scene, `timing.nnhs`, `reload_open_file_pc`.
- T2.2's conformance test: `tests/platform_conformance/test_reload_shared_open_pc.cpp` (2 tests in
  `nnh_conformance_pc`: a received file opened with `reload_stream_open` stays readable while the atomic save renames a
  new file over it and while it is removed; a missing file fails with a null stream).

### Stage 8 results and stage-end guards (third session)

- Guards after stage 7: `nnh_tests` reload/mesh/self-check filters green; ctest pc-debug **377/377** (with
  `reload_check_script_pc` and `reload_errors_script_pc`), pc-release **337/337**; 0 warnings on pc-debug, pc-release,
  3ds-release and 3ds-dev (one `-Wmissing-field-initializers` in `test_reload_controller.cpp`, seen only by pc-release,
  fixed); 3ds-release `hello.3dsx` SHA-256 `8c687f3dec3df9fa…` and `hello.elf` after `strip --strip-debug`
  `5cd5d9505dd67b73…` (T0.1's method: `--strip-debug`), identical to T0.1; pc-release `hello.exe` 134 880 / 22 052 /
  212 400, identical. `docs_tool check`: 0 broken links.
- T8.2: `check_release_strings.sh` passes on the 3ds-release `reload`, `sprites`, `data` and `hello` `.3dsx` (the `.elf`
  files keep symbol names such as `dev_pause`, as before this spec: the check is for the `.3dsx`); `nm -C` of the
  pc-release `reload.exe` has no reload symbol. The Release example logs nothing (logging is compiled out), so its start
  sum isn't observable there.
- T8.3 (partial): 3ds-dev `hello` links none of the reload core (`nm`: no `apply_batch`, no `ReloadReceiver`), 3ds-dev
  `reload` links it; **3ds-dev `hello` text 229 416 B** (227 088 at T0.1: +2 328 B, R23), unchanged since stage 5 — for
  the maintainer to confirm (a game that only reads plain files doesn't reload). Sizes: 3ds-dev `reload.elf` text 445 792,
  3ds-release 385 684. The texture-only probe is not built.
- T8.4 (partial): every example's `--pc` script ran in the full ctest. Edit `examples/reload/reload_scene.cpp` → pc-debug
  2.41–2.56 s, 3ds-release `.3dsx` 5.64–5.73 s; the same edit of `examples/hierarchy/showcase_scene.cpp` took 2.55 s on
  the same machine state, so the example costs what the others cost; both are over CLAUDE.md's 2 s / 5 s budgets in this
  run (`compare.sh` against T0.1 not run: no reference tree, the disk is tight).
- Asset build latency (coordinator's point): 0.88 s build, 1.03 s save → frame for a data asset (R22); in the example's
  script 1.0–1.3 s builds and 1.07–1.38 s save → frame per swap (a held swap includes the pause round trip). Under 1 s
  for the build alone but not save → frame: what is left is CMake's glob re-check (0.28 s, it runs because
  `nnh_add_game` globs the assets folder) and Ninja's start-up; removing the glob check needs the asset list written by
  `nnh` instead of globbed (a CMake change, not cheap): left for the lead.

### Console run 1 (2026-11-26, Old 3DS XL, 3ds-dev of `eac9abc`, run by the lead)

Logs: `E:\RANDOM
nhge-console-r-{a,b,d,e}.log` (A, B, D, E of "Stage 10: console run (prepared)"); C (the interactive
sprites yes/no) waived: B covers the same swap by script.

| Run | Verdict | Numbers |
|-----|---------|---------|
| A `data/tests/reload.nnhs` | PASS (11 lines), applied at frames 31 and 42 | save → frame 5.17 s and 5.09 s; the 3DS asset build through MSYS 3.00 s and 2.82 s |
| B `sprites/tests/reload.nnhs` | PASS, applied at frame 32 | 5.83 s (build 1.66 s); the `after` screenshot (`build/logs/sprites-20261126-230249/`) shows the swapped texture on the hero and on the rotating sprite |
| D `reload/tests/check.nnhs` | PASS (45 lines), put back 4 | all 9 sums equal the PC's (`2B7C6114` … `70C46B1C`); 8 reloads (hero, room, goblin, blip, strings/en + qps, gem, hero, room); save → frame 2.99–3.33 s, the first 5.93 s; builds 2.76–3.08 s; the frame-60 start line was in time |
| E `reload/tests/errors.nnhs` | PASS (17 lines), put back 2 | the broken map refused (conversion failed, "old content kept"); the valid map applied at frame 83 (3.10 s); the schema change refused by the game ("made for another struct: rebuild and restart"); that build 6.61 s (the schema program rebuilt) |

The engine side holds on the console: same sums, same frames as the simulator, every kind applied whole. **AC-8's
< 3 s save → frame is missed by a little** (3.0–3.3 s; 5–6 s for a session's first swap): the transfer and the apply are
a few frames, the 3DS asset build through devkitPro's MSYS is ≈ 2.8–3.1 s of it. Measured on the PC afterwards (warm):
`scripts/dkp.sh true` 1.0–1.2 s, a no-op `scripts/dkp.sh cmake --build --preset 3ds-dev --target data_assets` 2.7–2.8 s;
inside one MSYS shell a no-op `ninja data_assets` still costs 1.65–1.78 s, of which the host tool's ExternalProject steps
(`nnh_assetc` build, install and complete: three `/bin/sh` scripts and a sub-Ninja, run every time by `BUILD_ALWAYS`)
are ≈ 1.2 s and CMake's glob check ≈ 0.3 s. So the optimisation round (T10.3) is in the build, not in the engine.

**Optimisation round (T10.3, PC side only; no wire or golden change).** (1) `cmake/nnh_host_tools.cmake`: the
`nnh_assetc` ExternalProject loses `BUILD_ALWAYS`; its build step depends on the files it compiles (`tools/assetc/**`,
`engine/core/src|include`, the stb libraries, globbed at configure time); checked: touching an assetc source or
`crc32_update.cpp` rebuilds the tool, a data edit doesn't. (2) `nnh` calls devkitPro's Ninja (`CMAKE_MAKE_PROGRAM`,
`/usr/bin/ninja.exe`) inside its shell instead of `cmake --build --preset` (test `reload_build_commands_per_preset`).
Measured (3ds-dev, warm): a data-file edit's asset build 2.8–3.0 s → **1.57–1.63 s**, a PNG 1.95 s (tex3ds), a no-op
`scripts/dkp.sh … data_assets` 2.7 → 1.9 s. Expected save → frame ≈ 1.8–2.3 s on the console (run 1's apply and transfer
were ≈ 0.2–0.3 s on top of the build): console run 2 measures it. Left: a persistent MSYS shell (≈ 0.5 s more of
bash login per cycle) and the glob check (≈ 0.3 s).

- **R31 (findings of stage 7, fixed) The program check is `ninja -t inputs`, and scripts name their source.** The dry
  run `ninja -n <game>` never worked in a game with an assets folder: CMake's glob check makes Ninja plan a CMake re-run
  first, and a dry run stops there ("Re-running CMake", which it then ignored). `nnh` now lists the program's inputs
  (`ninja -t inputs <game>`, no manifest check) and refuses when the changed file outside the assets folder is one of
  them (`program_input`, test `reload_program_input_finds_a_changed_source`): `errors.nnhs`'s schema swap is now refused
  by `nnh` before anything is sent (`data_types.cpp is compiled into the program: rebuild and restart`; the game logs
  `data_types.cpp: needs a rebuild and restart`). A script's swap passes its source to the cycle, so a refusal names it
  (`levels/room.tmj: conversion failed`) instead of `assets` (test
  `reload_controller_a_script_source_names_the_notice_and_is_checked_when_outside`).

### Stages 5–7 completed (fourth session, after console run 1)

- **T5.3 music** (`audio/music_reload.cpp`, `music.cpp`'s `music_reload_reopen`, `MusicTrack::seek`): a playing track's
  file is checked at prepare (a temporary track and Ogg arena opened on the staged file, its loop point applied) and
  reopened at commit at the sample the decoder had reached (the loop start when the new file is shorter; the end for a
  track that doesn't loop); the platform's queued buffers play out first; `position()`, volume and fades go on (`submitted`
  is set so that position() is what the player hears). Tests `test_music_reload` (4: same sample, past the end, an Ogg
  track, a fade kept and a damaged file refused).
- **T5.1 sprite libraries** (`animation/sprite_reload.cpp` the kind, `sprite_reload_table.cpp` the 64-entry table kept
  apart so animators link no reload code, `animator_rebind.cpp`, `sprite_load.cpp`'s `sprite_block_from_file`): as planned,
  with two refinements. **R32** the identity block is flagged in `Header::reserved[0]` (memory only; files keep 0) and
  `sprite_view` forwards it to the current block (one byte test for other libraries); the previous current block is
  kept until the next reload (not freed at commit) so an animator bound to it can still read its clip names; the one
  before is freed. **R33** a freed block's address can come back as a later block (seen in the test: the same size), so
  the animator keeps the library's reload epoch in the two free bits of its `flags_` (no size change): a pointer match
  with another epoch is treated as unreadable. Animators rebind in `Access::library()` and, through `Access::fresh()`, in
  the queries that read their own fields (`clip()`, `state()`, `playing()`...); the old block is read only when it is the
  identity or the previous block of the previous epoch. A state machine is "the same" when the states' names, next-on-
  finish and clips by name and the rules byte for byte agree (clip indices may move). Tests `test_sprite_reload` (3),
  `test_animator_rebind` (5); `loading_leaves_only_the_librarys_block` counts the Development table.
- **T6.2 FR-23** (`live/live_reload.cpp`, two `LiveOps` entries, `app.cpp`): in a batch's frame, after the application
  point, up to 16 tunables away from their default are noted; at the frame end one whose bits changed logs
  `live: player/speed was set by a reload (panel value 5.25, file value 3)`; nothing written, `changed()` untouched.
  Test `test_live_reload` (1).
- **Example**: `examples/reload` gains `art/walker.ase.json` + `walker.png` (a sprite library and its animator), the HUD
  BMFont from `examples/localization` (the project's own art, made by its `make_hud_font.py`; a `.fnt` instead of the
  plan's OFL subset to keep 270 KB out of the example) and `music/loop.wav`; fixtures `walker_v2.ase.json`, `hud_wide.fnt`,
  `loop2.wav`; the self-check folds 21 fields (walker clips, clip name, position, a HUD-font width, music playing; test
  golden `F40673B9`). `check.nnhs` now swaps all eleven (hero, room, goblin, blip, strings, gem, walker, music, font,
  then the two restores): sums `770785CB` start, `85960F26`, `81450DE1`, `D21D8C71`, `6A8DD95C`, `50F69476`, `6EFB266D`,
  `1FDA416C`, `A8A08A0C`, `9A8E8270`, `098F5CDD`, `1294E48A` after 1–11 (PASS, 59 lines, frames 1–221; put back 7).
  `timing.nnhs`: unheld swaps of a PNG, a map and a data file (`within 600`), then X runs the bench (`reload bench: idle
  avg 0.042 ms worst 0.231 ms`, `reads 19.72 us per read` on pc-debug); ctest `reload_timing_script_pc`. The music swap
  in `check.nnhs` streams the file while it is swapped (the Windows sharing case of AC-9, so `reload_open_file_pc` isn't
  a separate test).
- More tests: `sound_reload_without_linear_memory_refuses_and_keeps_the_old_sound` (the fake platform gains
  `set_sample_create_fails`), `mesh_reload_without_memory_for_the_new_model_refuses`,
  `mesh_reload_draws_recorded_after_the_swap_use_the_new_model`.

### Guards at the end of the fourth session

ctest pc-debug **378/378** (with `reload_check_script_pc`, `reload_errors_script_pc`, `reload_timing_script_pc`),
pc-release **337/337**; 0 warnings on pc-debug, pc-release, 3ds-release and 3ds-dev; 3ds-release `hello.3dsx`
`8c687f3dec3df9fa…` and `hello.elf` (`strip --strip-debug`) `5cd5d9505dd67b73…`, identical to T0.1; pc-release `hello.exe`
134 880 / 22 052 / 212 400, identical; `check_release_strings.sh` passes on the 3ds-release `reload` and `sprites` `.3dsx`;
`docs_tool check` 0 broken links. **3ds-dev `hello` text 229 488 B** (+2 400 B over T0.1's 227 088; +72 B this session:
FR-23's frame flag and calls), for the maintainer to confirm as before. T8.3's probes by `nm` on 3ds-dev: `hello` (plain
files only) links no reload code; `sprites` (textures only) links the reload core and the texture kind only; `reload`
links the texture, sound, music, font, mesh and sprite-library kinds.

### Not done (open, for the lead or a next session)

- Stage 0: T0.1–T0.3's reference comparisons against `cf15822` need a reference tree (a clone plus four preset builds,
  ≈ 8–10 GB); E: has ≈ 10 GB free, so they were skipped. The hello checks and the full ctest stand in for them.
- Stage 1: T1.6's 60 s libFuzzer run (clang; the lead on VM 120).
- Stage 2: T2.8's guard bytes around the buffers (ASan on the VM checks the bounds).
- Stage 3: T3.4's loopback stress test (a game logging heavily during a 4 MB transfer).
- Stage 4: T4.1's tests of tileset and mesh textures following a texture reload and of a texture refused for Linear
  memory (the fake platform's texture_create has no failure control yet); T4.2's tests of the pseudo-language and of a
  font rebaked in the same batch.
- Stage 5: T5.1's test of a library moved after loading and ASan's proof (VM); T5.3's test of a stream whose file is
  replaced underneath (`fake::replace_asset`); T5.4's full-texture-table and fallback tests.
- Stage 6: T6.1's tests of more submeshes and new textures; T6.2's "17 edited → 16 tracked" and "no live values links
  nothing" tests.
- Stage 7: `RELOAD_SHOT` (the scripts take the screenshots instead), the `tight` scene with `huge.png` (errors.nnhs's
  linear-memory refusal on the console), the plan's OFL font copy (the HUD BMFont is used instead).
- Stage 8: T8.4's `compare.sh` against T0.1 (no reference tree).
- Stage 9 (the lead, VM 120) and console run 2 (below).
- Stage 11: final statuses after console run 2 and the Linux stage.

## Stage 10: console run (prepared)

Console run 2 (build first: `scripts/dkp.sh cmake --build --preset 3ds-dev --target reload_3dsx`, 0 warnings expected;
then Homebrew Launcher, press **Y**, for each of D, E, F). A, B and C passed or were waived in console run 1.

A. `build/pc-debug/tools/nnh/nnh run data --script examples/data/tests/reload.nnhs` (Homebrew Launcher, **Y**) → expected
   lines: `nnh: reload 1: 1 files, 0.1 KB (build …)`, the game's `I assets reloaded 1 (frame 31): enemies/goblin`,
   `nnh: line 6: expect-reload applied at frame 31`, then `reload 2` / `line 9: expect-reload applied at frame 42`,
   `nnh: PASS (11 lines, frames 1-53)`; afterwards `git status` clean (`goblin.enemy` restored and rebuilt). Note the
   `applied at frame …, X s after the save` times (the Wi-Fi transfer of a 0.1 KB file plus the build).
B. `build/pc-debug/tools/nnh/nnh run sprites --script examples/sprites/tests/reload.nnhs` → `expect-reload applied at
   frame 31`, `PASS`; compare `build/logs/sprites-<date>-<time>/frame-000033-after-top.png` with the `--pc` one (the hero
   drawn from the tile atlas).
D. `build/pc-debug/tools/nnh/nnh run reload --script examples/reload/tests/check.nnhs` → the same sums as the PC (console
   run 2: every kind): `reload: sum 770785CB after 0`, then `85960F26`, `81450DE1`, `D21D8C71`, `6A8DD95C`, `50F69476`,
   `6EFB266D`, `1FDA416C` (walker), `A8A08A0C` (music), `9A8E8270` (font), `098F5CDD`, `1294E48A` after 1–11,
   `nnh: PASS (59 lines, …)`, `nnh: put back 7 swapped assets`; compare the screenshots `hero_big`, `room_moved`,
   `strings`, `gem`, `walker`, `font` in `build/logs/reload-<date>-<time>/` with the `--pc` ones. Note each `applied at
   frame …, X s after the save` (AC-8; with T10.3 the build should be ≈ 1.6 s instead of 2.8–3.1 s).
E. `build/pc-debug/tools/nnh/nnh run reload --script examples/reload/tests/errors.nnhs` → `line 8: expect-reload refused`
   (the broken map: the game logs `levels/room.tmj: conversion failed`), `line 12: expect-reload applied`, `reload refused:
   …/data_types.cpp is compiled into the program: rebuild and restart (nnh run)` (now refused by `nnh` before sending),
   `PASS (17 lines, …)`, `put back 2`.
F. `build/pc-debug/tools/nnh/nnh run reload --script examples/reload/tests/timing.nnhs` → three `reload N applied at frame
   …, X s after the save` lines (a PNG, a map, a data file: AC-8's < 3 s), then `reload bench: idle avg … ms worst …` and
   `reload bench: reads … us per read`; `PASS (19 lines, …)`.

C. The interactive yes/no loop below (about 2 minutes; the lead edits a file on the PC):

1. PC: `scripts/dkp.sh cmake --build --preset 3ds-dev --target sprites_3dsx`. Console: Homebrew Launcher, press **Y**.
   PC: `build/pc-debug/tools/nnh/nnh run sprites` (it prints `nnh: hot reload on: saving a file in .../examples/sprites/
   assets reloads it`). Change a few pixels of `examples/sprites/assets/art/hero.png` in an image editor (same size) and
   save it. Expected on the PC: `nnh: reload 1: 1 files, ... (build ...)`, the game's line `I assets reloaded 1 (frame
   N): art/hero` and `nnh: reload 1 applied at frame N, X s after the save: art/hero`. **Does the console show the new
   hero within 3 s, without a restart? yes/no.** Note X (save to frame).
2. Same run: save the PNG at another power-of-two size (e.g. twice as wide). Expected: applied as above; the sprite draws
   at its new size. yes/no.
3. Same run: save it at 30 x 30 pixels. Expected: `nnh` prints the image check's error and `reload refused: conversion
   failed`; the console logs `W assets reload refused (frame N): art/hero.png: conversion failed (see nnh) (old content
   kept)` and keeps drawing the previous hero. yes/no.
4. `git checkout examples/sprites/assets/art/hero.png`, let it reload, close the game; `git status` clean.

Log: `build/logs/sprites-<date>-<time>.txt`. The first failure to look for: `transfer: could not write the file` (the SD
card's `sdmc:/3ds/<data folder>/.nnh_reload/` could not be created or written).
