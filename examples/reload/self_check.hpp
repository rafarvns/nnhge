#pragma once
// The hot reload example's self-check (spec 031 AC-1, AC-2): what the scene sees after the start and after each batch —
// texture sizes, the enemies' data, the room's tiles and platform, a translated text and its width, the sound's length,
// the gem's vertex count, reload_count(), the walker's library and animator, a width in the HUD font, the music — folded with FNV-1a field by field (no padding bytes, floats by their
// bits). Pure: also built into nnh_tests (tests/reload_example/test_self_check.cpp).

#include <cstddef>
#include <cstdint>

struct ReloadSnapshot {
    int backdrop_w = 0, backdrop_h = 0;
    int hero_w = 0, hero_h = 0;
    int goblin_hp = 0;
    float goblin_speed = 0;
    int bat_hp = 0;
    float bat_speed = 0;
    std::uint32_t map_tiles = 0; ///< FNV-1a of the ground layer's tile ids
    int platform_x = 0, platform_y = 0;
    std::uint32_t text_hash = 0; ///< FNV-1a of tr("demo.hello")
    int text_width = 0;          ///< its measured width in pixels
    int sound_ms = 0;
    int gem_vertices = 0;
    int reloads = 0;
    int walker_clips = 0;           ///< the sprite library's clip count
    std::uint32_t walker_clip = 0;  ///< FNV-1a of the animator's clip name
    int walker_position = 0;        ///< its frame position
    int font_width = 0;             ///< "12 34" measured in the HUD font
    int music_playing = 0;
};

inline constexpr int kReloadSnapshotFields = 21;

/// FNV-1a over the snapshot's fields in declaration order.
std::uint32_t reload_sum(const ReloadSnapshot& s);
/// FNV-1a of a NUL-terminated string (also the map's tile fold, byte by byte).
std::uint32_t reload_hash(const char* text);
std::uint32_t reload_fold(std::uint32_t hash, std::uint32_t value);
/// Changes field `field` (0..kReloadSnapshotFields-1) of `s` (tests).
void nudge(ReloadSnapshot& s, int field);
/// "reload: sum XXXXXXXX after N".
void format_reload_line(char* out, std::size_t capacity, std::uint32_t sum, int reloads);
