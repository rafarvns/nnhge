// A changed schema: tests/errors.nnhs swaps it over data_types.cpp (spec 031 FR-14). The struct itself is unchanged, but
// the schema the build checks data files against is not, so `nnh` refuses the reload: "rebuild and restart".

#include "data_types.hpp"

NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(name, .required = true), NNH_DATA_FIELD(hp, .min = 1, .max = 500),
              NNH_DATA_FIELD(speed, .min = 0, .max = 10), NNH_DATA_FIELD(tint));
