#pragma once
// Reload kinds (spec 031 D53, FR-4, FR-7–FR-12, FR-16): a module that loads assets of one kind registers a struct of
// function pointers the first time it loads one in a Debug/Development build, so a game that never loads fonts links no
// font reloader. At a batch's apply point every file is offered to every registered kind (registration order); a kind
// that claims it prepares new content (reading through the staged batch: its normal loaders see the new files; nothing
// visible changes), then either every prepared item is committed (swaps that cannot fail) or every one is abandoned.
// No `virtual`, no RTTI. Tests: tests/reload/test_reload_batch.cpp and each kind's tests.

#include <cstdint>

#include "reload/reload_wire.hpp"

namespace nnh::detail::reload {

/// One file of a batch, as the kinds see it.
struct ReloadItem {
    const char* path = nullptr; ///< the asset path ("art/hero.png")
    std::uint32_t hash = 0;     ///< path_hash(path): Q12's per-object tag
    std::uint32_t size = 0;     ///< bytes of a put
    std::uint16_t index = 0;    ///< in the batch
    remote::ReloadOp op = remote::ReloadOp::Put;
};

/// A prepare's verdict. `ok` with a state to commit or abandon (may be null: nothing to do for this item).
struct Prepared {
    bool ok = true;
    remote::ReloadStage stage = remote::ReloadStage::Validate;
    remote::ReloadReason reason = remote::ReloadReason::None;
    std::uint32_t missing = 0; ///< bytes an allocation lacked (FR-16's warning)
    void* state = nullptr;     ///< the kind's prepared content, given back to commit/abandon
};

inline Prepared prepared_ok(void* state = nullptr) { return Prepared{true, remote::ReloadStage::Validate,
                                                                     remote::ReloadReason::None, 0, state}; }
inline Prepared prepared_refused(remote::ReloadStage stage, remote::ReloadReason reason, std::uint32_t missing = 0) {
    return Prepared{false, stage, reason, missing, nullptr};
}

struct ReloadKind {
    const char* name = nullptr; ///< for logs ("texture")
    /// True for the files this kind handles (by path: its extension, a folder).
    bool (*claims)(const ReloadItem& item) = nullptr;
    /// New content for `item` (puts and removes): validate, allocate under the kind's tag, create resources. Must leave
    /// everything visible as it was. A failure refuses the whole batch.
    Prepared (*prepare)(const ReloadItem& item) = nullptr;
    /// Swaps the prepared content in (cannot fail); releases the old content (GPU resources through D47).
    void (*commit)(const ReloadItem& item, void* state) = nullptr;
    /// Frees the prepared content (another item of the batch failed).
    void (*abandon)(const ReloadItem& item, void* state) = nullptr;
    ReloadKind* next = nullptr; ///< the registration list (set by register_kind)
};

/// Adds `kind` (static storage in its module) once; later calls with the same kind do nothing.
void register_kind(ReloadKind& kind);
/// The registered kinds, in registration order.
ReloadKind* first_kind();
/// Forgets every registration (tests; nnh::shutdown() through the reload shutdown hook).
void clear_kinds();

} // namespace nnh::detail::reload
