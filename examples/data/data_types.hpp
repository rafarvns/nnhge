#pragma once
// The data example's types (spec 014): plain structs with default member initializers. Their schema — which
// fields data files may set, and the file extension of each type — is declared once in data_types.cpp, the
// file listed in DATA_SCHEMA (CMakeLists.txt). The values live in assets/enemies/*.enemy, assets/items/*.item
// and assets/game.settings.

#include <cstdint>
#include <nnh/data.hpp>
#include <nnh/gfx.hpp>

enum class Element : std::uint8_t { Normal, Fire, Ice, Earth };

/// Written as a [stats] section.
struct Stats {
    std::uint16_t hp = 10;
    std::uint8_t attack = 1;
    std::uint8_t defense = 0;
};

struct Item;

/// One [[drops]] block: an item and the chance (0..1) of getting it.
struct Drop {
    nnh::data::Ref<Item> item;
    float chance = 1.0f;
};

struct Enemy {
    nnh::data::String<16> name;
    Stats stats;
    float speed = 1.0f;
    Element element = Element::Normal;
    nnh::data::TextureRef sprite;
    nnh::data::SoundRef hurt_sound;
    nnh::data::List<Drop, 4> drops;
    nnh::Color tint{255, 255, 255, 255};
    nnh::Vec2 spawn{0, 0};
    bool flags[3] = {false, false, false}; ///< flying, boss, undead
};

struct Item {
    nnh::data::String<16> name;
    std::int32_t price = 10;
    nnh::data::TextureRef icon;
    std::int16_t heal = 0;
    nnh::data::Ref<Enemy> summons; ///< an enemy this item calls (the crystal: a reference cycle)
};

struct Settings {
    nnh::data::String<32> title;
    nnh::Color top_color{0, 0, 0, 255};
    nnh::Color bottom_color{0, 0, 0, 255};
    nnh::data::MusicRef music;
    nnh::data::Ref<Enemy> first; ///< the enemy selected at start
};
