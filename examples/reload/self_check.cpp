// The hot reload example's self-check fold (self_check.hpp).

#include "self_check.hpp"

#include <cstdio>
#include <cstring>

namespace {

constexpr std::uint32_t kOffset = 2166136261u;

std::uint32_t bits(float f) {
    std::uint32_t u = 0;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

std::uint32_t u(int v) { return static_cast<std::uint32_t>(v); }

} // namespace

std::uint32_t reload_fold(std::uint32_t hash, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        hash ^= (value >> (8 * i)) & 0xFFu;
        hash *= 16777619u;
    }
    return hash;
}

std::uint32_t reload_hash(const char* text) {
    std::uint32_t hash = kOffset;
    for (; *text != '\0'; ++text) {
        hash ^= static_cast<unsigned char>(*text);
        hash *= 16777619u;
    }
    return hash;
}

std::uint32_t reload_sum(const ReloadSnapshot& s) {
    const std::uint32_t values[kReloadSnapshotFields] = {
        u(s.backdrop_w), u(s.backdrop_h), u(s.hero_w),     u(s.hero_h),       u(s.goblin_hp),  bits(s.goblin_speed),
        u(s.bat_hp),     bits(s.bat_speed), s.map_tiles,   u(s.platform_x),   u(s.platform_y), s.text_hash,
        u(s.text_width), u(s.sound_ms),   u(s.gem_vertices), u(s.reloads)};
    std::uint32_t hash = kOffset;
    for (std::uint32_t v : values) hash = reload_fold(hash, v);
    return hash;
}

void nudge(ReloadSnapshot& s, int field) {
    int* ints[] = {&s.backdrop_w, &s.backdrop_h, &s.hero_w, &s.hero_h, &s.goblin_hp};
    if (field < 5) ++*ints[field];
    else if (field == 5) s.goblin_speed += 0.5f;
    else if (field == 6) ++s.bat_hp;
    else if (field == 7) s.bat_speed += 0.5f;
    else if (field == 8) ++s.map_tiles;
    else if (field == 9) ++s.platform_x;
    else if (field == 10) ++s.platform_y;
    else if (field == 11) ++s.text_hash;
    else if (field == 12) ++s.text_width;
    else if (field == 13) ++s.sound_ms;
    else if (field == 14) ++s.gem_vertices;
    else ++s.reloads;
}

void format_reload_line(char* out, std::size_t capacity, std::uint32_t sum, int reloads) {
    std::snprintf(out, capacity, "reload: sum %08X after %d", static_cast<unsigned>(sum), reloads);
}
