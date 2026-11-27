#include "self_check.hpp"

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <nnh/animation.hpp>
#include <nnh/audio.hpp>
#include <nnh/data.hpp>
#include <nnh/font.hpp>
#include <nnh/localization.hpp>
#include <nnh/mesh.hpp>
#include <nnh/texture.hpp>
#include <nnh/tilemap.hpp>

#include "data_types.hpp"

namespace {

/// A CRC-32 (zlib's) of values written one by one, little-endian, whatever the system.
class Sum {
public:
    void byte(std::uint8_t b) {
        crc_ ^= b;
        for (int k = 0; k < 8; ++k) crc_ = (crc_ >> 1) ^ (0xEDB88320u & (0u - (crc_ & 1u)));
    }
    void u32(std::uint32_t v) {
        for (int k = 0; k < 4; ++k) byte(static_cast<std::uint8_t>(v >> (8 * k)));
    }
    void i32(int v) { u32(static_cast<std::uint32_t>(v)); }
    void text(const char* s) {
        const std::size_t n = std::strlen(s);
        u32(static_cast<std::uint32_t>(n));
        for (std::size_t i = 0; i < n; ++i) byte(static_cast<std::uint8_t>(s[i]));
    }
    std::uint32_t value() const { return ~crc_; }

private:
    std::uint32_t crc_ = 0xFFFFFFFFu;
};

/// An image from ASSETS (art/hero.png) and one embedded with IMAGES (images/logo.png): their sizes.
const char* check_textures(Sum& sum) {
    auto hero = nnh::load_texture("art/hero");
    if (!hero) return "art/hero doesn't load";
    auto logo = nnh::load_texture("logo");
    if (!logo) return "the IMAGES logo doesn't load";
    for (const nnh::Texture* t : {&hero.value(), &logo.value()}) {
        sum.i32(t->width());
        sum.i32(t->height());
    }
    return "";
}

/// A Tiled map (levels/good.tmj with its tileset): its size and a map property.
const char* check_map(Sum& sum) {
    auto map = nnh::load_tilemap("levels/good");
    if (!map) return "levels/good doesn't load";
    sum.i32(map.value().width());
    sum.i32(map.value().height());
    sum.i32(map.value().properties().get_int("lives", -1));
    return "";
}

/// An Aseprite export (sprites/good.ase.json): its frames and clips.
const char* check_sprites(Sum& sum) {
    auto sprites = nnh::load_sprite_library("sprites/good");
    if (!sprites) return "sprites/good doesn't load";
    sum.i32(sprites.value().frame_count());
    sum.i32(sprites.value().clip_count());
    return "";
}

/// A data asset of the game's own schema (things/lamp.thing), its texture reference followed.
const char* check_data(Sum& sum) {
    Thing lamp;
    if (!nnh::data::load("things/lamp", lamp)) return "things/lamp doesn't load";
    if (nnh::data::count<Thing>() != 1) return "wrong number of things";
    sum.text(lamp.name.c_str());
    sum.i32(lamp.value);
    sum.text(nnh::data::path(lamp.sprite));
    return "";
}

/// A font baked from a .font description over a TTF (fonts/body_16): its metrics and one advance.
const char* check_font(Sum& sum) {
    auto font = nnh::load_font("fonts/body_16");
    if (!font) return "fonts/body_16 doesn't load";
    const nnh::FontMetrics m = font.value().metrics();
    sum.i32(m.line_height);
    sum.i32(m.ascent);
    sum.i32(font.value().advance(U'A'));
    return "";
}

/// Two languages (LANGUAGES en pt-BR BASE en): their names, and the base language's text of one key.
const char* check_strings(Sum& sum) {
    if (!nnh::loc::start({.language = "en"})) return "the strings don't load";
    // Debug/Development builds also ship the pseudo-language qps (spec 029): look the two up by code.
    for (const char* code : {"en", "pt-BR"}) {
        const int i = nnh::loc::find(code);
        if (i < 0) {
            nnh::loc::stop();
            return "a language is missing";
        }
        sum.text(nnh::loc::language_code(i));
        sum.text(nnh::loc::language_name(i));
    }
    sum.text(nnh::loc::tr("game.greeting"));
    nnh::loc::stop();
    return "";
}

/// A glTF model (models/vertex_colors.gltf + .bin): its vertices (3D is on: AppConfig::gfx.max_draws_3d).
const char* check_mesh(Sum& sum) {
    auto mesh = nnh::load_mesh("models/vertex_colors");
    if (!mesh) return "models/vertex_colors doesn't load";
    sum.i32(mesh.value().vertex_count());
    return "";
}

/// A sound (sfx/blip.wav): its length in whole milliseconds.
const char* check_sound(Sum& sum) {
    auto blip = nnh::load_sound("sfx/blip");
    if (!blip) return "sfx/blip doesn't load";
    sum.i32(static_cast<int>(blip.value().duration() * 1000.0f + 0.5f));
    return "";
}

} // namespace

ConsumerCheck run_consumer_check() {
    ConsumerCheck r;
    Sum sum;
    const char* failed = "";
    for (const char* (*step)(Sum&) :
         {check_textures, check_map, check_sprites, check_data, check_font, check_strings, check_mesh, check_sound}) {
        if (failed[0] == '\0') failed = step(sum);
    }
    std::snprintf(r.failure, sizeof r.failure, "%s", failed);
    r.pass = failed[0] == '\0';
    r.checksum = sum.value();
    return r;
}

void format_consumer_check(const ConsumerCheck& r, char* out, std::size_t size) {
    if (r.pass) {
        std::snprintf(out, size, "consumer: PASS %08X", static_cast<unsigned>(r.checksum));
    } else {
        std::snprintf(out, size, "consumer: FAIL %s", r.failure);
    }
}
