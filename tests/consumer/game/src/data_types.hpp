#pragma once
// The consumer game's data type (spec 032 FR-16): declared once in data_types.cpp (DATA_SCHEMA), values in
// assets/things/*.thing, checked and converted by the build from this nested folder of a game project.

#include <cstdint>
#include <nnh/data.hpp>

struct Thing {
    nnh::data::String<16> name;
    std::int32_t value = 0;
    nnh::data::TextureRef sprite;
};
