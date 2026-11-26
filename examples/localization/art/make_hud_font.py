"""Writes the example's HUD font (spec 029 T7.1): an original 8 px pixel BMFont, `assets/fonts/hud.fnt` (text form) and
`hud_0.png` (white texels, coverage in alpha; and the bench's sprite `assets/art/spark.png`) — digits, the group separators the example's languages use (",", ".",
no-break space), "x", ":", "+", "-", "/", the brackets and tilde the pseudo-language adds, and the space. Drawn here by hand (zlib licence, like the engine).

Usage: python -I make_hud_font.py <examples/localization/assets/fonts>   (the outputs are committed)
"""
import os
import struct
import sys
import zlib

# 5 × 7 glyphs, '#' = on
GLYPHS = {
    "0": [" ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### "],
    "1": ["  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "],
    "2": [" ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####"],
    "3": ["#####", "   # ", "  #  ", "   # ", "    #", "#   #", " ### "],
    "4": ["   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # "],
    "5": ["#####", "#    ", "#### ", "    #", "    #", "#   #", " ### "],
    "6": ["  ## ", " #   ", "#    ", "#### ", "#   #", "#   #", " ### "],
    "7": ["#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   "],
    "8": [" ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "],
    "9": [" ### ", "#   #", "#   #", " ####", "    #", "   # ", " ##  "],
    "x": ["     ", "     ", "#   #", " # # ", "  #  ", " # # ", "#   #"],
    ",": ["     ", "     ", "     ", "     ", " ##  ", " ##  ", "  #  "],
    ".": ["     ", "     ", "     ", "     ", "     ", " ##  ", " ##  "],
    ":": ["     ", " ##  ", " ##  ", "     ", " ##  ", " ##  ", "     "],
    "+": ["     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     "],
    "-": ["     ", "     ", "     ", "#####", "     ", "     ", "     "],
    "[": [" ### ", " #   ", " #   ", " #   ", " #   ", " #   ", " ### "],
    "]": [" ### ", "   # ", "   # ", "   # ", "   # ", "   # ", " ### "],
    "~": ["     ", "     ", " #   ", "# # #", "   # ", "     ", "     "],
    "/": ["    #", "    #", "   # ", "  #  ", " #   ", "#    ", "#    "],
}
NARROW = {",": 4, ".": 4, ":": 4}  # advances; the rest 6
SPACES = {0x20: 4, 0xA0: 3}       # space and no-break space: no texels


def png(path, width, height, rgba):
    raw = b"".join(b"\x00" + bytes(rgba[y * width * 4:(y + 1) * width * 4]) for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    body = chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    body += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + body)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    w, h = 64, 32
    rgba = [0] * (w * h * 4)
    chars = []
    for i, (c, rows) in enumerate(sorted(GLYPHS.items())):
        gx, gy = (i % 10) * 6, (i // 10) * 9
        for y, row in enumerate(rows):
            for x, bit in enumerate(row):
                if bit == "#":
                    p = ((gy + y) * w + gx + x) * 4
                    rgba[p:p + 4] = [255, 255, 255, 255]
        chars.append((ord(c), gx, gy, 5, 7, 0, 1, NARROW.get(c, 6)))
    for cp, adv in sorted(SPACES.items()):
        chars.append((cp, 0, 0, 0, 0, 0, 0, adv))
    chars.sort()
    png(os.path.join(out, "hud_0.png"), w, h, rgba)
    lines = ['info face="NNHGE Example HUD" size=8 bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=0 aa=0 '
             'padding=0,0,0,0 spacing=1,1 outline=0',
             "common lineHeight=9 base=8 scaleW=%d scaleH=%d pages=1 packed=0 alphaChnl=1 redChnl=0 greenChnl=0 "
             "blueChnl=0" % (w, h),
             'page id=0 file="hud_0.png"', "chars count=%d" % len(chars)]
    for cp, x, y, cw, ch, xo, yo, adv in chars:
        lines.append("char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d page=0 chnl=15"
                     % (cp, x, y, cw, ch, xo, yo, adv))
    open(os.path.join(out, "hud.fnt"), "w", encoding="utf-8", newline="\n").write("\n".join(lines) + "\n")
    # the bench's sprite (assets/art/spark.png): a 16 x 16 diamond
    spark = []
    for y in range(16):
        for x in range(16):
            d = abs(x - 7.5) + abs(y - 7.5)
            spark += [255, 200 + int(d * 3), 80, 255] if d < 7 else [0, 0, 0, 0]
    art = os.path.join(out, "..", "art")
    os.makedirs(art, exist_ok=True)
    png(os.path.join(art, "spark.png"), 16, 16, spark)
    print("hud.fnt: %d characters; art/spark.png" % len(chars))


if __name__ == "__main__":
    main()
