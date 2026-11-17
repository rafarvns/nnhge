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
- Each parser has a fuzz target in `tests/fuzz/` (today: path validator, save-file validator). They run as
  a deterministic mini-fuzz (20 000 mutated inputs) inside `nnh_tests` on every compiler, and with
  libFuzzer + AddressSanitizer on Linux (`-DNNH_BUILD_FUZZERS=ON` with clang; homelab build VM or the
  manual CI job).

## Supply chain and CI

- Already: downloaded dependencies pinned by URL + SHA-256 (`cmake/nnh_sdl3.cmake`); no binaries in git.
- Every workflow declares minimal `permissions` (CI: `contents: read`; only the wiki job has
  `contents: write`) and actions are pinned by commit SHA (`# vN` comment next to it). CI runs manually.
- New dependencies follow `docs/tech/cmake.md` (small, permissive, pinned, justified).

## Reporting a vulnerability

Open a private security advisory on GitHub (Security → Advisories → Report a vulnerability) instead of
a public issue.
