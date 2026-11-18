// Remembered simulator window settings (engine/platforms/pc/src/sim_settings.hpp, spec 006 FR-6).
// The file comes from the user's disk: parsing must be bounded and must never trust it.

#include <cstring>

#include "sim_settings.hpp"
#include "test.hpp"

using nnh::platform::pc::format_settings;
using nnh::platform::pc::parse_settings;
using nnh::platform::pc::SimLayout;
using nnh::platform::pc::SimSettings;

namespace {

bool parse(const char* text, SimSettings& out) { return parse_settings(text, std::strlen(text), out); }

} // namespace

NNH_TEST(settings_round_trip) {
    SimSettings s;
    s.layout = SimLayout::LargeTop;
    s.scale = 4;
    s.fit = true;
    s.fullscreen = true;
    s.has_window = true;
    s.x = -20;
    s.y = 30;
    s.w = 1280;
    s.h = 720;
    char text[256];
    const int length = format_settings(s, text, sizeof text);
    CHECK(length > 0);
    SimSettings back;
    CHECK(parse_settings(text, static_cast<std::size_t>(length), back));
    CHECK(back.layout == SimLayout::LargeTop && back.scale == 4 && back.fit && back.fullscreen);
    CHECK(back.has_window && back.x == -20 && back.y == 30 && back.w == 1280 && back.h == 720);
}

NNH_TEST(settings_keep_defaults_for_missing_or_unknown_keys) {
    SimSettings s;
    CHECK(parse("layout=top\ncolor=blue\n", s));
    CHECK(s.layout == SimLayout::TopOnly && s.scale == SimSettings{}.scale && !s.fit && !s.has_window);
}

NNH_TEST(settings_ignore_invalid_values) {
    SimSettings s;
    CHECK(parse("layout=sideways\nscale=99\nfit=maybe\nwindow=1,2,3\n", s));
    CHECK(s.layout == SimLayout::Stacked && s.scale == SimSettings{}.scale && !s.fit && !s.has_window);
    CHECK(parse("scale=0\nwindow=0,0,50,50\n", s)); // scale 1..6, window at least 100x100
    CHECK(s.scale == SimSettings{}.scale && !s.has_window);
}

NNH_TEST(settings_accept_windows_line_endings_and_spaces) {
    SimSettings s;
    CHECK(parse("layout = side\r\nscale= 3 \r\n", s));
    CHECK(s.layout == SimLayout::SideBySide && s.scale == 3);
}

NNH_TEST(settings_refuse_oversized_or_missing_text) {
    static char big[2048];
    std::memset(big, 'a', sizeof big);
    SimSettings s;
    CHECK(!parse_settings(big, sizeof big, s));
    CHECK(!parse_settings(nullptr, 10, s));
}

NNH_TEST(settings_text_without_a_terminator_is_read_safely) {
    const char text[] = {'s', 'c', 'a', 'l', 'e', '=', '5'}; // no '\0', no newline
    SimSettings s;
    CHECK(parse_settings(text, sizeof text, s));
    CHECK(s.scale == 5);
}

NNH_TEST(settings_format_refuses_a_small_buffer) {
    char tiny[8];
    CHECK(format_settings(SimSettings{}, tiny, sizeof tiny) < 0);
}
