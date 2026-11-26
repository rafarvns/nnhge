# stb_truetype

TrueType/OpenType (glyf and CFF outlines) parser and rasteriser used by `nnh-assetc font` to bake game fonts on the
developer's machine (spec 029, decision D49). Host-only: never compiled into a game.

| | |
|-|-|
| Upstream | <https://github.com/nothings/stb> — `stb_truetype.h` |
| Version | v1.28, commit `9f755b604db3492df2cbeb39bf84b81e3e3932e3` (the commit `stb_vorbis` is pinned to) |
| SHA-256 | `428cfe93520f2aed5e413a220b0db5bba5b17e2ebe5a64e4a8efa9eeeb67ec5f` |
| License | public domain (Unlicense) or MIT, at the user's choice — NNHGE uses the public-domain option |
| Changes | none: the file is unmodified |

`stb_truetype.c` defines the implementation once; `CMakeLists.txt` builds it as an object library with warnings off and
`-ffp-contract=off` (`/fp:precise` under MSVC), so converted fonts have the same bytes on every host (spec 029 AC-2).
stb_truetype does no range checking (its own README says so): `tools/assetc/src/ttf_check.cpp` checks every offset it
follows before a font reaches it, and the converter hands it a copy padded with zeros. Fuzzed through the converter
(`fuzz_assetc_ttf`).

To update: download the new `stb_truetype.h` at a pinned commit, update the table above, rerun `nnh_assetc_tests`, the
`font_fixtures_golden` ctest (a changed rasteriser changes the fixtures' checksums: review and record them) and the
`ttf` fuzz target (docs/tech/third-party.md).
