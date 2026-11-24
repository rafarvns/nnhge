// The NNH_DATA_ENUM names of a saved enum (spec 027, plan decision 22). In its own object: only a game whose
// save has an enum field references it, so only those games link spec 014's registry (schema_registry.o, no
// loader or index).

#include "data/schema_registry.hpp"
#include "nnh/saves.hpp"

namespace nnh::detail {

const EnumInfo* save_find_enum_names(const void* tag) {
    return find_enum(data_registry(), tag);
}

} // namespace nnh::detail
