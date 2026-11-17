# Security and safety

Homebrew runs **without a sandbox**: a bug in a game can delete or corrupt any file on the user's SD
card. Players trust that a game made with NNHGE won't. These rules make the engine safe by default
(constitution article X).

> Status: **design** for the items marked (spec 002); the rest is already in place.

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
- Writable root: `sdmc:/3ds/<game>/` on 3DS, a per-game data folder on PC. Read-only: `romfs:/` (assets).
- Paths are **relative** to those roots. Rejected with an error: absolute paths, `..` segments, drive or
  device prefixes (`sdmc:`, `C:`), empty segments, control characters, overly long paths.
- The engine never touches NAND, system saves or other titles' data.

## Atomic saves (spec 002)

Write: `save.bin.tmp` → flush → rename the old `save.bin` to `save.bin.bak` → rename tmp to `save.bin`.
File header: magic, format version, payload size, checksum (CRC-32). Load: validate `save.bin`; if
missing or invalid, try `save.bin.bak`; report which one was used. A power cut at any point leaves at
least one valid file.

## Bounded parsers and fuzzing (spec 002)

- Every reader of external data checks sizes and offsets before using them, never trusts counts from the
  file, and fails with a `Result` error instead of crashing.
- Each parser has a fuzz target (libFuzzer with sanitizers on Linux CI, time-limited) plus a fixed corpus
  of known-bad files kept as regular unit tests.

## Supply chain and CI

- Already: downloaded dependencies pinned by URL + SHA-256 (`cmake/nnh_sdl3.cmake`); no binaries in git.
- Spec 002: every workflow declares minimal `permissions`; third-party actions pinned by commit SHA;
  only the wiki job has `contents: write`.
- New dependencies follow `docs/tech/cmake.md` (small, permissive, pinned, justified).

## Reporting a vulnerability

Open a private security advisory on GitHub (Security → Advisories → Report a vulnerability) instead of
a public issue.
