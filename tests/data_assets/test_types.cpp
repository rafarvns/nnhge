// The tests' data schema (see test_types.hpp): written as a game's schema source would be.

#include "test_types.hpp"

using namespace test_data;

NNH_DATA_ENUM(Element, Normal, Fire, Ice);

NNH_DATA_STRUCT(Stats, NNH_DATA_FIELD(hp, .min = 1, .max = 999, .required = true), NNH_DATA_FIELD(attack));

NNH_DATA_STRUCT(Drop, NNH_DATA_FIELD(item, .required = true), NNH_DATA_FIELD(chance, .min = 0, .max = 1));

NNH_DATA_TYPE(Item, "item", NNH_DATA_FIELD(name), NNH_DATA_FIELD(price, .min = 0), NNH_DATA_FIELD(icon));

NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(name, .required = true), NNH_DATA_FIELD(stats),
              NNH_DATA_FIELD(speed, .min = 0), NNH_DATA_FIELD(element), NNH_DATA_FIELD(sprite),
              NNH_DATA_FIELD(hurt), NNH_DATA_FIELD(drops), NNH_DATA_FIELD(tint), NNH_DATA_FIELD(spawn),
              NNH_DATA_FIELD(flags), NNH_DATA_FIELD(level, .min = 1, .max = 99), NNH_DATA_FIELD(theme));
