#pragma once
// Creation checks of a UiDesc (spec 028 FR-1, plan "Creation checks"). Pure.

#include "nnh/result.hpp"
#include "nnh/ui.hpp"

namespace nnh::detail {

/// ErrorCode::None when `desc` is usable, else InvalidArgument naming the field.
Error check_ui_desc(const UiDesc& desc);

} // namespace nnh::detail
