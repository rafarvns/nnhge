// Must not compile (spec 014 FR-1; ctest data_schema_rejects_unknown_member / data_schema_explains_unknown_member): a field the struct doesn't have.
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

struct Enemy {
    int damage = 1;
};
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(damgae));
