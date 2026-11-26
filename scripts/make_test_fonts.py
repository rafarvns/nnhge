"""Writes the font converter's test fixtures into tests/data/fonts/ (spec 029 T4.2): original fonts of simple shapes —

  nnhtest.ttf        TrueType outlines (glyf), a `kern` table (A V −80 units)
  nnhtest_cff.otf    the same glyphs as CFF outlines, a GPOS pair adjustment (A V −80, T o −60)
  mono.fnt           a BMFont (text form) with mono_0.png: white texels, coverage in alpha
  mono_xml.fnt       the same in BMFont's XML form, with mono_xml_0.png
  colour.fnt         a pre-coloured BMFont with colour_0.png
  *.font             the descriptions the tests convert

The outputs are committed: building and testing need no Python. Fonts are made with fontTools (a dev-time tool:
`python -m pip install --user fonttools`), PNGs with the standard library. Rerunning writes the same bytes (fontTools'
timestamps are fixed). Usage: python -I scripts/make_test_fonts.py [out folder]
"""
import os
import struct
import sys
import zlib

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.pens.ttGlyphPen import TTGlyphPen

EM = 1000
ASCENT, DESCENT = 800, -200
FIXED_TIME = 3786825600  # 2020-01-01 in the font epoch (1904): reproducible bytes


def glyph_set():
    """(name, code point, advance, [rectangles (x0, y0, x1, y1)]) — boxes in font units."""
    g = [(".notdef", None, 500, [(50, 0, 450, 700), (100, 50, 400, 650)])]
    g.append(("space", 0x20, 300, []))
    for i, c in enumerate("ABCDEFGHIJKLMNOPQRSTUVWXYZ"):
        w = 560 + (i % 4) * 40
        g.append((c, ord(c), w, [(60, 0, w - 60, 700), (60 + (i % 3) * 30, 300, w - 60, 340)]))
    for i, c in enumerate("abcdefghijklmnopqrstuvwxyz"):
        w = 440 + (i % 3) * 30
        top = 700 if c in "bdfhklt" else 500
        bottom = -180 if c in "gjpqy" else 0
        g.append((c, ord(c), w, [(50, bottom, w - 50, top)]))
    for i, c in enumerate("0123456789"):
        g.append((c, ord(c), 520, [(60, 0, 460, 700), (120 + i * 10, 200, 400, 500)]))
    for c, w in ((".", 260), (",", 260), ("?", 500), ("!", 280), ("-", 400), ("'", 220)):
        g.append((c, ord(c), w, [(60, 0, w - 60, 120)] if c in ".,-" else [(60, 0, w - 60, 700)]))
    g.append(("ellipsis", 0x2026, 900, [(80, 0, 200, 120), (390, 0, 510, 120), (700, 0, 820, 120)]))
    g.append(("uniFFFD", 0xFFFD, 700, [(50, -100, 650, 750), (150, 0, 550, 650)]))
    for i, cp in enumerate((0x3042, 0x3044, 0x3046, 0x65E5, 0x672C, 0x8A9E, 0x3002, 0x300C, 0x300D)):
        g.append(("uni%04X" % cp, cp, 1000, [(80, -60, 920, 760), (200 + i * 20, 100, 800, 600 - i * 20)]))
    return g


def draw(pen, rects):
    for x0, y0, x1, y1 in rects:
        pen.moveTo((x0, y0))
        pen.lineTo((x0, y1))
        pen.lineTo((x1, y1))
        pen.lineTo((x1, y0))
        pen.closePath()


def build(cff, path):
    glyphs = glyph_set()
    names = [n for n, _, _, _ in glyphs]
    fb = FontBuilder(EM, isTTF=not cff)
    fb.setupGlyphOrder(names)
    fb.setupCharacterMap({cp: n for n, cp, _, _ in glyphs if cp is not None})
    if cff:
        charstrings = {}
        for n, _, w, rects in glyphs:
            pen = T2CharStringPen(w, None)
            draw(pen, rects)
            charstrings[n] = pen.getCharString()
        fb.setupCFF("NNHGETest-CFF", {"FullName": "NNHGE Test CFF"}, charstrings, {})
    else:
        tt = {}
        for n, _, _, rects in glyphs:
            pen = TTGlyphPen(None)
            draw(pen, rects)
            tt[n] = pen.glyph()
        fb.setupGlyf(tt)
    fb.setupHorizontalMetrics({n: (w, rects[0][0] if rects else 0) for n, _, w, rects in glyphs})
    fb.setupHorizontalHeader(ascent=ASCENT, descent=DESCENT)
    fb.setupNameTable({"familyName": "NNHGE Test", "styleName": "Regular"})
    fb.setupOS2(sTypoAscender=ASCENT, sTypoDescender=DESCENT, usWinAscent=ASCENT, usWinDescent=-DESCENT)
    fb.setupPost()
    if cff:
        fb.addOpenTypeFeatures("feature kern { pos A V -80; pos T o -60; } kern;")
    else:
        from fontTools.ttLib.tables._k_e_r_n import KernTable_format_0
        from fontTools.ttLib import newTable
        kern = newTable("kern")
        kern.version = 0
        sub = KernTable_format_0()
        sub.version, sub.coverage, sub.format = 0, 1, 0
        sub.kernTable = {("A", "V"): -80, ("V", "A"): -40}
        kern.kernTables = [sub]
        fb.font["kern"] = kern
    fb.font["head"].created = FIXED_TIME
    fb.font["head"].modified = FIXED_TIME
    fb.save(path)


def png(path, width, height, rgba):
    """A truecolour + alpha PNG (filter 0 on every row)."""
    raw = b"".join(b"\x00" + bytes(rgba[y * width * 4:(y + 1) * width * 4]) for y in range(height))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    body = chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    body += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + body)


PIXEL_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789?"


def pixel_font(colour):
    """An 8 × 8 pixel font on a 64 × 64 page: each glyph a frame with a pattern; coverage levels 0, 128, 255."""
    w, h = 64, 64
    rgba = [0] * (w * h * 4)
    chars = []
    for i, c in enumerate(PIXEL_CHARS):
        gx, gy = (i % 7) * 9, (i // 7) * 9
        for y in range(8):
            for x in range(7):
                on = x in (0, 6) or y in (0, 7) or (x + y + i) % 5 == 0
                if not on:
                    continue
                a = 255 if (x + y) % 3 else 128
                p = ((gy + y) * w + gx + x) * 4
                rgba[p:p + 4] = ([200, 40 + x * 20, 60 + y * 20, 255] if colour else [255, 255, 255, a])
        chars.append((ord(c), gx, gy, 7, 8, 0, 1, 8))
    return w, h, rgba, chars


def bmfont_text(name, chars, colour):
    lines = ['info face="NNHGE Pixel" size=8 bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=0 aa=1 '
             'padding=0,0,0,0 spacing=1,1 outline=0',
             "common lineHeight=10 base=8 scaleW=64 scaleH=64 pages=1 packed=0 alphaChnl=%d redChnl=0 greenChnl=0 "
             "blueChnl=0" % (0 if colour else 1),
             'page id=0 file="%s_0.png"' % name, "chars count=%d" % len(chars)]
    for cp, x, y, cw, ch, xo, yo, adv in chars:
        lines.append("char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d page=0 chnl=15"
                     % (cp, x, y, cw, ch, xo, yo, adv))
    lines += ["kernings count=2", "kerning first=65 second=86 amount=-1", "kerning first=86 second=65 amount=-1"]
    return "\n".join(lines) + "\n"


def bmfont_xml(name, chars):
    out = ['<?xml version="1.0"?>', "<font>",
           '  <info face="NNHGE Pixel" size="8" bold="0" italic="0" padding="0,0,0,0" spacing="1,1" outline="0"/>',
           '  <common lineHeight="10" base="8" scaleW="64" scaleH="64" pages="1" packed="0"/>',
           "  <pages>", '    <page id="0" file="%s_0.png" />' % name, "  </pages>",
           '  <chars count="%d">' % len(chars)]
    for cp, x, y, cw, ch, xo, yo, adv in chars:
        out.append('    <char id="%d" x="%d" y="%d" width="%d" height="%d" xoffset="%d" yoffset="%d" xadvance="%d" '
                   'page="0" chnl="15" />' % (cp, x, y, cw, ch, xo, yo, adv))
    out += ["  </chars>", '  <kernings count="1">', '    <kerning first="65" second="86" amount="-1" />',
            "  </kernings>", "</font>"]
    return "\n".join(out) + "\n"


DESCRIPTIONS = {
    "body_8.font": 'source = "nnhtest.ttf"\nsize = 8\nlicence = "own"\n',
    "body_12.font": 'source = "nnhtest.ttf"\nsize = 12\nchars = ["ascii", "U+2026", "U+FFFD"]\nlicence = "own"\n',
    "body_16.font": 'source = "nnhtest.ttf"\nsize = 16\nchars = ["ascii", "U+2026", "U+FFFD", "kana"]\n'
                    'licence = "own"\n',
    "body_32.font": 'source = "nnhtest.ttf"\nsize = 32\nchars = ["ascii"]\nlicence = "own"\n',
    "pixel_12.font": 'source = "nnhtest.ttf"\nsize = 12\nantialias = false\nthreshold = 128\nlicence = "own"\n',
    "outline_16.font": 'source = "nnhtest.ttf"\nsize = 16\noutline = 1\nlicence = "own"\n',
    "outline3_16.font": 'source = "nnhtest.ttf"\nsize = 16\noutline = 3\nchars = ["U+0041..U+005A"]\nlicence = "own"\n',
    "cff_16.font": 'source = "nnhtest_cff.otf"\nsize = 16\nchars = ["ascii", "U+2026"]\nlicence = "own"\n',
}


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "tests", "data", "fonts")
    os.makedirs(out, exist_ok=True)
    build(False, os.path.join(out, "nnhtest.ttf"))
    build(True, os.path.join(out, "nnhtest_cff.otf"))
    for name, colour in (("mono", False), ("colour", True)):
        w, h, rgba, chars = pixel_font(colour)
        png(os.path.join(out, name + "_0.png"), w, h, rgba)
        open(os.path.join(out, name + ".fnt"), "w", encoding="utf-8", newline="\n").write(bmfont_text(name, chars, colour))
    w, h, rgba, chars = pixel_font(False)
    png(os.path.join(out, "mono_xml_0.png"), w, h, rgba)
    open(os.path.join(out, "mono_xml.fnt"), "w", encoding="utf-8", newline="\n").write(bmfont_xml("mono_xml", chars))
    for name, text in DESCRIPTIONS.items():
        open(os.path.join(out, name), "w", encoding="utf-8", newline="\n").write(text)
    print("test fonts written to", out)


if __name__ == "__main__":
    main()
