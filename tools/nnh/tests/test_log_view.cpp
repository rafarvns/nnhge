// Game log lines (log_view.hpp, spec 010 FR-5): parsing the engine's format, colouring, pass-through, and a
// mini-fuzz of the parser.

#include <cstdint>
#include <string>

#include "log_view.hpp"
#include "test.hpp"

using namespace nnh::tool;

NNH_TEST(log_lines_from_the_engine_are_parsed) {
    const auto l = parse_log_line("[f12       0.200s] W audio  a sound was dropped");
    CHECK(l && l->stamp == "f12       0.200s" && l->level == 'W' && l->channel == "audio");
    CHECK(l && l->message == "a sound was dropped");
    const auto first = parse_log_line("[f0        0.000s] I app    started \"Hello\" (Development build)");
    CHECK(first && first->level == 'I' && first->channel == "app" && first->message.find("started") == 0);
    const auto empty_message = parse_log_line("[f1        0.016s] E gfx    ");
    CHECK(empty_message && empty_message->message.empty() && empty_message->channel == "gfx");
}

NNH_TEST(other_text_is_not_a_log_line) {
    for (const char* text : {"", "hello", "[f12 0.2s] Q app x", "[f12 0.2s]", "[x] I app y", "[f1 0.0s] I", "  [f1 0.0s] I a b"}) {
        CHECK(!parse_log_line(text).has_value());
    }
}

NNH_TEST(rendering_colours_engine_lines_and_keeps_others) {
    const std::string error = "[f3        0.050s] E fs     save failed";
    const std::string plain = render_log_line(error, false);
    CHECK(plain == error);
    const std::string coloured = render_log_line(error, true);
    CHECK(coloured.find("\x1b[") != std::string::npos);      // escape codes...
    CHECK(coloured.find("save failed") != std::string::npos); // ...around the same text
    CHECK(coloured.find("fs") != std::string::npos);
    CHECK(render_log_line("crash: data abort", true) == "crash: data abort");
}

NNH_TEST(log_parser_survives_mutated_lines) {
    const std::string seeds[] = {"[f12       0.200s] W audio  a sound was dropped", "[f0 0.0s] I a b", "[]"};
    std::uint32_t state = 99;
    for (const std::string& seed : seeds) {
        for (int i = 0; i < 20000; ++i) {
            std::string s = seed;
            state = state * 1664525u + 1013904223u;
            const int edits = 1 + static_cast<int>(state % 4);
            for (int e = 0; e < edits; ++e) {
                state = state * 1664525u + 1013904223u;
                const std::size_t at = s.empty() ? 0 : state % s.size();
                switch ((state >> 8) % 3) {
                case 0: if (!s.empty()) s[at] = static_cast<char>(state >> 16); break;
                case 1: if (!s.empty()) s.erase(at, 1); break;
                default: s.insert(at, 1, static_cast<char>(state >> 16)); break;
                }
            }
            const auto parsed = parse_log_line(s);
            if (parsed) CHECK(parsed->channel.size() <= s.size() && parsed->message.size() <= s.size());
            (void)render_log_line(s, true);
        }
    }
}
