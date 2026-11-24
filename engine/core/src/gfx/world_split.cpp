// Where the 3D world splits a 2D pass (world_split.hpp).

#include "gfx/world_split.hpp"

namespace nnh::detail {

WorldSplit world_split(const platform::DrawItem2D* items, int item_count, const TileSplit* splits, int split_count,
                       int world_layer) {
    WorldSplit s;
    while (s.item < item_count && items[s.item].layer < world_layer) ++s.item;
    while (s.split < split_count && splits[s.split].layer < world_layer) ++s.split;
    return s;
}

} // namespace nnh::detail
