// The schema source of the data example (listed in DATA_SCHEMA): declarations only. The build compiles it into
// the game and into a host program that describes the schema, which the build checks every data file against
// (enemies/goblin.enemy:7: unknown field 'damgae' (did you mean ...?)). It must not call engine or game functions.

#include "data_types.hpp"

NNH_DATA_ENUM(Element, Normal, Fire, Ice, Earth);

NNH_DATA_STRUCT(Stats, NNH_DATA_FIELD(hp, .min = 1, .max = 9999, .required = true), NNH_DATA_FIELD(attack),
                NNH_DATA_FIELD(defense));

NNH_DATA_STRUCT(Drop, NNH_DATA_FIELD(item, .required = true), NNH_DATA_FIELD(chance, .min = 0, .max = 1));

NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(name, .required = true), NNH_DATA_FIELD(stats, .required = true),
              NNH_DATA_FIELD(speed, .min = 0, .max = 10), NNH_DATA_FIELD(element), NNH_DATA_FIELD(sprite),
              NNH_DATA_FIELD(hurt_sound), NNH_DATA_FIELD(drops), NNH_DATA_FIELD(tint), NNH_DATA_FIELD(spawn),
              NNH_DATA_FIELD(flags));

NNH_DATA_TYPE(Item, "item", NNH_DATA_FIELD(name, .required = true), NNH_DATA_FIELD(price, .min = 0),
              NNH_DATA_FIELD(icon), NNH_DATA_FIELD(heal), NNH_DATA_FIELD(summons));

NNH_DATA_TYPE(Settings, "settings", NNH_DATA_FIELD(title), NNH_DATA_FIELD(top_color),
              NNH_DATA_FIELD(bottom_color), NNH_DATA_FIELD(music), NNH_DATA_FIELD(first, .required = true));
