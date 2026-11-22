// Must not compile (spec 014 FR-1; ctest data_schema_rejects_listed_twice / data_schema_explains_listed_twice): the same field twice.
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

struct Enemy {
    int hp = 1;
};
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(hp), NNH_DATA_FIELD(hp));
