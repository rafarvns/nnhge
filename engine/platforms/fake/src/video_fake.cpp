// Fake platform: frames and drawing are recorded, nothing is shown (contract nnh/platform/video.hpp).

#include <cstring>

#include "fake_internal.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {

using platform_fake::detail::mutable_state;

void begin_frame(Color top_clear, Color bottom_clear) {
    platform_fake::State& s = mutable_state();
    ++s.frames_begun;
    s.frame_open = true;
    s.top_clear = top_clear;
    s.bottom_clear = bottom_clear;
    s.text_count = 0;
}

void end_frame() {
    ++mutable_state().frames_ended;
    mutable_state().frame_open = false;
}

void debug_text(Screen screen, int x, int y, const char* text, Color color) {
    platform_fake::State& s = mutable_state();
    if (s.text_count == platform_fake::kMaxRecordedTexts) return;
    platform_fake::DrawnText& out = s.texts[s.text_count++];
    out.screen = screen;
    out.x = x;
    out.y = y;
    out.color = color;
    std::strncpy(out.text, text, sizeof out.text - 1);
    out.text[sizeof out.text - 1] = '\0';
}

int debug_line_height() { return 10; }

} // namespace nnh::platform
