// Must not compile (spec 014 FR-1; ctest data_schema_rejects_std_string / data_schema_explains_std_string): a member
// that is not trivially copyable (std::string).
#include <string>

#include "nnh/data.hpp"

struct Named {
    std::string name;
};
NNH_DATA_STRUCT(Named, NNH_DATA_FIELD(name));
