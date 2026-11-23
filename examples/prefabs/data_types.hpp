#pragma once
// The prefabs example's data type (spec 014 data assets; spec 021 FR-10): an enemy's stats, one file per enemy in
// assets/enemies/*.enemy. roster.cpp registers one prefab per file, named by the file's name, at load time.

#include <cstdint>
#include <nnh/data.hpp>
#include <nnh/gfx.hpp>

struct EnemyData {
    nnh::data::String<16> name;
    std::uint16_t hp = 1;
    float speed = 0.5f;  ///< pixels per frame
    std::uint8_t size = 12; ///< its square body's side, pixels
    nnh::Color color{255, 255, 255, 255};
};
