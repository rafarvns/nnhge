# stb_image

PNG decoder used by `nnh-assetc font` to read BMFont pages on the developer's machine (spec 029, decision D49, plan
question 2). PNG only, no stdio. Host-only: never compiled into a game.

| | |
|-|-|
| Upstream | <https://github.com/nothings/stb> — `stb_image.h` |
| Version | v2.32, commit `9f755b604db3492df2cbeb39bf84b81e3e3932e3` (the commit `stb_vorbis` is pinned to) |
| SHA-256 | `deb4f18c9ca21b751a1323019031c94c0bf5ca1d2929ef5913cc56f00aab6e91` |
| License | public domain (Unlicense) or MIT, at the user's choice — NNHGE uses the public-domain option |
| Changes | none: the file is unmodified |

`stb_image.c` defines the implementation once with `STBI_ONLY_PNG` and `STBI_NO_STDIO`; `CMakeLists.txt` builds it as an
object library with warnings off. `tools/assetc/src/png_pages.cpp` wraps it (sizes checked before and after decoding).
Fuzzed through the converter (`fuzz_assetc_png_pages`).

To update: as stb_truetype (README there; docs/tech/third-party.md).
