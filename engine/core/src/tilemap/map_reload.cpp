// The tilemap kind of hot reload (spec 031 FR-11, FR-18, Q6): a reloaded `.nnhmap` is only validated with spec 012's
// validator at prepare (a corrupted map refuses the whole batch); nothing is swapped: the game's Tilemap blocks are its
// own and it reloads them when assets::reloaded() says so. Registered by the first load_tilemap() in a Debug/Development
// build. Tests: tests/tilemap/test_map_reload.cpp.

#include <cstring>

#include "nnh/assets.hpp"
#include "reload/reload_kind.hpp"
#include "tilemap/map_validate.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace nnh::detail {
namespace {

using reload::Prepared;
using reload::ReloadItem;

bool claims(const ReloadItem& item) {
    const std::size_t n = std::strlen(item.path);
    return n > 7 && std::strcmp(item.path + n - 7, ".nnhmap") == 0;
}

Prepared prepare(const ReloadItem& item) {
    if (item.op == remote::ReloadOp::Remove) return reload::prepared_ok();
    auto file = assets::load(item.path); // the batch's new bytes (the overlay is staged during prepare)
    if (!file && file.error().code == ErrorCode::OutOfMemory) {
        return reload::prepared_refused(remote::ReloadStage::Memory, remote::ReloadReason::NoMemory, item.size);
    }
    if (!file) return reload::prepared_refused(remote::ReloadStage::Validate, remote::ReloadReason::Corrupted);
    const MapCheck check = validate_map(file.value().data(), file.value().size());
    if (!check.ok()) return reload::prepared_refused(remote::ReloadStage::Validate, remote::ReloadReason::Corrupted);
    return reload::prepared_ok();
}

void nothing(const ReloadItem&, void*) {}

reload::ReloadKind g_kind{"tilemap", claims, prepare, nothing, nothing};

} // namespace

void map_reload_register() { reload::register_kind(g_kind); }

} // namespace nnh::detail
