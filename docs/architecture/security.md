# Security and safety

Homebrew runs **without a sandbox**: a bug in a game can delete or corrupt any file on the user's SD
card. Players trust that a game made with NNHGE won't. These rules make the engine safe by default
(constitution article X).

> Status: **implemented** (spec 002): confined files, atomic saves, bounded parsers, fuzzing, CI hardening;
> NNH Link (spec 011), the console remote (spec 013) and the two map readers of spec 012 (the run-time
> `.nnhmap` validator and the build-time Tiled converter) below.
> API: [docs/api/fs.md](../api/fs.md).

## Threat model (what we protect against)

| Risk | Source | Protection |
|------|--------|------------|
| Writing outside the game's folder | bug in game or engine, malicious path in data | confined file access (below) |
| Corrupted save after power loss | battery dies mid-write | atomic saves with backup |
| Crash or memory corruption from bad data | corrupted/modified save, asset or downloaded file | bounded parsers + fuzz tests |
| A hostile level file in a game's sources | a downloaded or shared Tiled map (`.tmj`/`.tsj`) built by `nnh-assetc` on the developer's PC | bounded JSON reader, images only from inside the assets folder, every output checked by the run-time validator; both fuzzed ([below](#bounded-parsers-and-fuzzing-spec-002)) |
| Malicious code reaching users | compromised dependency or CI | pinned hashes, pinned actions, least privilege |
| Code pushed to a console over the network | NNH Link | pairing, mutual authentication, a MAC on every message, LAN only (spec 011, D24; [guide](../guides/nnh-link.md#safety)) |
| A game reading commands from the network | the console remote (`nnh run --script`, spec 013, D28) | Debug/Development only (no code in Release, checked); the game opens the connection to the PC that started it (simulator: loopback only); fixed-size messages, every field checked, bounded reads; fuzzed ([below](#console-remote-spec-013)) |
| Privacy | telemetry | none, ever |

Out of scope: protecting a game against cheating or piracy, and protecting the console from its owner.

## Confined file access (spec 002)

- All engine file I/O goes through `nnh::fs`. Games use it too; the engine never calls `fopen` elsewhere.
- Writable root: `sdmc:/3ds/<data_folder>/` on 3DS; on PC `<exe dir>/data/<data_folder>/` (Debug/Development)
  or the user's app-data folder (Release). Read-only assets (`romfs:/`) arrive with the asset pipeline.
- Paths are **relative** to those roots. Rejected with an error: absolute paths, `..` segments, drive or
  device prefixes (`sdmc:`, `C:`), empty segments, backslashes, characters outside `[A-Za-z0-9._-]`,
  more than 96 characters or 4 levels. Invalid paths never reach the platform (tested).
- The engine never touches NAND, system saves or other titles' data.

## Atomic saves (spec 002)

Write: `save.bin.tmp` → flush → rename the old `save.bin` to `save.bin.bak` → rename tmp to `save.bin`.
File header: magic, format version, payload size, checksum (CRC-32). Load: validate `save.bin`; if
missing or invalid, try `save.bin.bak`; report which one was used. A power cut at any point leaves at
least one valid file.

## Bounded parsers and fuzzing (spec 002)

- Every reader of external data checks sizes and offsets before using them, never trusts counts from the
  file, and fails with a `Result` error instead of crashing.
- Each parser has a fuzz target in `tests/fuzz/` (today: path validator, save-file validator, asset
  index, WAV reader, Ogg Vorbis reader — which drives the vendored stb_vorbis through open, decoding and
  seeking —, the console remote's reader, schedule and input merge of spec 013, with a round-trip check
  of its 16-byte messages; libFuzzer executable `fuzz_remote`; 2026-11-21, 60 s: 922 651 runs, cov 132, no crash —,
  and the `.nnhmap` map validator of spec 012 (`tilemap/map_validate.*`), which `tilemap_target` runs as given and
  again with the size and CRC fixed so mutations reach the content checks (animated tiles included since stage 6:
  a sorted table, frames inside the animated tile's tileset, 1–32 frames of ≥ 1 ms), then copies every accepted
  map, runs every query, the draw-time tile expansion at two animation times and a few edits (animations too) on
  it; libFuzzer executable `fuzz_tilemap`, not run
  on Linux yet: spec 012 T8.1). The build-time Tiled converter `nnh-assetc` (spec 012 stage 5, `tools/assetc/`) reads external
  JSON with its own bounded reader (`json*.cpp`: document ≤ 64 MiB, nesting ≤ 32, ≤ 8 Mi values, numbers ≤ 64
  characters and finite, strings valid UTF-8 with paired surrogates), reads only files inside the assets folder
  for images (tilesets may sit anywhere), and checks every map it writes with the run-time validator; its targets
  live in `tools/assetc/tests/fuzz_assetc.cpp` (`json_target`; `map_target`, a whole conversion of an in-memory map
  and tileset, whose accepted output must pass the validator), as a mini-fuzz in `nnh_assetc_tests` (8 000 JSON
  and 6 000 map/tileset mutants) and libFuzzer executables `fuzz_assetc_json`, `fuzz_assetc_map` (not run on
  Linux yet: T8.1). Converted data assets `NNHD` of spec 014 (stage 2) are checked by `data/data_validate.*`
  against the game's compiled schema (header, CRC, schema fingerprint, then every field index, value, string, enum
  index and list count, linear in the file size); sprite libraries `.nnhsprite` of spec 016 are checked by
  `animation/sprite_validate.*` (header, CRC, capacities, every frame, clip range, mode end, cycle duration, marker
  run, name hash, state and rule) — `sprite_library_target` validates as given and with the size and CRC fixed, then
  plays every clip and state of an accepted library (mini-fuzz in `nnh_tests`, libFuzzer `fuzz_sprite`, not run on
  Linux yet) — and the converter's Aseprite reader (`aseprite_*.cpp` on the same bounded JSON reader, output checked
  by that validator) has `sprite_target` (mini-fuzz in `nnh_assetc_tests`, libFuzzer `fuzz_assetc_sprite`); `data_target` validates as given and with the header fixed, applies
  every input into a guarded struct (apply clamps counts and lengths even on refused files) and checks the values of
  accepted ones; libFuzzer executable `fuzz_data`, not run on Linux yet (spec 014 T9.1). The data index `.nnh_data`
(stage 3) is checked by `data/index_validate.*` before any lookup (counts, sections, type keys, every path a valid
asset path whose FNV-1a hash matches, sorted runs, a lookup sorted by group and hash); `data_index_target` runs
every lookup on accepted indexes (libFuzzer `fuzz_data_index`). `nnh-assetc data` reads data files with a bounded
TOML-subset reader (`toml*.cpp`: ≤ 64 KiB, lines ≤ 1024 bytes, UTF-8 checked per line, arrays nested ≤ 2, headers ≤
4 parts) and the game's schema description with a bounded reader (≤ 1 MiB, counts and name lengths checked, every
fingerprint recomputed), and checks every file it writes with the engine's validator; mini-fuzz in
`nnh_assetc_tests`, libFuzzer `fuzz_assetc_toml` and `fuzz_assetc_data`, not run on Linux yet (T9.1).
`nnh-assetc data-index` reads the references sidecars the data step wrote with a strict reader (`data_refs.cpp`: ≤ 1
MiB each, every record and number checked, every name and path a valid asset path — a NUL inside one was found by
the mini-fuzz and is refused), and the lists of sidecars and assets (≤ 4 MiB); it checks the index it writes with
the engine's index validator. Fuzz target `refs_target` (mini-fuzz of 6 000 mutated sidecars, libFuzzer
`fuzz_assetc_refs`, not run on Linux yet). The data schema program (`tools/data_schema`) only runs the game's own
compiled tables through the engine's schema check. They run as a deterministic mini-fuzz (20 000 mutated inputs; 300 mutations of a real file
  for Ogg, where each run decodes; 12 000 mutations of three valid maps, one with animated tiles, for `.nnhmap`) inside `nnh_tests` on every
  compiler, and with libFuzzer + AddressSanitizer on Linux (`-DNNH_BUILD_FUZZERS=ON` with clang; homelab
  build VM or the manual CI job). The libraries under test (`nnh_core`, `nnh_stb_vorbis`, `nnh_fuzz_targets`) are built
  with `-fsanitize=fuzzer-no-link,address` in that configuration: without it libFuzzer gets no coverage
  feedback (it reported `cov: 1` until 2026-11-18) and only mutates blindly. Check that `cov:` grows
  when adding a target. Last run (60 s each): asset index cov 125, path cov 46, save cov 44, no crash.
  Audio readers (2026-11-18, seeded with `tests/data/sounds/`): WAV 60 s (5.3 M runs), Ogg 9 min, cov 953 —
  its first minute found a real bug (seeking into a damaged part reported success; now `Corrupted`,
  regression test `ogg_seek_into_a_damaged_part_is_an_error`); no memory error in stb_vorbis.
- NNH Link (spec 011) parses network input in `link/`: the message decoders, the frame reader with the
  sealed channel, and discovery answers each have a target in `link/tests/fuzz/` with a round-trip check
  (a decoded message must encode and decode again). Mini-fuzz in `nnh_link_tests`; libFuzzer executables
  `fuzz_link_*`. First run (2026-11-18, 60 s each): messages cov 451, frames cov 271, discovery cov 169,
  no crash. Second run at the end of the spec (2026-11-20, 60 s each from an empty corpus, RelWithDebInfo):
  messages cov 297 (4.5 M runs), frames cov 110 (1.9 M), discovery cov 138 (8.1 M), no crash; the same
  day every test executable (engine, `nnh_link`, NNH Link, `nnh`) passed under ASan + UBSan (clang).
  Pairing and authentication (mutual HMAC proofs, 5 wrong codes lock pairing) are unit tested
  in `link/tests/test_handshake.cpp`; the threat model is in the [NNH Link guide](../guides/nnh-link.md#safety).
- The sessions (`link/src/server_session.cpp`, `client_session.cpp`) are tested end to end on the
  loopback interface: an unpaired console can't start a build, the console refuses an impostor server
  and anything it sends, a received game is checked against its SHA-256 and announced size (a partial or
  damaged file is dropped), and every wait has a timeout. Also run under ASan + UBSan on Linux.
- `nnh serve` (spec 011) accepts connections and answers discovery only from local-network addresses,
  keeps pairing secrets in `nnh-pairs.ini` (written owner-only, replaced in one rename) and prints only
  the first 8 hex digits of a console's id. Pairing codes are valid for 2 minutes and for one pairing.
- NNH Link (`apps/nnh-link`) keeps its console id and pairing secrets in `sdmc:/3ds/nnh/link.sav`
  (`nnh::save`: atomic, checksummed; decoding refuses anything malformed, tested per truncation) and
  never shows them. The SD card has no access control: any homebrew can read that file, as with every
  homebrew secret (D24 limit). A received game is streamed to `<game>.3dsx.part` with stdio (`nnh::fs`
  writes whole files only), only for valid game names inside Link's data folder, after checking the free
  space; it replaces the old game only once complete and verified (SHA-256), and is deleted otherwise.
- Launching (`<nnh/3ds/launch.hpp>`, spec 011) only starts a `.3dsx` on the SD card (`sdmc:/...`, no
  `..` or empty segments, ≤ 255 characters) with arguments that fit the loader's 1 KB block; the same
  check applies to a game's `--nnh-return=<path>`. These rules are pure code tested on PC
  (`tests/platform_3ds/test_hbldr_args.cpp`).

## Console remote (spec 013)

The console remote is the first time a **game** reads from the network: `nnh run` sends it input and
screenshot requests (decision D28, [debugging](debugging.md#driving-a-game-from-the-pc)). The rules:

- **Development only.** Every core call is under `if constexpr (build::kDevToolsEnabled)`, so a Release build
  links none of it. `scripts/check_release_strings.sh` fails if a Release binary contains the remote's log
  texts, `NNH_REMOTE` or its function names (`remote_frame_start`, `remote_receive`, `remote_send`,
  `remote_capture`, `screenshots_send`, ...). Last checked 2026-11-21 on `3ds-release` and `pc-release`
  `hello`, `input` and `platformer`.
- **Only the PC that started the game.** The game opens the connection itself: on the 3DS, its log
  connection to the 3dslink host (`nnh run`); in the simulator, `NNH_REMOTE`, accepted only for a loopback
  address (127.0.0.0/8, tested). Nothing listens on the console. Without `Hello`, the game sends nothing
  new and behaves as before.
- **Bounded and checked.** Messages are exactly 16 bytes; every field is range-checked (magic, type, the 14
  button bits, disjoint masks, stick and touch ranges, reserved bytes zero, frames in order, `Hello` first).
  A bad message is dropped and reported; 8 bad messages in a row turn the remote off until the PC reconnects.
  Each frame reads at most what the 64-message queue has room for, never waits and never allocates.
  Disconnecting releases every remote control.
- **Fuzzed on both ends.** The console's reader, schedule and input merge are fuzz target `remote_target`
  (mini-fuzz in `nnh_tests`, libFuzzer `fuzz_remote`: 2026-11-21, 60 s, 922 651 runs, cov 132, no crash),
  with a round-trip check of every decoded message. `nnh` treats the game's stream as untrusted too: records
  over 1 MiB, unknown types and bad headers fall back to text, screen sizes must match exactly; its stream
  splitter and script parser have mini-fuzz tests in `nnh_tool_tests`. `nnh_tests` and `nnh_tool_tests`
  passed under ASan + UBSan (clang 18, Linux) the same day.

## Supply chain and CI

- Already: downloaded dependencies pinned by URL + SHA-256 (`cmake/nnh_sdl3.cmake`); no binaries in git.
  Vendored code (`third_party/stb_vorbis`) is unmodified, pinned by commit and SHA-256 in its README,
  and fuzzed through our reader; assets come from the game's own build, not from the network.
- Every workflow declares minimal `permissions` (CI: `contents: read`; only the wiki job has
  `contents: write`) and actions are pinned by commit SHA (`# vN` comment next to it). CI runs manually.
- New dependencies follow `docs/tech/cmake.md` (small, permissive, pinned, justified).

## Reporting a vulnerability

Open a private security advisory on GitHub (Security → Advisories → Report a vulnerability) instead of
a public issue.
