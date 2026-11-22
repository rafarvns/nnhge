// Must not compile (spec 014 FR-1; ctest data_schema_rejects_unknown_enum_name / data_schema_explains_unknown_enum_name): a name that is not an enumerator.
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

enum class Element : std::uint8_t { Normal, Fire };
NNH_DATA_ENUM(Element, Normal, Fier);
