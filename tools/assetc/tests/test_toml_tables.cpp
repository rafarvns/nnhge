// TOML tables and arrays of tables (spec 014 T4.1, FR-6): headers, their nesting, [a.b] under an array of
// tables, and every way a header can clash with what came before.

#include <string>

#include "test.hpp"
#include "toml.hpp"

using namespace nnh::assetc::toml;

namespace {

std::string problem_of(std::string_view text) {
    Document d;
    if (d.parse(text)) return "ok";
    return std::to_string(d.problem().line) + ": " + d.problem().message;
}

const Table& sub(const Document& d, const Table& t, const char* key) {
    const Entry* e = t.find(key);
    CHECK(e != nullptr && e->kind == EntryKind::Table);
    return d.table(e != nullptr && e->kind == EntryKind::Table ? e->table : 0);
}

} // namespace

NNH_TEST(toml_reads_a_section) {
    Document d;
    CHECK(d.parse("name = \"Goblin\"\n[stats]\nhp = 30\nattack = 2\n"));
    const Entry* stats = d.root().find("stats");
    CHECK(stats != nullptr && stats->kind == EntryKind::Table && stats->line == 2);
    const Table& t = d.table(stats->table);
    CHECK(t.path == "stats" && t.line == 2 && t.defined);
    CHECK(t.find("hp")->value.integer == 30 && t.find("attack")->value.integer == 2);
    CHECK(d.root().find("hp") == nullptr);
}

NNH_TEST(toml_reads_nested_sections_and_implied_tables) {
    Document d;
    CHECK(d.parse("[a.b.c]\nx = 1\n[a]\ny = 2\n[ a . b ]\nz = 3"));
    const Table& a = sub(d, d.root(), "a");
    CHECK(a.defined && a.line == 3 && a.find("y") != nullptr);
    const Table& b = sub(d, a, "b");
    CHECK(b.path == "a.b" && b.find("z") != nullptr && b.line == 5);
    const Table& c = sub(d, b, "c");
    CHECK(c.path == "a.b.c" && c.find("x")->value.integer == 1);
}

NNH_TEST(toml_reads_arrays_of_tables_in_order) {
    Document d;
    CHECK(d.parse("[[drops]]\nitem = \"a\"\n\n[[drops]]\nitem = \"b\"\nchance = 0.5\n[[drops]]\n"));
    const Entry* drops = d.root().find("drops");
    CHECK(drops != nullptr && drops->kind == EntryKind::TableArray && drops->line == 1);
    CHECK(drops->elements.size() == 3);
    CHECK(d.table(drops->elements[0]).find("item")->value.text == "a");
    CHECK(d.table(drops->elements[1]).find("chance") != nullptr && d.table(drops->elements[1]).line == 4);
    CHECK(d.table(drops->elements[2]).entries.empty());
    CHECK(d.table(drops->elements[1]).path == "drops");
}

NNH_TEST(toml_puts_a_subsection_in_the_last_element_of_an_array_of_tables) {
    Document d;
    CHECK(d.parse("[[waves]]\nn = 1\n[waves.boss]\nhp = 9\n[[waves]]\nn = 2\n[waves.boss]\nhp = "
                  "7\n[[waves.adds]]\nk = 1"));
    const Entry* waves = d.root().find("waves");
    CHECK(waves->elements.size() == 2);
    CHECK(sub(d, d.table(waves->elements[0]), "boss").find("hp")->value.integer == 9);
    const Table& second = d.table(waves->elements[1]);
    CHECK(sub(d, second, "boss").find("hp")->value.integer == 7);
    CHECK(second.find("adds")->kind == EntryKind::TableArray && second.find("adds")->elements.size() == 1);
}

NNH_TEST(toml_refuses_a_table_defined_twice) {
    CHECK(problem_of("[stats]\nhp = 1\n[stats]") == "3: [stats] is defined twice (first on line 1)");
    CHECK(problem_of("[a.b]\n[a.b]") == "2: [a.b] is defined twice (first on line 1)");
    CHECK(problem_of("[a.b]\n[a]\n[a]") == "3: [a] is defined twice (first on line 2)");
}

NNH_TEST(toml_refuses_headers_that_clash_with_keys) {
    CHECK(problem_of("stats = 1\n[stats]") == "2: 'stats' is already a value (line 1), not a table");
    CHECK(problem_of("stats = 1\n[stats.x]") == "2: 'stats' is already a value (line 1), not a table");
    CHECK(problem_of("[stats]\n[[stats]]") == "2: 'stats' is a table (line 1), not an array of tables");
    CHECK(problem_of("[[drops]]\n[drops]") == "2: 'drops' is an array of tables (line 1), not a table");
    CHECK(problem_of("[a.b]\n[a]\nb = 1") == "3: 'b' is set twice (first on line 1)");
    CHECK(problem_of("[[a]]\n[a.b]\nx = 1\n[a]") == "4: 'a' is an array of tables (line 1), not a table");
}

NNH_TEST(toml_refuses_malformed_headers) {
    CHECK(problem_of("[stats") == "1: expected ']' to close the header");
    CHECK(problem_of("[[drops]") == "1: expected ']]' to close the header");
    CHECK(problem_of("[]") == "1: expected a name in the header");
    CHECK(problem_of("[a.]") == "1: expected a name in the header");
    CHECK(problem_of("[a..b]") == "1: expected a name in the header");
    CHECK(problem_of("[\"a\"]") == "1: quoted keys aren't supported");
    CHECK(problem_of("[a] x = 1") == "1: expected a comment or the end of the line after the header");
    CHECK(problem_of("[a.b.c.d.e]") == "1: a header has at most 4 parts");
    CHECK(problem_of("[a.b.c.d]") == "ok");
    CHECK(problem_of("[ [a]]") == "1: expected a name in the header");
}
