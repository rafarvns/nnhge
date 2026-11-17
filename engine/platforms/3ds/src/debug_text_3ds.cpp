// Nintendo 3DS: debug text with the system font (citro2d).

#include <3ds.h>
#include <citro2d.h>

#include "context_3ds.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {
namespace {

constexpr float kDebugTextScale = 0.5f;

} // namespace

void debug_text(Screen screen, int x, int y, const char* text, Color color) {
    ctr::draw_on(screen);
    C2D_Text parsed;
    C2D_TextParse(&parsed, ctr::context().text_buffer, text);
    C2D_TextOptimize(&parsed);
    C2D_DrawText(&parsed, C2D_WithColor, static_cast<float>(x), static_cast<float>(y), 0.5f,
                 kDebugTextScale, kDebugTextScale, ctr::to_c2d(color));
}

int debug_line_height() {
    const FINF_s* info = fontGetInfo(nullptr);
    return static_cast<int>(info->lineFeed * kDebugTextScale + 0.5f);
}

} // namespace nnh::platform
