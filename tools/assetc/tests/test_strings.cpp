// The string converter (spec 029 FR-15..FR-20, FR-31): reading string files, placeholders, plural rules (CLDR 46 integer
// samples), the build's checks with their exact messages, the tables and the pseudo-language.

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "loc/loc_file.hpp"
#include "loc/plural.hpp"
#include "nnh/detail/name_hash.hpp"
#include "plural_compile.hpp"
#include "plural_rules_cldr.hpp"
#include "strings_check.hpp"
#include "strings_index.hpp"
#include "strings_read.hpp"
#include "test.hpp"

using namespace nnh::assetc;
namespace lf = nnh::detail::loc;

namespace {

StringsLanguage read(const char* code, const char* text, std::vector<std::string>* problems = nullptr) {
    StringsLanguage l;
    l.code = code;
    std::vector<std::string> p;
    read_strings(text, std::string("strings/") + code + ".strings", l, problems != nullptr ? *problems : p);
    return l;
}

bool has(const std::vector<std::string>& v, const std::string& want) {
    return std::find(v.begin(), v.end(), want) != v.end();
}

lf::Category category_of(const char* code, std::uint64_t n) {
    StringsLanguage l;
    l.code = code;
    std::vector<std::uint8_t> rule;
    language_rule(l, rule);
    return lf::plural_category(rule.data(), rule.size(), n);
}

const char* kEn = "[language]\nname = \"English\"\n\n[menu]\nnew_game = \"New game\"\noptions = \"Options\"\n"
                  "[items]\ncount.one = \"{count} item\"\ncount.other = \"{count} items\"\n"
                  "found = \"{name} found {count} {item}!\"\n[hud]\nscore = \"Score: {score}\"\n";

} // namespace

NNH_TEST(strings_read_sections_dotted_keys_and_multiline_strings) {
    std::vector<std::string> problems;
    const StringsLanguage l = read("en", "[menu]\nnew_game = \"New\" # comment\nsub.key = \"x\"\n"
                                          "story = \"\"\"\nOnce \\\n   upon\na time\"\"\"\nesc = \"a\\\"b\\u00E9\\n\"\n",
                                   &problems);
    CHECK(problems.empty());
    CHECK(l.keys.count("menu.new_game") == 1 && l.keys.at("menu.new_game").text == "New");
    CHECK(l.keys.count("menu.sub.key") == 1);
    CHECK(l.keys.at("menu.story").text == "Once upon\na time");
    CHECK(l.keys.at("menu.esc").text == "a\"b\xC3\xA9\n");
}

NNH_TEST(strings_read_refuses_duplicates_and_bad_values) {
    std::vector<std::string> problems;
    read("en", "a = \"x\"\n[b]\nc = 1\na = \"y\"\n", &problems);
    CHECK(has(problems, "strings/en.strings:3: a value is a string (\"text\" or \"\"\"text\"\"\")"));
    std::vector<std::string> twice;
    read("en", "[menu]\nnew_game = \"a\"\n\n\nnew_game = \"b\"\n", &twice);
    CHECK(has(twice, "strings/en.strings:5: \"menu.new_game\" is set twice (first at line 2)"));
    std::vector<std::string> bad;
    read("de", "a = \"x\xC3\"\n", &bad);
    CHECK(!bad.empty() && bad[0].rfind("strings/de.strings:1: invalid UTF-8 (byte 0xC3", 0) == 0);
}

NNH_TEST(strings_language_metadata) {
    const StringsLanguage l = read("ja", "[language]\nname = \"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\"\nfallback = \"en\"\n"
                                         "grouping = \".\"\ngrouping_min = 5\n[language.fonts]\nbody = \"fonts/body_ja\"\n");
    CHECK(l.meta.name == "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" && l.meta.fallback == "en" && l.meta.grouping == ".");
    CHECK(l.meta.grouping_min == 5 && l.meta.fonts.size() == 1 && l.meta.fonts[0].second == "fonts/body_ja");
}

NNH_TEST(strings_placeholders_and_braces) {
    std::string stored;
    std::vector<Placeholder> ps;
    CHECK(parse_placeholders("{name} has {{x}} {count}", stored, ps).empty());
    CHECK(stored == "{name} has {x} {count}" && ps.size() == 2 && ps[0].name == "name" && ps[1].at == 15 && ps[1].span == 7);
    CHECK(parse_placeholders("Score: {score", stored, ps) == "unclosed \"{\" at column 8 (write \"{{\" for a brace)");
    CHECK(parse_placeholders("a } b", stored, ps) == "a lone \"}\" at column 3 (write \"}}\" for a brace)");
    CHECK(!parse_placeholders("{1x}", stored, ps).empty());
}

NNH_TEST(strings_plural_rules_match_cldr_46_integer_samples) {
    // @integer samples of CLDR 46's plurals.xml, transcribed
    const struct {
        const char* code;
        lf::Category c;
        std::uint64_t samples[6];
    } kSamples[] = {
        {"en", lf::One, {1, 1, 1, 1, 1, 1}}, {"en", lf::Other, {0, 2, 16, 100, 1000, 1000000}},
        {"fr", lf::One, {0, 1, 0, 1, 0, 1}}, {"fr", lf::Many, {1000000, 2000000, 1000000, 1000000, 1000000, 1000000}},
        {"fr", lf::Other, {2, 17, 100, 1000, 10000, 100000}},
        {"es", lf::One, {1, 1, 1, 1, 1, 1}}, {"es", lf::Many, {1000000, 3000000, 1000000, 1000000, 1000000, 1000000}},
        {"pt", lf::One, {0, 1, 0, 1, 0, 1}}, {"pt-PT", lf::One, {1, 1, 1, 1, 1, 1}}, {"pt-PT", lf::Other, {0, 2, 3, 10, 0, 2}},
        {"ru", lf::One, {1, 21, 31, 41, 101, 1001}}, {"ru", lf::Few, {2, 3, 4, 22, 24, 1002}},
        {"ru", lf::Many, {0, 5, 11, 19, 100, 1000}}, {"uk", lf::Many, {11, 12, 14, 25, 100, 10000}},
        {"pl", lf::One, {1, 1, 1, 1, 1, 1}}, {"pl", lf::Few, {2, 3, 4, 22, 23, 1002}},
        {"pl", lf::Many, {0, 5, 12, 21, 100, 1000}}, {"cs", lf::Few, {2, 3, 4, 2, 3, 4}},
        {"cs", lf::Other, {0, 5, 19, 100, 1000, 10000}}, {"sl", lf::One, {1, 101, 201, 301, 1001, 10001}},
        {"sl", lf::Two, {2, 102, 202, 302, 1002, 10002}}, {"sl", lf::Few, {3, 4, 103, 104, 203, 1003}},
        {"lt", lf::One, {1, 21, 31, 41, 51, 101}}, {"lt", lf::Few, {2, 9, 22, 29, 102, 1002}},
        {"lt", lf::Other, {0, 10, 11, 19, 20, 30}}, {"lv", lf::Zero, {0, 10, 11, 19, 20, 30}},
        {"lv", lf::One, {1, 21, 31, 41, 51, 101}}, {"lv", lf::Other, {2, 9, 22, 29, 102, 1002}},
        {"ro", lf::One, {1, 1, 1, 1, 1, 1}}, {"ro", lf::Few, {0, 2, 16, 101, 119, 1001}},
        {"ro", lf::Other, {20, 35, 100, 1000, 10000, 100000}}, {"hr", lf::Few, {2, 4, 22, 24, 32, 102}},
        {"ja", lf::Other, {0, 1, 2, 15, 100, 1000000}}, {"it", lf::Many, {1000000, 1000000, 1000000, 1000000, 1000000, 1000000}},
        {"da", lf::One, {1, 1, 1, 1, 1, 1}}, {"da", lf::Other, {0, 2, 16, 100, 1000, 10000}},
    };
    int wrong = 0;
    for (const auto& s : kSamples)
        for (const std::uint64_t n : s.samples) wrong += category_of(s.code, n) == s.c ? 0 : 1;
    CHECK(wrong == 0);
    CHECK(cldr_rule_count() == 36);
}

NNH_TEST(strings_required_categories) {
    StringsLanguage es;
    es.code = "es";
    std::vector<std::uint8_t> rule;
    language_rule(es, rule);
    CHECK(required_categories(rule) == ((1 << lf::One) | (1 << lf::Many) | (1 << lf::Other))); // many: 1 000 000
    StringsLanguage ja;
    ja.code = "ja";
    language_rule(ja, rule);
    CHECK(required_categories(rule) == (1 << lf::Other));
}

NNH_TEST(strings_check_messages) {
    const StringsLanguage en = read("en", kEn);
    const StringsLanguage de = read("de", "[menu]\noptons = \"Optionen\"\n[items]\ncount.one = \"{cnt} Ding\"\n"
                                          "count.other = \"{count} Dinge\"\n[hud]\nscore = \"Punkte\"\n");
    CheckResult r;
    check_language(de, en, r);
    CHECK(has(r.errors, "strings/de.strings:2: \"menu.optons\" is not in the base language en (did you mean \"menu.options\"?)"));
    CHECK(has(r.errors, "strings/de.strings:4: \"items.count\": placeholder {cnt} is not in en's text (en has {count})"));
    CHECK(has(r.errors, "strings/de.strings:7: \"hud.score\": placeholder {score} missing (en has it)"));
    CHECK(!r.warnings.empty() && r.warnings.back().find("strings/de.strings: warning: 3 keys of en have no translation") == 0);
    const StringsLanguage ru = read("ru", "[items]\ncount.one = \"{count} \xD0\xBF\"\ncount.other = \"{count} \xD0\xBF\"\n");
    CheckResult rr;
    check_language(ru, en, rr);
    CHECK(has(rr.errors, "strings/ru.strings:2: \"items.count\" needs the plural forms one, few, many, other for ru "
                         "(missing: few, many)"));
}

NNH_TEST(strings_table_is_sorted_by_hash_without_names_in_release) {
    const StringsLanguage en = read("en", kEn);
    const std::vector<std::uint8_t> debug = write_table(en, false), release = write_table(en, true);
    CHECK(lf::validate_table(debug.data(), debug.size()) == nullptr);
    CHECK(lf::validate_table(release.data(), release.size()) == nullptr);
    const std::string r(release.begin(), release.end()), d(debug.begin(), debug.end());
    CHECK(d.find("menu.new_game") != std::string::npos && r.find("menu.new_game") == std::string::npos);
    lf::TableHeader h;
    std::memcpy(&h, release.data(), sizeof h);
    CHECK(h.key_count == 5 && h.flags == 0);
    std::uint32_t prev = 0;
    bool sorted = true;
    for (std::uint32_t i = 0; i < h.key_count; ++i) {
        lf::KeyRecord k;
        std::memcpy(&k, release.data() + h.off_keys + i * 12, 12);
        sorted = sorted && (i == 0 || k.hash > prev);
        prev = k.hash;
    }
    CHECK(sorted);
}

NNH_TEST(strings_pseudo_language) {
    CHECK(pseudo_text("Options") == "[\xC3\x96pt\xC3\xAE\xC3\xB6\xC3\xB1s~~~]");
    CHECK(pseudo_text("{count} items") == "[{count} \xC3\xAEt\xC3\xA9ms~~]"); // " items": 6 characters, ⌈1.8⌉ = 2
}
