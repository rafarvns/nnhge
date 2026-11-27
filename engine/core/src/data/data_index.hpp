#pragma once
// The data index at run time (spec 014 FR-13, plan 014 "Data index"): `.nnh_data` read into one engine block
// under the memory tag "data" together with a scratch buffer the size of the largest data asset, validated,
// and a table of the game's compiled data types (their fingerprints and positions in the index). Loaded by
// the first data call, freed by release() or a shutdown hook (Debug/Development). Internal to the data
// module. Tests: tests/data_assets/test_data_index.cpp.

#include <cstddef>
#include <cstdint>

#include "data/index_validate.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "nnh/result.hpp"

namespace nnh {
// The data module's log channel, defined in data_index.cpp (not with the engine's channels in log/, so games
// without data assets don't link it).
NNH_LOG_CHANNEL_EXTERN(data);
} // namespace nnh

namespace nnh::detail {

/// A data type the game compiled (NNH_DATA_TYPE), as the loader needs it.
struct DataTypeEntry {
    const TypeInfo* info = nullptr;
    std::uint32_t fingerprint = 0; ///< schema_fingerprint, computed once
    int index_type = -1;           ///< its type in the index; -1 when the build's schema doesn't have it
};

/// Loads the index if it isn't loaded. Errors: Unsupported (before nnh::init(); a newer format), NotFound (no
/// `.nnh_data` asset), Corrupted, LimitReached, OutOfMemory, IoError — logged on the "data" channel.
Result<void> data_index_load();

bool data_index_loaded();
void data_index_release();

/// Valid while the index is loaded.
const IndexView& data_index();
std::uint8_t* data_scratch();
std::size_t data_scratch_size();

/// The compiled data type with this id (keyed types only), or null. Valid while the index is loaded.
const DataTypeEntry* data_type(TypeId type);

MemTag& data_memory_tag();

/// Registers the data kind of hot reload (spec 031, data_reload.cpp; Debug/Development builds, once the index is loaded).
void data_reload_register();

} // namespace nnh::detail
