#pragma once
// The hot reload example's data type (spec 014/031): one plain struct, its schema in data_types.cpp (DATA_SCHEMA).

#include <cstdint>
#include <nnh/data.hpp>
#include <nnh/gfx.hpp>

struct Enemy {
    nnh::data::String<16> name;
    std::int32_t hp = 1;
    float speed = 1.0f;
    nnh::Color tint{255, 255, 255, 255};
};
