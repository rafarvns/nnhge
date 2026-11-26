// The TrueType/OpenType pre-validator (ttf_check.hpp). Big-endian reads, 64-bit bounds, no recursion deeper than the
// composite glyph limit.

#include "ttf_check.hpp"

#include <cstdint>
#include <cstring>

namespace nnh::assetc {
namespace {

struct Span {
    const unsigned char* p = nullptr;
    std::uint64_t size = 0;
    bool has(std::uint64_t at, std::uint64_t n) const { return at <= size && n <= size - at; }
    std::uint32_t u8(std::uint64_t at) const { return has(at, 1) ? p[at] : 0; }
    std::uint32_t u16(std::uint64_t at) const { return has(at, 2) ? (std::uint32_t{p[at]} << 8) | p[at + 1] : 0; }
    std::uint32_t u32(std::uint64_t at) const { return (u16(at) << 16) | u16(at + 2); }
    Span sub(std::uint64_t at, std::uint64_t n) const { return has(at, n) ? Span{p + at, n} : Span{}; }
};

struct Tables {
    Span cmap, head, hhea, hmtx, maxp, loca, glyf, kern, gpos, cff;
    bool cff2 = false;
};

std::string directory(const Span& file, std::uint64_t at, Tables& t) {
    const std::uint32_t version = file.u32(at);
    if (version != 0x00010000u && version != 0x74727565u && version != 0x4F54544Fu) return "not a TrueType or OpenType font";
    const std::uint32_t count = file.u16(at + 4);
    if (!file.has(at + 12, std::uint64_t{count} * 16)) return "the table directory is cut short";
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint64_t r = at + 12 + std::uint64_t{i} * 16;
        const std::uint32_t off = file.u32(r + 8), len = file.u32(r + 12);
        if (!file.has(off, len)) return "a table lies outside the file";
        const Span s = file.sub(off, len);
        const char* tag = reinterpret_cast<const char*>(file.p + r);
        if (std::memcmp(tag, "cmap", 4) == 0) t.cmap = s;
        else if (std::memcmp(tag, "head", 4) == 0) t.head = s;
        else if (std::memcmp(tag, "hhea", 4) == 0) t.hhea = s;
        else if (std::memcmp(tag, "hmtx", 4) == 0) t.hmtx = s;
        else if (std::memcmp(tag, "maxp", 4) == 0) t.maxp = s;
        else if (std::memcmp(tag, "loca", 4) == 0) t.loca = s;
        else if (std::memcmp(tag, "glyf", 4) == 0) t.glyf = s;
        else if (std::memcmp(tag, "kern", 4) == 0) t.kern = s;
        else if (std::memcmp(tag, "GPOS", 4) == 0) t.gpos = s;
        else if (std::memcmp(tag, "CFF ", 4) == 0) t.cff = s;
        else if (std::memcmp(tag, "CFF2", 4) == 0) t.cff2 = true;
    }
    return "";
}

std::string simple_glyph(const Span& g, int contours, std::uint32_t& points) {
    const std::uint64_t ends = 10;
    if (!g.has(ends, std::uint64_t(contours) * 2 + 2)) return "a glyph's contours are cut short";
    points = g.u16(ends + std::uint64_t(contours - 1) * 2) + 1;
    const std::uint32_t instr = g.u16(ends + std::uint64_t(contours) * 2);
    std::uint64_t at = ends + std::uint64_t(contours) * 2 + 2 + instr;
    std::uint64_t xbytes = 0, ybytes = 0;
    for (std::uint32_t n = 0; n < points;) {
        if (!g.has(at, 1)) return "a glyph's flags are cut short";
        const std::uint32_t f = g.u8(at++);
        std::uint32_t repeat = 1;
        if ((f & 8) != 0) {
            if (!g.has(at, 1)) return "a glyph's flags are cut short";
            repeat += g.u8(at++);
        }
        xbytes += std::uint64_t(repeat) * ((f & 2) != 0 ? 1 : ((f & 16) != 0 ? 0 : 2));
        ybytes += std::uint64_t(repeat) * ((f & 4) != 0 ? 1 : ((f & 32) != 0 ? 0 : 2));
        n += repeat;
    }
    return g.has(at, xbytes + ybytes) ? "" : "a glyph's coordinates are cut short";
}

std::string composite_glyph(const Span& g, std::uint32_t glyphs) {
    std::uint64_t at = 10;
    for (int parts = 0; parts < 256; ++parts) {
        if (!g.has(at, 4)) return "a composite glyph is cut short";
        const std::uint32_t flags = g.u16(at), index = g.u16(at + 2);
        if (index >= glyphs) return "a composite glyph names a glyph that doesn't exist";
        at += 4 + ((flags & 1) != 0 ? 4 : 2);
        at += (flags & 8) != 0 ? 2 : ((flags & 0x40) != 0 ? 4 : ((flags & 0x80) != 0 ? 8 : 0));
        if (!g.has(0, at)) return "a composite glyph is cut short";
        if ((flags & 0x20) == 0) return "";
    }
    return "a composite glyph has too many parts";
}

std::string glyf(const Tables& t, std::uint32_t glyphs, bool long_loca) {
    const std::uint64_t entry = long_loca ? 4 : 2;
    if (!t.loca.has(0, (std::uint64_t{glyphs} + 1) * entry)) return "loca is shorter than the glyph count";
    std::uint64_t prev = 0;
    for (std::uint32_t i = 0; i <= glyphs; ++i) {
        const std::uint64_t at = long_loca ? t.loca.u32(i * 4) : std::uint64_t{t.loca.u16(i * 2)} * 2;
        if (at < prev || at > t.glyf.size) return "loca points outside glyf";
        if (i > 0 && at > prev) {
            const Span g = t.glyf.sub(prev, at - prev);
            if (g.size < 10) return "a glyph is cut short";
            const auto contours = static_cast<std::int16_t>(g.u16(0));
            std::uint32_t points = 0;
            const std::string why = contours >= 0 ? (contours > 0 ? simple_glyph(g, contours, points) : "")
                                                  : composite_glyph(g, glyphs);
            if (!why.empty()) return why;
        }
        prev = at;
    }
    return "";
}

std::string cmap(const Span& c) {
    const std::uint32_t count = c.u16(2);
    if (!c.has(4, std::uint64_t{count} * 8)) return "cmap's encoding records are cut short";
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t off = c.u32(4 + std::uint64_t{i} * 8 + 4);
        if (!c.has(off, 4)) return "a cmap subtable lies outside cmap";
        const std::uint32_t format = c.u16(off);
        const std::uint64_t length = format >= 8 ? c.u32(off + 4) : c.u16(off + 2);
        if (!c.has(off, length)) return "a cmap subtable is cut short";
        const Span s = c.sub(off, length);
        if (format == 4 && (s.u16(6) % 2 != 0 || !s.has(14, std::uint64_t{s.u16(6)} * 4 + 2))) return "a cmap format 4 is cut short";
        if (format == 12 && !s.has(16, std::uint64_t{s.u32(12)} * 12)) return "a cmap format 12 is cut short";
        if (format == 6 && !s.has(10, std::uint64_t{s.u16(8)} * 2)) return "a cmap format 6 is cut short";
        if (format == 0 && length < 262) return "a cmap format 0 is cut short";
    }
    return "";
}

std::string kern(const Span& k) {
    if (k.size == 0) return "";
    if (k.u16(0) != 0 || k.u16(2) == 0) return ""; // stb reads only version 0 with a first format 0 subtable
    if (!k.has(4, 14)) return "kern is cut short";
    const std::uint32_t pairs = k.u16(10);
    return k.has(18, std::uint64_t{pairs} * 6) ? "" : "kern's pairs are cut short";
}

std::string gpos(const Span& g) {
    if (g.size == 0) return "";
    if (!g.has(0, 10)) return "GPOS is cut short";
    const std::uint32_t lookups = g.u16(8);
    if (!g.has(lookups, 2) || !g.has(lookups + 2, std::uint64_t{g.u16(lookups)} * 2)) return "GPOS's lookup list is cut short";
    for (std::uint32_t i = 0; i < g.u16(lookups); ++i) {
        const std::uint64_t l = lookups + g.u16(lookups + 2 + i * 2);
        if (!g.has(l, 6) || !g.has(l + 6, std::uint64_t{g.u16(l + 4)} * 2)) return "a GPOS lookup is cut short";
        for (std::uint32_t k = 0; k < g.u16(l + 4); ++k) {
            const std::uint64_t st = l + g.u16(l + 6 + k * 2);
            if (!g.has(st, 10)) return "a GPOS subtable is cut short";
            const std::uint64_t cov = st + g.u16(st + 2);
            if (!g.has(cov, 4)) return "a GPOS coverage lies outside GPOS";
            const std::uint32_t cf = g.u16(cov), cn = g.u16(cov + 2);
            if (!g.has(cov + 4, std::uint64_t{cn} * (cf == 1 ? 2 : 6))) return "a GPOS coverage is cut short";
            if (g.u16(l) != 2) continue;
            if (g.u16(st) == 1 && !g.has(st + 10, std::uint64_t{g.u16(st + 8)} * 2)) return "a GPOS pair set list is cut short";
            if (g.u16(st) == 1) {
                for (std::uint32_t s = 0; s < g.u16(st + 8); ++s) {
                    const std::uint64_t set = st + g.u16(st + 10 + s * 2);
                    if (!g.has(set, 2) || !g.has(set + 2, std::uint64_t{g.u16(set)} * 4)) return "a GPOS pair set is cut short";
                }
            } else if (g.u16(st) == 2) {
                const std::uint64_t c1 = g.u16(st + 12), c2 = g.u16(st + 14);
                if (!g.has(st + 16, c1 * c2 * 2) || !g.has(st + g.u16(st + 8), 4) || !g.has(st + g.u16(st + 10), 4))
                    return "a GPOS class pair table is cut short";
            }
        }
    }
    return "";
}

/// A CFF INDEX at `at`: its end (or 0 when it is cut short).
std::uint64_t cff_index(const Span& c, std::uint64_t at) {
    if (!c.has(at, 2)) return 0;
    const std::uint32_t count = c.u16(at);
    if (count == 0) return at + 2;
    const std::uint32_t size = c.u8(at + 2);
    if (size < 1 || size > 4 || !c.has(at + 3, (std::uint64_t{count} + 1) * size)) return 0;
    std::uint64_t last = 0;
    for (std::uint32_t k = 0; k < size; ++k) last = (last << 8) | c.u8(at + 3 + std::uint64_t{count} * size + k);
    const std::uint64_t data = at + 3 + (std::uint64_t{count} + 1) * size - 1;
    return last >= 1 && c.has(data, last) ? data + last : 0;
}

std::string cff(const Span& c) {
    const std::uint64_t names = c.u8(2);
    const std::uint64_t top = cff_index(c, names);
    const std::uint64_t strings = top != 0 ? cff_index(c, top) : 0;
    const std::uint64_t globals = strings != 0 ? cff_index(c, strings) : 0;
    if (top == 0 || strings == 0 || globals == 0 || cff_index(c, globals) == 0) return "a CFF INDEX is cut short";
    return ""; // charstrings, private DICTs and subroutines: stb_truetype reads them through bounded buffers
}

} // namespace

std::string check_ttf(const unsigned char* data, std::size_t size, int face) {
    const Span file{data, size};
    if (data == nullptr || size < 12 || size > kMaxFontFileBytes) return "not a font file (or larger than 32 MiB)";
    std::uint64_t at = 0;
    if (file.u32(0) == 0x74746366u) { // 'ttcf'
        if (face < 0 || static_cast<std::uint32_t>(face) >= file.u32(8) || !file.has(12, std::uint64_t(face + 1) * 4))
            return "the collection has no face " + std::to_string(face);
        at = file.u32(12 + std::uint64_t(face) * 4);
    } else if (face != 0) { // a single font has face 0 only (stb_truetype would answer -1 for the offset)
        return "not a collection: face is 0";
    }
    Tables t;
    std::string why = directory(file, at, t);
    if (!why.empty()) return why;
    if (t.cff2 && t.cff.size == 0) return "CFF2 (variable CFF) fonts aren't read: use a static instance";
    if (t.cmap.size == 0 || t.head.size < 54 || t.hhea.size < 36 || t.maxp.size < 6 || t.hmtx.size == 0)
        return "a required table (cmap, head, hhea, hmtx, maxp) is missing or short";
    const std::uint32_t glyphs = t.maxp.u16(4), metrics = t.hhea.u16(34);
    const std::uint32_t em = t.head.u16(18), loca_format = t.head.u16(50);
    if (glyphs == 0 || metrics == 0 || metrics > glyphs || em < 16 || em > 16384 || loca_format > 1)
        return "head, hhea or maxp hold impossible values";
    if (!t.hmtx.has(0, std::uint64_t{metrics} * 4 + std::uint64_t{glyphs - metrics} * 2)) return "hmtx is cut short";
    if (t.cff.size == 0) {
        if (t.glyf.size == 0 || t.loca.size == 0) return "neither glyf/loca nor CFF outlines";
        if (!(why = glyf(t, glyphs, loca_format == 1)).empty()) return why;
    } else if (!(why = cff(t.cff)).empty()) {
        return why;
    }
    if (!(why = cmap(t.cmap)).empty()) return why;
    if (!(why = kern(t.kern)).empty()) return why;
    return gpos(t.gpos);
}

} // namespace nnh::assetc
