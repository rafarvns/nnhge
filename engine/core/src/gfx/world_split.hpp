#pragma once
// Where the 3D world sits in a screen's 2D pass (spec 026 FR-18, D43): the first item and the first tile batch at or
// above the world layer. The pass is sorted by layer, so everything before the split is below the world. Pure; tested
// in tests/gfx/test_world_split.cpp.

#include "gfx/draw_list.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::detail {

struct WorldSplit {
    int item = 0;  ///< items [0, item) are below the world layer
    int split = 0; ///< tile batches [0, split) are below it
};

WorldSplit world_split(const platform::DrawItem2D* items, int item_count, const TileSplit* splits, int split_count,
                       int world_layer);

} // namespace nnh::detail
