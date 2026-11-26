# The localization example's fonts

Everything here is outside `assets/`: nothing in this folder is packed into the game.

## NNHGE Example Sans JP (`assets/fonts/src/NNHExampleSansJP.otf`)

A subset of **Noto Sans JP Regular**, renamed as the SIL Open Font License 1.1 allows for a modified version. The
licence, with the font's copyright notice, ships with the game: `assets/licences/OFL-NNHExampleSansJP.txt` (every
`.font` description names it in `licence`).

| | |
|---|---|
| Upstream file | <https://raw.githubusercontent.com/notofonts/noto-cjk/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/SubsetOTF/JP/NotoSansJP-Regular.otf> |
| Release | noto-cjk `Sans2.004` (commit `f8d157532fbfaeda587e826d4cd5b21a49186f7c`) |
| Upstream SHA-256 | `dff723ba59d57d136764a04b9b2d03205544f7cd785a711442d6d2d085ac5073` (4 533 028 bytes) |
| Upstream licence | `Sans/LICENSE` at the same commit, OFL 1.1, SHA-256 `6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2` |
| Subset SHA-256 | `eb31dac6f012a4bcfade2f52ee1f988fba0e6a8bbbefab520097772da2b2ff7c` (269 036 bytes, 1 671 code points) |
| Made by | `python -I make_subset.py <NotoSansJP-Regular.otf> <repo root> <out.otf>` (fontTools 4.63; byte-identical on reruns) |

Kept: printable ASCII and Latin-1, the ellipsis, basic Cyrillic, general punctuation, the euro sign, CJK punctuation,
hiragana and katakana, the full-width forms, the 1 006 kyoiku kanji (from `tools/assetc/src/charsets_cjk.cpp`) and every
character of the example's string files; the layout tables only for pair kerning (`kern`, `palt`). Characters the
upstream font doesn't have (U+FFFD, parts of the Cyrillic block) are reported by the build as notes. To add text that
uses new kanji, rerun the script and commit the new subset with its SHA-256 here.

## The HUD font (`assets/fonts/hud.fnt`, `hud_0.png`)

An original 5 × 7 pixel font drawn in `art/make_hud_font.py` (zlib, like the engine): digits, the group separators,
`x : + - / [ ] ~` and the space. The same script writes the bench's sprite `assets/art/spark.png`.
