// Must not compile (spec 014 FR-1; ctest data_schema_rejects_not_aggregate / data_schema_explains_not_aggregate): a struct with a constructor of its own.
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

struct Enemy {
    Enemy() : hp(3) {}
    int hp;
};
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(hp));
