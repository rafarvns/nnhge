// Must not compile (spec 014 FR-1; ctest data_schema_rejects_int64 / data_schema_explains_int64): a 64-bit integer member.
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

struct Enemy {
    std::int64_t gold = 1;
};
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(gold));
