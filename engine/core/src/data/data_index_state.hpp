#pragma once
// The data index's state (data_index.cpp), shared with hot reload's data kind (spec 031, data_reload.cpp), which
// prepares a new index from a batch and swaps it in. Internal to the data module.

#include <cstddef>
#include <cstdint>

#include "data/data_index.hpp"
#include "data/data_format.hpp"

namespace nnh::detail {

struct DataIndexState {
    std::uint8_t* block = nullptr; ///< the index file, then the scratch buffer
    std::size_t index_size = 0;
    std::size_t scratch_size = 0;
    IndexView view;
    DataTypeEntry types[datafmt::kMaxDataTypes];
    int type_count = 0;
};

extern DataIndexState g_data_index_state;

/// Reads and validates `.nnh_data` (as the assets are now) into a new block described by `into` (freed on any error).
Result<void> data_index_read(DataIndexState& into);
/// Matches the game's compiled data types with the current index (warns about those the build didn't see).
void data_index_match_types();

} // namespace nnh::detail
