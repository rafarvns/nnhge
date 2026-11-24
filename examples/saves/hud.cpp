// The top screen's text of the saves example (hud.hpp).

#include "hud.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>

namespace saves_example {

void format_report(char* out, std::size_t capacity, const nnh::save::Report& r) {
    std::snprintf(out, capacity, "v%lu -> v%lu  steps %d%s\nunknown %d  dropped %d  adjusted %d",
                  static_cast<unsigned long>(r.version), static_cast<unsigned long>(kProgressVersion),
                  r.migrated, r.used_backup ? "  backup used" : "", r.unknown, r.dropped, r.adjusted);
}

void draw_hud(const Progress& p, int volume, const char* language, const char* last_report) {
    namespace gfx = nnh::gfx;
    const nnh::Screen top = nnh::Screen::Top;
    const nnh::Color text = nnh::rgb(220, 228, 240);
    const unsigned long seconds = static_cast<unsigned long>(p.play_frames / 60);
    char line[96];
    std::snprintf(line, sizeof line, "%s  level %u  gems %lu  time %lu:%02lu", p.name.c_str(),
                  static_cast<unsigned>(p.level), static_cast<unsigned long>(p.gems), seconds / 60,
                  seconds % 60);
    gfx::draw_text(top, {6, 6}, line, {.color = text});
    std::snprintf(line, sizeof line, "room %u  tile %u  items %d  volume %d  %s",
                  static_cast<unsigned>(p.room), static_cast<unsigned>(p.tile), p.inventory.size(), volume,
                  language);
    gfx::draw_text(top, {6, 18}, line, {.color = text});
    gfx::draw_text(top, {6, 200}, "last load:", {.color = nnh::rgb(150, 160, 190)});
    gfx::draw_text(top, {6, 212}, last_report[0] != '\0' ? last_report : "none yet",
                   {.color = nnh::colors::yellow});
}

} // namespace saves_example
