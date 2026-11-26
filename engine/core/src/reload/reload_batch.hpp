#pragma once
// The batch engine (spec 031 D53, FR-4, FR-11, FR-12, FR-16): one staged batch → prepare every file through the
// registered kinds (the asset index first, when the batch carries one), check the batch's consistency and the overlay's
// room, then commit everything (the index and overlay switch, the kinds swap) or abandon everything.
// Consistency: with `.nnh_assets` in the batch, every put is in the new index and every remove is absent from it;
// without it, every put is in the current index and there are no removes. Tests: tests/reload/test_reload_batch.cpp.

#include <cstdint>

#include "reload/reload_wire.hpp"

namespace nnh::detail::reload {

struct BatchFile;

struct BatchResult {
    bool ok = false;
    remote::ReloadStage stage = remote::ReloadStage::Validate;
    remote::ReloadReason reason = remote::ReloadReason::None;
    std::uint16_t file = remote::kNoFile; ///< the batch index of the refused file
    std::uint32_t missing = 0;
    const char* kind = nullptr;           ///< the kind that refused (null: the engine itself)
    int changed = 0;                      ///< puts applied
    int removed = 0;                      ///< removes applied
};

/// Prepares and commits (or abandons) `files`. On success the overlay names the batch's puts and old received files
/// it replaced are removed; on failure nothing visible changed (the batch's own files are the caller's to remove).
/// Allocates (the kinds' new content, the prepared table, a new index); called only at the apply point.
BatchResult apply_batch(const BatchFile* files, int count);

/// Calls of the kinds' functions since the start (test probes): no walk happens without a batch (FR-17).
int kind_calls();

} // namespace nnh::detail::reload
