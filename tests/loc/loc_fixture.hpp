#pragma once
// Localization tests' fixture (spec 029 stage 6): a small writer of `.nnhstr` tables and the `.nnh_loc` manifest in the
// layout nnh-assetc writes (loc_file.hpp; tools/assetc/src/strings_check.cpp, strings_index.cpp), so core tests build
// exactly the languages, keys, chains, rules and fonts they need, and corrupt them on purpose. The assetc tests check
// that the real writer's files pass the same validator.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/crc32.hpp"
#include "../font/font_load_fixture.hpp"
#include "loc/loc_file.hpp"
#include "nnh/detail/name_hash.hpp"
#include "nnh/localization.hpp"

namespace loc_test {

namespace lf = nnh::detail::loc;

/// A fresh engine without localization: loc::stop() first, because Release's nnh::shutdown() skips the shutdown hooks
/// (a Release game that restarts the engine calls stop() itself; docs/api/localization.md).
inline void fresh_engine() {
    nnh::loc::stop();
    font_test::start_engine();
}

/// Plural rules in the interpreter's bytecode (plural.hpp).
struct Rules {
    static void range(std::vector<std::uint8_t>& out, std::uint32_t lo, std::uint32_t hi) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(lo >> (8 * i)));
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(hi >> (8 * i)));
    }
    /// English: one = n is 1.
    static std::vector<std::uint8_t> english() {
        std::vector<std::uint8_t> r = {1, lf::One, 1, 1, 0, 1};
        range(r, 1, 1);
        return r;
    }
    /// Russian (integers): one = n%10 is 1 and n%100 not 11; few = n%10 in 2..4 and n%100 not in 12..14; many = the
    /// rest (n%10 is 0, or 5..9, or n%100 in 11..14).
    static std::vector<std::uint8_t> russian() {
        std::vector<std::uint8_t> r = {3};
        r.insert(r.end(), {lf::One, 1, 2, 2, 1});
        range(r, 1, 1);
        r.insert(r.end(), {5, 1}); // %100, negated
        range(r, 11, 11);
        r.insert(r.end(), {lf::Few, 1, 2, 2, 1});
        range(r, 2, 4);
        r.insert(r.end(), {5, 1});
        range(r, 12, 14);
        r.insert(r.end(), {lf::Many, 3, 1, 2, 1});
        range(r, 0, 0);
        r.insert(r.end(), {1, 2, 1});
        range(r, 5, 9);
        r.insert(r.end(), {1, 4, 1});
        range(r, 11, 14);
        return r;
    }
};

struct TestKey {
    std::string name;
    std::string text;        ///< a plain key's text, as written ("{{" escapes, "{name}" placeholders)
    std::string forms[6];    ///< a plural key's forms by category ("" absent)
    std::string plural_arg;  ///< a plural key's argument
};

struct TestLanguage {
    std::string code;
    std::string name;
    int fallback = -1;       ///< another language's index; −1: the base
    std::string grouping = ",";
    int grouping_min = 4;
    std::vector<std::uint8_t> rule;
    std::vector<TestKey> keys;
    std::vector<std::pair<std::string, std::string>> fonts; ///< (role, font asset path)

    TestLanguage& key(const std::string& name, const std::string& text) {
        keys.push_back({name, text, {}, ""});
        return *this;
    }
    TestLanguage& plural(const std::string& name, const std::string& arg, const std::string& one, const std::string& other,
                         const std::string& few = "", const std::string& many = "") {
        TestKey k{name, "", {}, arg};
        k.forms[lf::One] = one;
        k.forms[lf::Few] = few;
        k.forms[lf::Many] = many;
        k.forms[lf::Other] = other;
        keys.push_back(k);
        return *this;
    }
    TestLanguage& font(const std::string& role, const std::string& path) {
        fonts.emplace_back(role, path);
        return *this;
    }
};

inline void put(std::vector<std::uint8_t>& out, std::size_t at, const void* p, std::size_t n) {
    std::memcpy(out.data() + at, p, n);
}

/// "{{" → "{", "}}" → "}", placeholders recorded at their offsets in the stored text.
inline std::string store(const std::string& text, std::vector<lf::PlaceholderRecord>& out) {
    std::string s;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if ((text[i] == '{' || text[i] == '}') && i + 1 < text.size() && text[i + 1] == text[i]) {
            s += text[i++];
        } else if (text[i] == '{') {
            const std::size_t end = text.find('}', i);
            lf::PlaceholderRecord p;
            p.name = nnh::detail::fnv1a32(text.substr(i + 1, end - i - 1).c_str());
            p.at = static_cast<std::uint16_t>(s.size());
            p.span = static_cast<std::uint8_t>(end - i + 1);
            out.push_back(p);
            s += text.substr(i, end - i + 1);
            i = end;
        } else {
            s += text[i];
        }
    }
    return s;
}

inline void seal_table(std::vector<std::uint8_t>& t) {
    if (t.size() < sizeof(lf::TableHeader)) return;
    std::memset(t.data() + lf::kTableCrcOffset, 0, 4);
    const std::uint32_t crc = nnh::detail::crc32(t.data(), t.size());
    put(t, lf::kTableCrcOffset, &crc, 4);
}

inline void seal_manifest(std::vector<std::uint8_t>& m) {
    if (m.size() < sizeof(lf::ManifestHeader)) return;
    std::memset(m.data() + lf::kManifestCrcOffset, 0, 4);
    const std::uint32_t crc = nnh::detail::crc32(m.data(), m.size());
    put(m, lf::kManifestCrcOffset, &crc, 4);
}

/// A language's `.nnhstr` (names kept unless `release`).
inline std::vector<std::uint8_t> write_table(const TestLanguage& l, bool release = false, int bits = 2) {
    std::vector<const TestKey*> keys;
    for (const TestKey& k : l.keys) keys.push_back(&k);
    std::sort(keys.begin(), keys.end(), [](const TestKey* a, const TestKey* b) {
        return nnh::detail::fnv1a32(a->name.c_str()) < nnh::detail::fnv1a32(b->name.c_str());
    });
    std::vector<lf::KeyRecord> records;
    std::vector<lf::StringRecord> strings;
    std::vector<lf::PlaceholderRecord> placeholders;
    std::string text, names;
    std::vector<std::uint32_t> name_offsets;
    const auto add = [&](const std::string& written) {
        lf::StringRecord s;
        s.text = static_cast<std::uint32_t>(text.size());
        s.first_placeholder = static_cast<std::uint16_t>(placeholders.size());
        const std::string stored = store(written, placeholders);
        s.length = static_cast<std::uint16_t>(stored.size());
        text += stored;
        text += '\0';
        strings.push_back(s);
    };
    for (const TestKey* k : keys) {
        lf::KeyRecord r;
        r.hash = nnh::detail::fnv1a32(k->name.c_str());
        r.first = static_cast<std::uint16_t>(strings.size());
        if (!k->plural_arg.empty()) {
            r.flags = 1;
            r.plural_arg = nnh::detail::fnv1a32(k->plural_arg.c_str());
            for (int c = 0; c < 6; ++c) {
                if (k->forms[c].empty()) continue;
                r.forms = static_cast<std::uint8_t>(r.forms | (1u << c));
                add(k->forms[c]);
            }
        } else {
            add(k->text);
        }
        records.push_back(r);
        name_offsets.push_back(static_cast<std::uint32_t>(names.size()));
        names += k->name;
        names += '\0';
    }
    lf::StringRecord sentinel;
    sentinel.text = static_cast<std::uint32_t>(text.size());
    sentinel.first_placeholder = static_cast<std::uint16_t>(placeholders.size());
    const std::size_t string_count = strings.size();
    strings.push_back(sentinel);
    lf::TableHeader h;
    std::strncpy(h.code, l.code.c_str(), sizeof h.code - 1);
    h.flags = release ? 0 : lf::kFlagNames;
    h.key_count = static_cast<std::uint32_t>(records.size());
    h.string_count = static_cast<std::uint32_t>(string_count);
    h.placeholder_count = static_cast<std::uint32_t>(placeholders.size());
    h.text_bytes = static_cast<std::uint32_t>(text.size());
    h.bucket_bits = static_cast<std::uint8_t>(bits);
    const auto align4 = [](std::size_t n) { return (n + 3) & ~std::size_t{3}; };
    std::size_t at = sizeof h;
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
    for (std::uint32_t b = 0, k = 0; b <= (1u << bits); ++b) {
        while (k < records.size() && lf::bucket_of(records[k].hash, bits) < b) ++k;
        const auto v = static_cast<std::uint16_t>(k);
        put(out, h.off_buckets + b * 2, &v, 2);
    }
    if (!records.empty()) put(out, h.off_keys, records.data(), records.size() * sizeof(lf::KeyRecord));
    put(out, h.off_strings, strings.data(), strings.size() * sizeof(lf::StringRecord));
    if (!placeholders.empty()) put(out, h.off_placeholders, placeholders.data(), placeholders.size() * 8);
    if (!text.empty()) put(out, h.off_text, text.data(), text.size());
    if (!release) {
        for (std::size_t i = 0; i < name_offsets.size(); ++i) {
            const auto o = static_cast<std::uint32_t>(name_offsets.size() * 4 + name_offsets[i]);
            put(out, h.off_names + i * 4, &o, 4);
        }
        if (!names.empty()) put(out, h.off_names + name_offsets.size() * 4, names.data(), names.size());
    }
    put(out, 0, &h, sizeof h);
    seal_table(out);
    return out;
}

/// A string built in this program (snprintf, then the const char* constructor): never `std::string` operator+, whose
/// out-of-line instantiation lives in libstdc++-6.dll — when another toolchain's copy of that DLL comes first on PATH
/// (Git Bash's /mingw64/bin), a string it allocates and this program frees corrupts the heap (seen in pc-release).
inline std::string joined(const char* a, const char* b, const char* c = "") {
    char buf[128];
    std::snprintf(buf, sizeof buf, "%s%s%s", a, b, c);
    return std::string(buf);
}

/// Stops localization when a test ends: Release's nnh::shutdown() skips the shutdown hooks, so a started module would
/// keep its after-input hook registered into the next test (tests/app/test_input_hooks.cpp counts them).
struct StopAtEnd {
    StopAtEnd() = default;
    StopAtEnd(const StopAtEnd&) = delete;
    StopAtEnd& operator=(const StopAtEnd&) = delete;
    ~StopAtEnd() { nnh::loc::stop(); }
};

/// `strings/<code>.nnhstr` = `bytes` (kept by the caller), added to the fake's assets at the next remount().
inline void add_table_asset(const TestLanguage& l, const std::vector<std::uint8_t>& bytes) {
    font_test::test_assets().push_back({joined("strings/", l.code.c_str(), ".nnhstr"), &bytes});
}

/// The languages of a test, `base` the base's index, and the `.nnh_loc` they make.
struct LocBuilder {
    std::vector<TestLanguage> languages;
    int base = 0;
    int pseudo = -1;
    bool release = false;
    std::vector<std::string> roles; ///< in order (default: every role a language names, first seen first)

    TestLanguage& add(const std::string& code, const std::string& name, std::vector<std::uint8_t> rule = Rules::english()) {
        TestLanguage l;
        l.code = code;
        l.name = name;
        l.rule = static_cast<std::vector<std::uint8_t>&&>(rule);
        languages.push_back(l);
        return languages.back();
    }

    std::vector<int> chain(int i) const {
        std::vector<int> c{i};
        while (c.back() != base) {
            const int f = languages[static_cast<std::size_t>(c.back())].fallback;
            c.push_back(f >= 0 ? f : base);
        }
        return c;
    }

    std::vector<std::uint8_t> manifest() const {
        std::vector<std::string> r = roles;
        for (const TestLanguage& l : languages)
            for (const auto& f : l.fonts)
                if (std::find(r.begin(), r.end(), f.first) == r.end()) r.push_back(f.first);
        std::string blob(1, '\0'); // offset 0 = none
        const auto add_blob = [&](const std::string& s) {
            const auto o = static_cast<std::uint32_t>(blob.size());
            blob += s;
            blob += '\0';
            return o;
        };
        const std::size_t n = languages.size();
        std::vector<lf::LanguageRecord> records(n);
        std::vector<std::uint32_t> role_fonts(n * r.size(), 0);
        std::vector<std::uint8_t> rules;
        std::uint32_t block = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const TestLanguage& l = languages[i];
            lf::LanguageRecord& rec = records[i];
            std::strncpy(rec.code, l.code.c_str(), sizeof rec.code - 1);
            rec.name = add_blob(l.name);
            const std::vector<int> c = chain(static_cast<int>(i));
            std::uint32_t sum = 0;
            for (std::size_t k = 0; k < c.size(); ++k) {
                rec.chain[k] = static_cast<std::uint8_t>(c[k]);
                sum += static_cast<std::uint32_t>(write_table(languages[static_cast<std::size_t>(c[k])], release).size());
            }
            block = std::max(block, sum);
            rec.rule = static_cast<std::uint32_t>(rules.size());
            rec.rule_bytes = static_cast<std::uint16_t>(l.rule.size());
            rules.insert(rules.end(), l.rule.begin(), l.rule.end());
            rec.grouping_min = static_cast<std::uint8_t>(l.grouping_min);
            std::strncpy(rec.grouping, l.grouping.c_str(), sizeof rec.grouping - 1);
            rec.table_bytes = static_cast<std::uint32_t>(write_table(l, release).size());
            for (const auto& f : l.fonts) {
                const auto role = static_cast<std::size_t>(std::find(r.begin(), r.end(), f.first) - r.begin());
                role_fonts[i * r.size() + role] = add_blob(f.second);
            }
        }
        lf::ManifestHeader h;
        h.flags = release ? 0 : lf::kFlagNames;
        h.language_count = static_cast<std::uint8_t>(n);
        h.base = static_cast<std::uint8_t>(base);
        h.pseudo = static_cast<std::uint8_t>(pseudo >= 0 ? pseudo : lf::kNoIndex);
        h.role_count = static_cast<std::uint8_t>(r.size());
        h.block_bytes = block;
        std::size_t at = sizeof h;
        h.off_languages = static_cast<std::uint32_t>(at);
        at += n * sizeof(lf::LanguageRecord);
        h.off_roles = static_cast<std::uint32_t>(at);
        at += r.size() * 4;
        h.off_role_fonts = static_cast<std::uint32_t>(at);
        at += role_fonts.size() * 4;
        h.off_rules = static_cast<std::uint32_t>(at);
        at += rules.size();
        h.off_blob = static_cast<std::uint32_t>(at);
        h.blob_bytes = static_cast<std::uint32_t>(blob.size());
        at += blob.size();
        h.size = static_cast<std::uint32_t>(at);
        std::vector<std::uint8_t> out(at, 0);
        put(out, h.off_languages, records.data(), n * sizeof(lf::LanguageRecord));
        for (std::size_t k = 0; k < r.size(); ++k) {
            const std::uint32_t hash = nnh::detail::fnv1a32(r[k].c_str());
            put(out, h.off_roles + k * 4, &hash, 4);
        }
        if (!role_fonts.empty()) put(out, h.off_role_fonts, role_fonts.data(), role_fonts.size() * 4);
        if (!rules.empty()) put(out, h.off_rules, rules.data(), rules.size());
        put(out, h.off_blob, blob.data(), blob.size());
        put(out, 0, &h, sizeof h);
        seal_manifest(out);
        return out;
    }

    /// The tables and the manifest added to the fake's assets (the engine restarted once).
    void install() const {
        using namespace font_test;
        for (const TestLanguage& l : languages)
            add_table_asset(l, keep(write_table(l, release)));
        test_assets().push_back({".nnh_loc", &keep(manifest())});
        remount();
    }
};

/// English (base, "1,000"), Brazilian Portuguese ("1.000"), Russian (NBSP from 5 digits, three plural forms) and
/// Japanese (no grouping) with a few keys; pt-BR leaves `menu.quit` to the base.
inline LocBuilder four_languages() {
    LocBuilder b;
    b.add("en", "English")
        .key("menu.new_game", "New game")
        .key("menu.quit", "Quit")
        .key("hud.score", "Score: {score}")
        .key("hud.hello", "Hello, {name}!")
        .key("hud.braces", "{{literal}} {n}")
        .key("item.sword", "sword")
        .key("hud.found", "You found a {item}.")
        .plural("items.count", "count", "{count} item", "{count} items");
    b.add("pt-BR", "Português (Brasil)")
        .key("menu.new_game", "Novo jogo")
        .key("hud.score", "Pontos: {score}")
        .key("item.sword", "espada")
        .key("hud.found", "Você achou: {item}.")
        .plural("items.count", "count", "{count} item", "{count} itens");
    b.languages[1].grouping = ".";
    b.add("ru", "Русский", Rules::russian())
        .key("menu.new_game", "Новая игра")
        .plural("items.count", "count", "{count} предмет", "{count} предмета", "{count} предмета", "{count} предметов");
    b.languages[2].grouping = "\xC2\xA0";
    b.languages[2].grouping_min = 5;
    b.add("ja", "日本語", {})
        .key("menu.new_game", "ニューゲーム")
        .plural("items.count", "count", "", "アイテム{count}個");
    b.languages[3].grouping = "";
    return b;
}

} // namespace loc_test
