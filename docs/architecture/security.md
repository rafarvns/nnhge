# Security and safety

Homebrew runs **without a sandbox**: a bug in a game can delete or corrupt any file on the user's SD
card. Players trust that a game made with NNHGE won't. These rules make the engine safe by default
(constitution article X).

> Status: **implemented** (spec 002): confined files, atomic saves, bounded parsers, fuzzing, CI hardening.
> API: [docs/api/fs.md](../api/fs.md).

## Threat model (what we protect against)

| Risk | Source | Protection |
|------|--------|------------|
| Writing outside the game's folder | bug in game or engine, malicious path in data | confined file access (below) |
| Corrupted save after power loss | battery dies mid-write | atomic saves with backup |
| Crash or memory corruption from bad data | corrupted/modified save, asset or downloaded file | bounded parsers + fuzz tests |
| Malicious code reaching users | compromised dependency or CI | pinned hashes, pinned actions, least privilege |
| Code pushed to a console over the network | NNH Link | pairing + LAN only (NNH Link spec, later) |
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
  seeking). They run as a deterministic mini-fuzz (20 000 mutated inputs; 300 mutations of a real file
  for Ogg, where each run decodes) inside `nnh_tests` on every
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
  no crash. Pairing and authentication (mutual HMAC proofs, 5 wrong codes lock pairing) are unit tested
  in `link/tests/test_handshake.cpp`; the threat model comes with the spec 011 guide.
- The sessions (`link/src/server_session.cpp`, `client_session.cpp`) are tested end to end on the
  loopback interface: an unpaired console can't start a build, the console refuses an impostor server
  and anything it sends, a received game is checked against its SHA-256 and announced size (a partial or
  damaged file is dropped), and every wait has a timeout. Also run under ASan + UBSan on Linux.
- `nnh serve` (spec 011) accepts connections and answers discovery only from local-network addresses,
  keeps pairing secrets in `nnh-pairs.ini` (written owner-only, replaced in one rename) and prints only
  the first 8 hex digits of a console's id. Pairing codes are valid for 2 minutes and for one pairing.
- Launching (`<nnh/3ds/launch.hpp>`, spec 011) only starts a `.3dsx` on the SD card (`sdmc:/...`, no
  `..` or empty segments, ≤ 255 characters) with arguments that fit the loader's 1 KB block; the same
  check applies to a game's `--nnh-return=<path>`. These rules are pure code tested on PC
  (`tests/platform_3ds/test_hbldr_args.cpp`).

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
