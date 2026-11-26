# Tasks 031: Hot reload of assets

Status: approved 2026-11-26 (with plan.md); in progress (stages 1–2 done; stage 3 and T4.1 partly)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug` and `pc-release`, a
screenshot, a `--pc` script). The work happens in a **separate git worktree** (branch `spec-031`) created from `main`
**after spec 030 is merged** (plan.md, "Spec 030 overlap"); shared files only get insertions after 030's blocks. The
order: the pure pieces shared with `nnh` first (the wire, the reader's payload mode, the receiver, names, the overlay),
then the game side's batch engine and apply point with the asset-index, data and tilemap kinds, then the `nnh` side (so
the loop runs end to end on PC from stage 3), then the kinds in the spec's Q8 order (textures and strings; sprite
libraries, sounds, music and fonts; meshes), then the example, the self-check, the scripts and the ARM pre-check before
the only console stage (10). Stage 9 is run on the Linux build VM by the lead. Deviations found while building go into
plan.md as "Stage N refinements", as in plans 012–030. While developing, run the module's tests (`nnh_tests reload_`,
`remote_`, `assets_` and the kind's filter; `nnh_tool_tests reload script`); the full suite at stage ends.

Guards held at every stage end (the "stage-end guards"): ctest pc-debug and pc-release; 3ds-release and 3ds-dev without a
warning; 3ds-release `hello` `.3dsx` and stripped `.elf` **SHA-256 identical** to T0.1's and pc-release `hello` size and
`nm -C` **identical** (plan 030's rules; when a hash moves, the touched objects' disassembly is compared);
Debug/Development `hello` growth recorded; every earlier golden (the values re-read from the tree at T0.3: `ui: PASS
30400DA6`, `camera`, `queries`, `entities`, `prefabs`, `hierarchy`, `tweens`, `pathfinding`, `particles`, `transitions`,
the models and stereo goldens, `data check`, the saves goldens, 029's localization golden, 030's live goldens, the
platformer replay ctest with the tree's lines); every example's `--pc` script still passes.

## Stage 0: baseline

- [x] **T0.0** Start: spec 030 merged into `main` (the lead); worktree `spec-031` from that `main`; the numbers of plan.md
  confirmed against the merged `remote_wire.hpp` (messages up to `Pause = 11`, records up to `LiveState = 12`); 030's
  final names used by this plan confirmed (`dev_pause_state()`, the `Pause` message ops, `LiveState`'s flow and its
  catalogue condition, `LiveOps`, the live point in `app.cpp`). Files: plan.md ("Stage 0 results"). Covers D28, FR-6.
- [ ] **T0.1** Baseline numbers (Windows, 3 runs each; scripts in `build/ref-031/`, not committed): `hello` sizes, `nm -C`
  lists and SHA-256 for 3ds-release, 3ds-dev, pc-release, pc-debug; the 3ds-release objects this spec edits under Dev
  branches (`app.o`, `assets.o`, `textures.o`, `gfx_frame.o`, `sounds.o`, `music.o`, `data_index.o`, the sprite loader
  and animator objects, `font_load.o`, `mesh_table.o`, `loc_state.o`, the UI's update object) — bytes and disassembly for
  later diffs, and which of them `hello` links; edit `examples/ui/title_scene.cpp` → simulator and `.3dsx` times; clean
  `nnh_core` and `nnh`; preprocessed lines and `-fsyntax-only` time of one-line TUs including `<nnh/assets.hpp>` and
  `<nnh/texture.hpp>`. Files: plan.md. Covers III, FR-19, AC-7.
- [ ] **T0.2** [P] The edit loop's pieces (3 runs each): `cmake --build --preset pc-debug --target platformer_assets` as a
  no-op, after touching one `.tmj` and after adding a file (configure rerun); the same for `data_assets` and for
  `3ds-dev` through `scripts/dkp.sh` and through `nnh`'s MSYS command form; MSYS bash start-up alone (decides plan risk
  2's persistent shell, threshold 0.3 s); `--target platformer -- -n` dry-run time and output after (a) an asset edit,
  (b) a file addition, (c) a `DATA_SCHEMA` source edit, (d) an `IMAGES` file edit — (a) and (b) must be empty, (c) and
  (d) must list commands (plan risk 3); the polling cost of `examples/platformer/assets` and `examples/ui/assets` (1000
  polls); on Windows: a file opened by `SDL_IOFromFile` while `cmake -E copy` writes over it (expected: the copy fails —
  AC-9's reason), and the same with a `FILE_SHARE_READ | WRITE | DELETE` open (expected: the copy succeeds). Files:
  plan.md ("Stage 0 results"). Covers FR-1, FR-2, FR-14, AC-8, AC-9.
- [ ] **T0.3** [P] Goldens, counts and references: the stage-end guards' values as in the tree; `nnh_tests` count against
  `kMaxTests` (8192; this spec adds ≈ 350 tests); reference screenshots in `build/ref-031/ref/` (`hello`, `sprites`, the
  platformer level 1, `ui` title, `models` check) with `build/ref-031/compare.sh`. Files: plan.md. Covers VI, AC-7.

## Stage 1: the pure pieces shared with `nnh`

- [x] **T1.1** The wire: `engine/core/src/reload/reload_wire.{hpp,cpp}` (messages 12–17 and records 13–15: encode,
  decode, validate, `payload_length()`), `remote/remote_wire.{hpp,cpp}` (type numbers, decoder dispatch, record length
  rules; `Message` stays 32 bytes). Test: `tests/reload/test_reload_wire.cpp` (round trips; every refusal: batch 0, files
  0 and 257, total > 32 MiB, op 0 and 3, path length 0, size > 16 MiB, a remove with a size, chunk length 0 and 16385,
  frame ≠ 0 on a transfer message, non-zero reserved bytes; record lengths at their bounds and one past);
  `test_remote_wire.cpp` and 030's `test_live_wire.cpp` unchanged cases pass. Covers FR-18, X, D54.
- [x] **T1.2** The reader's payload mode: `remote/remote_reader.{hpp,cpp}` (after a message with a payload, the next
  bytes come out as payload pieces pointing into the caller's buffer, split across reads; `bytes_wanted()` in payload
  mode; decoding resumes after it; refusals of a payload-carrying message still skip its declared payload). Test:
  `tests/remote/test_remote_reader_payload.cpp` (every split point of a message + payload + message; payload of 1 and
  16384 bytes; a refused chunk keeps the stream in sync; 8 malformed headers in a row still switch the reader off);
  `test_remote_reader.cpp` unchanged cases pass. Covers FR-18, X.
- [x] **T1.3** The receiver: `reload/reload_receiver.{hpp,cpp}` (pure over a sink of function pointers: begin file,
  append, finish with CRC, discard all; one batch at a time; order, offsets, totals, paths through `valid_asset_path`,
  CRC-32 through `core/crc32`; staged; apply request; cancel; detach). Test: `tests/reload/test_reload_receiver.cpp`
  (a complete batch of puts and removes → staged with the exact sink calls; every refusal and its record reason: out of
  order, wrong offset, short or long file, total mismatch, bad CRC, a bad path, `Begin` while busy, `Apply` for another
  batch or count, cancel and detach mid-file → sink discard; every limit and limit + 1). Covers FR-4, FR-18, X.
- [x] **T1.4** [P] Names: `reload/reload_names.{hpp,cpp}` (load names, `matches(path_or_name, path)`, the log line's list
  cut at 160 characters with `, ... (+N)`). Test: `tests/reload/test_reload_names.cpp` (paths with several dots, no
  extension, index files, the cut at every length around 160). Covers FR-13, FR-15.
- [x] **T1.5** [P] The overlay table: `reload/reload_overlay.{hpp,cpp}` pure part (path hash → file number and flags,
  512 slots, ≤ 256 paths, exact compare against the current index's stored path; replace returns the previous file to
  remove). Test: `tests/reload/test_reload_overlay.cpp` (insert, replace, remove, collisions forced by a test hash,
  capacity + 1 refused). Covers FR-5, FR-12, FR-16.
- [ ] **T1.6** Fuzzing: `tests/fuzz/fuzz_reload_receiver.cpp` (mini-fuzz in `nnh_tests`: random byte streams and random
  well-formed-but-wrong message sequences through the reader and receiver into a recording sink; invariants: no sink
  write outside an open file, bytes written ≤ declared, no crash, state reset after cancel) and
  `tests/fuzz/libfuzzer_reload_receiver.cpp` (`NNH_BUILD_FUZZERS`); the remote fuzz corpus gains the new messages. Test:
  60 s clean locally (the VM's clang run at T9.1). Covers FR-18, X, AC-6.
- [x] **T1.7** Stage end: the stage-end guards (nothing links the new files yet); plan.md ("Stage 1 results"). Covers
  AC-7.

## Stage 2: the game side — files, overlay, batch engine, apply point, queries

- [x] **T2.1** Files on the data folder: `reload/reload_files.{hpp,cpp}` (`.nnh_reload/<n>.bin` names, the receive buffer
  allocated at the first batch with tag `reload`, the sink over `fs_append`/`fs_rename`/`fs_remove`, the wipe at the
  first batch (counter files until 16 missing) and the Dev shutdown hook). Test: `tests/reload/test_reload_files.cpp`
  (names pass `fs::validate_path`; an SD write failure (the fake's failure injection) refuses the batch and removes its
  files; the wipe; nothing outside `.nnh_reload/`; the buffer allocated once). Covers FR-18, X, II.
- [ ] **T2.2** The contract: `engine/core/contract/nnh/platform/reload.hpp` (`reload_stream_open`, `asset_open_shared`),
  `engine/platforms/{pc,3ds,fake}/src/reload_<system>.cpp`, `platform_fake.hpp` (`fake::replace_asset`, counters), the
  platforms' CMake lists. Test: `tests/platform_conformance/test_reload_streams.cpp` (a data-folder file read through
  `asset_read_at`/`asset_close`; on the PC platform a shared stream survives a rewrite of its file by another handle —
  Windows); 3ds-dev builds. Covers FR-5, AC-9, I.
- [x] **T2.3** Reads through the overlay: `reload/reload_overlay.cpp` glue (`g_reload_assets`: the current index and the
  table), `assets/assets.cpp` and `assets_internal.hpp` (Dev branches in `locate`, `asset_lookup`, `size`, `read`; the
  `detail::asset_stream_open/read_at/close` wrappers), `audio/music.cpp` and `data/data_index.cpp` through the wrappers.
  Test: `tests/assets/test_assets_overlay.cpp` (reads, sizes, `exists`, streams and case hints through the overlay; a
  removed path → `NotFound`; the original file untouched; Release-path behaviour unchanged in pc-release). Covers FR-5,
  FR-12.
- [x] **T2.4** The batch engine: `reload/reload_kind.hpp`, `reload/reload_batch.{hpp,cpp}` (kinds registered by
  modules, prepare in batch order, consistency rules, commit, abandon, refusal texts), `reload/reload_index.cpp` (the
  asset-index kind). Test: `tests/reload/test_reload_batch.cpp` (fake kinds recording calls: one failing prepare abandons
  every prepared item and commits nothing; an allocation failure injected at each step; consistency with and without
  `.nnh_assets`; plain files; two kinds claiming one path; the index kind's add/remove/rename). Covers FR-4, FR-11,
  FR-12, FR-16.
- [x] **T2.5** The session and the apply point: `remote/remote_reload.cpp` (payload reads into the receive buffer at most
  16 KB per frame, transfer messages at receipt, `ReloadApply` from the schedule, `Cancel`, `Notice`, detach → cancel;
  records only after a `Begin` or `Notice`), `remote_session.cpp`, `remote_internal.hpp`, `reload/reload_apply.cpp`
  (pending apply, the apply point, `ReloadApplied` with its µs, `ReloadRefused`), `app/app.cpp`'s Dev call right before
  030's live point. Test: `tests/reload/test_reload_apply.cpp` (a batch stamped F applies at F's point on the fake's
  `nnh::frame()`, before the live point and the input events; frame 0 = next; a late `Apply` → `Late`; paused → files
  still received on paused ticks, the apply waits for step or resume; a disconnect mid-transfer removes the files; no
  record before a `Begin`), `tests/remote/test_remote_session.cpp` unchanged cases pass. Covers FR-6, FR-18, Q10.
- [x] **T2.6** Queries, event, log line: `<nnh/assets.hpp>` (`AssetsReloaded`, `kReloadEnabled`, `ReloadedPaths`,
  `reload_count`, `reloaded`, `reloaded_paths`; Release inline), their back ends in `reload_apply.cpp`, the
  `I assets reloaded N (frame F): …` line, the removal and refusal warnings. Test:
  `tests/reload/test_reload_queries.cpp` (counter, by path and by load name, this frame only, the event delivered in the
  batch's frame before the scene's update, the exact log lines), `tests/reload/test_reload_release.cpp` (pc-release:
  constants, nothing published, `sizeof` of the event 8). Covers FR-13, FR-15, FR-19, Q5.
- [x] **T2.7** [P] Data and tilemap kinds: `data/data_reload.cpp` (data files validated against the compiled schema —
  a fingerprint mismatch refuses with "rebuild and restart"; the index into a new block, swapped at commit),
  `data/data_index.cpp` (Dev: the swap hook), `tilemap/map_reload.cpp` (spec 012's validator only). Test:
  `tests/data/test_data_reload.cpp` (`count`/`name`/`find`/`path` after add, remove and rename; the game's structs
  untouched; fingerprint refusal; an index larger than the old block), `tests/tilemap/test_map_reload.cpp` (a valid map
  changes nothing; a corrupted one refuses the batch). Covers FR-11, FR-14, FR-16, FR-18.
- [ ] **T2.8** [P] Cost and random operations: `tests/reload/test_reload_cost.cpp` (100 000 frames without a batch: no
  allocation, a walk counter at 0; allocations only at the first batch and in apply frames),
  `tests/reload/test_reload_random.cpp` (10 000 seeded batches of puts/removes/refusals against a model of the expected
  index and overlay, guard bytes around every buffer), `tests/reload/test_reload_plain_loop.cpp` (a `while
  (nnh::frame())` game applies batches). Covers FR-17, FR-20, FR-18.
- [ ] **T2.9** Stage end: the stage-end guards (the first stage editing Release-linked files: per-object diffs recorded);
  the header check (`<nnh/assets.hpp>` within +30 lines and +0.01 s of T0.1); Debug/Development `hello` growth so far;
  plan.md ("Stage 2 results"). Covers AC-7, III.

## Stage 3: the `nnh` side (end to end on PC)

- [x] **T3.1** Where and what: `cmake/nnh_assets.cmake` (`nnh_reload_<game>.txt` at configure time),
  `tools/nnh/src/reload/output_snapshot.{hpp,cpp}` (the mirror: FNV-1a 64 + size per output, rescans by size/mtime, the
  diff → batch list in plan.md's order with indexes last; hash-collision warning). Test:
  `tools/nnh/tests/test_output_snapshot.cpp` (fixture build folders: changed, unchanged, added, removed; order; the mirror
  moves only on `Applied`). Covers FR-2, FR-4.
- [x] **T3.2** [P] Watching: `reload/source_watcher.{hpp,cpp}` (100 ms polls, 250 ms quiet period restarted by changes;
  injected clock and lister). Test: `tools/nnh/tests/test_source_watcher.cpp` (one save, a temporary file + rename, a
  `.tmj` + `.tsj` burst, an add, a remove, a rename, a change during the quiet period, nothing for touch-free polls).
  Covers FR-1.
- [ ] **T3.3** Building: `reload/asset_build.{hpp,cpp}` (the asset target's command per preset, the MSYS form and — if
  T0.2 measured > 0.3 s — the persistent shell with a sentinel; output passthrough; one retry after 200 ms on failure;
  the dry run and its first command's description). Test: `tools/nnh/tests/test_asset_build.cpp` (command lines,
  dry-run parsing of real Ninja output samples recorded at T0.2, the retry, a failing build's lines kept). Covers FR-2,
  FR-3, FR-14.
- [ ] **T3.4** Sending: `reload/batch_sender.{hpp,cpp}` (messages, CRC-32, 16 KB chunks, non-blocking writes interleaved
  with reads, 256 KB beyond the last acknowledgement), `remote/game_session.cpp` (the send queue, the controller tick),
  `remote/game_records.*` (records 13–15). Test: `tools/nnh/tests/test_batch_sender.cpp` (exact bytes of a batch;
  limits refused locally with "too many changes for one reload: restart"), `test_game_session.cpp` (over loopback with a
  fake game that logs heavily during a 4 MB transfer: no deadlock). Covers FR-4, FR-18.
- [x] **T3.5** The controller: `reload/reload_controller.{hpp,cpp}` (idle → quiet → building → sending → waiting;
  coalescing; the dry run beside the transfer and `Apply` after it; notices; the printed lines with times), `cli.cpp`
  (`--no-reload`), `commands/run.cpp`, `run_pc.cpp`, `remote/script_interactive.cpp` (`reload [PATH]`), the older-game
  message. Test: `tools/nnh/tests/test_reload_controller.cpp` (a fake builder and game: every state change, a save
  during a cycle, a failed build, a dry run with commands, a refused batch keeps the mirror, an older game turns watching
  off; the exact lines). Covers FR-1, FR-3, FR-14, FR-15, FR-22.
- [ ] **T3.6** Script commands: `remote/script.hpp`, `script_parser.cpp`, `script_reload.cpp` (`swap-asset SOURCE FIXTURE
  [unheld]`, `restore-asset SOURCE`, `expect-reload [PATH] [refused] [within N]`; 030's paused-section rules),
  `script_timeline.*`, `script_runner.*`, `script_runner_reload.cpp` (held flow: pause at F, wait for `LiveState`,
  build, send, `Apply` + `resume` at F; unheld flow; refusals; expectations), `reload/asset_swap.{hpp,cpp}` (backups,
  the journal, restore at end/failure/Ctrl-C, the start refusal), `commands/restore_assets.cpp`. Test:
  `tools/nnh/tests/test_script_reload.cpp` (syntax, line errors, paths outside the game's folder, paused rules),
  `test_script_runner_reload.cpp` (a fake console: held and unheld, applied and refused, the clock rules),
  `test_asset_swap.cpp` (byte-for-byte restore, the journal, interrupted runs). Covers FR-22, Q11.
- [ ] **T3.7** End to end on PC: `nnh run data --pc` by hand (edit `goblin.enemy`, see `I assets reloaded 1` and
  `nnh: reload 1 applied …`; a broken field shows `file:line`), and a ctest `reload_data_smoke_pc` (`--pc --script` with a
  `swap-asset` of a data file and `expect-reload`). Files: plan.md ("Stage 3 results": first save → frame times). Covers
  FR-1–FR-6, FR-13.
- [ ] **T3.8** Stage end: the stage-end guards; plan.md. Covers AC-5, AC-7.

## Stage 4: textures and string tables (Q8 step 1)

- [ ] **T4.1** Textures: `gfx/texture_reload.cpp` (path hashes per slot, the kind, raw-handle retirement through D47,
  `texture_width_dev`), `textures.cpp` and `gfx_frame.cpp` (Dev branches), `<nnh/texture.hpp>` (Dev `width()/height()`).
  Test: `tests/gfx/test_texture_reload.cpp` (same handle, new pixels and size through `width()`, draws use the new
  texture, the old handle destroyed only after D47's frames, two textures from one path, a tilemap's tileset texture and
  a mesh's texture follow, linear memory exhausted → refused with the bytes and the old kept; pc-release: `width()` from
  the field). Covers FR-7, FR-16.
- [ ] **T4.2** [P] String tables: `loc/loc_reload.cpp`, `loc_state.*` (Dev hooks). Test: `tests/loc/test_loc_reload.cpp`
  (a file of the current language and of its fallback; serial bump and `text_changed()`; a larger chain reallocates; a
  removed key shows its name; a manifest with another language list refuses; the pseudo-language follows the base; a
  font rebaked in the same batch). Covers FR-10, FR-14, FR-16.
- [ ] **T4.3** UI follows: the UI's update/draw (Dev branch: relayout when `assets::reload_count()` changed). Test:
  `tests/ui/test_ui_reload.cpp` (an image widget's natural size and a 9-slice after a texture reload; a label after a
  font or strings reload; golden `30400DA6` unchanged). Covers FR-7, FR-10.
- [ ] **T4.4** Stage end: the stage-end guards; by hand `nnh run sprites --pc`, save a PNG of another size → the new
  sprite at once (AC-4b's preview); plan.md ("Stage 4 results"). Covers AC-4b, AC-7.

## Stage 5: sprite libraries, sounds, music, fonts (Q8 step 2)

- [ ] **T5.1** Sprite libraries: `animation/sprite_reload.cpp` (the 64-entry table, the kind, identity and current
  blocks), `animator_rebind.cpp`, `sprite_library_internal.hpp`, `sprite_load.cpp`, the animator files (Dev compare
  beside the serial check). Test: `tests/animation/test_sprite_reload.cpp` (queries through the identity block after a
  larger reload; the previous current block freed; destroying the library frees both),
  `tests/animation/test_animator_rebind.cpp` (plan.md's FR-8 cases with exact warnings; ASan proves no dereference of a
  freed block; a moved library). Covers FR-7, FR-8.
- [ ] **T5.2** [P] Sounds: `audio/sound_reload.cpp`, `sounds.cpp`, `audio_internal.hpp`. Test:
  `tests/audio/test_sound_reload.cpp` (voices stopped with `SoundFinished` in that frame; the next `play` uses the new
  samples; old samples freed one frame later; `duration()` new; refused without memory). Covers FR-7, FR-9, Q7.
- [ ] **T5.3** [P] Music: `audio/music_reload.cpp`, `music.cpp` (Dev: `asset_open_shared` for build-folder streams).
  Test: `tests/audio/test_music_reload.cpp` (Ogg and WAV: the same sample position; past the new end → loop start; a
  crossfade keeps its frames; a stream whose file is replaced underneath (`fake::replace_asset`) never crashes). Covers
  FR-9, AC-9.
- [ ] **T5.4** [P] Fonts: `font/font_reload.cpp`, `font_load.cpp`, `font_table.*`. Test:
  `tests/font/test_font_reload.cpp` (same `Font` and `FontRef`, new metrics and glyphs, old pages retired through D47, a
  full texture table refuses, fallbacks kept). Covers FR-7, FR-16.
- [ ] **T5.5** Stage end: the stage-end guards; plan.md ("Stage 5 results"). Covers AC-7.

## Stage 6: meshes (Q8 step 3) and live values

- [ ] **T6.1** Meshes: `mesh/mesh_reload.cpp`, `load_mesh.cpp`, `mesh_table.cpp`. Test: `tests/mesh/test_mesh_reload.cpp`
  (same handle, more vertices and submeshes, new textures, the old record retired through D47, draws recorded later use
  the new mesh). Covers FR-7.
- [ ] **T6.2** [P] FR-23: `live/live_reload.cpp`, `debug/live_hook.hpp` (`reload_snapshot`, `reload_compare`), the calls
  at the apply point and 030's frame end. Test: `tests/live/test_live_reload.cpp` (a tunable on a data-loaded field shows
  the file's value after the game reloads it; `changed()` untouched; the one warning when the panel value differed; 17
  edited tunables → 16 tracked; a game without live values links nothing). Covers FR-23, Q9.
- [ ] **T6.3** Stage end: the stage-end guards; plan.md ("Stage 6 results"). Covers AC-7.

## Stage 7: `examples/reload`, the self-check, the scripts, the ARM pre-check

- [ ] **T7.1** Assets and fixtures (original): `examples/reload/assets/…` and `tests/fixtures/…` as plan.md lists,
  `data_types.cpp`, `CMakeLists.txt` (`LANGUAGES en pt-BR`, `DATA_SCHEMA`, `ASSETS`), the OFL font copied with its
  licence, `examples/CMakeLists.txt`. Test: the example builds on pc-debug, 3ds-dev and 3ds-release. Covers AC-1, IX.
- [ ] **T7.2** The self-check and the scenes: `examples/reload/self_check.{hpp,cpp}`, `main.cpp`, `demos.hpp`,
  `reload_scene.{hpp,cpp}`, `check_scene.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, the `tight` scene. Test:
  `tests/reload_example/test_self_check.cpp` (the start golden; the sum changes when one value changes — once by hand).
  Covers AC-1, FR-21.
- [ ] **T7.3** Screenshots: `RELOAD_SHOT=start|hero_big|room_moved|strings|font|gem`, read back. Covers AC-1, FR-7.
- [ ] **T7.4** [P] The bench: `idle` and `reads` phases with their lines. Test: a pc-dev run logs every line. Covers
  AC-8, FR-17.
- [ ] **T7.5** The scripts: `examples/reload/tests/check.nnhs`, `errors.nnhs`, `timing.nnhs`,
  `examples/sprites/tests/reload.nnhs` + fixture; ctests `reload_check_script_pc`, `reload_errors_script_pc`,
  `sprites_reload_script_pc` (each checks `git status --porcelain` of the example's folder is empty afterwards;
  `RESOURCE_LOCK nnh_remote_port`) and `reload_open_file_pc` (Windows only: music streaming and a large `assets::load`
  while their files are swapped). Test: each passes with `--pc --no-build` twice in a row; save → frame times recorded.
  Files: plan.md ("Stage 10: console run (prepared)" with every expected line and sum). Covers AC-2, AC-3, AC-4b, AC-8,
  AC-9.
- [ ] **T7.6** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -drC` of the 3ds-dev objects of
  `src/reload/*`, `remote_reload.o`, the kind objects and the reader; per plan.md's cost table record instructions and
  calls on the idle path, the CRC loop (no `__aeabi_*`), `-fstack-usage` (< 1 KiB in the receiver and the batch engine);
  fix what differs without changing a byte on the wire or a golden. Files: plan.md ("Stage 7 ARM pre-check"). Covers
  AC-8, II.

## Stage 8: mutation testing, release hygiene, opt-in checks, build times

- [ ] **T8.1** Mutation pass as in plans 019–030: plan.md's planned hand mutants (`build/ref-031/mutants.py`, one at a time
  with the module filters) and Release-path mutants in pc-release; survivors become tests or are argued equivalent.
  Files: plan.md ("Mutation testing"). Covers VI.
- [ ] **T8.2** [P] Release hygiene: `scripts/check_release_strings.sh` gains the reload texts (`assets reloaded`,
  `reload refused`, `was removed`, `rebuild and restart`, `.nnh_reload`, the rebind warnings) and the example's names;
  run on the 3ds-release and pc-release `reload` and `sprites`; `nm` shows no `reload` symbol; the Release example prints
  its start sum. Covers FR-19, AC-7.
- [ ] **T8.3** [P] Opt-in by linking: probes (`build/ref-031/probe_*`, not committed) on 3ds-dev and 3ds-release: a
  Development game loading only plain files (the reload core, no kind objects), one loading textures (the texture kind
  only); `hello` 3ds-release bytes and pc-release size/`nm` identical to T0.1's; Debug/Development `hello` growth against
  question 9's estimate. Files: plan.md ("Stage 8 results"). Covers FR-19, FR-20, XI.
- [ ] **T8.4** Build times and regressions: every example's console script passes with `--pc`; `compare.sh`; edit
  `examples/reload/reload_scene.cpp` → simulator and `.3dsx` times; clean `nnh_core` and `nnh`. Files: plan.md. Covers
  III, AC-7.

## Stage 9: Linux checks (VM 120, run by the lead)

- [ ] **T9.1** On `nnhge-build` (start it per CLAUDE.md, never with the Bazzite VM; shut it down after): GCC 13, GCC 14 and
  clang 18 (ASan/UBSan): `nnh_tests`, `nnh_tool_tests` and ctest pass in pc-debug and pc-release; the self-check sums
  identical on all three and on the 32-bit host build; libFuzzer `libfuzzer_reload_receiver` and the remote targets 10
  minutes each, plus the mini-fuzzers; the `--pc` scripts (the polling watcher and the build on Linux). Files: tasks.md
  (results), plan.md ("Stage 9 results"). Covers I, X, AC-6, FR-21.

## Stage 10: console check (the only steps on the 3DS)

- [ ] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh
  cmake --build --preset 3ds-dev --target reload_3dsx sprites_3dsx` without a warning; T7.6's table still true;
  3ds-release `hello` identical to T0.1's. Covers AC-7, II.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run reload --script examples/reload/tests/check.nnhs` → every kind reloaded, every sum
     equal to the PC's, screenshots compared with the `--pc` run's;
  2. `build/pc-debug/tools/nnh/nnh run reload --script examples/reload/tests/errors.nnhs` → the refusals, including the
     linear-memory one, the old content on screen;
  3. `build/pc-debug/tools/nnh/nnh run reload --script examples/reload/tests/timing.nnhs` → save → frame times for the
     PNG, the map and the data file (target < 3 s), transfer rate, apply times per kind, the bench lines;
  4. `build/pc-debug/tools/nnh/nnh run sprites --script examples/sprites/tests/reload.nnhs` → PASS;
  5. (yes/no, by hand, ≈ 1 minute) `build/pc-debug/tools/nnh/nnh run reload` (no script): the lead saves
     `examples/reload/assets/art/backdrop.png` with a visible change — does the console show it within 3 s without a
     restart? yes/no.
  `git status` clean after each. Files: plan.md ("Console run 1": verdicts, numbers, log paths, the screenshot
  comparison). Covers AC-2, AC-3, AC-4b, AC-8, FR-21.
- [ ] **T10.3** (only if a target is missed) Optimisation round: find the cost (3ds-dev disassembly, the timing lines),
  apply risk 4's fallback or other fixes without changing the wire or a golden (tests first); T10.2 run 3 again; then the
  budgets are set from the console measurement with ≈ 25 % margin and written into the spec's AC-8, `timing.nnhs` and
  `docs/api/assets.md`. Files: plan.md ("Optimisation round", "Console run 2 and budgets set"). Covers AC-8.

## Stage 11: docs, roadmap, status

- [ ] **T11.1** `docs/api/assets.md` (reloading: what changes behind handles, what is announced, `reload_count`,
  `reloaded`, `reloaded_paths`, `AssetsReloaded`, refusals, memory, Release), header comments (allocates? per-frame safe?
  misuse?); the kinds' pages: `texture.md`/`gfx.md` (`width()` in Development), `animation.md` (rebind rules),
  `audio.md` (voices stopped, music position), `font.md`, `localization.md`, `mesh.md`, `data.md`, `tilemap.md`, `ui.md`,
  `live.md` (FR-23). Covers VIII, AC-10.
- [ ] **T11.2** `docs/guides/editing-assets-live.md` ("editing assets while the game runs": the loop, what reloads by
  itself, reacting to `AssetsReloaded` for maps and data, errors and refusals, the console, limits, `--no-reload`). Covers
  AC-10.
- [ ] **T11.3** Other pages: `docs/guides/nnh-tool.md` (watching, `--no-reload`, `reload`, `restore-assets`, script
  commands), `docs/guides/testing-on-the-console.md` (`swap-asset`, `expect-reload`), `docs/architecture/debugging.md`
  (the payload messages and records), `security.md` (the `.nnh_reload` folder), `foundation.md` and
  `project-structure.md` (`src/reload/`, `tools/nnh/src/reload/`), `docs/tech/cmake.md` (`nnh_reload_<game>.txt`),
  `docs/tech/devkitpro-3ds.md` (SD files, transfer rates), `docs/REFERENCES.md` (Bevy, Defold, Godot, the rename-save
  note), `CLAUDE.md` (layout paragraph and commands); `python -I scripts/docs_tool.py check` → 0 broken links. Covers
  AC-10.
- [ ] **T11.4** `docs/architecture/decisions.md`: D53 and D54 change-log notes ("approved"/"implemented") with the stage
  refinements and the maintainer's answers; D28's payload messages, D31's table, D47's raw handles, D50's reloadable
  chain noted. Covers AC-10.
- [ ] **T11.5** Final measurements (header cost, build times, `hello` sizes, the reload core's `.text`, save → frame
  times) in plan.md and `docs/ROADMAP.md`'s baseline table if it changed. Covers III, AC-8.
- [ ] **T11.6** `docs/ROADMAP.md`: the "Hot reload of assets in the simulator" checkbox (now simulator and console), with
  the implemented summary.
- [ ] **T11.7** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
- [ ] **T11.8** (the lead) Merge `spec-031` into `main` (rebased; insertions after 030's blocks); then the full ctest
  (pc-debug, pc-release), every example's `--pc` script, the self-check sums and every earlier golden, one 3ds-release
  build; T10.2 runs repeated if the merge happened after them.
