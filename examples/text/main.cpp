// text: the built-in NNHGE font — the same pixels on every system (spec 005).
// The whole character set, Portuguese with accents, alignment, scale, shadow and a box sized by
// measuring the text. START exits.
//
// Screenshot (Debug/Development): NNH_EXIT_AFTER_FRAMES=10 NNH_SCREENSHOT=build/text.png \
//   build/pc-debug/examples/text/text.exe

#include <nnh/app.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace {

using nnh::Screen;
using nnh::TextAlign;

/// Appends the UTF-8 encoding of a Latin-1 code point (below 256) to `out`.
char* append_utf8(char* out, unsigned code) {
    if (code < 0x80) {
        *out++ = static_cast<char>(code);
    } else {
        *out++ = static_cast<char>(0xC0 | (code >> 6));
        *out++ = static_cast<char>(0x80 | (code & 0x3F));
    }
    return out;
}

/// The character set in lines of 48: ASCII 32-126, then Latin-1 161-255.
void build_charset(char* out) {
    int column = 0;
    for (unsigned code = 32; code <= 255; ++code) {
        if (code > 126 && code < 161) continue;
        out = append_utf8(out, code);
        if (++column == 48) {
            *out++ = '\n';
            column = 0;
        }
    }
    *out = '\0';
}

void draw_top(const char* charset) {
    nnh::gfx::draw_text(Screen::Top, {200, 6}, "NNHGE built-in font",
                        {.color = nnh::rgb(255, 210, 80), .scale = 2, .align = TextAlign::Center, .shadow = true});
    nnh::gfx::draw_text(Screen::Top, {8, 34}, charset);
    nnh::gfx::draw_text(Screen::Top, {8, 100}, "Olá! Coração, ação, pão, avô, você, açúcar.\n"
                                              "ÁGUA, ÇÃO, ÊXITO, ÍNDIO, ÓCULOS, ÚNICO.\n"
                                              "¿Qué? ¡Sí! Ñandú. Ça va? Größe: 25°C ±1.",
                        {.color = nnh::rgb(160, 220, 255)});
    nnh::gfx::draw_text(Screen::Top, {8, 150}, "1x", {.scale = 1});
    nnh::gfx::draw_text(Screen::Top, {40, 150}, "2x", {.scale = 2});
    nnh::gfx::draw_text(Screen::Top, {90, 150}, "3x", {.scale = 3});
    nnh::gfx::draw_text(Screen::Top, {8, 190}, "Shadow over a busy background",
                        {.color = nnh::colors::white, .shadow = true, .layer = 1});
    for (int i = 0; i < 12; ++i) { // stripes behind the shadowed text
        nnh::gfx::fill_rect(Screen::Top, {static_cast<float>(i * 34), 184, 17, 20}, nnh::rgb(90, 140, 90));
    }
}

void draw_bottom() {
    const float x = 160;
    nnh::gfx::draw_line(Screen::Bottom, {x, 8}, {x, 70}, nnh::rgb(90, 90, 110));
    nnh::gfx::draw_text(Screen::Bottom, {x, 12}, "Left", {.align = TextAlign::Left});
    nnh::gfx::draw_text(Screen::Bottom, {x, 30}, "Center", {.align = TextAlign::Center});
    nnh::gfx::draw_text(Screen::Bottom, {x, 48}, "Right", {.align = TextAlign::Right});

    // A box sized by measuring the text: the same layout on every system.
    const char* label = "Measured box: fits on every system";
    const nnh::Vec2 size = nnh::gfx::measure_text(label);
    const nnh::Rect box{(320 - size.x) / 2 - 6, 100, size.x + 12, size.y + 8};
    nnh::gfx::fill_rect(Screen::Bottom, box, nnh::rgb(40, 60, 110));
    nnh::gfx::draw_rect(Screen::Bottom, box, nnh::colors::white);
    nnh::gfx::draw_text(Screen::Bottom, {box.x + 6, box.y + 5}, label, {.layer = 1});

    nnh::gfx::draw_text(Screen::Bottom, {160, 220}, "START: exit", {.color = nnh::colors::gray, .align = TextAlign::Center});
}

} // namespace

int main() {
    if (!nnh::init({.title = "NNHGE Text"})) return 1;
    nnh::gfx::set_clear_color(Screen::Top, nnh::rgb(24, 28, 44));
    nnh::gfx::set_clear_color(Screen::Bottom, nnh::rgb(24, 24, 32));
    static char charset[512];
    build_charset(charset);
    while (nnh::frame()) {
        if (nnh::input::pressed(nnh::Button::Start)) break;
        draw_top(charset);
        draw_bottom();
    }
    nnh::shutdown();
    return 0;
}
