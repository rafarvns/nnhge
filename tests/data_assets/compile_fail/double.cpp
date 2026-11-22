// Must not compile (spec 014 FR-1; ctest data_schema_rejects_double / data_schema_explains_double): a double member.
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

struct Enemy {
    double speed = 1;
};
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(speed));
