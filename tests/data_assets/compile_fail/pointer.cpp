// Must not compile (spec 014 FR-1; ctest data_schema_rejects_pointer / data_schema_explains_pointer): a pointer member.
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

struct Enemy {
    const int* target = nullptr;
};
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(target));
