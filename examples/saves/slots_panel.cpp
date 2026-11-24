// The bottom screen of the saves example (slots_panel.hpp).

#include "slots_panel.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>

namespace saves_example {
namespace {

constexpr int kRowTop[3] = {8, 72, 136};
constexpr int kRowHeight = 56;
constexpr int kColumnX[4] = {8, 88, 168, 248};
constexpr int kColumnW[4] = {72, 72, 72, 64};
constexpr const char* kColumnNames[4] = {"Save", "Load", "Delete", "Damage"};

struct Button {
    int x, w;
    PanelAction action;
    const char* name;
};
constexpr Button kBottom[5] = {{8, 40, PanelAction::VolumeDown, "Vol-"},
                               {56, 40, PanelAction::VolumeUp, "Vol+"},
                               {104, 72, PanelAction::Language, "Language"},
                               {184, 72, PanelAction::GoldenV1, "Golden v1"},
                               {264, 48, PanelAction::Rename, "Rename"}};
constexpr int kBottomTop = 200;
constexpr int kBottomHeight = 32;

const nnh::Color kButton = nnh::rgb(48, 60, 92);
const nnh::Color kText = nnh::rgb(220, 228, 240);

void button(float x, float y, float w, float h, const char* name) {
    nnh::gfx::fill_rect(nnh::Screen::Bottom, {x, y, w, h}, kButton);
    nnh::gfx::draw_text(nnh::Screen::Bottom, {x + w / 2, y + h / 2 - 4}, name,
                        {.color = kText, .align = nnh::TextAlign::Center});
}

} // namespace

PanelHit panel_hit(int x, int y) {
    for (int row = 0; row < 3; ++row) {
        if (y < kRowTop[row] || y >= kRowTop[row] + kRowHeight) continue;
        for (int c = 0; c < 4; ++c) {
            if (x >= kColumnX[c] && x < kColumnX[c] + kColumnW[c]) {
                return {static_cast<PanelAction>(static_cast<int>(PanelAction::Save) + c), row};
            }
        }
    }
    if (y >= kBottomTop && y < kBottomTop + kBottomHeight) {
        for (const Button& b : kBottom) {
            if (x >= b.x && x < b.x + b.w) return {b.action, 0};
        }
    }
    return {};
}

void format_slot_short(char* out, std::size_t capacity, const SlotView& slot) {
    if (!slot.used) {
        std::snprintf(out, capacity, "empty");
    } else {
        std::snprintf(out, capacity, "L%u", static_cast<unsigned>(slot.summary.level));
    }
}

void draw_panel(const SlotView (&slots)[3], int selected) {
    namespace gfx = nnh::gfx;
    const nnh::Screen bottom = nnh::Screen::Bottom;
    for (int row = 0; row < 3; ++row) {
        const float top = static_cast<float>(kRowTop[row]);
        gfx::draw_rect(bottom, {4, top - 2, 312, kRowHeight + 2.0f},
                       row == selected ? nnh::colors::yellow : nnh::rgb(70, 80, 110));
        char line[80];
        const SlotView& s = slots[row];
        if (!s.used) {
            std::snprintf(line, sizeof line, "Slot %d: empty", row + 1);
        } else {
            const unsigned long seconds = static_cast<unsigned long>(s.summary.play_frames / 60);
            std::snprintf(line, sizeof line, "Slot %d: %s  L%u  gems %lu  %lu:%02lu%s", row + 1,
                          s.summary.name.c_str(), static_cast<unsigned>(s.summary.level),
                          static_cast<unsigned long>(s.summary.gems), seconds / 60, seconds % 60,
                          s.from_backup ? "  (backup)" : "");
        }
        gfx::draw_text(bottom, {10, top + 4}, line, {.color = kText});
        for (int c = 0; c < 4; ++c) {
            button(static_cast<float>(kColumnX[c]) + 2, top + 22, static_cast<float>(kColumnW[c]) - 4, 26,
                   kColumnNames[c]);
        }
    }
    for (const Button& b : kBottom) {
        button(static_cast<float>(b.x), kBottomTop, static_cast<float>(b.w), kBottomHeight, b.name);
    }
}

} // namespace saves_example
