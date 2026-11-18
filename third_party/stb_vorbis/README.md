# stb_vorbis

Ogg Vorbis decoder used by the engine's music and sound loading (spec 008).

| | |
|-|-|
| Upstream | <https://github.com/nothings/stb> — `stb_vorbis.c` |
| Version | v1.24, commit `9f755b604db3492df2cbeb39bf84b81e3e3932e3` (2026-11-12) |
| SHA-256 | `b52cf518355f7b2d181f18b97b812f361ffeb26299d935c8cf29e842401d60a6` |
| License | public domain (Unlicense) or MIT, at the user's choice — NNHGE uses the public-domain option, so games ship no notice |
| Changes | none: the file is unmodified |

Built as its own object library (`CMakeLists.txt` here) with the push-data API only
(`STB_VORBIS_NO_PULLDATA_API`, which also removes stdio), compiler warnings off, and linked into
`nnh::core`. Core includes it through `engine/core/src/audio/stb_vorbis_api.hpp` and gives it a fixed
memory arena, so it never calls `malloc`. In sanitizer builds UBSan's `shift-base` check is off for this
file only: it computes Ogg checksums with `byte << 24` on `int` (undefined in C, correct on GCC and
Clang); all other checks, and ASan, cover it.

To update: download the new `stb_vorbis.c` at a pinned commit, update the table above, run the audio
tests and the `ogg` fuzz target (constitution X; `docs/architecture/security.md`).
