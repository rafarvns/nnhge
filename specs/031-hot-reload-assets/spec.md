# Spec 031: Hot reload of assets

Status: approved 2026-11-26 (the maintainer, every open question as recommended); in progress (stages 1–3 done; stages 4–5 partly; see plan "Not done")
Author: NNHGE maintainers · Date: 2026-11-26
Depends on: [spec 007](../007-assets-and-romfs/spec.md) (the assets folder, one build step per file, the asset index
`.nnh_assets`, romfs on 3DS and a folder next to the program on PC), [spec 010](../010-nnh-cli-run/spec.md) (`nnh run`
builds and keeps the game's log connection), [spec 013](../013-console-remote/spec.md) (D28: frame-stamped messages and
in-band records over that connection, `.nnhs` scripts), [spec 002](../002-core-foundation/spec.md) (build flavors and
`kDevToolsEnabled`, logs, tagged memory, `nnh::fs` confined to the game's folder),
[spec 030](../030-live-values/spec.md) (D51's application point in `nnh::frame()`, D52's pause; its open question Q5,
"writing tunables back to data assets", was deferred to this spec). Reloads the kinds of
[spec 004](../004-2d-graphics/spec.md) (textures), [spec 012](../012-tilemaps-and-collision/spec.md) (Tiled maps),
[spec 014](../014-data-assets/spec.md) (data assets), [spec 016](../016-sprite-animation/spec.md) (Aseprite sprite
libraries), [spec 008](../008-audio/spec.md) (sounds and music), [spec 026](../026-3d-foundation/spec.md) (meshes) and
[spec 029](../029-game-fonts-and-localization/spec.md) (fonts and string tables) without needing any of them. Implements
the ROADMAP item "Hot reload of assets in the simulator" (Features in the spirit of Godot and Unity), the fourth and last
feature before NNHGE 1.0, and extends it to the console (open question Q1). A development tool: Debug and Development
builds only; optional for games (constitution XI).

## Problem and goal

Art, levels, tables, sounds and text change far more often than code, and today every such change in NNHGE costs a
rebuild and a restart: `nnh run` rebuilds the game, the developer plays back to the room they were looking at, and on
the console the `.3dsx` is sent again and Y pressed again. A pixel artist touching up a sprite, a designer moving a
platform in Tiled or a writer fixing a line of dialogue waits for a whole cycle each time. Engines with editors reload
changed files in the running game: Bevy swaps the data behind the handles a game already holds and tells it with an
`AssetEvent::Modified` ([Bevy hot reloading](https://github.com/bevyengine/bevy-website/blob/main/content/learn/book/development-practices/hot-reloading.md)),
and Defold pushes edited resources from the editor to a debug build running on a phone
([Defold hot reload](https://defold.com/manuals/hot-reload/)). After this spec, while a Debug or Development game runs
under `nnh run` — in the PC simulator **and on the 3DS** — saving an asset file makes the build reconvert it exactly as
a normal build would, and the running game picks up the change **at one point of a frame**, without a restart, a
relink or a resend: textures, meshes, fonts, sprite libraries, sounds and string tables change behind the handles the
game already holds; data assets, tilemaps and plain files are announced so the game reloads its own copies; a broken
file never reaches the game. A Release build has none of it.

## User stories

- As a game developer, I want to save a PNG in my image editor and see the new pixels on the running simulator and on
  the console in hand a moment later, without leaving the room I'm testing, so that art iterations take seconds.
- As a game developer, I want to move a platform in Tiled, save, and have my level scene reload the map when I ask it
  to (or at once, by my own choice in code), so that I can try a jump against the new layout immediately.
- As a game developer, I want to change an enemy's speed in `goblin.enemy`, save, and have the goblins use the new value
  on the next frame because my scene reloads the data it was told changed, so that data files become my tuning tool.
- As a game developer, I want to re-export an Aseprite sheet and see the walking animation change while the character
  keeps walking, so that animation timing is tuned in place.
- As a game developer, I want to fix a typo in `strings/pt-BR.strings` and see my menus show the new text (and a font
  rebaked for a new character) without a restart, so that localisation can be checked in context.
- As a game developer, I want a file with a mistake to show me `file:line: reason` and leave the game running with the
  old content, so that a bad save never crashes my session.
- As a game developer, I want a test script to swap an asset and check the result on the PC and on the console, so that
  reloading is tested like the rest of the engine.
- As a game developer, I want my Release build to contain none of this, and to pay nothing for it when I don't use it.

## Functional requirements

### The edit loop

- **FR-1 Noticing a change.** While `nnh run <game> [--pc]` is connected to a Debug or Development game, saving,
  adding, removing or renaming a file in the game's assets folder (its subfolders included) starts a reload. Who
  notices (the `nnh` tool or the game) is open question Q2. Saves made in several steps (editors that write a temporary
  file and rename it, a Tiled save that rewrites a map and its tileset) are gathered into **one** reload after a short
  quiet period. The developer can also start a reload by hand (an `nnh` command) and turn watching off (`nnh run
  --no-reload`).
- **FR-2 Converted exactly as the build would.** A changed source is reconverted with the **same tools, checks and
  output bytes** as a normal build (spec 007's steps, `nnh-assetc`, `tex3ds` on 3DS), together with everything that
  depends on it: maps using an edited tileset, fonts whose character set comes from an edited string file (spec 029),
  the data index after any data asset (spec 014), the asset index after an addition or removal. How (`cmake --build` of
  the game's asset target, or `nnh-assetc` called directly) is open question Q3; either way the game's code is not
  recompiled or relinked and, on 3DS, the `.3dsx` is not repacked or resent.
- **FR-3 Errors never reach the game.** A source that fails its conversion (a refused map, a data file with a wrong
  field, an image of a bad size) reloads nothing: `nnh` prints the converter's `file:line: reason` messages as a normal
  build does, the game logs one warning line naming the file, and it keeps running with what it had. Saving a fixed
  file reloads normally.
- **FR-4 One batch, applied whole.** Every converted output that changed in one reconversion forms **one batch**,
  applied in a single frame: a game never sees a new string table with the old font, or a new map with the old tileset
  image. Outputs whose bytes did not change are not reloaded. If any file of a batch can't be applied on the game's
  side (FR-16, FR-18), none is.
- **FR-5 Delivery to the game.** In the simulator the game reads the new outputs from its assets folder. On the console
  the converted bytes are sent over the existing connection (spec 013) and kept where the game's asset reads find them
  before the romfs (open questions Q1 and Q4); the romfs itself never changes. In both cases the game reads a reloaded
  asset through the same `assets::` calls and loaders as any other, and later loads of that path (a scene entered
  again) get the new content too, until the program ends.
- **FR-6 One point in the frame.** A batch is applied at a fixed point of `nnh::frame()` — with spec 030's live-value
  changes (D51), after the frame counter advances and before the frame's input events, D32's hooks, audio, event
  dispatch and the scene update — never during the game's update or drawing. Every batch is stamped with the frame it
  applies to, so a script's reload lands on the frame it names, the same in the simulator and on the console. What
  happens while the game is paused (D52) is open question Q10.

### What a reload does to loaded assets

- **FR-7 Engine-owned handles stay valid and show the new content.** A `Texture`, `Mesh` (and the textures it owns),
  `Font`/`FontRef`, sprite library and `Sound` loaded from a reloaded path keep working without any game code: the same
  handle now refers to the new content, including when its size changed (a texture of another power-of-two size, a mesh
  with more vertices, a font with other metrics, a library with more frames). Engine objects built on them follow: a
  tilemap's tileset textures, UI images and 9-slices, font pages, a mesh's textures. Memory the old content used is
  freed only once the GPU no longer reads it (D47). A draw recorded in a frame uses one version for the whole frame.
- **FR-8 Running animations survive.** A sprite animator whose library was reloaded never reads outside the new
  library: it continues the clip of the same name at its current frame (clamped into the clip's new length) with the new
  durations and markers; if that clip no longer exists it switches to the library's first clip and logs one warning; a
  state machine whose states or transitions changed restarts at its entry state with a warning.
- **FR-9 Sounds and music.** A reloaded `Sound` plays its new samples on the next `audio::play`; voices already playing
  it are handled as open question Q7 recommends (stopped at the reload point, ending with the usual `SoundFinished`).
  Music streaming a reloaded file continues from the same position in the new file (or its loop start, when the
  position is past the new end); a crossfade in progress keeps its timing.
- **FR-10 String tables.** When a string file of the current language or of its fallback chain is reloaded, the
  localisation module reloads the chain, bumps its serial so `text_changed()` and spec 028's widgets re-resolve their
  text, and keeps `tr` pointers valid exactly as long as D50's language switch does (until the reload point). Keys
  added or removed take effect at once (a removed key shows its name in Debug/Development, as an unknown key does).
  Fonts rebaked for new characters arrive in the same batch (FR-4). The Debug pseudo-language follows the base.
- **FR-11 Game-interpreted content is announced, not rewritten.** The engine never changes the game's own copies of
  content: structs filled by `data::load`, a `Tilemap` the game created or loaded (and the grids, path maps, prefab
  spawns and `set_tile` edits made from it), bytes read with `assets::load`/`read`. For these the game is told what
  changed (FR-13) and reloads them itself, e.g. `data::load` into the same struct or `load_tilemap` again; the data
  index is refreshed in the batch, so `count`/`name`/`find`/`path` describe the new set of data assets. Whether
  tilemaps should also reload in place automatically is open question Q6.
- **FR-12 Added, removed and renamed assets.** An added asset can be loaded from the frame its batch is applied. A
  removed asset can no longer be loaded (`NotFound`), but what the game already loaded from it keeps working with its
  last content, and one warning is logged; a rename is a removal plus an addition.
- **FR-13 Knowing what changed.** The game can ask, in any frame: a **reload counter** that increases by one per
  applied batch, whether a given asset path was reloaded **in this frame**, and the list of paths reloaded in this
  frame (asset names as `load_*` takes them). The exact notification shape (queries only, or queries plus one bus
  event) is open question Q5. In Release the counter is a constant 0, the path query `false` and the list empty.
- **FR-14 Changes that need a restart are refused.** A change the running program can't take is refused with a message
  that says to rebuild and restart, and nothing of its batch is applied: a data asset converted for a struct whose C++
  changed (D29's fingerprint), a change to the game's CMake declaration (languages, the assets folder), images embedded
  with `IMAGES` (D19), anything compiled into the program. `nnh run` without `--no-build` still rebuilds and restarts
  as today.
- **FR-15 Visible feedback.** Each applied batch is logged on the game's side as one line with its frame, the number of
  assets and their names (truncated), for example `I assets reloaded 3 (frame 1234): art/hero, levels/level1, ...`;
  `nnh` prints the same with the time from the save to the applied frame. A refused batch (FR-3, FR-14, FR-16) is one
  warning line with the reason.

### Memory, safety, cost

- **FR-16 Different sizes, no half states.** A reload never assumes the new content fits the old memory: new memory is
  allocated before the old is released (under the asset kind's own memory tag), and when an allocation, a texture or
  font slot, linear memory or a configured capacity runs out, the whole batch is refused (FR-4), the old content stays,
  and the warning says which asset and how many bytes were missing. Blocks the engine sized at start from the game's
  assets (the data index, the localisation block sized by the largest chain, D50) are reallocated when a reload makes
  them larger.
- **FR-17 No per-frame heap; no cost when nothing changes.** Allocation happens only when a batch is applied. A frame
  without a batch costs no measurable time (one check, no walk over loaded assets). Bookkeeping that lets a reload find
  the loaded objects of a path (open question Q12) has fixed or load-time-allocated memory, never per frame.
- **FR-18 Bounded and safe.** Every byte the game receives from the PC for a reload is bounded and validated: transfer
  messages by size and sequence, every converted file by the same validator its loader already runs (maps, sprite
  libraries, meshes, fonts, string tables, data files, the indexes) **before** anything changes; a bad message or file
  is ignored, logged and counted like spec 013's rejected messages, never a crash; the console-side reader is a fuzz
  target. Files the game writes for this (if any, Q4) stay inside the game's own folder (`nnh::fs`).
- **FR-19 Release has none of it.** In a Release build the game reads only its packed assets (romfs or the folder next
  to the program), has no reload code, data, messages or strings, and FR-13's queries are constants;
  `examples/hello`'s Release `.text` is unchanged on PC and 3DS. Debug and Development builds have all of it.
- **FR-20 Optional and out of the way.** A game that does nothing gets FR-7 to FR-10 for free in Debug/Development and
  behaves exactly as today when no file changes. A game that ignores FR-13 keeps its own copies of data and maps until
  it reloads them. Hot reload needs no data assets, no scenes, no UI and no live values; a plain `while (nnh::frame())`
  game gets it too.
- **FR-21 Same results on PC and 3DS.** The same batch applied at the same frame gives the same handles, the same
  values loaded by the game, the same animator states and the same pixels (within each system's existing drawing
  tolerance) in the simulator and on the console.

### Scripts and `nnh`

- **FR-22 Commands.** `nnh run` gains a typed `reload` (reconvert and send now, even if nothing was saved) and
  `.nnhs` scripts gain commands to swap a source asset for a test fixture and to wait for the batch to be applied
  (open question Q11 for the form), so a script checks a reload with the existing `expect` lines and screenshots, on
  the PC and on the console. Script errors are reported with their line before anything is sent, as today.

### Live values (spec 030)

- **FR-23 Reloads and tunables.** A reload never writes a tunable by itself. When a game reloads a data asset into a
  struct whose fields are registered as tunables (030's `live::tune(…, &enemy.speed, …)`), the panel and `nnh` show the
  file's value from that frame, and a value set earlier in the panel is overwritten by the file (the file wins); `nnh`
  warns once when a batch overwrites a tunable that differed from its default. How a tunable's default follows a reload,
  and whether tuned values are ever written back into data asset files (030's Q5), is open question Q9.

## Acceptance criteria

- **AC-1 Example** `examples/reload` (name confirmed by the plan; original art): one scene with a static image, an
  Aseprite-animated character, a small Tiled room, two data-driven enemies, a sound on a button, music, a label in a
  game font from a string table, and a 3D model when 3D is in scope (Q8). Reloads are shown by screenshots before and
  after (`RELOAD_SHOT=…`, `NNH_EXIT_AFTER_FRAMES`, `NNH_SCREENSHOT`) and by a self-check line (a checksum of the values
  and handles the scene sees after each batch).
- **AC-2 One script, PC and console.** `examples/reload/tests/check.nnhs` swaps each kind in turn for a fixture (image
  of another size, sheet with a renamed clip, map with a moved platform, data file with new values, string file with a
  new character, sound, music, font, mesh), waits for each batch and checks the log, the self-check line and
  screenshots; it passes with `--pc` and on the Old 3DS XL with the same self-check values, the maintainer only pressing
  Y; the source tree is unchanged afterwards (`git status` clean).
- **AC-3 Errors and refusals.** The script also saves a map with an error (the `file:line` message appears in `nnh`'s
  output, the game logs one warning and keeps drawing the old room, then the fixed map reloads), a data file for a
  changed struct (refused, "rebuild and restart") and, on the console, an image too large for the remaining linear
  memory (whole batch refused, old content kept).
- **AC-4 Unit tests** (written first, constitution VI) on the fake platform: batch application at FR-6's point and
  stamped frame; all-or-nothing batches (one bad file leaves every old asset); handle validity for each kind across size
  changes; GPU-deferred frees (D47) of replaced textures and meshes; animator survival (same clip, clamped frame, removed
  clip, changed state machine); voices and music on reload; string chain reload, serial bump and block growth; data
  index refresh (added, removed, renamed); FR-13's queries (counter, per-path, list, Release constants); FR-14 refusals;
  allocation-free frames without a batch; a tunable registered on a data-loaded field shows the file's value after the
  game reloads it, with `changed()` untouched by the reload (FR-23).
- **AC-4b Nothing to write.** An existing example with no reload-aware code (`examples/sprites`) run under `nnh run
  --pc --script` picks up a swapped PNG on its next frame (screenshot), proving FR-20.
- **AC-5 `nnh` tests** (tool side): change detection with temporary-file-and-rename saves and bursts gathered into one
  reload; outputs unchanged by a reconversion not sent; dependents included (tileset → maps, strings → fonts, data →
  index); conversion errors reported without sending; the new script commands in the `.nnhs` parser with line errors;
  fixtures swapped and restored even when the script fails or is interrupted.
- **AC-6 Bounded and fuzzed.** The console-side reader of reload messages is in the mini-fuzz of `nnh_tests` and a
  libFuzzer target, run under ASan/UBSan on the Linux build VM without findings; tests feed every limit plus one.
- **AC-7 Release is clean.** `scripts/check_release_strings.sh` finds no reload string in the Release
  `examples/reload`; a symbol check finds no reload code; `examples/hello`'s Release `.text` is unchanged on PC and 3DS.
- **AC-8 Speed and cost**, measured and written into the budgets: time from saving a 64×64 PNG, a `.tmj` and a data file
  to the applied frame, in the simulator (proposed target < 1 s) and on the Old 3DS XL over Wi-Fi (proposed < 3 s for
  ≤ 256 KB of converted bytes); frame time without a batch unchanged within noise; the length of a reload frame for
  each kind; the game's build budgets (constitution III) unchanged.
- **AC-9 The simulator never blocks the build.** On Windows, reconverting an asset the running simulator holds open
  (music being streamed, a file read with `assets::load` in progress) succeeds and reloads normally (test on Windows).
- **AC-10 Docs.** `docs/api/assets.md` (reloading, FR-13's queries), the API pages of every reloaded kind (what happens
  to handles, animators, voices, strings), the nnh tool guide (watching, `--no-reload`, `reload`, script commands), the
  testing-on-the-console guide, a guide section "editing assets while the game runs", a decision row in
  `docs/architecture/decisions.md`, `docs/tech/cmake.md` if the asset pipeline changes, CLAUDE.md and ROADMAP updated,
  `docs/REFERENCES.md` citing the prior art below.

## Non-goals

- Hot reload of game code (the ROADMAP's separate later item), of `DATA_SCHEMA` structs, of the game's CMake
  declaration or of `AppConfig`: these restart (FR-14).
- An editor, an asset browser or a file picker; reloading assets the game didn't declare in its assets folder;
  images embedded with `IMAGES`.
- Rebuilding game state from new content: the engine doesn't respawn a map's objects, redo `set_tile` edits, re-sync
  path maps or reload a game's structs; it tells the game (FR-11, FR-13).
- Reloading in Release builds, in games not started by `nnh run` (NNH Link-started games, spec 011, like spec 013; a
  simulator started from an IDE, see Q2), or from a PC other than the one `nnh run` connected to.
- Undo, history or rollback of reloaded assets; keeping reloaded content after the program ends (the next `nnh run`
  builds and sends it normally).
- Writing tunables into data asset text files or C++ sources, unless the maintainer chooses otherwise in Q9.
- Streaming the whole asset folder to the console instead of packing it (Godot's network filesystem): the `.3dsx`
  keeps its romfs; only changed outputs travel.

## Constraints

- **I — Same game code everywhere.** No platform `#ifdef` in game code or public headers; reload behaviour follows the
  build flavor (`kDevToolsEnabled`), as spec 030's tools do, not the platform. The console and the simulator apply the
  same batches at the same frames (FR-21); the console's different delivery (FR-5) is invisible to the game.
- **II — The 3DS is the constraint.** A reload holds old and new content at once for a moment (FR-16), so a large
  texture may not fit on an Old 3DS where it would after a restart; refusing cleanly is the rule, and the plan sizes any
  staging memory for the Development budget. Received files must not need a RAM copy of the whole asset folder.
- **III — Build speed is a feature.** A reload reconverts only what changed and never compiles or links game code; the
  plan measures save-to-frame times and checks the edit-one-`.cpp` budgets are unchanged.
- **IV — Event-driven.** Changes are queried (FR-13) and applied at one point of the frame (FR-6); whether one bus event
  for "a batch was applied" is worth adding is open question Q5.
- **V — No hidden costs.** No exceptions, no RTTI, no `virtual` per object, no per-frame heap; allocations only in the
  frame that applies a batch (FR-17).
- **VI — Spec first, test first.** Batch logic, per-kind swaps, queries, refusals, the wire reader and `nnh`'s watcher
  and script commands are unit-tested first on the fake platform and the tool's tests; the visible result is checked by
  screenshots and the script on PC and console.
- **VII — Small files**, one public header per module (Q13: FR-13's queries in `<nnh/assets.hpp>`).
- **VIII — Documented API** (AC-10).
- **X — Safe by default.** The console reads untrusted bytes from the PC: bounded, validated by the loaders' own
  validators before use, fuzzed (FR-18); any file written for reloads stays in the game's own folder and is never read
  by a Release build.
- **XI — Opt-in, never in the way; no editor.** Files keep coming from common tools (image editors, Tiled, Aseprite,
  text editors); nothing is required of the game (FR-20); `--no-reload` turns it off.
- **Spec 013 compatibility.** New messages and records extend D28's wire format additively; scripts written for specs
  013 and 030 run unchanged.
- **D47.** Replaced GPU resources are freed only after the GPU is done with them.

## Prior art

- Bevy: a file watcher reruns the loader, `Assets<T>` swaps the value behind existing handles and an
  `AssetEvent::Modified` tells systems ([hot reloading in the Bevy book](https://github.com/bevyengine/bevy-website/blob/main/content/learn/book/development-practices/hot-reloading.md),
  [example](https://github.com/bevyengine/bevy/blob/main/examples/asset/hot_asset_reloading.rs)); editors that save
  through a temporary file and a rename (Aseprite on Windows) are a known source of missed reloads
  ([note](https://gamineai.com/help/bevy-0-17-asset-hot-reload-silent-png-watcher-not-triggering-asset-plugin-fix)) —
  hence FR-1's quiet period. NNHGE keeps "same handle, new content" (FR-7) and "tell the game" (FR-13).
- Defold: debug builds on phones run an engine service over TCP; the editor uploads changed resources to the running
  game and the engine logs each reloaded file; lifecycle functions are not rerun
  ([Defold hot reload](https://defold.com/manuals/hot-reload/), [dev app](https://defold.com/manuals/dev-app/)). The
  closest model for the console part (Q1, Q4); recent work sends changed and deleted build artifacts in dependency order
  ([defold#13586](https://github.com/defold/defold/pull/13586)), as FR-2 and FR-4 require.
- Godot: "Synchronize Scene Changes" replicates edits into a running remote game, and "Small Deploy with Network
  Filesystem" serves the project's files over the network instead of exporting them
  ([debugging tools](https://docs.godotengine.org/en/stable/tutorials/scripting/debug/overview_of_debugging_tools.html));
  users report crashes with the network filesystem on devices, one reason this spec sends only changed outputs.

## Open questions

Answered 2026-11-26: the maintainer approved every recommendation below.

- [x] **Q1 Simulator only, or the console too?** The ROADMAP item says "in the simulator". — **Recommendation: both, in
      one spec, with the plan free to split it into 031a (simulator) and 031b (console) as spec 026 did.** The console
      is the constraint (II) and the reason the edit loop hurts most (resend + Y); spec 013's connection already exists
      and spec 030 put its tools on both systems. A simulator-only 1.0 is the fallback if the console part doesn't fit
      before the release.
- [x] **Q2 Who watches files: `nnh` or the game?** — **Recommendation: `nnh`, on the PC.** It owns the source folder,
      runs conversions and already talks to both systems; the game then only applies batches, one code path for PC and
      console. A game watching its own folder would work only in the simulator, would see converted outputs being
      written (races) and would need a file-watching contract per OS. Consequence: a simulator started from an IDE
      without `nnh run` doesn't reload (documented); a later `nnh watch <game>` that attaches to a running simulator is
      a possible follow-up.
- [x] **Q3 Reconversion: the build (`cmake --build --target <game>_assets`) or `nnh-assetc` directly?** —
      **Recommendation: the build's asset target**, then the batch is the outputs whose bytes changed (the tools already
      rewrite only changed bytes; `nnh` compares content hashes). The build is the one place that knows every rule
      (D27, D29, D31, D42, D49, D50 routing, depfiles for tilesets/strings/fonts, both indexes, `tex3ds`); calling
      `nnh-assetc` directly would duplicate that routing in `nnh` and drift. Cost: one Ninja invocation (spec 012
      measured 0.2 s for a map); adding a file reruns CMake's configure through `CONFIGURE_DEPENDS` (slower, still no
      compile). The plan checks that building the asset target on 3DS doesn't repack the `.3dsx`.
- [x] **Q4 How do converted files reach the console, and where are they kept?** Options: (a) chunked transfer messages
      over the log connection (additive to D28), files kept in a Development-only folder inside the game's own data
      folder on the SD card (`sdmc:/3ds/<data_folder>/` + a reserved subfolder), wiped at start and at exit, looked up
      before the romfs; (b) the same messages, files kept in a RAM overlay with a fixed capacity; (c) the console
      fetches files from an HTTP server in `nnh` (spec 010's `FileServer`). — **Recommendation: (a).** One connection
      and one ordered stream keep batches frame-exact; the SD card has room for large textures and lets music stream a
      reloaded file like any other, where a RAM overlay would compete with the game for the Old 3DS's memory (II); the
      folder is inside the game's own folder (X) and never read in Release. (c) needs an HTTP client on the console and
      a second port for no gain.
- [x] **Q5 What must a game do, and how is it told?** — **Recommendation: nothing for engine-owned handles (FR-7 to
      FR-10); for the rest, FR-13's queries plus one small bus event `AssetsReloaded` (the batch's counter and number
      of paths) delivered in the frame the batch is applied; no per-asset callbacks.** A scene that keeps data or a map
      reacts with a subscription or a check in `update()`. Unlike spec 030's Q6 (no `TunableChanged`), a reload is a
      fact about the world the game's scenes naturally listen for, and it fires only in development builds by
      definition; the docs say game logic must not depend on it. Alternative: queries only, like spec 030.
- [x] **Q6 Tilemaps: announced only, or reloaded in place?** — **Recommendation: announced only (FR-11) in this spec;
      the example shows the usual reaction (load the map again and re-enter the room, or replace the scene).** A
      `Tilemap` block is referenced by the game's `collision::Grid` values, path maps synced from it, objects spawned
      from it and edits made with `set_tile`; swapping the block would leave those stale or silently undo game changes.
      A follow-up `Tilemap::reload()` that swaps in place when the game asks is possible once a real game shows the
      need.
- [x] **Q7 Voices of a reloaded sound.** — **Recommendation: stopped at the reload point (with `SoundFinished`); music
      continues at the same position (FR-9).** Keeping old samples alive until each voice ends would double the memory
      of a reloaded sound and need per-voice ownership; one cut-off sound in development is harmless. Alternative: let
      voices finish on the old samples.
- [x] **Q8 Which kinds for 1.0?** — **Recommendation: all of the kinds in this spec, staged by the plan:** (1) the
      edit loop, batches, FR-13's announcements for every kind and textures, data assets and string tables; (2) sprite
      libraries, sounds, music and fonts; (3) meshes; (4) the console. If the release can't wait, cut from the end:
      meshes, then fonts, fall back to "announced only" (FR-11) for any kind not reloaded in place.
- [x] **Q9 Live values and writing back (spec 030's Q5).** Options: (a) no coupling: a reload doesn't touch tunables
      (FR-23); a game that wants the panel's "changed" marks to follow the file registers its tunables again after
      `data::load` (030's RAII handle captures the new default); spec 030's dump stays the way to keep panel values;
      (b) `nnh` patches spec 014 data asset text files on the PC for tunables named `<data path>/<field>` (a
      comment-preserving TOML-subset writer), and the patched file then hot-reloads; (c) a live-module call to rebase
      defaults after a reload. — **Recommendation: (a) in this spec; (b) as a follow-up spec after 1.0.** With hot
      reload, "dump → paste the lines into the data file → save" already closes the loop in seconds; (b) needs a
      binding from tunables to asset fields and a writer that keeps comments and layout, a feature of its own.
- [x] **Q10 A batch while the game is paused (D52).** — **Recommendation: like D51's edits, it waits and is applied at
      the next game frame or step.** The held frame's lists stay consistent with the content they were recorded with.
      Alternative: apply while paused and redraw the held frame (it re-resolves textures by slot and generation, so new
      texture pixels would appear while paused), which is more immediate but makes a paused frame change content.
- [x] **Q11 How does a script test a reload without leaving the source tree dirty?** Options: (a) a script command
      `swap-asset <source path> <fixture>` that copies the fixture over the source file, waits for its batch, and
      restores the original byte for byte when the script ends, fails or is interrupted (a backup next to the build
      logs, and `nnh` refuses to start while an unrestored backup exists); (b) the build reads the assets folder through
      an overlay folder in the build tree that scripts write into; (c) a command that converts a fixture with the
      build's command line for that asset and sends the result, bypassing the source. — **Recommendation: (a)** for
      testing exactly the developer's path (save → build → batch); plus `expect-reload [path] within N frames`.
      (b) changes every game's build for a test need; (c) skips the build the spec promises to use.
- [x] **Q12 How does the engine find what to swap?** Per-kind tables already exist for some kinds (textures by slot,
      fonts' 32-entry table, mesh slots); sounds and sprite libraries are game-held blocks. Options: (a) Development
      builds record each loaded object's asset path hash beside it (a few bytes per object, in the existing block or
      slot); (b) a fixed reload registry of loaded objects. — **Recommendation: (a)**, decided in detail by the plan:
      no new registry, nothing in Release, and objects created from code (not loaded from a path) are never touched.
- [x] **Q13 Where does the API live?** — **Recommendation: FR-13's queries (and the event, if Q5 keeps it) in
      `<nnh/assets.hpp>`, namespace `nnh::assets` (`reload_count()`, `reloaded(path)`, `reloaded_paths()`)**: reloading
      is a property of assets, and every kind already depends on that module. Alternative: a new `<nnh/reload.hpp>`.

## Constitution pressure

- **XI "opt-in":** FR-7 to FR-10 change engine-owned content in a Debug/Development game without the game asking. It
  happens only when the developer saves a file under `nnh run`, and `--no-reload` turns it off; the docs state it.
- **I "same game code everywhere" vs Release:** assets change while running only in Debug/Development; game logic must
  not depend on a reload (FR-13 is constant in Release), as with spec 030's tunables.
- **II:** holding old and new content at once (FR-16) may refuse a reload on an Old 3DS that a restart would accept.
- **X:** the console writes files received from the PC (Q4 (a)), inside its own folder, Development only, validated
  before use — new territory for the engine (spec 013 and 030 only received fixed messages).
- **IV:** an `AssetsReloaded` event (Q5) would be the first development-only event on the gameplay bus.
