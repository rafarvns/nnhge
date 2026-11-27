// The hooks of hot reload (see reload_hooks.hpp): only the pointers, so referencing them links nothing else.

#include "reload/reload_hooks.hpp"

namespace nnh::detail {

const ReloadSessionOps* g_reload_session = nullptr;
const ReloadAssetOps* g_reload_asset_ops = nullptr;
void (*g_reload_apply)(std::uint32_t frame) = nullptr;
bool g_reload_assets_active = false;
const AssetIndex* g_reload_original_index = nullptr;

} // namespace nnh::detail
