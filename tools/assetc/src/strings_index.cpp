// What spans languages (strings_index.hpp).

#include "strings_index.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

#include "core/crc32.hpp"
#include "font/font_format.hpp"
#include "loc/loc_file.hpp"
#include "nnh/detail/name_hash.hpp"
#include "suggest.hpp"
#include "text/break_class.hpp"
#include "text/utf8.hpp"

namespace nnh::assetc {
namespace {

namespace lf = nnh::detail::loc;
namespace ff = nnh::detail::font;

std::string where(const Where& w) { return w.file + ":" + std::to_string(w.line) + ": "; }

int index_of(const IndexInput& in, const std::string& code) {
    for (std::size_t i = 0; i < in.languages.size(); ++i)
        if (in.languages[i].code == code) return static_cast<int>(i);
    return -1;
}

/// A converted font's code points (and its fallbacks'): false when it isn't a valid font.
bool font_cps(FileSource& files, const std::string& dir, const std::string& path, std::set<char32_t>& out, int depth) {
    std::string bytes;
    if (!files.read(dir + "/" + path + ".nnhfont", 64u << 20, bytes)) return false;
    const auto* d = reinterpret_cast<const std::uint8_t*>(bytes.data());
    if (ff::font_validate(d, bytes.size()) != nullptr) return false;
    const ff::Header h = ff::read_header(d);
    for (std::uint32_t i = 0; i < h.glyph_count; ++i) {
        std::uint32_t cp;
        std::memcpy(&cp, d + h.off_codepoints + i * 4, 4);
        out.insert(cp);
    }
    for (int i = 0; i < h.fallback_count && depth == 0; ++i) {
        font_cps(files, dir, reinterpret_cast<const char*>(d + h.off_fallbacks + static_cast<std::size_t>(i) * ff::kPathBytes), out, 1);
    }
    return true;
}

void chains(const IndexInput& in, std::vector<std::vector<int>>& out, CheckResult& r) {
    out.assign(in.languages.size(), {});
    for (std::size_t i = 0; i < in.languages.size(); ++i) {
        std::vector<int>& c = out[i];
        int at = static_cast<int>(i);
        while (at >= 0 && at != in.base && c.size() < 4) {
            if (std::find(c.begin(), c.end(), at) != c.end()) {
                r.errors.push_back(where(in.languages[i].meta.fallback_at) + "fallback chain of " + in.languages[i].code +
                                   " loops");
                break;
            }
            c.push_back(at);
            const StringsLanguage& l = in.languages[static_cast<std::size_t>(at)];
            if (l.meta.fallback.empty()) break;
            const int next = index_of(in, l.meta.fallback);
            if (next < 0) {
                r.errors.push_back(where(l.meta.fallback_at) + "fallback \"" + l.meta.fallback + "\" is not a shipped language");
                break;
            }
            at = next;
        }
        if (c.size() >= 4) c.resize(3);
        c.push_back(in.base);
    }
}

} // namespace

std::string pseudo_text(const std::string& text) {
    static const char* const kMap[][2] = {{"a", "\xC3\xA5"}, {"e", "\xC3\xA9"}, {"i", "\xC3\xAE"}, {"o", "\xC3\xB6"},
                                          {"u", "\xC3\xBC"}, {"c", "\xC3\xA7"}, {"n", "\xC3\xB1"}, {"y", "\xC3\xBD"},
                                          {"A", "\xC3\x85"}, {"E", "\xC3\x89"}, {"I", "\xC3\x8E"}, {"O", "\xC3\x96"},
                                          {"U", "\xC3\x9C"}, {"C", "\xC3\x87"}, {"N", "\xC3\x91"}, {"Y", "\xC3\x9D"}};
    std::string out = "[";
    int chars = 0, depth = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '{' && i + 1 < text.size() && text[i + 1] == '{') { // a brace: kept
            out += "{{";
            ++i;
            ++chars;
            continue;
        }
        if (c == '{') ++depth;
        if (c == '}' && depth > 0) {
            --depth;
            out += c;
            continue;
        }
        const char* mapped = nullptr;
        for (const auto& m : kMap)
            if (depth == 0 && c == m[0][0]) mapped = m[1];
        out += mapped != nullptr ? std::string(mapped) : std::string(1, c);
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80 && depth == 0) ++chars;
    }
    out.append(static_cast<std::size_t>((chars * 3 + 9) / 10), '~');
    return out + "]";
}

StringsLanguage make_pseudo(const StringsLanguage& base) {
    StringsLanguage q = base;
    q.code = "qps";
    q.meta.name = "[Ps\xC3\xA9\xC3\xBC\xC3\xB0\xC3\xB6]";
    q.meta.fallback.clear();
    for (auto& [name, k] : q.keys) {
        if (k.plural()) {
            for (std::string& f : k.forms)
                if (!f.empty()) f = pseudo_text(f);
        } else {
            k.text = pseudo_text(k.text);
        }
    }
    return q;
}

void build_index(FileSource& files, const IndexInput& in, std::vector<std::uint8_t>& out, CheckResult& r) {
    const std::size_t n = in.languages.size();
    // hash collisions across every key of every language
    std::map<std::uint32_t, std::pair<std::string, Where>> hashes;
    for (const StringsLanguage& l : in.languages) {
        for (const auto& [name, k] : l.keys) {
            const std::uint32_t h = nnh::detail::fnv1a32(name.c_str());
            const auto it = hashes.find(h);
            if (it != hashes.end() && it->second.first != name) {
                char hex[16];
                std::snprintf(hex, sizeof hex, "0x%08X", static_cast<unsigned>(h));
                r.errors.push_back(where(k.where) + "keys \"" + name + "\" and \"" + it->second.first + "\" (" +
                                   it->second.second.file + ":" + std::to_string(it->second.second.line) +
                                   ") have the same hash " + hex + ": rename one");
            }
            hashes.emplace(h, std::make_pair(name, k.where));
        }
    }
    std::vector<std::vector<int>> chain;
    chains(in, chain, r);
    // roles: names across languages; each language's font per role (its own, else its chain's)
    std::vector<std::string> roles;
    for (const StringsLanguage& l : in.languages)
        for (const auto& [role, path] : l.meta.fonts)
            if (std::find(roles.begin(), roles.end(), role) == roles.end()) roles.push_back(role);
    if (roles.size() > lf::kMaxRoles) r.errors.push_back("at most 8 font roles");
    std::string blob(1, '\0'); // offset 0 = none
    const auto add_blob = [&](const std::string& s) {
        const auto at = static_cast<std::uint32_t>(blob.size());
        blob += s;
        blob += '\0';
        return at;
    };
    std::vector<std::uint32_t> role_fonts(n * roles.size(), 0);
    for (std::size_t i = 0; i < n; ++i) {
        const StringsLanguage& l = in.languages[i];
        for (const auto& [role, path] : l.meta.fonts) {
            std::set<char32_t> probe;
            if (!in.fonts_dir.empty() && !font_cps(files, in.fonts_dir, path, probe, 0)) {
                r.errors.push_back(where(l.meta.fonts_at) + "font role " + role + " names " + path + ", which is not a font");
            }
            const std::size_t k = static_cast<std::size_t>(std::find(roles.begin(), roles.end(), role) - roles.begin());
            role_fonts[i * roles.size() + k] = add_blob(path);
        }
    }
    // coverage: each language's strings by its first role's font (own or its chain's) and that font's fallbacks
    for (std::size_t i = 0; i < n && !in.fonts_dir.empty() && !roles.empty(); ++i) {
        const StringsLanguage& l = in.languages[i];
        std::string font;
        std::string role = roles[0];
        for (const int c : chain[i]) {
            for (const auto& [rn, path] : in.languages[static_cast<std::size_t>(c)].meta.fonts)
                if (font.empty() && rn == role) font = path;
        }
        std::set<char32_t> cps;
        if (font.empty() || !font_cps(files, in.fonts_dir, font, cps, 0)) continue;
        for (const auto& [name, k] : l.keys) {
            const std::string* texts = k.plural() ? k.forms : &k.text;
            bool reported = false;
            for (int t = 0; t < (k.plural() ? 6 : 1) && !reported; ++t) {
                for (const char* p = texts[t].c_str(); *p != 0 && !reported;) {
                    const char32_t c = nnh::detail::next_codepoint(p);
                    if (c < 0x20 || c == U' ' || c == 0xA0 || c == 0x3000 ||
                        (nnh::detail::break_class(c) & nnh::detail::kBreakZeroWidth) != 0 || cps.count(c) != 0)
                        continue;
                    char cp[24];
                    std::snprintf(cp, sizeof cp, "U+%04X", static_cast<unsigned>(c));
                    const char* start = p;
                    while (start > texts[t].c_str() && (static_cast<unsigned char>(*(start - 1)) & 0xC0) == 0x80) --start;
                    r.errors.push_back(where(k.where) + "\"" + name + "\" uses " + cp + " '" + std::string(start, p) + "' not in font " + font + " (role " +
                                       role + ") or its fallbacks");
                    reported = true;
                }
            }
        }
    }
    // the manifest
    std::vector<lf::LanguageRecord> records(n);
    std::vector<std::uint8_t> rules;
    std::uint32_t block = 0;
    std::vector<std::uint32_t> table_bytes(n);
    for (std::size_t i = 0; i < n; ++i) table_bytes[i] = static_cast<std::uint32_t>(write_table(in.languages[i], in.release).size());
    for (std::size_t i = 0; i < n; ++i) {
        const StringsLanguage& l = in.languages[i];
        lf::LanguageRecord& rec = records[i];
        std::strncpy(rec.code, l.code.c_str(), sizeof rec.code - 1);
        rec.name = add_blob(l.meta.name.empty() ? l.code : l.meta.name);
        std::uint32_t sum = 0;
        for (std::size_t c = 0; c < chain[i].size() && c < lf::kMaxChain; ++c) {
            rec.chain[c] = static_cast<std::uint8_t>(chain[i][c]);
            sum += table_bytes[static_cast<std::size_t>(chain[i][c])];
        }
        block = std::max(block, sum);
        std::vector<std::uint8_t> rule;
        const std::string why = language_rule(l.code == "qps" ? in.languages[static_cast<std::size_t>(in.base)] : l, rule);
        if (!why.empty()) r.errors.push_back((l.files.empty() ? l.code : l.files[0]) + ": plural rule: " + why);
        rec.rule = static_cast<std::uint32_t>(rules.size());
        rec.rule_bytes = static_cast<std::uint16_t>(rule.size());
        rules.insert(rules.end(), rule.begin(), rule.end());
        rec.grouping_min = static_cast<std::uint8_t>(l.meta.grouping_min);
        std::strncpy(rec.grouping, l.meta.grouping.c_str(), sizeof rec.grouping - 1);
        rec.table_bytes = table_bytes[i];
    }
    lf::ManifestHeader h;
    h.flags = in.release ? 0 : lf::kFlagNames;
    h.language_count = static_cast<std::uint8_t>(n);
    h.base = static_cast<std::uint8_t>(in.base);
    h.pseudo = static_cast<std::uint8_t>(index_of(in, "qps") >= 0 ? index_of(in, "qps") : lf::kNoIndex);
    h.role_count = static_cast<std::uint8_t>(roles.size());
    h.block_bytes = block;
    std::vector<std::uint32_t> role_hashes;
    for (const std::string& role : roles) role_hashes.push_back(nnh::detail::fnv1a32(role.c_str()));
    std::size_t at = sizeof h;
    h.off_languages = static_cast<std::uint32_t>(at);
    at += records.size() * sizeof(lf::LanguageRecord);
    h.off_roles = static_cast<std::uint32_t>(at);
    at += role_hashes.size() * 4;
    h.off_role_fonts = static_cast<std::uint32_t>(at);
    at += role_fonts.size() * 4;
    h.off_rules = static_cast<std::uint32_t>(at);
    at += rules.size();
    h.off_blob = static_cast<std::uint32_t>(at);
    h.blob_bytes = static_cast<std::uint32_t>(blob.size());
    at += blob.size();
    h.size = static_cast<std::uint32_t>(at);
    out.assign(at, 0);
    std::memcpy(out.data() + h.off_languages, records.data(), records.size() * sizeof(lf::LanguageRecord));
    if (!role_hashes.empty()) std::memcpy(out.data() + h.off_roles, role_hashes.data(), role_hashes.size() * 4);
    if (!role_fonts.empty()) std::memcpy(out.data() + h.off_role_fonts, role_fonts.data(), role_fonts.size() * 4);
    if (!rules.empty()) std::memcpy(out.data() + h.off_rules, rules.data(), rules.size());
    std::memcpy(out.data() + h.off_blob, blob.data(), blob.size());
    std::memcpy(out.data(), &h, sizeof h);
    const std::uint32_t crc = nnh::detail::crc32(out.data(), out.size());
    std::memcpy(out.data() + lf::kManifestCrcOffset, &crc, 4);
}

} // namespace nnh::assetc
