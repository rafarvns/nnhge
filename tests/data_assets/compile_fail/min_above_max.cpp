// Must not compile (spec 014 FR-1; ctest data_schema_rejects_min_above_max / data_schema_explains_min_above_max): a range with no value in it.
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

struct Enemy {
    int hp = 1;
};
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(hp, .min = 10, .max = 5));
