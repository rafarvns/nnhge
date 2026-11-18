"""Generates the built-in NNHGE font (spec 005): run `python -I scripts/make_font.py`.

Writes engine/core/src/text/builtin_font.cpp (committed; building needs no Python) and, for review,
build/font_preview.png (not committed). The font is original to NNHGE and under the project's zlib license.

Grid of every glyph: an 8x8 cell, monospace. Glyphs use columns 1-6 (written below as 6-character
strings); columns 0 and 7 stay empty, so characters are at least 2 pixels apart. Rows:
  0      accents over capitals            1-6  capitals and digits (6 rows)
  3-6    lowercase x-height               1    ascenders (b, d, h, k, l)
  7      descenders (g, j, p, q, y)       1-2  accents over lowercase
Accented capitals use 5-row capitals (rows 2-6) under a 2-row accent (rows 0-1).

Glyph order (must match engine/core/src/text/glyphs.hpp): 0-94 ASCII 32-126, 95 the replacement glyph,
96-191 Latin-1 160-255.
"""

import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GLYPHS = {}  # code point -> 8 strings of 6 characters ('#' = pixel)


def glyph(char, first_row, *rows):
    """Defines a glyph: `rows` are consecutive rows starting at `first_row`; the rest stay empty."""
    cells = ["......"] * 8
    for i, row in enumerate(rows):
        assert len(row) == 6, (char, row)
        cells[first_row + i] = row
    GLYPHS[ord(char) if isinstance(char, str) else char] = cells


# ---------------------------------------------------------------- ASCII 32-126
glyph(" ", 0)
glyph("!", 1, "..#...", "..#...", "..#...", "..#...", "......", "..#...")
glyph('"', 1, ".#.#..", ".#.#..")
glyph("#", 1, ".#.#..", "#####.", ".#.#..", ".#.#..", "#####.", ".#.#..")
glyph("$", 0, "..#...", ".####.", "#.#...", ".###..", "..#.#.", "####..", "..#...")
glyph("%", 1, "##..#.", "##.#..", "...#..", "..#...", ".#.##.", "#..##.")
glyph("&", 1, ".##...", "#..#..", ".##...", "#.#.#.", "#..#..", ".##.#.")
glyph("'", 1, "..#...", "..#...")
glyph("(", 1, "...#..", "..#...", "..#...", "..#...", "..#...", "...#..")
glyph(")", 1, ".#....", "..#...", "..#...", "..#...", "..#...", ".#....")
glyph("*", 2, "..#...", "#.#.#.", ".###..", "#.#.#.", "..#...")
glyph("+", 2, "..#...", "..#...", "#####.", "..#...", "..#...")
glyph(",", 6, "..#...", ".#....")
glyph("-", 4, "#####.")
glyph(".", 6, "..#...")
glyph("/", 1, "....#.", "...#..", "..#...", "..#...", ".#....", "#.....")
glyph("0", 1, ".###..", "#...#.", "#..##.", "##..#.", "#...#.", ".###..")
glyph("1", 1, "..#...", ".##...", "..#...", "..#...", "..#...", ".###..")
glyph("2", 1, ".###..", "#...#.", "...#..", "..#...", ".#....", "#####.")
glyph("3", 1, "####..", "....#.", ".###..", "....#.", "....#.", "####..")
glyph("4", 1, "...#..", "..##..", ".#.#..", "#..#..", "#####.", "...#..")
glyph("5", 1, "#####.", "#.....", "####..", "....#.", "#...#.", ".###..")
glyph("6", 1, ".###..", "#.....", "####..", "#...#.", "#...#.", ".###..")
glyph("7", 1, "#####.", "....#.", "...#..", "..#...", "..#...", "..#...")
glyph("8", 1, ".###..", "#...#.", ".###..", "#...#.", "#...#.", ".###..")
glyph("9", 1, ".###..", "#...#.", "#...#.", ".####.", "....#.", ".###..")
glyph(":", 3, "..#...", "......", "......", "..#...")
glyph(";", 3, "..#...", "......", "......", "..#...", ".#....")
glyph("<", 2, "...#..", "..#...", ".#....", "..#...", "...#..")
glyph("=", 3, "#####.", "......", "#####.")
glyph(">", 2, ".#....", "..#...", "...#..", "..#...", ".#....")
glyph("?", 1, ".###..", "#...#.", "...#..", "..#...", "......", "..#...")
glyph("@", 1, ".###..", "#...#.", "#.###.", "#.###.", "#.....", ".####.")
glyph("A", 1, ".###..", "#...#.", "#...#.", "#####.", "#...#.", "#...#.")
glyph("B", 1, "####..", "#...#.", "####..", "#...#.", "#...#.", "####..")
glyph("C", 1, ".###..", "#...#.", "#.....", "#.....", "#...#.", ".###..")
glyph("D", 1, "###...", "#..#..", "#...#.", "#...#.", "#..#..", "###...")
glyph("E", 1, "#####.", "#.....", "####..", "#.....", "#.....", "#####.")
glyph("F", 1, "#####.", "#.....", "####..", "#.....", "#.....", "#.....")
glyph("G", 1, ".###..", "#.....", "#.....", "#..##.", "#...#.", ".####.")
glyph("H", 1, "#...#.", "#...#.", "#####.", "#...#.", "#...#.", "#...#.")
glyph("I", 1, ".###..", "..#...", "..#...", "..#...", "..#...", ".###..")
glyph("J", 1, "..###.", "...#..", "...#..", "...#..", "#..#..", ".##...")
glyph("K", 1, "#...#.", "#..#..", "###...", "#.#...", "#..#..", "#...#.")
glyph("L", 1, "#.....", "#.....", "#.....", "#.....", "#.....", "#####.")
glyph("M", 1, "#...#.", "##.##.", "#.#.#.", "#...#.", "#...#.", "#...#.")
glyph("N", 1, "#...#.", "##..#.", "#.#.#.", "#..##.", "#...#.", "#...#.")
glyph("O", 1, ".###..", "#...#.", "#...#.", "#...#.", "#...#.", ".###..")
glyph("P", 1, "####..", "#...#.", "#...#.", "####..", "#.....", "#.....")
glyph("Q", 1, ".###..", "#...#.", "#...#.", "#.#.#.", "#..#..", ".##.#.")
glyph("R", 1, "####..", "#...#.", "#...#.", "####..", "#..#..", "#...#.")
glyph("S", 1, ".####.", "#.....", ".###..", "....#.", "....#.", "####..")
glyph("T", 1, "#####.", "..#...", "..#...", "..#...", "..#...", "..#...")
glyph("U", 1, "#...#.", "#...#.", "#...#.", "#...#.", "#...#.", ".###..")
glyph("V", 1, "#...#.", "#...#.", "#...#.", "#...#.", ".#.#..", "..#...")
glyph("W", 1, "#...#.", "#...#.", "#...#.", "#.#.#.", "##.##.", "#...#.")
glyph("X", 1, "#...#.", ".#.#..", "..#...", "..#...", ".#.#..", "#...#.")
glyph("Y", 1, "#...#.", ".#.#..", "..#...", "..#...", "..#...", "..#...")
glyph("Z", 1, "#####.", "....#.", "...#..", "..#...", ".#....", "#####.")
glyph("[", 1, ".##...", ".#....", ".#....", ".#....", ".#....", ".##...")
glyph("\\", 1, "#.....", ".#....", "..#...", "..#...", "...#..", "....#.")
glyph("]", 1, ".##...", "..#...", "..#...", "..#...", "..#...", ".##...")
glyph("^", 1, "..#...", ".#.#..")
glyph("_", 7, "#####.")
glyph("`", 1, ".#....", "..#...")
glyph("a", 3, ".####.", "#...#.", "#...#.", ".####.")
glyph("b", 1, "#.....", "#.....", "####..", "#...#.", "#...#.", "####..")
glyph("c", 3, ".####.", "#.....", "#.....", ".####.")
glyph("d", 1, "....#.", "....#.", ".####.", "#...#.", "#...#.", ".####.")
glyph("e", 3, ".###..", "#####.", "#.....", ".####.")
glyph("f", 1, "..##..", ".#....", "####..", ".#....", ".#....", ".#....")
glyph("g", 3, ".####.", "#...#.", ".####.", "....#.", ".###..")
glyph("h", 1, "#.....", "#.....", "####..", "#...#.", "#...#.", "#...#.")
glyph("i", 1, "..#...", "......", ".##...", "..#...", "..#...", ".###..")
glyph("j", 1, "...#..", "......", "..##..", "...#..", "...#..", "...#..", ".##...")
glyph("k", 1, "#.....", "#.....", "#..#..", "###...", "#..#..", "#...#.")
glyph("l", 1, ".##...", "..#...", "..#...", "..#...", "..#...", ".###..")
glyph("m", 3, "##.#..", "#.#.#.", "#.#.#.", "#...#.")
glyph("n", 3, "####..", "#...#.", "#...#.", "#...#.")
glyph("o", 3, ".###..", "#...#.", "#...#.", ".###..")
glyph("p", 3, "####..", "#...#.", "####..", "#.....", "#.....")
glyph("q", 3, ".####.", "#...#.", ".####.", "....#.", "....#.")
glyph("r", 3, "#.##..", "##....", "#.....", "#.....")
glyph("s", 3, ".####.", "##....", "...##.", "####..")
glyph("t", 1, ".#....", ".#....", "####..", ".#....", ".#....", "..##..")
glyph("u", 3, "#...#.", "#...#.", "#...#.", ".####.")
glyph("v", 3, "#...#.", "#...#.", ".#.#..", "..#...")
glyph("w", 3, "#...#.", "#.#.#.", "#.#.#.", ".#.#..")
glyph("x", 3, "#...#.", ".#.#..", ".#.#..", "#...#.")
glyph("y", 3, "#...#.", "#...#.", ".####.", "....#.", ".###..")
glyph("z", 3, "#####.", "...#..", ".#....", "#####.")
glyph("{", 1, "..##..", "..#...", ".#....", "..#...", "..#...", "..##..")
glyph("|", 1, "..#...", "..#...", "..#...", "..#...", "..#...", "..#...", "..#...")
glyph("}", 1, ".##...", "..#...", "...#..", "..#...", "..#...", ".##...")
glyph("~", 3, ".##.#.", "#..#..")
REPLACEMENT = ["......", "#####.", "#...#.", "#...#.", "#...#.", "#...#.", "#####.", "......"]

# ---------------------------------------------------------------- Latin-1 symbols 160-191
glyph(0xA0, 0)  # no-break space
glyph("¡", 1, "..#...", "......", "..#...", "..#...", "..#...", "..#...")
glyph("¢", 1, "..#...", ".####.", "#.#...", "#.#...", ".####.", "..#...")
glyph("£", 1, "..##..", ".#..#.", ".#....", "###...", ".#....", "#####.")
glyph("¤", 2, "#...#.", ".###..", ".#.#..", ".###..", "#...#.")
glyph("¥", 1, "#...#.", ".#.#..", "#####.", "..#...", "#####.", "..#...")
glyph("¦", 1, "..#...", "..#...", "......", "..#...", "..#...", "..#...")
glyph("§", 0, ".###..", "#.....", ".##...", "#..#..", ".##...", "...#..", "###...")
glyph("¨", 1, ".#.#..")
glyph("©", 0, ".####.", "#....#", "#.##.#", "#.#..#", "#.##.#", "#....#", ".####.")
glyph("ª", 1, ".###..", "#..#..", ".####.", "......", "#####.")
glyph("«", 2, "..#.#.", ".#.#..", "#.#...", ".#.#..", "..#.#.")
glyph("¬", 3, "#####.", "....#.")
glyph(0xAD, 4, "#####.")  # soft hyphen
glyph("®", 0, ".####.", "#....#", "#.##.#", "#.##.#", "#.#.##", "#....#", ".####.")
glyph("¯", 0, "#####.")
glyph("°", 1, ".##...", "#..#..", ".##...")
glyph("±", 1, "..#...", "..#...", "#####.", "..#...", "..#...", "#####.")
glyph("²", 0, "##....", "..#...", ".#....", "###...")
glyph("³", 0, "###...", ".##...", "..#...", "###...")
glyph("´", 1, "...#..", "..#...")
glyph("µ", 3, "#..#..", "#..#..", "#..#..", "###.#.", "#.....")
glyph("¶", 1, ".####.", "###.#.", "###.#.", ".##.#.", "..#.#.", "..#.#.")
glyph("·", 4, "..#...")
glyph("¸", 6, "..#...", ".#....")
glyph("¹", 0, ".#....", "##....", ".#....", "###...")
glyph("º", 1, ".###..", "#...#.", ".###..", "......", "#####.")
glyph("»", 2, "#.#...", ".#.#..", "..#.#.", ".#.#..", "#.#...")
glyph("¼", 0, "#.....", "#...#.", "#..#..", "..#...", ".#.#..", "#.###.", "...#..")
glyph("½", 0, "#.....", "#...#.", "#..#..", "..#...", ".#.##.", "#...#.", "...#..", "...##.")
glyph("¾", 0, "##....", ".#..#.", "##.#..", "..#...", ".#.#..", "#.###.", "...#..")
glyph("¿", 1, "..#...", "......", "..#...", ".#....", "#...#.", ".###..")

# ---------------------------------------------------------------- Latin-1 letters 192-255
# Capitals shortened to rows 2-6, so a 2-row accent fits on rows 0-1.
SHORT_CAPS = {
    "A": [".###..", "#...#.", "#####.", "#...#.", "#...#."],
    "E": ["#####.", "#.....", "####..", "#.....", "#####."],
    "I": [".###..", "..#...", "..#...", "..#...", ".###.."],
    "O": [".###..", "#...#.", "#...#.", "#...#.", ".###.."],
    "U": ["#...#.", "#...#.", "#...#.", "#...#.", ".###.."],
    "N": ["#...#.", "##..#.", "#.#.#.", "#..##.", "#...#."],
    "Y": ["#...#.", ".#.#..", "..#...", "..#...", "..#..."],
}
DOTLESS_I = [".##...", "..#...", "..#...", ".###.."]  # rows 3-6
ACCENTS = {  # two rows, centred over a 5-pixel letter
    "grave": [".#....", "..#..."],
    "acute": ["...#..", "..#..."],
    "circumflex": ["..#...", ".#.#.."],
    "tilde": [".##.#.", "#..#.."],
    "diaeresis": ["......", ".#.#.."],
    "ring": [".###..", ".#.#.."],
}


def accented_capital(char, base, accent):
    glyph(char, 0, *ACCENTS[accent], *SHORT_CAPS[base])


def accented_lower(char, base, accent):
    rows = list(GLYPHS[ord(base)]) if base != "dotless_i" else ["......"] * 3 + DOTLESS_I + ["......"]
    rows[1], rows[2] = ACCENTS[accent]
    GLYPHS[ord(char)] = rows


for chars, base in (("ÀÁÂÃÄÅ", "A"), ("ÈÉÊË", "E"), ("ÌÍÎÏ", "I"), ("ÒÓÔÕÖ", "O"), ("ÙÚÛÜ", "U")):
    for char, accent in zip(chars, ("grave", "acute", "circumflex", "tilde" if base in "AO" else "diaeresis",
                                    "diaeresis", "ring")):
        accented_capital(char, base, accent)
accented_capital("Ñ", "N", "tilde")
accented_capital("Ý", "Y", "acute")
glyph("Æ", 1, ".#####", "#..#..", "#..###", "#####.", "#..#..", "#..###")
glyph("Ç", 1, ".###..", "#...#.", "#.....", "#.....", "#...#.", ".###..", "..##..")
glyph("Ð", 1, ".###..", ".#..#.", "###.#.", ".#..#.", ".#..#.", ".###..")
glyph("×", 2, "#...#.", ".#.#..", "..#...", ".#.#..", "#...#.")
glyph("Ø", 1, ".###.#", "#..##.", "#.#.#.", "#.#.#.", "##..#.", "#.###.")
glyph("Þ", 1, "#.....", "####..", "#...#.", "#...#.", "####..", "#.....")
glyph("ß", 1, ".###..", "#...#.", "#.##..", "#...#.", "#...#.", "#.##..")

for chars, base in (("àáâãäå", "a"), ("èéêë", "e"), ("ìíîï", "dotless_i"), ("òóôõö", "o"), ("ùúûü", "u")):
    names = ("grave", "acute", "circumflex", "tilde", "diaeresis", "ring") if base in "ao" else \
            ("grave", "acute", "circumflex", "diaeresis")
    for char, accent in zip(chars, names):
        accented_lower(char, base, accent)
glyph("æ", 3, "##.##.", "..####", "####..", "#.#.##")
glyph("ç", 3, ".####.", "#.....", "#.....", ".####.", "..##..")
glyph("ð", 1, ".#.#..", "..##..", ".####.", "#...#.", "#...#.", ".###..")
accented_lower("ñ", "n", "tilde")
glyph("÷", 2, "..#...", "......", "#####.", "......", "..#...")
glyph("ø", 3, ".####.", "#..##.", "##..#.", ".####.")
accented_lower("ý", "y", "acute")
glyph("þ", 1, "#.....", "#.....", "####..", "#...#.", "####..", "#.....", "#.....")
accented_lower("ÿ", "y", "diaeresis")


# ---------------------------------------------------------------- output
def glyph_order():
    """Code points in glyph order; None for the replacement glyph."""
    return list(range(32, 127)) + [None] + list(range(160, 256))


def rows_of(code):
    return REPLACEMENT if code is None else GLYPHS[code]


def to_bytes(rows):
    """One byte per row; bit 7 = column 0. The 6 defined columns are columns 1-6."""
    out = []
    for row in rows:
        value = 0
        for i, c in enumerate(row):
            if c == "#":
                value |= 0x80 >> (i + 1)
        out.append(value)
    return out


def label(code):
    if code is None:
        return "replacement"
    name = {32: "space", 0xA0: "no-break space", 0xAD: "soft hyphen", 0x5C: "backslash"}.get(code)
    return f"U+{code:04X} {name}" if name else f"U+{code:04X} {chr(code)}"


def write_cpp(order):
    lines = [
        "// Generated by scripts/make_font.py. Do not edit: change the script and run it again.",
        "// The built-in NNHGE font (spec 005): 192 glyphs of 8x8 pixels, one byte per row, bit 7 = left.",
        "// Original to NNHGE, zlib license (see LICENSE).",
        "",
        '#include "text/builtin_font.hpp"',
        "",
        "namespace nnh::detail {",
        "",
        "const std::uint8_t kBuiltinFont[kGlyphCount][8] = {",
    ]
    for index, code in enumerate(order):
        data = ", ".join(f"0x{b:02X}" for b in to_bytes(rows_of(code)))
        lines.append(f"    {{{data}}}, // {index:3d} {label(code)}")
    lines += ["};", "", "} // namespace nnh::detail", ""]
    path = ROOT / "engine/core/src/text/builtin_font.cpp"
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    return path


def write_png(path, width, height, pixels):
    raw = b"".join(b"\x00" + b"".join(bytes(p) for p in row) for row in pixels)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def write_preview(order, samples):
    """All glyphs (x4, 16 per row, on a grid) and sample sentences (x2) as in a game."""
    scale, sample_scale = 4, 2
    grid_w = 16 * 8 * scale
    grid_h = 12 * 8 * scale
    text_h = len(samples) * 10 * sample_scale + 8
    width, height = max(grid_w, 50 * 8 * sample_scale) + 8, grid_h + text_h + 16
    bg, cell_a, cell_b, ink = (24, 24, 32, 255), (44, 44, 58, 255), (36, 36, 48, 255), (240, 240, 240, 255)
    px = [[bg] * width for _ in range(height)]
    for index, code in enumerate(order):
        gx, gy = 4 + (index % 16) * 8 * scale, 4 + (index // 16) * 8 * scale
        shade = cell_a if (index + index // 16) % 2 == 0 else cell_b
        rows = rows_of(code)
        for y in range(8 * scale):
            for x in range(8 * scale):
                on = rows[y // scale][(x // scale) - 1] == "#" if 1 <= x // scale <= 6 else False
                px[gy + y][gx + x] = ink if on else shade
    lookup = {code: i for i, code in enumerate(order) if code is not None}
    for line, text in enumerate(samples):
        y0 = grid_h + 12 + line * 10 * sample_scale
        for col, ch in enumerate(text):
            rows = rows_of(ord(ch) if ord(ch) in lookup else None)
            for y in range(8 * sample_scale):
                for x in range(8 * sample_scale):
                    cx = x // sample_scale
                    if 1 <= cx <= 6 and rows[y // sample_scale][cx - 1] == "#":
                        px[y0 + y][4 + col * 8 * sample_scale + x] = ink
    path = ROOT / "build/font_preview.png"
    path.parent.mkdir(exist_ok=True)
    write_png(path, width, height, px)
    return path


def main():
    order = glyph_order()
    missing = [hex(c) for c in order if c is not None and c not in GLYPHS]
    if missing:
        print("missing glyphs:", " ".join(missing))
        return 1
    print("wrote", write_cpp(order).relative_to(ROOT))
    samples = [
        "The quick brown fox jumps over the lazy dog.",
        "THE QUICK BROWN FOX JUMPS: 0123456789 (+-*/=)",
        "Olá! Coração, ação, pão, avô, você, açúcar.",
        "ÁGUA ÇÃO ÊXITO ÍNDIO ÓCULOS ÚNICO À ÃO",
        "¿Qué? ¡Sí! Ñandú. Ça va? Größe: 25°C ±1.",
        "Score 00420  HP [####  ] x3  50% #1 @nnhge",
    ]
    print("wrote", write_preview(order, samples).relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
