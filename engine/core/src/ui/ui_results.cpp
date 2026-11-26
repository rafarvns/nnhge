// Results (spec 028 FR-20): what happened this frame, in a fixed order (focus changed, activated, value changed,
// refused, blocked, cancelled, page) by a stable sort at the end of the update; per-key and per-widget queries; the
// widgets' callbacks, called after the sort in results order (a callback may change the UI: its changes are laid out
// before update() returns; it may not destroy the UI); then the opt-in bus events.

#include "ui/ui_block.hpp"

namespace nnh::detail {

void add_result(UiBlock& b, UiResultKind kind, int s, std::int32_t value, std::int32_t index, int other) {
    if (b.result_count >= b.results_capacity) {
        ++b.dropped_results;
        NNH_UI_MISUSE(b, "Ui: more results in one frame than UiDesc::results");
        return;
    }
    UiResult& r = b.results[b.result_count++];
    r = UiResult{};
    r.kind = kind;
    r.widget = s >= 0 ? handle_of(b, s) : UiWidget{};
    r.key = s >= 0 ? UiKey(static_cast<int>(b.hot[s].key)) : UiKey{};
    r.other = other >= 0 ? handle_of(b, other) : UiWidget{};
    r.value = value;
    r.index = index;
}

void finish_results(UiBlock& b) {
    // stable insertion sort by kind (a few results per frame)
    for (int i = 1; i < b.result_count; ++i) {
        const UiResult r = b.results[i];
        int j = i;
        while (j > 0 && static_cast<int>(b.results[j - 1].kind) > static_cast<int>(r.kind)) {
            b.results[j] = b.results[j - 1];
            --j;
        }
        b.results[j] = r;
    }
    layout_if_dirty(b);
    ++b.busy;
    for (int i = 0; i < b.result_count; ++i) {
        const UiResult r = b.results[i];
        if (r.kind != UiResultKind::Activated && r.kind != UiResultKind::ValueChanged) continue;
        const int s = slot_of(b, r.widget); // an earlier callback may have destroyed it
        if (s < 0) continue;
        const std::uint8_t slot = r.kind == UiResultKind::Activated ? b.cold[s].on_activate : b.cold[s].on_change;
        if (slot == kUiNoSlot || slot >= b.fns_capacity || b.fn_used[slot] == 0) continue;
        const UiCallback cb = fn_at<UiCallback>(b, slot); // a copy: the callback may release its own slot
        cb(r);
    }
    --b.busy;
    if (b.publish != nullptr) b.publish(b);
    if (b.prompt_hook != nullptr) b.prompt_hook(b);
    layout_if_dirty(b);
}

} // namespace nnh::detail

namespace nnh {

UiResults Ui::results() const {
    if (block_ == nullptr) return {};
    return UiResults{block_->results, block_->result_count};
}

namespace {

bool any(const detail::UiBlock* b, UiResultKind kind, UiKey k, UiWidget w) {
    if (b == nullptr) return false;
    for (int i = 0; i < b->result_count; ++i) {
        const UiResult& r = b->results[i];
        if (r.kind == kind && (w ? r.widget == w : (!k.none() && r.key == k))) return true;
    }
    return false;
}

} // namespace

bool Ui::activated(UiKey k) const { return any(block_, UiResultKind::Activated, k, {}); }
bool Ui::activated(UiWidget w) const { return w && any(block_, UiResultKind::Activated, {}, w); }
bool Ui::changed(UiKey k) const { return any(block_, UiResultKind::ValueChanged, k, {}); }

bool Ui::cancelled() const {
    if (block_ == nullptr) return false;
    for (int i = 0; i < block_->result_count; ++i) {
        if (block_->results[i].kind == UiResultKind::Cancelled) return true;
    }
    return false;
}

int Ui::value(UiKey k) const {
    const UiWidget w = find(k);
    return w ? value(w) : 0;
}

} // namespace nnh
