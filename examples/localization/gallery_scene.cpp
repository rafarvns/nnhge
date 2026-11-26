#include <nnh/font.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

#include "scenes.hpp"

using nnh::UiText;

namespace {

const char* const kRoleNames[] = {"body", "title", "hud"};

} // namespace

bool GalleryScene::create() {
    if (!make_ui({.widgets = 4, .text_bytes = 0, .styles = 1, .functions = 2})) return false;
    const nnh::UiStyleId title = ui_.add_style(title_style());
    ui_.label(ui_.root(nnh::Screen::Bottom), {.text = UiText::name("gallery.title"), .style = title,
                                              .layout = {.place = {.anchor = nnh::UiAnchor::Bottom, .y = -8}}});
    return true;
}

void GalleryScene::language_changed() {
    if (nnh::scenes::top() != this) return;
    for (const char* role : kRoleNames) {
        const nnh::FontMetrics m = nnh::gfx::font_metrics(nnh::loc::font(nnh::loc::key(role)));
        NNH_LOG_INFO(example, "loc: fonts %s size %d glyphs %d pages %d", role, m.size, m.glyphs, m.pages);
    }
}

void GalleryScene::step() {
    if (nnh::input::pressed(nnh::Button::B)) nnh::scenes::pop();
}

void GalleryScene::paint() const {
    float y = 30; // under the HUD
    const nnh::FontRef body = nnh::loc::font("body");
    for (const char* role : kRoleNames) {
        const nnh::FontRef font = nnh::loc::font(nnh::loc::key(role));
        const nnh::FontMetrics m = nnh::gfx::font_metrics(font);
        char metrics[96];
        nnh::loc::format(metrics, sizeof metrics, "gallery.metrics", nnh::loc::arg("size", m.size),
                         nnh::loc::arg("line", m.line_height), nnh::loc::arg("ascent", m.ascent));
        nnh::gfx::draw_text(nnh::Screen::Top, {8, y}, role, {.color = nnh::rgb(150, 170, 200)});
        nnh::gfx::draw_text(nnh::Screen::Top, {56, y - 4}, metrics, {.color = nnh::rgb(150, 170, 200), .font = body});
        y += 18;
        const char* sample = role[0] == 'h' ? "12,345 x67 1.000/9" : nnh::loc::tr("gallery.sample");
        // the title fonts carry a baked outline (title_32.font, title_ja_32.font: outline = 2)
        const nnh::Color outline = role[0] == 't' ? nnh::rgb(20, 20, 40) : nnh::Color{0, 0, 0, 0};
        nnh::gfx::draw_text(nnh::Screen::Top, {8, y}, sample,
                            {.font = font, .wrap_width = 384, .max_lines = 2, .outline_color = outline});
        y += static_cast<float>(m.line_height * 2 + 6);
    }
    if (shot_is("pages")) nnh::font::debug_draw_pages(nnh::Screen::Bottom, nnh::loc::font("body"), {0, 0});
}
