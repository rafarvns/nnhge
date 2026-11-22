// The problems of one data file as `<shown path>:<line>: <reason>` (spec 014 T5.2, FR-9): sorted by line (in
// the order found within a line), at most 20, then how many more.

#include <string>

#include "diagnostics.hpp"
#include "test.hpp"

using nnh::assetc::Diagnostics;

NNH_TEST(diagnostics_print_each_problem_with_the_shown_path_and_line) {
    Diagnostics d;
    CHECK(d.empty());
    d.add(7, "unknown field 'damgae' (did you mean 'damage'?)");
    CHECK(!d.empty() && d.count() == 1);
    CHECK(d.text("assets/enemies/goblin.enemy") ==
          "assets/enemies/goblin.enemy:7: unknown field 'damgae' (did you mean 'damage'?)\n");
}

NNH_TEST(diagnostics_are_sorted_by_line_keeping_the_order_within_a_line) {
    Diagnostics d;
    d.add(9, "c");
    d.add(2, "a");
    d.add(9, "d");
    d.add(5, "b");
    CHECK(d.text("f") == "f:2: a\nf:5: b\nf:9: c\nf:9: d\n");
}

NNH_TEST(diagnostics_print_at_most_20_problems) {
    Diagnostics d;
    for (int i = 25; i >= 1; --i)
        d.add(i, "x");
    const std::string text = d.text("f");
    CHECK(text.find("f:20: x\n") != std::string::npos);
    CHECK(text.find("f:21: x") == std::string::npos);
    const std::string last = "f:20: x\nf: ... and 5 more\n";
    CHECK(text.size() >= last.size() && text.substr(text.size() - last.size()) == last);
    CHECK(d.count() == 25);
}

NNH_TEST(diagnostics_without_problems_print_nothing) {
    CHECK(Diagnostics{}.text("f").empty());
}
