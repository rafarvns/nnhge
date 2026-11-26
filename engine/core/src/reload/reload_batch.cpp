// The batch engine (see reload_batch.hpp) and the kinds' registration (reload_kind.hpp).

#include "reload/reload_batch.hpp"

#include <cstring>

#include "assets/asset_index.hpp"
#include "assets/assets_internal.hpp"
#include "nnh/memory.hpp"
#include "reload/reload_assets.hpp"
#include "reload/reload_files.hpp"
#include "reload/reload_index.hpp"
#include "reload/reload_kind.hpp"
#include "reload/reload_memory.hpp"
#include "reload/reload_overlay.hpp"

namespace nnh::detail::reload {
namespace {

using remote::ReloadOp;
using remote::ReloadReason;
using remote::ReloadStage;

ReloadKind* g_first = nullptr;
int g_kind_calls = 0;

/// What one (file, kind) pair prepared.
struct Done {
    ReloadKind* kind;
    int file;
    void* state;
};

ReloadItem item_of(const BatchFile& f, int index) {
    return ReloadItem{f.path, f.hash, f.size, static_cast<std::uint16_t>(index), f.op};
}

bool is_index(const BatchFile& f) { return std::strcmp(f.path, kAssetIndexFile) == 0; }

/// Consistency (FR-12): with a new index puts are in it and removes aren't; without one, puts are in the current
/// index and nothing is removed.
bool consistent(const BatchFile* files, int count, const AssetIndex* fresh) {
    const AssetIndex* now = current_index();
    for (int i = 0; i < count; ++i) {
        const BatchFile& f = files[i];
        if (is_index(f)) continue;
        const bool put = f.op == ReloadOp::Put;
        if (fresh != nullptr) {
            if ((fresh->find(f.path) != nullptr) != put) return false;
        } else if (!put || now == nullptr || now->find(f.path) == nullptr) {
            return false;
        }
    }
    return true;
}

/// Puts that would take a new overlay slot.
int new_slots(const BatchFile* files, int count) {
    const OverlayTable* o = overlay();
    int n = 0;
    for (int i = 0; i < count; ++i) {
        if (files[i].op != ReloadOp::Put || is_index(files[i])) continue;
        if (o == nullptr || o->find(files[i].path, files[i].hash) == nullptr) ++n;
    }
    return n;
}

void remove_received(void*, std::uint16_t file) {
    if (file != 0) ReloadFiles::remove_file(file);
}

void abandon_all(const BatchFile* files, Done* done, int count) {
    for (int i = count - 1; i >= 0; --i) {
        ++g_kind_calls;
        if (done[i].kind->abandon != nullptr) done[i].kind->abandon(item_of(files[done[i].file], done[i].file), done[i].state);
    }
}

/// The overlay names every put (stable paths: the current index's), old received files go.
void switch_overlay(const BatchFile* files, int count) {
    OverlayTable* o = overlay();
    const AssetIndex* index = current_index();
    for (int i = 0; i < count; ++i) {
        const BatchFile& f = files[i];
        if (f.op == ReloadOp::Put && is_index(f)) remove_received(nullptr, f.file); // parsed: its file has done its job
        if (f.op != ReloadOp::Put || is_index(f) || o == nullptr || index == nullptr) continue;
        const char* stored = index->find(f.path);
        std::uint16_t replaced = 0;
        if (stored != nullptr && o->put(stored, f.hash, f.file, f.size, replaced)) remove_received(nullptr, replaced);
    }
}

BatchResult refusal(ReloadStage stage, ReloadReason reason, int file, std::uint32_t missing = 0,
                    const char* kind = nullptr) {
    BatchResult r;
    r.stage = stage;
    r.reason = reason;
    r.file = file < 0 ? remote::kNoFile : static_cast<std::uint16_t>(file);
    r.missing = missing;
    r.kind = kind;
    return r;
}

/// Prepares every (file, kind) pair into `done`. @return the refusal, or ok.
BatchResult prepare_all(const BatchFile* files, int count, Done* done, int capacity, int& prepared) {
    for (int i = 0; i < count; ++i) {
        const ReloadItem item = item_of(files[i], i);
        for (ReloadKind* k = g_first; k != nullptr; k = k->next) {
            ++g_kind_calls;
            if (!k->claims(item)) continue;
            if (prepared == capacity) return refusal(ReloadStage::Memory, ReloadReason::NoMemory, i);
            ++g_kind_calls;
            const Prepared p = k->prepare(item);
            if (!p.ok) return refusal(p.stage, p.reason, i, p.missing, k->name);
            done[prepared++] = Done{k, i, p.state};
        }
    }
    BatchResult ok;
    ok.ok = true;
    return ok;
}

} // namespace

void register_kind(ReloadKind& kind) {
    ReloadKind** at = &g_first;
    while (*at != nullptr) {
        if (*at == &kind) return;
        at = &(*at)->next;
    }
    kind.next = nullptr;
    *at = &kind;
}

ReloadKind* first_kind() { return g_first; }

void clear_kinds() {
    for (ReloadKind* k = g_first; k != nullptr;) {
        ReloadKind* next = k->next;
        k->next = nullptr;
        k = next;
    }
    g_first = nullptr;
}

int kind_calls() { return g_kind_calls; }

BatchResult apply_batch(const BatchFile* files, int count) {
    // 1. The batch's own index first: consistency is checked against it.
    int index_file = -1;
    for (int i = 0; i < count; ++i) {
        if (is_index(files[i]) && files[i].op == ReloadOp::Put) index_file = i;
    }
    if (index_file >= 0) {
        const Prepared p = index_prepare(files[index_file]);
        if (!p.ok) return refusal(p.stage, p.reason, index_file, p.missing, "asset index");
    }
    if (!consistent(files, count, prepared_index())) {
        index_abandon();
        return refusal(ReloadStage::Validate, ReloadReason::Consistency, -1);
    }
    const OverlayTable* o = overlay();
    if (o == nullptr || o->paths() + new_slots(files, count) > kOverlayPaths) {
        index_abandon();
        return refusal(ReloadStage::Restart, ReloadReason::TooManyPaths, -1);
    }
    // 2. Every kind prepares, reading through the staged batch.
    int kinds = 0;
    for (ReloadKind* k = g_first; k != nullptr; k = k->next) ++kinds;
    const int capacity = count * (kinds > 0 ? kinds : 1);
    auto* done = static_cast<Done*>(mem::alloc(MemRegion::Heap, reload_memory_tag(), sizeof(Done) * capacity));
    if (done == nullptr) {
        index_abandon();
        return refusal(ReloadStage::Memory, ReloadReason::NoMemory, -1, static_cast<std::uint32_t>(sizeof(Done) * capacity));
    }
    set_staging(files, count);
    int prepared = 0;
    BatchResult result = prepare_all(files, count, done, capacity, prepared);
    set_staging(nullptr, 0);
    if (!result.ok) {
        abandon_all(files, done, prepared);
        index_abandon();
        mem::free(done);
        return result;
    }
    // 3. Commit: the index and the overlay switch, then the kinds swap (nothing can fail from here).
    index_commit(remove_received, nullptr);
    switch_overlay(files, count);
    for (int i = 0; i < prepared; ++i) {
        ++g_kind_calls;
        done[i].kind->commit(item_of(files[done[i].file], done[i].file), done[i].state);
    }
    mem::free(done);
    for (int i = 0; i < count; ++i) {
        if (is_index(files[i])) continue;
        if (files[i].op == ReloadOp::Put) ++result.changed;
        else ++result.removed;
    }
    return result;
}

} // namespace nnh::detail::reload
