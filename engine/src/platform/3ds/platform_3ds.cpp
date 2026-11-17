// Nintendo 3DS backend: libctru + citro3d/citro2d.

#include <3ds.h>
#include <citro2d.h>

#include "nnh/input.hpp"
#include "platform/platform.hpp"

namespace nnh::platform {
namespace {

constexpr float kDebugTextScale = 0.5f;
constexpr std::size_t kDebugTextGlyphs = 4096;

C3D_RenderTarget* g_targets[2] = {nullptr, nullptr};
C3D_RenderTarget* g_current_target = nullptr;
C2D_TextBuf g_text_buffer = nullptr;

u32 to_c2d(Color c) { return C2D_Color32(c.r, c.g, c.b, c.a); }

C3D_RenderTarget* target_for(Screen screen) { return g_targets[screen == Screen::Top ? 0 : 1]; }

void draw_on(C3D_RenderTarget* target) {
    if (g_current_target != target) {
        C2D_SceneBegin(target);
        g_current_target = target;
    }
}

struct KeyBinding {
    u32 key;
    Button button;
};

constexpr KeyBinding kKeyBindings[] = {
    {KEY_A, Button::A},         {KEY_B, Button::B},           {KEY_X, Button::X},
    {KEY_Y, Button::Y},         {KEY_L, Button::L},           {KEY_R, Button::R},
    {KEY_ZL, Button::ZL},       {KEY_ZR, Button::ZR},         {KEY_START, Button::Start},
    {KEY_SELECT, Button::Select}, {KEY_DUP, Button::Up},      {KEY_DDOWN, Button::Down},
    {KEY_DLEFT, Button::Left},  {KEY_DRIGHT, Button::Right},
};

} // namespace

bool init(const AppConfig& config) {
    (void)config; // title and icon come from the SMDH on 3DS
    gfxInitDefault();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxExit();
        return false;
    }
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        C3D_Fini();
        gfxExit();
        return false;
    }
    C2D_Prepare();
    g_targets[0] = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_targets[1] = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_text_buffer = C2D_TextBufNew(kDebugTextGlyphs);
    return true;
}

void shutdown() {
    C2D_TextBufDelete(g_text_buffer);
    g_text_buffer = nullptr;
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

bool poll_events() { return aptMainLoop(); }

std::uint32_t read_buttons() {
    hidScanInput();
    const u32 keys = hidKeysHeld();
    std::uint32_t mask = 0;
    for (const KeyBinding& binding : kKeyBindings) {
        if (keys & binding.key) mask |= static_cast<std::uint32_t>(binding.button);
    }
    return mask;
}

void begin_frame(Color top_clear, Color bottom_clear) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(g_text_buffer);
    C2D_TargetClear(g_targets[0], to_c2d(top_clear));
    C2D_TargetClear(g_targets[1], to_c2d(bottom_clear));
    // Begin a scene on both targets so each one is presented even if nothing is drawn on it.
    g_current_target = nullptr;
    draw_on(g_targets[1]);
    draw_on(g_targets[0]);
}

void end_frame() { C3D_FrameEnd(0); }

void debug_text(Screen screen, int x, int y, const char* text, Color color) {
    draw_on(target_for(screen));
    C2D_Text parsed;
    C2D_TextParse(&parsed, g_text_buffer, text);
    C2D_TextOptimize(&parsed);
    C2D_DrawText(&parsed, C2D_WithColor, static_cast<float>(x), static_cast<float>(y), 0.5f,
                 kDebugTextScale, kDebugTextScale, to_c2d(color));
}

int debug_line_height() {
    const FINF_s* info = fontGetInfo(nullptr);
    return static_cast<int>(info->lineFeed * kDebugTextScale + 0.5f);
}

} // namespace nnh::platform
