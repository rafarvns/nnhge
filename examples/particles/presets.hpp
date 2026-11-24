#pragma once
// Effect presets in text files (spec 024 FR-21, the spec's fallback pattern): the game declares its own small preset
// type in its data schema (data_types.cpp: plain values, times in frames) and turns it into an nnh::ParticleDesc.
// Editing assets/fx/*.particles rebuilds data only; ParticleEmitter::create() checks the result.

#include <cstdint>
#include <nnh/gfx.hpp>
#include <nnh/math.hpp>
#include <nnh/particles.hpp>

/// One effect preset (data assets `*.particles`).
struct FxPreset {
    float rate = 0;          ///< particles per second
    int shape = 0;           ///< 0 point, 1 box, 2 circle, 3 line
    float radius = 0;        ///< circle radius, box side, line length
    float angle = 0;         ///< radians, clockwise from +x
    float spread = 0;        ///< radians
    float speed_min = 0;     ///< px/s
    float speed_max = 0;
    nnh::Vec2 gravity{};     ///< px/s²
    float damping = 0;       ///< 1/s
    int life_min = 30;       ///< frames
    int life_max = 30;
    nnh::Color start{255, 255, 255, 255}; ///< colour at birth
    nnh::Color end{255, 255, 255, 0};     ///< colour at the end of the life
    float size_min = 1;
    float size_max = 1;
    float size_end = 1;      ///< size multiplier at the end of the life
    int frame = 0;           ///< the sheet's frame (8x8 cells of the top row: 0 spark, 1 smoke, 2 ember, 3 star)
    int seed = 1;
};

/// The description of a preset.
nnh::ParticleDesc to_desc(const FxPreset& p);
/// Loads `fx/<name>` (a data asset); false (and an error log) if it is missing or bad.
bool load_preset(const char* name, nnh::ParticleDesc& out);
