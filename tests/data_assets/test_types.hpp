#pragma once
// The tests' data schema (spec 014): one struct of each shape a game would write, declared in test_types.cpp
// with the NNH_DATA_* macros and so registered in nnh_tests' registry. Used by the registry, fingerprint and
// description tests (and later by the round trip through the real converter). Keep it stable:
// test_schema_write.cpp pins its text.

#include <cstdint>

#include "nnh/data.hpp"
#include "nnh/gfx.hpp"

namespace test_data {

enum class Element : std::uint8_t { Normal, Fire = 5, Ice };

struct Stats {
    std::uint16_t hp = 10;
    std::int8_t attack = 1;
};

struct Item {
    nnh::data::String<16> name;
    std::int32_t price = 10;
    nnh::data::TextureRef icon;
};

struct Drop {
    nnh::data::Ref<Item> item;
    float chance = 0.5f;
};

struct Enemy {
    nnh::data::String<24> name;
    Stats stats;
    float speed = 1.0f;
    Element element = Element::Normal;
    nnh::data::TextureRef sprite;
    nnh::data::SoundRef hurt;
    nnh::data::List<Drop, 4> drops;
    nnh::Color tint{255, 255, 255, 255};
    nnh::Vec2 spawn{8, 16};
    bool flags[3] = {false, true, false};
    std::uint8_t level = 1;
    nnh::data::MusicRef theme;
};

} // namespace test_data
