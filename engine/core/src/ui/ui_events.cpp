// Opt-in bus events (spec 028 FR-20): with set_events(true), each update publishes UiActivated, UiValueChanged,
// UiFocusChanged and UiCancelled for its results, after the callbacks (delivered at the next frame's dispatch). The only
// UI file that includes <nnh/events.hpp>: linked only by games that call set_events.

#include "nnh/events.hpp"
#include "ui/ui_block.hpp"

namespace nnh {
namespace detail {
namespace {

static_assert(sizeof(UiActivated) <= 16 && sizeof(UiValueChanged) <= 16 && sizeof(UiFocusChanged) <= 16 &&
                  sizeof(UiCancelled) <= 16,
              "UI events are at most 16 bytes");

void publish_results(const UiBlock& b) {
    for (int i = 0; i < b.result_count; ++i) {
        const UiResult& r = b.results[i];
        switch (r.kind) {
        case UiResultKind::Activated: events::publish(UiActivated{b.serial, r.key, r.widget.bits(), r.index}); break;
        case UiResultKind::ValueChanged:
            events::publish(UiValueChanged{b.serial, r.key, r.widget.bits(), r.value});
            break;
        case UiResultKind::FocusChanged:
            events::publish(UiFocusChanged{b.serial, r.key, r.widget.bits(), r.other.bits()});
            break;
        case UiResultKind::Cancelled: events::publish(UiCancelled{b.serial, r.key}); break;
        default: break;
        }
    }
}

} // namespace
} // namespace detail

void Ui::set_events(bool publish) {
    if (block_ == nullptr) return;
    block_->publish = publish ? &detail::publish_results : nullptr;
}

} // namespace nnh
