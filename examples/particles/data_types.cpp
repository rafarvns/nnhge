// The particles example's data schema (listed in DATA_SCHEMA; spec 014): declarations only, compiled into the game and
// into the build's schema program that checks assets/fx/*.particles with `file:line` messages.

#include "presets.hpp"

#include <nnh/data.hpp>

NNH_DATA_TYPE(FxPreset, "particles", NNH_DATA_FIELD(rate, .min = 0, .max = 491520), NNH_DATA_FIELD(shape, .min = 0, .max = 3),
              NNH_DATA_FIELD(radius, .min = 0), NNH_DATA_FIELD(angle), NNH_DATA_FIELD(spread, .min = 0, .max = 6.2832),
              NNH_DATA_FIELD(speed_min), NNH_DATA_FIELD(speed_max), NNH_DATA_FIELD(gravity),
              NNH_DATA_FIELD(damping, .min = 0, .max = 60), NNH_DATA_FIELD(life_min, .min = 1, .max = 3600),
              NNH_DATA_FIELD(life_max, .min = 1, .max = 3600), NNH_DATA_FIELD(start), NNH_DATA_FIELD(end),
              NNH_DATA_FIELD(size_min, .min = 0), NNH_DATA_FIELD(size_max, .min = 0), NNH_DATA_FIELD(size_end, .min = 0),
              NNH_DATA_FIELD(frame, .min = 0, .max = 3), NNH_DATA_FIELD(seed));
