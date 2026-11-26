"""Makes the example's Japanese font (spec 029 T7.1): a reproducible subset of Noto Sans JP Regular, renamed
"NNHGE Example Sans JP" as the SIL Open Font License 1.1 asks of a modified version (Reserved Font Name: none is
declared by Noto, the rename keeps the original's name for the original).

Upstream (approved by the maintainer, 2026-11-26):
  https://raw.githubusercontent.com/notofonts/noto-cjk/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/SubsetOTF/JP/NotoSansJP-Regular.otf
  noto-cjk release Sans2.004, SHA-256 dff723ba59d57d136764a04b9b2d03205544f7cd785a711442d6d2d085ac5073 (4 533 028 bytes)
  licence: Sans/LICENSE at the same commit, OFL 1.1, SHA-256 6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2

Characters kept: printable ASCII, Latin-1's printable range, the ellipsis and U+FFFD, CJK punctuation (U+3000..U+303F),
hiragana and katakana (U+3041..U+30FF), the full-width forms (U+FF01..U+FF5E), basic Cyrillic (U+0400..U+045F), general
punctuation (U+2010..U+2027) and the euro sign, the 1 006 kyoiku kanji (read from tools/assetc/src/charsets_cjk.cpp, the
converter's own table) and every character of the example's string files.

Usage (fontTools 4.x; outputs are byte-identical across runs):
  python -I make_subset.py <NotoSansJP-Regular.otf> <repo root> <out.otf>
"""
import hashlib
import os
import re
import sys

from fontTools import subset
from fontTools.ttLib import TTFont

UPSTREAM_SHA256 = "dff723ba59d57d136764a04b9b2d03205544f7cd785a711442d6d2d085ac5073"
FAMILY = "NNHGE Example Sans JP"
POSTSCRIPT = "NNHGEExampleSansJP-Regular"


def kyoiku(root):
    text = open(os.path.join(root, "tools", "assetc", "src", "charsets_cjk.cpp"), encoding="utf-8").read()
    block = text[text.index("kKyoiku[1006]"):]
    block = block[:block.index("};")]
    cps = [int(h, 16) for h in re.findall(r"0x([0-9A-F]{4})", block)]
    assert len(cps) == 1006, len(cps)
    return cps


def string_chars(root):
    cps = set()
    folder = os.path.join(root, "examples", "localization", "assets", "strings")
    for base, _, files in sorted(os.walk(folder)):
        for name in sorted(files):
            if name.endswith(".strings"):
                cps |= {ord(c) for c in open(os.path.join(base, name), encoding="utf-8").read() if ord(c) >= 0x20}
    return sorted(cps)


def characters(root):
    cps = set(range(0x20, 0x7F)) | set(range(0xA0, 0x100)) | {0x2026, 0xFFFD}
    cps |= set(range(0x0400, 0x0460)) | set(range(0x2010, 0x2028)) | {0x20AC}
    cps |= set(range(0x3000, 0x3040)) | set(range(0x3041, 0x3100)) | set(range(0xFF01, 0xFF5F))
    cps |= set(kyoiku(root)) | set(string_chars(root))
    return sorted(cps)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    source, root, out = sys.argv[1:4]
    digest = hashlib.sha256(open(source, "rb").read()).hexdigest()
    if digest != UPSTREAM_SHA256:
        sys.exit("unexpected upstream file: SHA-256 %s" % digest)
    options = subset.Options()
    options.layout_features = ["kern", "palt"]  # what the converter reads (pair kerning); the rest is size
    options.name_IDs = ["*"]
    options.name_languages = ["*"]
    options.notdef_outline = True
    options.glyph_names = False
    options.hinting = False
    options.desubroutinize = False
    font = TTFont(source, recalcTimestamp=False)
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=characters(root))
    subsetter.subset(font)
    names = font["name"]
    for record in list(names.names):
        if record.nameID in (1, 4, 16, 21):
            record.string = FAMILY if record.nameID != 4 else FAMILY + " Regular"
        elif record.nameID == 3:
            record.string = "1.000;NNHGE;" + POSTSCRIPT
        elif record.nameID == 6:
            record.string = POSTSCRIPT
        elif record.nameID in (17, 22):
            record.string = "Regular"
    if "CFF " in font:
        cff = font["CFF "].cff
        cff.fontNames = [POSTSCRIPT]
        top = cff.topDictIndex[0]
        top.FullName = FAMILY + " Regular"
        top.FamilyName = FAMILY
    font["head"].modified = font["head"].created  # no timestamp: the same bytes on every run
    font.save(out)
    data = open(out, "rb").read()
    print("%s: %d bytes, %d code points, SHA-256 %s" % (out, len(data), len(characters(root)),
                                                       hashlib.sha256(data).hexdigest()))


if __name__ == "__main__":
    main()
