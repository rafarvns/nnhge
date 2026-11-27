// The hot reload example's assets (world.hpp).

#include "world.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <nnh/assets.hpp>
#include <nnh/gfx.hpp>
#include <nnh/localization.hpp>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL(example);

bool demo_is(const char* name) {
    const char* demo = std::getenv("RELOAD_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

namespace {

template <typename T>
bool take(nnh::Result<T> r, T& into, const char* what) {
    if (!r) {
        NNH_LOG_ERROR(example, "reload example: %s: %s", what, r.error().message);
        return false;
    }
    into = std::move(r.value());
    return true;
}

bool load_room(World& w) {
    if (!take(nnh::load_tilemap("levels/room"), w.room, "levels/room")) return false;
    w.platform = w.room.find_object("platform");
    return true;
}

bool load_walker(World& w) {
    if (!take(nnh::load_sprite_library("art/walker"), w.walker, "art/walker")) return false;
    w.walking = nnh::SpriteAnimator{w.walker};
    w.walking.play("walk");
    return true;
}

bool load_enemies(World& w) {
    return nnh::data::load("enemies/goblin", w.goblin).ok() && nnh::data::load("enemies/bat", w.bat).ok();
}

} // namespace

bool World::load() {
    return take(nnh::load_texture("art/backdrop"), backdrop, "art/backdrop") &&
           take(nnh::load_texture("art/hero"), hero, "art/hero") && load_room(*this) && load_enemies(*this) &&
           take(nnh::load_sound("sfx/blip"), blip, "sfx/blip") && take(nnh::load_mesh("models/gem"), gem, "models/gem") &&
           take(nnh::load_texture("art/walker"), walker_art, "art/walker") && load_walker(*this) &&
           take(nnh::load_font("fonts/hud"), hud, "fonts/hud");
}

bool World::refresh() {
    last[0] = '\0';
    const nnh::assets::ReloadedPaths paths = nnh::assets::reloaded_paths();
    for (int i = 0; i < paths.count; ++i) {
        if (paths.paths[i][0] == '.') continue; // index files
        const std::size_t used = std::strlen(last);
        std::snprintf(last + used, sizeof last - used, "%s%s", used > 0 ? " " : "", paths.paths[i]);
    }
    bool ok = true;
    if (nnh::assets::reloaded("levels/room")) ok = load_room(*this) && ok;
    if (nnh::assets::reloaded("enemies/goblin") || nnh::assets::reloaded("enemies/bat")) ok = load_enemies(*this) && ok;
    return ok;
}

ReloadSnapshot World::snapshot() const {
    ReloadSnapshot s;
    s.backdrop_w = backdrop.width();
    s.backdrop_h = backdrop.height();
    s.hero_w = hero.width();
    s.hero_h = hero.height();
    s.goblin_hp = goblin.hp;
    s.goblin_speed = goblin.speed;
    s.bat_hp = bat.hp;
    s.bat_speed = bat.speed;
    std::uint32_t tiles = reload_hash("");
    for (int y = 0; y < room.height(); ++y) {
        for (int x = 0; x < room.width(); ++x) tiles = reload_fold(tiles, room.tile(0, x, y).id);
    }
    s.map_tiles = tiles;
    if (platform >= 0) {
        const nnh::MapObject o = room.object(platform);
        s.platform_x = static_cast<int>(o.bounds.x);
        s.platform_y = static_cast<int>(o.bounds.y);
    }
    const char* hello = nnh::loc::tr(nnh::loc::key("demo.hello"));
    s.text_hash = reload_hash(hello);
    s.text_width = static_cast<int>(nnh::gfx::measure_text(hello).x);
    s.sound_ms = static_cast<int>(blip.duration() * 1000.0f + 0.5f);
    s.gem_vertices = gem.vertex_count();
    s.reloads = static_cast<int>(nnh::assets::reload_count());
    s.walker_clips = walker.clip_count();
    s.walker_clip = reload_hash(walking.clip_name());
    s.walker_position = walking.position();
    s.font_width = static_cast<int>(nnh::gfx::measure_text("12 34", {.font = hud}).x);
    s.music_playing = nnh::music::playing() ? 1 : 0;
    return s;
}
