// One of two schema sources declaring the same key (spec 014 T7.2): their schema program must refuse to write a
// description and name both sources (ctest data_schema_program_names_both_sources_of_a_key).

#include "nnh/data.hpp"

namespace dup_a {
struct Sword {
    int damage = 1;
};
} // namespace dup_a

NNH_DATA_TYPE(dup_a::Sword, "thing", NNH_DATA_FIELD(damage));
