// The schema source of the prefabs example (listed in DATA_SCHEMA): declarations only (spec 014).

#include "data_types.hpp"

NNH_DATA_TYPE(EnemyData, "enemy", NNH_DATA_FIELD(name, .required = true), NNH_DATA_FIELD(hp, .min = 1, .max = 999),
              NNH_DATA_FIELD(speed, .min = 0, .max = 4), NNH_DATA_FIELD(size, .min = 4, .max = 24),
              NNH_DATA_FIELD(color));
