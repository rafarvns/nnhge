#pragma once
// Scene hooks used by the frame loop (app/app.cpp).

namespace nnh::detail {

/// Applies scene changes requested during the previous frame (start of nnh::frame()).
void scenes_apply_pending();

/// Exits every scene left on the stack and drops pending changes (nnh::shutdown()).
void scenes_shutdown();

} // namespace nnh::detail
