// Converting one font asset (convert_font.hpp).

#include "convert_font.hpp"

#include <cstdio>
#include <set>

#include "bmfont_convert.hpp"
#include "charsets.hpp"
#include "font_desc.hpp"
#include "font_writer.hpp"
#include "glyph_pack.hpp"
#include "ttf_check.hpp"
#include "ttf_raster.hpp"

namespace nnh::assetc {
namespace {

std::string folder_of(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

/// The display names of a string file's [language] table (the "language-names" set).
void add_language_names(const std::string& text, std::set<char32_t>& out) {
    bool in_language = false;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t nl = text.find('\n', at);
        if (nl == std::string::npos) nl = text.size();
        const std::string line = text.substr(at, nl - at);
        at = nl + 1;
        if (!line.empty() && line[0] == '[') in_language = line.rfind("[language]", 0) == 0;
        if (in_language && line.rfind("name", 0) == 0) {
            const std::size_t q = line.find('"'), e = line.rfind('"');
            if (q != std::string::npos && e > q) add_text_characters(line.substr(q + 1, e - q - 1), out);
        }
    }
}

std::string page_label(const BakedFont& f, const std::vector<PageSize>& pages) {
    std::string s = std::to_string(pages.size()) + (pages.size() == 1 ? " page " : " pages ") +
                    std::to_string(pages[0].w) + "x" + std::to_string(pages[0].h);
    return s + (f.colour ? " (RGBA8)" : " (A4)");
}

std::string finish(BakedFont& font, const FontInput& in, FontOutput& out) {
    std::vector<PageSize> pages;
    std::string why = pack_glyphs(font, pages);
    if (!why.empty()) return in.shown + ": " + why;
    why = write_font(font, pages, out.bytes);
    if (!why.empty()) return in.shown + ": " + why;
    std::size_t bytes = 0;
    for (const PageSize& p : pages) bytes += static_cast<std::size_t>(p.w * p.h) * (font.colour ? 4u : 1u);
    out.report = "font " + in.shown + ": " + std::to_string(font.glyphs.size()) + " glyphs, " + page_label(font, pages) +
                 ", " + std::to_string((bytes + out.bytes.size() + 1023) / 1024) + " KB, licence " +
                 (font.licence.empty() ? "own" : font.licence);
    return "";
}

std::string from_description(FileSource& files, const FontInput& in, FontOutput& out) {
    std::string text;
    if (!files.read(in.path, 64 * 1024, text)) return in.path + ": can't be read";
    FontDesc d;
    Diagnostics problems;
    if (!read_font_desc(text, d, problems)) return problems.text(in.path);
    std::set<char32_t> wanted;
    for (const std::string& c : d.chars) {
        const std::string why = add_charset(c, wanted);
        if (!why.empty()) return in.path + ": " + why;
        if (c == "language-names") {
            for (const auto& s : in.strings) {
                std::string st;
                if (files.read(s.second, 1u << 20, st)) add_language_names(st, wanted);
            }
        }
    }
    const std::string dir = folder_of(in.path);
    for (const std::string& f : d.chars_from) {
        std::string chars;
        if (!files.read(dir + f, 1u << 20, chars)) return in.path + ": " + f + " can't be read";
        add_text_characters(chars, wanted);
        out.inputs.push_back(dir + f);
    }
    for (const auto& s : in.strings) {
        bool listed = false;
        for (const std::string& code : d.strings) listed = listed || code == s.first;
        std::string st;
        if (!listed || !files.read(s.second, 1u << 20, st)) continue;
        add_text_characters(st, wanted);
        out.inputs.push_back(s.second);
    }
    if (d.replacement) {
        wanted.insert(0xFFFD);
        wanted.insert(U'?');
    }
    std::string source;
    if (!files.read(dir + d.source, kMaxFontFileBytes + 1, source) || source.size() > kMaxFontFileBytes) {
        return in.path + ":" + std::to_string(d.line) + ": " + d.source + " can't be read (or is larger than 32 MiB)";
    }
    out.inputs.push_back(dir + d.source);
    BakedFont font;
    int skipped = 0;
    char32_t first = 0;
    const std::vector<unsigned char> bytes(source.begin(), source.end());
    const std::string why = raster_font(bytes, {d.face, d.size, d.antialias, d.threshold}, wanted, font, skipped, first);
    if (!why.empty()) return in.path + ": " + d.source + ": " + why;
    if (skipped > 0) {
        char cp[16];
        std::snprintf(cp, sizeof cp, "U+%04X", static_cast<unsigned>(first));
        out.notes.push_back(in.path + ": note: " + std::to_string(skipped) + " characters are not in " + d.source +
                            " (first: " + cp + ")");
    }
    if (d.outline > 0)
        for (BakedGlyph& g : font.glyphs)
            if (g.glyph.w > 0) g.outline = bake_outline(g.glyph, d.outline);
    if (d.licence != "own") {
        std::string licence;
        if (!files.read(in.assets + "/" + d.licence, 1u << 20, licence))
            return in.path + ": the licence " + d.licence + " is not in the assets";
        font.licence = d.licence;
    }
    font.fallbacks = d.fallback;
    font.replacement = d.replacement;
    font.page_side = d.page;
    font.padding = d.padding;
    return finish(font, in, out);
}

} // namespace

std::string convert_font(FileSource& files, const FontInput& in, FontOutput& out) {
    out.inputs.push_back(in.path);
    const bool bmfont = in.path.size() > 4 && in.path.compare(in.path.size() - 4, 4, ".fnt") == 0;
    if (!bmfont) return from_description(files, in, out);
    BakedFont font;
    const std::string why = read_bmfont_glyphs(files, in.path, font, out.inputs);
    if (!why.empty()) return why;
    return finish(font, in, out);
}

} // namespace nnh::assetc
