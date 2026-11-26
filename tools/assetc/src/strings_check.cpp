// Checking string files and building tables (strings_check.hpp).

#include "strings_check.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/crc32.hpp"
#include "loc/loc_file.hpp"
#include "nnh/detail/name_hash.hpp"
#include "plural_rules_cldr.hpp"
#include "suggest.hpp"
#include "text/break_class.hpp"
#include "text/utf8.hpp"

namespace nnh::assetc {
namespace {

namespace lf = nnh::detail::loc;

bool ident_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool ident(char c) { return ident_start(c) || (c >= '0' && c <= '9'); }

std::string where(const Where& w) { return w.file + ":" + std::to_string(w.line) + ": "; }

std::vector<std::string> names_of(const std::string& text) {
    std::string stored;
    std::vector<Placeholder> ps;
    parse_placeholders(text, stored, ps);
    std::vector<std::string> out;
    for (const Placeholder& p : ps) out.push_back(p.name);
    return out;
}

bool contains(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }

/// Every placeholder name of a key (all its forms).
std::vector<std::string> key_names(const StringKey& k) {
    std::vector<std::string> out;
    if (!k.plural()) return names_of(k.text);
    for (const std::string& f : k.forms)
        for (const std::string& n : names_of(f))
            if (!contains(out, n)) out.push_back(n);
    return out;
}

std::string plural_arg_of(const StringKey& k) {
    if (!k.plural_arg.empty()) return k.plural_arg;
    const std::vector<std::string> names = names_of(k.forms[lf::Other]);
    if (contains(names, "count")) return "count";
    return names.empty() ? std::string() : names[0];
}

int characters(const std::string& s) {
    int n = 0;
    for (const char* p = s.c_str(); *p != 0;) n += nnh::detail::next_codepoint(p) != 0 ? 1 : 0;
    return n;
}

void put16(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t v) {
    out[at] = static_cast<std::uint8_t>(v);
    out[at + 1] = static_cast<std::uint8_t>(v >> 8);
}

} // namespace

std::string parse_placeholders(const std::string& text, std::string& stored, std::vector<Placeholder>& out) {
    stored.clear();
    out.clear();
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '{' && i + 1 < text.size() && text[i + 1] == '{') {
            stored += '{';
            ++i;
        } else if (c == '}' && i + 1 < text.size() && text[i + 1] == '}') {
            stored += '}';
            ++i;
        } else if (c == '}') {
            return "a lone \"}\" at column " + std::to_string(i + 1) + " (write \"}}\" for a brace)";
        } else if (c == '{') {
            std::size_t e = i + 1;
            while (e < text.size() && ident(text[e])) ++e;
            if (e >= text.size() || text[e] != '}' || e == i + 1 || !ident_start(text[i + 1]))
                return "unclosed \"{\" at column " + std::to_string(i + 1) + " (write \"{{\" for a brace)";
            if (out.size() == 16) return "more than 16 placeholders";
            out.push_back({text.substr(i + 1, e - i - 1), static_cast<int>(stored.size()), static_cast<int>(e - i + 1)});
            stored.append(text, i, e - i + 1);
            i = e;
        } else {
            stored += c;
        }
    }
    return "";
}

std::string language_rule(const StringsLanguage& lang, std::vector<std::uint8_t>& bytecode) {
    std::vector<PluralSource> rules = lang.meta.plural;
    if (rules.empty()) {
        if (const CldrRule* r = cldr_rule(lang.code.c_str())) {
            for (const CldrCategoryRule& c : r->categories)
                if (c.category != nullptr) rules.push_back({c.category, c.rule});
        }
    }
    return compile_plural(rules, bytecode);
}

void check_language(const StringsLanguage& lang, const StringsLanguage& base, CheckResult& r) {
    std::vector<std::uint8_t> rule;
    const std::string rule_problem = language_rule(lang, rule);
    if (!rule_problem.empty()) r.errors.push_back(lang.files.empty() ? rule_problem : lang.files[0] + ": plural rule: " + rule_problem);
    const std::uint8_t needed = required_categories(rule);
    std::vector<std::string> base_names;
    for (const auto& [name, k] : base.keys) base_names.push_back(name);
    for (const auto& [name, k] : lang.keys) {
        // placeholders of every text well-formed; combining marks warned
        const std::string* texts = k.plural() ? k.forms : &k.text;
        const int count = k.plural() ? 6 : 1;
        for (int i = 0; i < count; ++i) {
            std::string stored;
            std::vector<Placeholder> ps;
            const std::string bad = parse_placeholders(texts[i], stored, ps);
            if (!bad.empty()) r.errors.push_back(where(k.where) + "\"" + name + "\": " + bad);
            for (const char* p = texts[i].c_str(); *p != 0;) {
                const char32_t c = nnh::detail::next_codepoint(p);
                if ((nnh::detail::break_class(c) & nnh::detail::kBreakZeroWidth) != 0) {
                    char what[96];
                    std::snprintf(what, sizeof what, "contains U+%04X (a combining mark draws nothing)", static_cast<unsigned>(c));
                    r.warnings.push_back(where(k.where) + "warning: \"" + name + "\" " + what);
                    break;
                }
            }
        }
        const auto b = base.keys.find(name);
        if (b == base.keys.end()) {
            const std::string near = closest(name, base_names);
            r.errors.push_back(where(k.where) + "\"" + name + "\" is not in the base language " + base.code +
                               (near.empty() ? "" : " (did you mean \"" + near + "\"?)"));
            continue;
        }
        if (k.plural()) {
            if ((k.form_bits & (1u << lf::Other)) == 0) r.errors.push_back(where(k.where) + "\"" + name + "\" has no \"other\" form");
            const std::uint8_t missing = static_cast<std::uint8_t>(needed & ~k.form_bits);
            if (missing != 0 && (k.form_bits & (1u << lf::Other)) != 0) {
                std::string all, miss;
                for (int c = 0; c < 6; ++c) {
                    if ((needed >> c) & 1) all += std::string(all.empty() ? "" : ", ") + category_name(c);
                    if ((missing >> c) & 1) miss += std::string(miss.empty() ? "" : ", ") + category_name(c);
                }
                r.errors.push_back(where(k.where) + "\"" + name + "\" needs the plural forms " + all + " for " + lang.code +
                                   " (missing: " + miss + ")");
            }
        }
        // placeholders against the base's
        const std::vector<std::string> mine = key_names(k), theirs = key_names(b->second);
        for (const std::string& n : mine) {
            if (!contains(theirs, n)) {
                std::string list;
                for (const std::string& t : theirs) list += std::string(list.empty() ? "" : " ") + "{" + t + "}";
                r.errors.push_back(where(k.where) + "\"" + name + "\": placeholder {" + n + "} is not in " + base.code +
                                   "'s text" + (list.empty() ? "" : " (" + base.code + " has " + list + ")"));
            }
        }
        const std::string arg = k.plural() ? plural_arg_of(b->second) : std::string();
        for (const std::string& n : theirs) {
            if (!contains(mine, n) && n != arg) {
                r.errors.push_back(where(k.where) + "\"" + name + "\": placeholder {" + n + "} missing (" + base.code + " has it)");
            }
        }
        for (const auto& [prefix, limit] : base.meta.limits) {
            const int chars = characters(k.plural() ? k.forms[lf::Other] : k.text);
            if (name.rfind(prefix, 0) == 0 && chars > limit) {
                r.warnings.push_back(where(k.where) + "warning: \"" + name + "\" is " + std::to_string(chars) +
                                     " characters (limit " + std::to_string(limit) + " for \"" + prefix + "\")");
            }
        }
    }
    if (&lang != &base) {
        std::vector<std::string> missing;
        for (const auto& [name, k] : base.keys)
            if (lang.keys.count(name) == 0) missing.push_back(name);
        if (!missing.empty()) {
            std::string first;
            for (std::size_t i = 0; i < missing.size() && i < 5; ++i) first += (i > 0 ? ", " : "") + missing[i];
            r.warnings.push_back((lang.files.empty() ? lang.code : lang.files[0]) + ": warning: " +
                                 std::to_string(missing.size()) + " keys of " + base.code + " have no translation (" +
                                 base.code + "'s text is used): " + first + (missing.size() > 5 ? ", … (first 5)" : ""));
        }
    }
}

std::vector<std::uint8_t> write_table(const StringsLanguage& lang, bool release) {
    struct Entry {
        std::uint32_t hash;
        const std::string* name;
        const StringKey* key;
    };
    std::vector<Entry> keys;
    for (const auto& [name, k] : lang.keys) keys.push_back({nnh::detail::fnv1a32(name.c_str()), &name, &k});
    std::sort(keys.begin(), keys.end(), [](const Entry& a, const Entry& b) { return a.hash < b.hash; });
    int bits = 0;
    while ((1u << bits) < keys.size() / 2 && bits < 15) ++bits;
    std::vector<lf::KeyRecord> records;
    std::vector<lf::StringRecord> strings;
    std::vector<lf::PlaceholderRecord> placeholders;
    std::string text, names;
    std::vector<std::uint32_t> name_offsets;
    const auto add_string = [&](const std::string& source) {
        std::string stored;
        std::vector<Placeholder> ps;
        parse_placeholders(source, stored, ps);
        lf::StringRecord s;
        s.text = static_cast<std::uint32_t>(text.size());
        s.length = static_cast<std::uint16_t>(stored.size());
        s.first_placeholder = static_cast<std::uint16_t>(placeholders.size());
        for (const Placeholder& p : ps) {
            lf::PlaceholderRecord pr;
            pr.name = nnh::detail::fnv1a32(p.name.c_str());
            pr.at = static_cast<std::uint16_t>(p.at);
            pr.span = static_cast<std::uint8_t>(p.span);
            placeholders.push_back(pr);
        }
        text += stored;
        text += '\0';
        strings.push_back(s);
    };
    for (const Entry& e : keys) {
        lf::KeyRecord k;
        k.hash = e.hash;
        k.first = static_cast<std::uint16_t>(strings.size());
        if (e.key->plural()) {
            k.forms = e.key->form_bits;
            k.flags = 1;
            k.plural_arg = nnh::detail::fnv1a32(plural_arg_of(*e.key).c_str());
            for (int c = 0; c < 6; ++c)
                if ((e.key->form_bits >> c) & 1) add_string(e.key->forms[c]);
        } else {
            add_string(e.key->text);
        }
        records.push_back(k);
        name_offsets.push_back(static_cast<std::uint32_t>(names.size()));
        names += *e.name;
        names += '\0';
    }
    lf::StringRecord sentinel;
    sentinel.text = static_cast<std::uint32_t>(text.size());
    sentinel.first_placeholder = static_cast<std::uint16_t>(placeholders.size());
    const std::size_t string_count = strings.size();
    strings.push_back(sentinel);
    lf::TableHeader h;
    std::strncpy(h.code, lang.code.c_str(), sizeof h.code - 1);
    h.flags = release ? 0 : lf::kFlagNames;
    h.key_count = static_cast<std::uint32_t>(records.size());
    h.string_count = static_cast<std::uint32_t>(string_count);
    h.placeholder_count = static_cast<std::uint32_t>(placeholders.size());
    h.text_bytes = static_cast<std::uint32_t>(text.size());
    h.bucket_bits = static_cast<std::uint8_t>(bits);
    std::size_t at = sizeof h;
    const auto align4 = [](std::size_t n) { return (n + 3) & ~std::size_t{3}; };
    h.off_buckets = static_cast<std::uint32_t>(at);
    at = align4(at + ((std::size_t{1} << bits) + 1) * 2);
    h.off_keys = static_cast<std::uint32_t>(at);
    at += records.size() * sizeof(lf::KeyRecord);
    h.off_strings = static_cast<std::uint32_t>(at);
    at += strings.size() * sizeof(lf::StringRecord);
    h.off_placeholders = static_cast<std::uint32_t>(at);
    at += placeholders.size() * sizeof(lf::PlaceholderRecord);
    h.off_text = static_cast<std::uint32_t>(at);
    at = align4(at + text.size());
    h.off_names = release ? 0 : static_cast<std::uint32_t>(at);
    if (!release) at = align4(at + name_offsets.size() * 4 + names.size());
    h.size = static_cast<std::uint32_t>(at + lf::kGuardBytes);
    std::vector<std::uint8_t> out(h.size, 0);
    for (std::uint32_t b = 0, k = 0; b <= (1u << bits); ++b) { // first key of bucket b
        while (k < records.size() && lf::bucket_of(records[k].hash, bits) < b) ++k;
        put16(out, h.off_buckets + b * 2, k);
    }
    std::memcpy(out.data() + h.off_keys, records.data(), records.size() * sizeof(lf::KeyRecord));
    std::memcpy(out.data() + h.off_strings, strings.data(), strings.size() * sizeof(lf::StringRecord));
    if (!placeholders.empty()) std::memcpy(out.data() + h.off_placeholders, placeholders.data(), placeholders.size() * 8);
    std::memcpy(out.data() + h.off_text, text.data(), text.size());
    if (!release) {
        for (std::size_t i = 0; i < name_offsets.size(); ++i) {
            const std::uint32_t o = static_cast<std::uint32_t>(name_offsets.size() * 4) + name_offsets[i];
            std::memcpy(out.data() + h.off_names + i * 4, &o, 4);
        }
        std::memcpy(out.data() + h.off_names + name_offsets.size() * 4, names.data(), names.size());
    }
    std::memcpy(out.data(), &h, sizeof h);
    const std::uint32_t crc = nnh::detail::crc32(out.data(), out.size());
    std::memcpy(out.data() + lf::kTableCrcOffset, &crc, 4);
    return out;
}

} // namespace nnh::assetc
