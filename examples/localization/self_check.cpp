#include "self_check.hpp"

#include <climits>
#include <nnh/gfx.hpp>
#include <nnh/localization.hpp>

namespace loc_check {

// Recorded from pc-debug; pc-release, the build VM's compilers and the console must agree.
const std::uint32_t kCheckpoints[kLanguages] = {0x92FEBB1Du, 0xA166BC22u, 0x1DE842A2u, 0x7E4FE058u, 0x21809981u};
const std::uint32_t kLocCheckGolden = 0x21809981u;

namespace {

constexpr std::uint32_t kPrime = 16777619u;

std::uint32_t fold_bytes(std::uint32_t h, const char* s) {
    for (; *s != 0; ++s) h = (h ^ static_cast<unsigned char>(*s)) * kPrime;
    return (h ^ 0xFFu) * kPrime; // the terminator: "ab"+"c" differs from "a"+"bc"
}

std::uint32_t fold(std::uint32_t h, std::int64_t v) {
    for (int i = 0; i < 8; ++i) h = (h ^ static_cast<std::uint8_t>(static_cast<std::uint64_t>(v) >> (8 * i))) * kPrime;
    return h;
}

const nnh::loc::Key kKeys[] = {"game.title",     "game.hint",       "menu.new_game",  "menu.continue", "menu.shop",
                               "menu.dialog",    "menu.fonts",      "menu.options",   "menu.quit",     "options.title",
                               "options.language", "options.system", "options.back",  "dialog.speaker", "dialog.text",
                               "dialog.hint",    "shop.title",      "shop.buy",       "gallery.title", "gallery.sample"};

constexpr long long kCounts[] = {0, 1, 2, 5, 21, 1000, -1, INT32_MAX};

std::uint32_t layout(std::uint32_t h, const char* text, const nnh::TextOptions& o) {
    const nnh::Vec2 box = nnh::gfx::measure_text(text, o);
    h = fold(h, static_cast<std::int64_t>(box.x));
    h = fold(h, static_cast<std::int64_t>(box.y));
    nnh::TextCaret carets[64]; // the first 64 code points: where each sits after wrapping and alignment
    const int n = nnh::gfx::measure_chars(text, o, carets, 64);
    h = fold(h, n);
    for (int i = 0; i < n; ++i) {
        h = fold(h, carets[i].byte);
        h = fold(h, (static_cast<std::int64_t>(carets[i].x) << 32) | (static_cast<std::uint16_t>(carets[i].y) << 16) |
                        static_cast<std::uint16_t>(carets[i].advance));
    }
    return h;
}

} // namespace

std::uint32_t language_sum(std::uint32_t h) {
    h = fold_bytes(h, nnh::loc::code());
    for (const nnh::loc::Key& k : kKeys) h = fold_bytes(h, nnh::loc::tr(k));
    char line[160];
    for (const long long n : kCounts) {
        nnh::loc::format(line, sizeof line, "shop.items", nnh::loc::arg("count", n));
        h = fold_bytes(h, line);
    }
    nnh::loc::format(line, sizeof line, "shop.price", nnh::loc::arg("gold", LLONG_MIN));
    h = fold_bytes(h, line);
    nnh::loc::format(line, sizeof line, "shop.greeting", nnh::loc::arg("name", "Ana"));
    h = fold_bytes(h, line);
    const nnh::FontRef body = nnh::loc::font("body"), title = nnh::loc::font("title"), hud = nnh::loc::font("hud");
    const char* text = nnh::loc::tr("dialog.text");
    h = layout(h, text, {.font = body, .wrap_width = 300});
    h = layout(h, text, {.font = body, .wrap_width = 200});
    h = layout(h, text, {.shadow = true, .font = body, .wrap_width = 300});
    h = layout(h, text, {.font = body, .wrap_width = 200, .max_lines = 3});
    h = layout(h, text, {.align = nnh::TextAlign::Center, .font = body, .wrap_width = 300});
    h = layout(h, text, {.align = nnh::TextAlign::Right, .font = body, .wrap_width = 300});
    h = layout(h, text, {.scale = 2, .font = body, .wrap_width = 300});
    h = layout(h, nnh::loc::tr("game.title"), {.font = title, .outline_color = nnh::rgb(0, 0, 0)});
    nnh::loc::format(line, sizeof line, "hud.coins", nnh::loc::arg("coins", 1234567));
    h = layout(h, line, {.scale = 2, .font = hud});
    for (const nnh::FontRef f : {body, title, hud}) {
        const nnh::FontMetrics m = nnh::gfx::font_metrics(f);
        h = fold(h, m.size);
        h = fold(h, m.line_height);
        h = fold(h, m.ascent);
        h = fold(h, m.descent);
        h = fold(h, m.glyphs);
        h = fold(h, m.pages);
    }
    return h;
}

} // namespace loc_check
