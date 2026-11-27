// The consumer game's schema source (DATA_SCHEMA): declarations only.

#include "data_types.hpp"

NNH_DATA_TYPE(Thing, "thing", NNH_DATA_FIELD(name, .required = true), NNH_DATA_FIELD(value, .min = 0, .max = 1000),
              NNH_DATA_FIELD(sprite));
