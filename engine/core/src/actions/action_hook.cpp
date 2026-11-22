// The set's after-input hook (decision D32, spec 017 FR-15): registered at creation, removed at destruction.

#include "actions/actions_internal.hpp"

namespace nnh::detail {

void actions_attach(ActionBlock&) {}
void actions_detach(ActionBlock&) {}

} // namespace nnh::detail
