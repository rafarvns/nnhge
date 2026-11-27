// The strings kind of hot reload (spec 031 FR-10, FR-14, FR-16): a batch with a string table (`strings/*.nnhstr`) or the
// manifest (`.nnh_loc`) reloads the current language's chain. The first such file of a batch prepares everything — the
// manifest as the batch has it (validated; another language list, base or pseudo-language refuses: rebuild and restart),
// a new block sized by it, the chain read through the staged batch and validated — and the commit swaps the block (and
// the manifest), then bumps the serial so text_changed() and spec 028's widgets re-resolve. `tr` pointers obtained before
// the apply point are invalid after it, as after a language switch (D50). Registered by loc::start() in a
// Debug/Development build. Tests: tests/loc/test_loc_reload.cpp.

#include <cstring>

#include "loc/loc_state.hpp"
#include "nnh/assets.hpp"
#include "nnh/memory.hpp"
#include "reload/reload_kind.hpp"

namespace nnh::detail::loc {
namespace {

using reload::Prepared;
using reload::ReloadItem;
using remote::ReloadReason;
using remote::ReloadStage;

struct PreparedChain {
    std::uint8_t* manifest = nullptr; ///< the manifest as the batch has it (replaces the current one)
    ManifestHeader header{};
    std::uint8_t* block = nullptr;
    std::size_t block_bytes = 0;
    LoadedTable tables[kMaxChain]{};
    int count = 0;
};

PreparedChain* g_prepared = nullptr; // this batch's chain (the first claimed file prepares it)

bool claims(const ReloadItem& item) {
    if (std::strcmp(item.path, ".nnh_loc") == 0) return true;
    const std::size_t n = std::strlen(item.path);
    return n > 15 && std::strncmp(item.path, "strings/", 8) == 0 && std::strcmp(item.path + n - 7, ".nnhstr") == 0;
}

bool same_languages(const ManifestHeader& h, const std::uint8_t* manifest) {
    const LocState& s = loc_state();
    if (h.language_count != s.header.language_count || h.base != s.header.base || h.pseudo != s.header.pseudo) {
        return false;
    }
    const auto* now = reinterpret_cast<const LanguageRecord*>(manifest + h.off_languages);
    for (int i = 0; i < h.language_count; ++i) {
        if (std::strcmp(now[i].code, s.languages[i].code) != 0) return false;
    }
    return true;
}

void release(PreparedChain* p) {
    if (p == nullptr) return;
    mem::free(p->block);
    mem::free(p->manifest);
    mem::free(p);
}

Prepared refuse(PreparedChain* p, ReloadStage stage, ReloadReason reason, std::uint32_t missing = 0) {
    release(p);
    return reload::prepared_refused(stage, reason, missing);
}

/// The manifest as the batch has it (read through the staged batch: the new one when the batch carries `.nnh_loc`).
Prepared read_manifest(PreparedChain* p) {
    LocState& s = loc_state();
    auto size = assets::size(".nnh_loc");
    if (!size) return refuse(p, ReloadStage::Validate, ReloadReason::Corrupted);
    p->manifest = static_cast<std::uint8_t*>(mem::alloc(MemRegion::Heap, *s.tag, size.value(), 8));
    if (p->manifest == nullptr) {
        return refuse(p, ReloadStage::Memory, ReloadReason::NoMemory, static_cast<std::uint32_t>(size.value()));
    }
    auto read = assets::read(".nnh_loc", p->manifest, size.value());
    if (!read || validate_manifest(p->manifest, read.value()) != nullptr) {
        return refuse(p, ReloadStage::Validate, ReloadReason::Corrupted);
    }
    std::memcpy(&p->header, p->manifest, sizeof p->header);
    if (!same_languages(p->header, p->manifest)) return refuse(p, ReloadStage::Restart, ReloadReason::Languages);
    return reload::prepared_ok();
}

Prepared prepare(const ReloadItem&) {
    LocState& s = loc_state();
    if (!s.started || g_prepared != nullptr) return reload::prepared_ok(); // the batch's chain is prepared already
    auto* p = static_cast<PreparedChain*>(mem::alloc(MemRegion::Heap, *s.tag, sizeof(PreparedChain), 8));
    if (p == nullptr) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, sizeof(PreparedChain));
    *p = PreparedChain{};
    const Prepared m = read_manifest(p);
    if (!m.ok) return m;
    const auto* languages = reinterpret_cast<const LanguageRecord*>(p->manifest + p->header.off_languages);
    p->block_bytes = chain_block_bytes(p->header);
    p->block = static_cast<std::uint8_t*>(mem::alloc(MemRegion::Heap, *s.tag, p->block_bytes, 8));
    if (p->block == nullptr) {
        return refuse(p, ReloadStage::Memory, ReloadReason::NoMemory, static_cast<std::uint32_t>(p->block_bytes));
    }
    if (load_chain_into(languages, s.current, p->block, p->block_bytes, p->tables, p->count) != nullptr) {
        return refuse(p, ReloadStage::Validate, ReloadReason::Corrupted);
    }
    g_prepared = p;
    return reload::prepared_ok(p);
}

void commit(const ReloadItem&, void* state) {
    auto* p = static_cast<PreparedChain*>(state);
    if (p == nullptr) return;
    LocState& s = loc_state();
    mem::free(s.block);
    s.block = p->block;
    s.block_bytes = p->block_bytes;
    for (int i = 0; i < kMaxChain; ++i) s.tables[i] = p->tables[i];
    s.table_count = p->count;
    if (p->manifest != nullptr) {
        mem::free(s.manifest);
        s.manifest = p->manifest;
        s.header = p->header;
        s.languages = reinterpret_cast<const LanguageRecord*>(s.manifest + s.header.off_languages);
    }
    ++s.serial; // text_changed() and the UI's widgets resolve their keys again
    mem::free(p);
    g_prepared = nullptr;
}

void abandon(const ReloadItem&, void* state) {
    auto* p = static_cast<PreparedChain*>(state);
    if (p == nullptr) return;
    release(p);
    g_prepared = nullptr;
}

reload::ReloadKind g_kind{"strings", claims, prepare, commit, abandon};

} // namespace

void loc_reload_register() { reload::register_kind(g_kind); }

} // namespace nnh::detail::loc
