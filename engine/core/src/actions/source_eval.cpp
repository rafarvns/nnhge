// One slot's reading per source kind (spec 017 FR-6; plan 017 "Semantics"): pure functions of the input view. The
// readings themselves are inline in slot_read.hpp, shared with the per-frame loop (read_slots.cpp).

#include "actions/slot_read.hpp"

namespace nnh::detail {

SlotOut eval_slot(ActionKind kind, SlotRec& slot, const InputView& v) {
    SlotOut out = read_slot(kind, slot, v);
    if (kind != ActionKind::Vector && slot.cur.kind != Source::Kind::None) out.vec = {out.value, 0.0f};
    return out;
}

bool slot_engaged(const SlotRec& slot, const InputView& v, float threshold) { return engaged(slot, v, threshold); }

} // namespace nnh::detail
