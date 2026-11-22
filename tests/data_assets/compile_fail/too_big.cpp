// Must not compile (spec 014 FR-1; ctest data_schema_rejects_too_big / data_schema_explains_too_big): a struct over 16 KiB.
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

struct Enemy {
    int hp = 1;
    std::uint8_t pad[16 * 1024];
};
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(hp));
