#pragma once
// The asset-index kind (spec 031 D53, FR-12): a batch's `.nnh_assets` parsed into a new block at prepare, made the
// index every lookup uses at commit (the overlay's entries repointed into it, paths it no longer lists removed); the
// original index stays the assets module's. Internal to the batch engine. Tests: tests/reload/test_reload_batch.cpp.

#include "reload/reload_kind.hpp"

namespace nnh::detail {
class AssetIndex;
}

namespace nnh::detail::reload {

struct BatchFile;

/// Reads and parses the batch's index file. A refusal leaves nothing allocated.
Prepared index_prepare(const BatchFile& file);
/// The index prepared (null when none).
const AssetIndex* prepared_index();
/// Makes the prepared index current: overlay entries repointed (`removed` gets every received file of a path the new
/// index drops), the previous batch index freed.
void index_commit(void (*removed)(void* context, std::uint16_t file), void* context);
void index_abandon();
/// Frees the batch index (shutdown).
void index_release();

} // namespace nnh::detail::reload
