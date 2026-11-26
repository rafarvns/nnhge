// A BMFont's glyphs (bmfont_convert.hpp).

#include "bmfont_convert.hpp"

#include "bmfont_text.hpp"
#include "png_pages.hpp"

namespace nnh::assetc {
namespace {

struct Page {
    int w = 0, h = 0;
    std::vector<std::uint8_t> rgba;
};

std::string stem_of(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    return name.substr(0, name.size() - 4); // ".fnt"
}

bool mono_texel(const std::uint8_t* p) {
    const bool white = p[0] == 255 && p[1] == 255 && p[2] == 255;
    const bool grey = p[0] == p[1] && p[1] == p[2] && p[3] == 255;
    return white || grey || p[3] == 0;
}

std::uint8_t level_of(const std::uint8_t* p) {
    const int a = p[0] == 255 && p[1] == 255 && p[2] == 255 ? p[3] : (p[3] == 255 ? p[0] : 0);
    return static_cast<std::uint8_t>((a * 15 + 127) / 255);
}

} // namespace

std::string read_bmfont_glyphs(FileSource& files, const std::string& path, BakedFont& out,
                               std::vector<std::string>& inputs) {
    std::string text;
    if (!files.read(path, kMaxBmFontBytes + 1, text)) return path + ": can't be read";
    BmFont bm;
    Diagnostics problems;
    if (!read_bmfont(text, bm, problems)) return problems.text(path);
    const std::size_t slash = path.find_last_of("/\\");
    const std::string dir = slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
    const std::string stem = stem_of(path);
    std::vector<Page> pages(bm.page_files.size());
    bool colour = false;
    for (std::size_t i = 0; i < bm.page_files.size(); ++i) {
        const std::string& f = bm.page_files[i];
        if (f.rfind(stem + "_", 0) != 0 || f.size() < 5 || f.compare(f.size() - 4, 4, ".png") != 0)
            return path + ": page " + std::to_string(i) + " is \"" + f + "\": name pages " + stem + "_<n>.png beside the .fnt";
        std::string png;
        if (!files.read(dir + f, 64u << 20, png)) return path + ": page " + f + " is missing";
        inputs.push_back(dir + f);
        const std::string why = decode_png(png, pages[i].w, pages[i].h, pages[i].rgba);
        if (!why.empty()) return dir + f + ": " + why;
        for (std::size_t p = 0; p + 3 < pages[i].rgba.size() && !colour; p += 4) colour = !mono_texel(&pages[i].rgba[p]);
    }
    out.colour = colour;
    out.size = bm.size > 0 ? bm.size : bm.line_height;
    out.line_height = bm.line_height;
    out.ascent = bm.base;
    out.descent = bm.base - bm.line_height;
    for (const BmChar& c : bm.chars) {
        if (c.page >= static_cast<int>(pages.size())) return path + ":" + std::to_string(c.line) + ": page " + std::to_string(c.page) + " doesn't exist";
        const Page& p = pages[static_cast<std::size_t>(c.page)];
        if (c.x + c.w > p.w || c.y + c.h > p.h) return path + ":" + std::to_string(c.line) + ": character " + std::to_string(c.id) + "'s rectangle is outside its page";
        BakedGlyph g;
        g.cp = static_cast<char32_t>(c.id);
        g.advance = c.xadvance;
        if (c.w > 0 && c.h > 0 && c.id != ' ') {
            g.glyph = {c.w, c.h, c.xoffset, c.yoffset, {}};
            for (int y = 0; y < c.h; ++y) {
                for (int x = 0; x < c.w; ++x) {
                    const std::uint8_t* t = &p.rgba[static_cast<std::size_t>(((c.y + y) * p.w + c.x + x) * 4)];
                    if (colour) g.glyph.pixels.insert(g.glyph.pixels.end(), t, t + 4);
                    else g.glyph.pixels.push_back(level_of(t));
                }
            }
        }
        out.glyphs.push_back(std::move(g));
    }
    for (const BmKerning& k : bm.kernings) out.kerning.push_back({static_cast<char32_t>(k.first), static_cast<char32_t>(k.second), k.amount});
    return "";
}

} // namespace nnh::assetc
