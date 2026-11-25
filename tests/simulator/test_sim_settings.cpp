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

// ---- the 3D slider and the eye view (spec 026 FR-16/FR-22, T11.2) ----------------------------------------------

using nnh::platform::pc::SimEye;

NNH_TEST(settings_remember_the_slider_and_the_eye_view) {
    SimSettings s;
    s.slider = 35;
    s.eye = SimEye::Anaglyph;
    char text[256];
    const int length = format_settings(s, text, sizeof text);
    CHECK(length > 0 && std::strstr(text, "slider=0.35\n") != nullptr && std::strstr(text, "eye=anaglyph\n") != nullptr);
    SimSettings back;
    CHECK(parse_settings(text, static_cast<std::size_t>(length), back));
    CHECK(back.slider == 35 && back.eye == SimEye::Anaglyph);
    CHECK(parse("slider=2\neye=middle\n", back)); // invalid: kept
    CHECK(back.slider == 35 && back.eye == SimEye::Anaglyph);
}

NNH_TEST(slider_text_is_0_to_1_with_at_most_two_decimals) {
    using nnh::platform::pc::parse_slider;
    int h = -1;
    CHECK(parse_slider("0", h) && h == 0);
    CHECK(parse_slider("1", h) && h == 100);
    CHECK(parse_slider("0.5", h) && h == 50);
    CHECK(parse_slider("0.05", h) && h == 5);
    CHECK(parse_slider("1.00", h) && h == 100);
    h = 7;
    CHECK(!parse_slider("1.5", h) && !parse_slider("-0.1", h) && !parse_slider("0.123", h));
    CHECK(!parse_slider("", h) && !parse_slider(".5", h) && !parse_slider("0.", h) && !parse_slider("x", h));
    CHECK(!parse_slider(nullptr, h) && h == 7);
}

NNH_TEST(slider_steps_stay_in_range) {
    using nnh::platform::pc::step_slider;
    CHECK(step_slider(35, 10) == 45 && step_slider(35, -1) == 34);
    CHECK(step_slider(95, 10) == 100 && step_slider(4, -10) == 0);
}

NNH_TEST(eye_views_cycle_and_have_names) {
    using nnh::platform::pc::eye_id;
    using nnh::platform::pc::next_eye;
    using nnh::platform::pc::parse_eye;
    CHECK(next_eye(SimEye::Left) == SimEye::Right && next_eye(SimEye::Right) == SimEye::Anaglyph &&
          next_eye(SimEye::Anaglyph) == SimEye::Left);
    SimEye e = SimEye::Left;
    CHECK(parse_eye("right", e) && e == SimEye::Right && std::strcmp(eye_id(e), "right") == 0);
    CHECK(!parse_eye("both", e) && e == SimEye::Right && !parse_eye(nullptr, e));
}
