// Font descriptions (font_desc.hpp).

#include "font_desc.hpp"

#include "suggest.hpp"
#include "toml.hpp"

namespace nnh::assetc {
namespace {

const std::vector<std::string> kKeys = {"source", "face", "size", "chars", "chars_from", "strings", "antialias",
                                        "threshold", "padding", "outline", "fallback", "replacement", "licence", "page"};

struct Reader {
    Diagnostics& p;
    bool ok = true;
    void fail(int line, std::string what) {
        p.add(line, std::move(what));
        ok = false;
    }
    bool integer(const toml::Entry& e, int lo, int hi, int& out, const char* what) {
        if (e.value.type != toml::Type::Integer) {
            fail(e.line, std::string(e.key) + " is a whole number");
            return false;
        }
        if (e.value.integer < lo || e.value.integer > hi) {
            fail(e.line, what);
            return false;
        }
        out = static_cast<int>(e.value.integer);
        return true;
    }
    void text(const toml::Entry& e, std::string& out) {
        if (e.value.type != toml::Type::String) fail(e.line, e.key + " is a string");
        else out = e.value.text;
    }
    void list(const toml::Entry& e, std::vector<std::string>& out) {
        if (e.value.type != toml::Type::Array) {
            fail(e.line, e.key + " is an array of strings");
            return;
        }
        out.clear();
        for (const toml::Value& v : e.value.items) {
            if (v.type != toml::Type::String) fail(e.line, e.key + " is an array of strings");
            else out.push_back(v.text);
        }
    }
};

bool power_side(int v) { return v == 0 || (v >= 64 && v <= 1024 && (v & (v - 1)) == 0); }

} // namespace

bool read_font_desc(std::string_view text, FontDesc& out, Diagnostics& problems) {
    toml::Document doc;
    if (!doc.parse(text)) {
        problems.add(doc.problem().line, doc.problem().message);
        return false;
    }
    Reader r{problems};
    bool has_source = false, has_size = false, has_licence = false;
    for (const toml::Entry& e : doc.root().entries) {
        if (e.kind != toml::EntryKind::Value) {
            r.fail(e.line, "a font description has no tables");
            continue;
        }
        if (e.key == "source") {
            r.text(e, out.source);
            has_source = true;
            out.line = e.line;
        } else if (e.key == "size") {
            r.integer(e, 6, 128, out.size, "size is 6 to 128");
            has_size = true;
        } else if (e.key == "face") r.integer(e, 0, 255, out.face, "face is 0 to 255");
        else if (e.key == "chars") r.list(e, out.chars);
        else if (e.key == "chars_from") r.list(e, out.chars_from);
        else if (e.key == "strings") r.list(e, out.strings);
        else if (e.key == "antialias") {
            if (e.value.type == toml::Type::Bool) out.antialias = e.value.boolean;
            else r.fail(e.line, "antialias is true or false");
        } else if (e.key == "threshold") r.integer(e, 1, 255, out.threshold, "threshold is 1 to 255");
        else if (e.key == "padding") r.integer(e, 1, 4, out.padding, "padding is 1 to 4");
        else if (e.key == "outline") r.integer(e, 0, 3, out.outline, "outline is 0 to 3");
        else if (e.key == "fallback") {
            r.list(e, out.fallback);
            if (out.fallback.size() > 3) r.fail(e.line, "at most 3 fallback fonts");
        } else if (e.key == "replacement") {
            std::string v;
            r.text(e, v);
            if (v != "auto" && v != "none") r.fail(e.line, "replacement is \"auto\" or \"none\"");
            out.replacement = v != "none";
        } else if (e.key == "licence") {
            r.text(e, out.licence);
            has_licence = true;
        } else if (e.key == "page") {
            if (r.integer(e, 0, 1024, out.page, "page is 0 or a power of two from 64 to 1024") && !power_side(out.page))
                r.fail(e.line, "page is 0 or a power of two from 64 to 1024");
        } else {
            const std::string near = closest(e.key, kKeys);
            r.fail(e.line, "unknown key \"" + e.key + "\"" + (near.empty() ? "" : " (did you mean \"" + near + "\"?)"));
        }
    }
    if (!has_source) r.fail(1, "\"source\" is required (the font file beside the description)");
    if (!has_size) r.fail(1, "\"size\" is required (pixels, 6 to 128)");
    if (!has_licence) r.fail(1, "\"licence\" is required (a file in the assets or \"own\")");
    return r.ok;
}

} // namespace nnh::assetc
