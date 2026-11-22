// Must not compile (spec 014 FR-1; ctest data_schema_rejects_min_on_string / data_schema_explains_min_on_string): a range on a field that is not a number.
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

struct Enemy {
    nnh::data::String<8> name;
};
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(name, .min = 1));
