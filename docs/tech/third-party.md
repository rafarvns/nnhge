# Third-party code

NNHGE vendors a few single-file libraries, unmodified and pinned, in `third_party/<name>/` (each with a `README.md`
giving the upstream, commit, SHA-256 and licence, and its own `CMakeLists.txt`). Nothing else is downloaded at build
time except SDL 3 for the simulator ([cmake.md](cmake.md)). Adding one needs the maintainer's approval (constitution:
no new dependencies without it).

| Library | Version (commit) | Used by | Where it runs | Fuzzed by |
|---------|------------------|---------|---------------|-----------|
| [stb_vorbis](../../third_party/stb_vorbis/README.md) | v1.24 (`nothings/stb` `9f755b6`) | Ogg music and sounds (spec 008), linked into `nnh::core` | every game that plays Ogg | `fuzz_audio_*` (tests/fuzz) |
| [stb_truetype](../../third_party/stb_truetype/README.md) | v1.28 (`9f755b6`) | `nnh-assetc font`: rasterising TTF/OTF (spec 029) | the developer's machine only | `fuzz_assetc_ttf`, behind `ttf_check.cpp` |
| [stb_image](../../third_party/stb_image/README.md) | v2.32 (`9f755b6`), PNG only, no stdio | `nnh-assetc font`: BMFont pages (spec 029) | the developer's machine only | `fuzz_assetc_png_pages` |

## Rules

- **Unmodified**: configuration goes in the one `.c` file that defines the implementation (`STBI_ONLY_PNG`, assertions
  off), never in the header.
- **Warnings off, determinism on**: each is an object library with warnings disabled and `-ffp-contract=off`
  (`/fp:precise` under MSVC), so converted assets have the same bytes on every compiler and host.
- **Untrusted input goes through our own checks first** (constitution X): stb_truetype does no range checking, so
  `tools/assetc/src/ttf_check.cpp` validates every offset it will follow; `png_pages.cpp` checks sizes before and after
  stb_image decodes.
- **Host-only libraries never reach a game**: stb_truetype and stb_image are linked into `nnh-assetc` only; a
  3ds-release game has no `stbtt_`/`stbi_` symbol (checked by the link probes of plan 029, stage 8).

## Updating one

1. Download the file at a pinned commit of the upstream; record the commit, version and SHA-256 in its `README.md`.
2. Rebuild and run `nnh_tests`/`nnh_assetc_tests` and the goldens that depend on it: for the font libraries, the
   `font_fixtures_golden` ctest (a changed rasteriser changes the fixtures' checksums: review the difference, then
   record them with `-DRECORD=1`, see `tests/data/fonts/golden.cmake`) and the localization example's self-check.
3. Run the library's libFuzzer target for 10 minutes under ASan/UBSan on the build VM ([cmake.md](cmake.md)).
