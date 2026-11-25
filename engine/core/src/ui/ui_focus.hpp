#pragma once
// Focus internals shared by focus, navigation, modals, setters and destroy (ui_focus.cpp).

#include "ui/ui_block.hpp"

namespace nnh::detail {

/// Where a widget sat before it stopped being a target: its container, siblings and root.
struct RepairPoint {
    int parent = -1, next = -1, prev = -1;
    int root = kUiNone;
};
RepairPoint repair_point(const UiBlock& b, int s);
/// FR-15: the next target among the following siblings, else the previous ones, else the root's default, else -1.
int repair_target(const UiBlock& b, const RepairPoint& p);

bool inert(const UiBlock& b, int s);           ///< disabled but focusable (activation is refused)
bool inert_allowed(const UiBlock& b, int s);   ///< an ancestor container makes disabled widgets focusable
bool enabled_chain(const UiBlock& b, int s);   ///< ui_queries.cpp
UiState state_of(const UiBlock& b, int s);
int value_of(const UiBlock& b, int s);

} // namespace nnh::detail
