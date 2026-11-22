// The other schema source declaring the key "thing" (see schema_dup_a.cpp).

#include "nnh/data.hpp"

namespace dup_b {
struct Shield {
    int defense = 1;
};
} // namespace dup_b

NNH_DATA_TYPE(dup_b::Shield, "thing", NNH_DATA_FIELD(defense));
