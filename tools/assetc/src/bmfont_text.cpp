// Reading AngelCode BMFont files (bmfont_text.hpp).

#include "bmfont_text.hpp"

#include <charconv>
#include <map>

namespace nnh::assetc {
namespace {

using Attributes = std::map<std::string, std::string>;

/// The element name and its attributes of one line (text form: `char id=1 ...`; XML: `<char id="1" .../>`).
bool tokenize(std::string_view line, std::string& name, Attributes& attrs) {
    std::size_t i = 0;
    const auto skip = [&] {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '<' || line[i] == '/' ||
                                   line[i] == '>'))
            ++i;
    };
    skip();
    const std::size_t start = i;
    while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '>' && line[i] != '/' && line[i] != '\r') ++i;
    name = std::string(line.substr(start, i - start));
    attrs.clear();
    for (;;) {
        skip();
        if (i >= line.size()) return true;
        const std::size_t k = i;
        while (i < line.size() && line[i] != '=' && line[i] != ' ' && line[i] != '>') ++i;
        if (i >= line.size() || line[i] != '=') return !name.empty() && name[0] == '?'; // "<?xml ...?>" lines
        const std::string key(line.substr(k, i - k));
        ++i;
        std::string value;
        if (i < line.size() && line[i] == '"') {
            const std::size_t end = line.find('"', i + 1);
            if (end == std::string_view::npos) return false;
            value = std::string(line.substr(i + 1, end - i - 1));
            i = end + 1;
        } else {
            const std::size_t v = i;
            while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '/' && line[i] != '>') ++i;
            value = std::string(line.substr(v, i - v));
        }
        attrs[key] = value;
    }
}

struct Reader {
    Diagnostics& p;
    int line = 0;
    bool ok = true;
    int number(const Attributes& a, const char* key, int lo, int hi, int fallback = 0) {
        const auto it = a.find(key);
        if (it == a.end()) return fallback;
        int v = 0;
        const char* first = it->second.data();
        const auto r = std::from_chars(first, first + it->second.size(), v);
        if (r.ec != std::errc{} || r.ptr != first + it->second.size() || v < lo || v > hi) {
            p.add(line, std::string(key) + " is " + std::to_string(lo) + " to " + std::to_string(hi));
            ok = false;
            return fallback;
        }
        return v;
    }
};

} // namespace

bool read_bmfont(std::string_view text, BmFont& out, Diagnostics& problems) {
    if (text.size() >= 3 && text.substr(0, 3) == "BMF") {
        problems.add(1, "binary BMFont files aren't read: export the text or XML form");
        return false;
    }
    if (text.size() > kMaxBmFontBytes) {
        problems.add(1, "larger than 4 MiB");
        return false;
    }
    Reader r{problems};
    std::map<int, int> ids; // id → line
    std::size_t at = 0;
    for (r.line = 1; at < text.size(); ++r.line) {
        std::size_t nl = text.find('\n', at);
        if (nl == std::string_view::npos) nl = text.size();
        const std::string_view line = text.substr(at, nl - at);
        at = nl + 1;
        if (line.size() > kMaxBmFontLine) {
            problems.add(r.line, "a line longer than 4096 bytes");
            return false;
        }
        std::string name;
        Attributes a;
        if (!tokenize(line, name, a)) {
            problems.add(r.line, "a quoted value isn't closed");
            r.ok = false;
            continue;
        }
        if (name == "info") {
            out.size = std::abs(r.number(a, "size", -1024, 1024));
        } else if (name == "common") {
            out.line_height = r.number(a, "lineHeight", 1, 255);
            out.base = r.number(a, "base", 0, 255);
            out.pages = r.number(a, "pages", 1, 8);
            out.packed = r.number(a, "packed", 0, 1) == 1;
            if (out.packed) problems.add(r.line, "packed BMFont channels aren't read: export one glyph per texel"), r.ok = false;
        } else if (name == "page") {
            const int id = r.number(a, "id", 0, 7);
            if (out.page_files.size() <= static_cast<std::size_t>(id)) out.page_files.resize(static_cast<std::size_t>(id) + 1);
            out.page_files[static_cast<std::size_t>(id)] = a.count("file") != 0 ? a["file"] : "";
        } else if (name == "char") {
            BmChar c;
            c.line = r.line;
            c.id = r.number(a, "id", 0, 0x10FFFF);
            c.x = r.number(a, "x", 0, 4096);
            c.y = r.number(a, "y", 0, 4096);
            c.w = r.number(a, "width", 0, 255);
            c.h = r.number(a, "height", 0, 255);
            c.xoffset = r.number(a, "xoffset", -128, 127);
            c.yoffset = r.number(a, "yoffset", -128, 127);
            c.xadvance = r.number(a, "xadvance", 0, 255);
            c.page = r.number(a, "page", 0, 7);
            if (ids.count(c.id) != 0) {
                problems.add(r.line, "character " + std::to_string(c.id) + " is defined twice (first at line " +
                                         std::to_string(ids[c.id]) + ")");
                r.ok = false;
            }
            ids[c.id] = r.line;
            out.chars.push_back(c);
        } else if (name == "kerning") {
            out.kernings.push_back({r.number(a, "first", 0, 0x10FFFF), r.number(a, "second", 0, 0x10FFFF),
                                    r.number(a, "amount", -128, 127)});
        }
    }
    if (out.line_height == 0) problems.add(1, "no \"common\" line (lineHeight, base, pages)"), r.ok = false;
    return r.ok;
}

} // namespace nnh::assetc
