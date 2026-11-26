# Ryu (`f2s` only)

Shortest round-trip printing of `float`, used by live values (spec 030) to print every float the same way in the panel,
in `nnh` and in dumps, on every system ([plan](../../specs/030-live-values/plan.md), question 3).

| | |
|-|-|
| Upstream | <https://github.com/ulfjack/ryu> — `ryu/f2s.c` and the headers it includes |
| Commit | `4c0618b0e44f7ef027ebae05d2cc7812048f7c8f` (master, 2026-02-09) |
| License | Apache-2.0 **or** Boost Software License 1.0, at the user's choice — NNHGE uses **BSL-1.0** (`LICENSE-Boost`, compatible with zlib; binaries need no notice) |
| Changes | none: the files are unmodified |

| File | SHA-256 |
|------|---------|
| `ryu/f2s.c` | `0a98f4fce6be4c599f9602b8059e95bae6bffbb622dcde52deb4a8f8b51f5e0e` |
| `ryu/ryu.h` | `b7feab0ba1df5e9ef3d602f386592ad152491405e49b408fb21c0e0e9e6bfb16` |
| `ryu/common.h` | `0bbd71d26da6193e678d0776cf418f43f287c73d6fd6725353df0aadf70f2a19` |
| `ryu/digit_table.h` | `8b782573abc0b8554d74163ae6c02f0beb5c30d1e63eaebdb0ddf2c98d817e01` |
| `ryu/f2s_intrinsics.h` | `662d2903454b8f6c44f18d0567e6d3f8f73f360d0d50a46e91fd245fdfa8fdc0` |
| `ryu/f2s_full_table.h` | `ec08e3a1993d29ed5cabd54c97f2bd7dcf9e5f7b61cf854502613d6634855fb3` |
| `LICENSE-Boost` | `c9bff75738922193e67fa726fa225535870d2aa1059f91452c411736284ad566` |

Built as its own object library (`CMakeLists.txt` here) with warnings off, `-ffp-contract=off` and two definitions set
on the command line, never in the files:

- `RYU_FLOAT_FULL_TABLE`: `f2s_intrinsics.h` uses the 3 KB float table (`f2s_full_table.h`) instead of the double tables
  (`d2s_full_table.h` / `d2s_small_table.h`, not vendored: never included with this definition).
- `NDEBUG`: Ryu's internal `assert`s off in every flavour (they check its own invariants, not input).

Ryu is integer-only (32 × 64-bit multiplies, no floating point and no division by a variable), so the same digits come
out on the PC and on the 3DS. Only `f2s_buffered_n()` is called (`engine/core/src/live/live_format.cpp`); `f2s()`
(which calls `malloc`) is never referenced. Linked into `nnh::core` (reached only by games that call `live::`) and the
`nnh` tool. A 3ds-release game has no `f2s` symbol (spec 030 T8.2).

To update: download the same files at a new pinned commit, update both tables above, then run `nnh_tests live_format`
and the exhaustive ctest `live_float_text_exhaustive` (all 2^32 floats, build VM).
