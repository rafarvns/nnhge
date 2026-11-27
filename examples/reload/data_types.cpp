// The schema source of the hot reload example (listed in DATA_SCHEMA): declarations only.

#include "data_types.hpp"

NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(name, .required = true), NNH_DATA_FIELD(hp, .min = 1, .max = 999),
              NNH_DATA_FIELD(speed, .min = 0, .max = 10), NNH_DATA_FIELD(tint));
