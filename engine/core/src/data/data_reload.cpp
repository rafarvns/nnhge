// The data kind of hot reload (spec 031 FR-11, FR-14, FR-16, FR-18): a batch's data files are validated against the
// game's compiled types (D29's fingerprint: a mismatch is "made for another struct: rebuild and restart"); a batch's
// `.nnh_data` is read into a new block at prepare and swapped in at commit, so count/name/find/path describe the new
// set. Structs the game filled are never touched (it reloads them when assets::reloaded() says so). Registered once the
// index is loaded, in Debug/Development builds. Tests: tests/data/test_data_reload.cpp.

#include <cstring>
#include <new>

#include "data/data_apply.hpp"
#include "data/data_index.hpp"
#include "data/data_index_state.hpp"
#include "data/data_validate.hpp"
#include "nnh/assets.hpp"
#include "nnh/data.hpp"
#include "nnh/memory.hpp"
#include "reload/reload_kind.hpp"

namespace nnh::detail {
namespace {

using reload::Prepared;
using reload::ReloadItem;
using remote::ReloadReason;
using remote::ReloadStage;

constexpr const char* kIndexName = ".nnh_data";

bool is_index(const ReloadItem& item) { return std::strcmp(item.path, kIndexName) == 0; }

/// The compiled type whose key is `path`'s extension, or null.
const DataTypeEntry* type_of(const char* path) {
    const char* dot = std::strrchr(path, '.');
    const char* slash = std::strrchr(path, '/');
    if (dot == nullptr || (slash != nullptr && dot < slash) || dot == path) return nullptr;
    for (int i = 0; i < g_data_index_state.type_count; ++i) {
        const DataTypeEntry& t = g_data_index_state.types[i];
        if (std::strcmp(t.info->key, dot + 1) == 0) return &t;
    }
    return nullptr;
}

bool claims(const ReloadItem& item) { return is_index(item) || type_of(item.path) != nullptr; }

Prepared check_file(const ReloadItem& item, const DataTypeEntry& t) {
    void* bytes = mem::alloc(MemRegion::Heap, data_memory_tag(), item.size > 0 ? item.size : 1);
    if (bytes == nullptr) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, item.size);
    auto read = assets::read(item.path, bytes, item.size); // the batch's new bytes (staged during prepare)
    DataCheck check;
    if (read) check = validate_data(bytes, read.value(), data_registry(), *t.info, t.fingerprint);
    mem::free(bytes);
    if (!read) return reload::prepared_refused(ReloadStage::Validate, ReloadReason::Corrupted);
    if (check.problem == DataProblem::Stale) return reload::prepared_refused(ReloadStage::Restart, ReloadReason::Schema);
    if (!check.ok()) return reload::prepared_refused(ReloadStage::Validate, ReloadReason::Corrupted);
    return reload::prepared_ok();
}

Prepared prepare(const ReloadItem& item) {
    if (item.op == remote::ReloadOp::Remove) return reload::prepared_ok(); // the new index drops it
    if (!is_index(item)) return check_file(item, *type_of(item.path));
    auto* fresh = static_cast<DataIndexState*>(mem::alloc(MemRegion::Heap, data_memory_tag(), sizeof(DataIndexState)));
    if (fresh == nullptr) {
        return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, sizeof(DataIndexState));
    }
    new (fresh) DataIndexState{};
    auto read = data_index_read(*fresh);
    if (!read) {
        mem::free(fresh);
        const bool memory = read.error().code == ErrorCode::OutOfMemory;
        return reload::prepared_refused(memory ? ReloadStage::Memory : ReloadStage::Validate,
                                        memory ? ReloadReason::NoMemory : ReloadReason::Corrupted, item.size);
    }
    return reload::prepared_ok(fresh);
}

void commit(const ReloadItem&, void* state) {
    auto* fresh = static_cast<DataIndexState*>(state);
    if (fresh == nullptr) return;
    mem::free(g_data_index_state.block);
    g_data_index_state.block = fresh->block;
    g_data_index_state.index_size = fresh->index_size;
    g_data_index_state.scratch_size = fresh->scratch_size;
    g_data_index_state.view = fresh->view;
    mem::free(fresh);
    data_index_match_types();
}

void abandon(const ReloadItem&, void* state) {
    auto* fresh = static_cast<DataIndexState*>(state);
    if (fresh == nullptr) return;
    mem::free(fresh->block);
    mem::free(fresh);
}

reload::ReloadKind g_kind{"data", claims, prepare, commit, abandon};

} // namespace

void data_reload_register() { reload::register_kind(g_kind); }

} // namespace nnh::detail
