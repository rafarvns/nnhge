// nnh new's name, title, author and unique ID rules (new/project_name.hpp, new/unique_id.hpp; spec 033 FR-4,
// FR-11).

#include "new/project_name.hpp"
#include "new/unique_id.hpp"
#include "test.hpp"

#include <set>
#include <string>

using namespace nnh::tool;

namespace {

bool refused(const std::string& name, const char* rule_part) {
    const NameCheck c = check_name(name);
    return !c.ok && c.rule.find(rule_part) != std::string::npos;
}

} // namespace

NNH_TEST(valid_game_names_are_accepted) {
    for (const char* name : {"a", "demo", "my-game", "my_game", "game2", "z9-_",
                             "abcdefghijklmnopqrstuvwxyz012345", "console", "nnhgame", "distant", "tester"}) {
        const NameCheck c = check_name(name);
        CHECK(c.ok);
        CHECK(c.rule.empty());
    }
}

NNH_TEST(names_with_separators_dots_or_drive_letters_are_refused) {
    CHECK(refused("a/b", "path separator"));
    CHECK(refused("a\\b", "path separator"));
    CHECK(refused("../evil", "path separator"));
    CHECK(refused("a..b", "'..'"));
    CHECK(refused("..", "'..'"));
    CHECK(refused("c:game", "drive letter"));
    CHECK(refused("C:", "drive letter"));
    CHECK(refused("-game", "starts with '-' or '.'"));
    CHECK(refused(".game", "starts with '-' or '.'"));
    CHECK(refused("", "1 to 32 characters"));
}

NNH_TEST(names_with_other_characters_case_or_length_are_refused) {
    CHECK(refused("My Game", "only lowercase letters, digits, '_' and '-'"));
    CHECK(refused("Game", "only lowercase letters, digits, '_' and '-'"));
    CHECK(refused("game!", "only lowercase letters, digits, '_' and '-'"));
    CHECK(refused("jogo\xc3\xa7", "only lowercase letters, digits, '_' and '-'")); // UTF-8 c-cedilla
    CHECK(refused("2game", "start with a letter"));
    CHECK(refused("_game", "start with a letter"));
    CHECK(refused("abcdefghijklmnopqrstuvwxyz0123456", "longer than 32"));
}

NNH_TEST(names_windows_reserves_are_refused) {
    for (const char* name : {"con", "nul", "aux", "prn", "com1", "com9", "lpt1", "lpt9"})
        CHECK(refused(name, "Windows"));
    CHECK(check_name("com0").ok);
    CHECK(check_name("com10").ok);
    CHECK(check_name("lpt").ok);
}

NNH_TEST(target_names_the_engine_or_cmake_use_are_refused) {
    for (const char* name :
         {"dist", "all", "clean", "install", "test", "help", "package", "package_source", "edit_cache",
          "rebuild_cache", "list_install_components", "nnh", "nnh_x", "nnh-x"}) {
        CHECK(refused(name, "target name"));
    }
}

NNH_TEST(a_refusal_suggests_a_valid_spelling_when_one_is_obvious) {
    CHECK(check_name("My Game").suggestion == "my-game");
    CHECK(check_name("Space  Rocks!").suggestion == "space-rocks");
    CHECK(check_name("Game").suggestion == "game");
    CHECK(check_name("2048 clone").suggestion == "clone");
    CHECK(check_name("abcdefghijklmnopqrstuvwxyz0123456").suggestion == "abcdefghijklmnopqrstuvwxyz012345");
    CHECK(check_name("dist").suggestion == "dist-game");
    CHECK(check_name("con").suggestion == "con-game");
    CHECK(check_name("nnh_x").suggestion.empty()); // nothing obvious
    CHECK(check_name("!!!").suggestion.empty());
    CHECK(check_name("demo").suggestion.empty()); // valid: nothing to suggest
}

NNH_TEST(titles_and_authors_are_short_printable_utf8) {
    CHECK(check_text_field("My Game", false).ok);
    CHECK(check_text_field("Jo\xc3\xa3o's Game 2: \xe3\x82\xb2\xe3\x83\xbc\xe3\x83\xa0", false).ok); // UTF-8
    CHECK(check_text_field(std::string(64, 'x'), false).ok);
    std::string sixty_four_chars;
    for (int i = 0; i < 64; ++i)
        sixty_four_chars += "\xc3\xa9"; // 64 characters, 128 bytes
    CHECK(check_text_field(sixty_four_chars, false).ok);
    CHECK(!check_text_field(sixty_four_chars + "x", false).ok);
    CHECK(check_text_field(std::string(65, 'x'), false).rule.find("64") != std::string::npos);
    CHECK(check_text_field("", true).ok);
    CHECK(check_text_field("", false).rule.find("empty") != std::string::npos);
    for (const char* bad : {"a\"b", "a\\b", "a$b", "a;b", "a@b", "a`b"}) {
        const TextCheck c = check_text_field(bad, false);
        CHECK(!c.ok && c.rule.find("can't hold") != std::string::npos);
    }
    for (const char* bad : {"a\tb", "a\nb", "a\x7f", "a\xc2\x85"})
        CHECK(check_text_field(bad, false).rule.find("control") != std::string::npos);
    for (const char* bad : {"\xff", "a\xc3", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80"}) {
        CHECK(check_text_field(bad, false).rule.find("UTF-8") != std::string::npos);
    }
}

NNH_TEST(unique_ids_are_parsed_in_the_homebrew_range) {
    std::string e;
    CHECK(parse_unique_id("0x5A3C1", e) == 0x5A3C1u);
    CHECK(parse_unique_id("0X5a3c1", e) == 0x5A3C1u);
    CHECK(parse_unique_id("5A3C1", e) == 0x5A3C1u);
    CHECK(parse_unique_id("0x00300", e) == 0x300u);
    CHECK(parse_unique_id("0x300", e) == 0x300u);
    CHECK(parse_unique_id("0xF7FFF", e) == 0xF7FFFu);
    CHECK(!parse_unique_id("0x2FF", e) && e.find("0x00300") != std::string::npos);
    CHECK(!parse_unique_id("0xF8000", e) && e.find("0xF7FFF") != std::string::npos);
    CHECK(!parse_unique_id("0x100000000", e));
    CHECK(!parse_unique_id("0xFFFFFFFFFFFFFFFFFFFF", e));
    for (const char* bad : {"", "0x", "x5", "0x5G", "-5", "5 ", " 5", "0x 5"}) {
        CHECK(!parse_unique_id(bad, e) && e.find("hex") != std::string::npos);
    }
    CHECK(format_unique_id(0x5A3C1) == "0x5A3C1");
    CHECK(format_unique_id(0x300) == "0x00300");
}

NNH_TEST(random_unique_ids_stay_in_the_homebrew_range) {
    std::set<std::uint32_t> seen;
    for (int i = 0; i < 10000; ++i) {
        const std::uint32_t id = random_unique_id();
        CHECK(id >= kUniqueIdMin && id <= kUniqueIdMax);
        seen.insert(id);
    }
    CHECK(seen.size() > 9000); // 10,000 draws from 1,015,040 values: a handful of repeats at most
}
