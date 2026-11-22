// Listening for the next control (spec 017 FR-16; plan 017 "Semantics"): controls held at the start are ignored
// until released; each evaluation checks the timeout, then the cancel buttons, then captures the first newly-down
// control the slot can take (buttons in bit order, then Circle Pad, then C-Stick directions). While listening the
// whole set reads idle (action_eval.cpp).

#include "actions/actions_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh {
namespace detail {
namespace {

constexpr float kCapture = 0.5f;
constexpr float kForget = 0.4f;
constexpr std::uint32_t kZButtons = static_cast<std::uint32_t>(Button::ZL) | static_cast<std::uint32_t>(Button::ZR);

bool has(std::uint8_t caps, Capability c) { return (caps & (1u << static_cast<unsigned>(c))) != 0; }

std::uint8_t dirs_past(const InputView& v, float threshold) {
    std::uint8_t bits = 0;
    for (int d = 0; d < 8; ++d) {
        if (v.push[d] >= threshold) bits = static_cast<std::uint8_t>(bits | (1u << d));
    }
    return bits;
}

void finish(ActionBlock& b, ListenState state) {
    b.listen.active = false;
    b.result.state = state;
}

/// The source this slot takes from the newly-down controls, or None. `invert` is set for a negative axis push.
Source pick(const ActionBlock& b, std::uint32_t buttons, std::uint8_t dirs, bool& invert) {
    const ListenRec& l = b.listen;
    const ActionKind kind = b.actions[l.action].kind;
    const bool takes_buttons = kind == ActionKind::Button || l.options.part != Part::Whole;
    const bool takes_sticks = l.options.sticks && l.options.part == Part::Whole;
    if (takes_buttons && buttons != 0) {
        for (int i = 0; i < 14; ++i) {
            if ((buttons & (1u << i)) != 0) return Source::button(static_cast<Button>(1u << i));
        }
    }
    if (!takes_sticks || dirs == 0) return {};
    int d = 0;
    while ((dirs & (1u << d)) == 0) ++d;
    const auto stick = static_cast<AnalogStick>(d / 4);
    const auto dir = static_cast<Direction>(d % 4);
    if (kind == ActionKind::Button) return Source::stick_direction(stick, dir);
    if (kind == ActionKind::Vector) return Source::stick(stick);
    invert = dir == Direction::Down || dir == Direction::Left;
    return Source::stick_axis(stick, dir == Direction::Up || dir == Direction::Down ? StickAxis::Y : StickAxis::X);
}

} // namespace

void listen_step(ActionBlock& b, const InputView& v) {
    ListenRec& l = b.listen;
    l.ignored &= v.held; // forgotten once released
    l.ignored_dirs = static_cast<std::uint8_t>(l.ignored_dirs & dirs_past(v, kForget));
    if (l.options.timeout_frames != 0 && ++l.frames > l.options.timeout_frames) return finish(b, ListenState::TimedOut);
    const std::uint32_t down = v.held & ~l.ignored;
    if ((down & l.options.cancel) != 0) return finish(b, ListenState::Cancelled);
    std::uint32_t buttons = down & ~l.options.exclude & 0x3FFFu;
    if (!has(b.caps, Capability::ZButtons)) buttons &= ~kZButtons;
    std::uint8_t dirs = static_cast<std::uint8_t>(dirs_past(v, kCapture) & ~l.ignored_dirs);
    if (!has(b.caps, Capability::CirclePad)) dirs &= 0xF0;
    if (!has(b.caps, Capability::CStick)) dirs &= 0x0F;
    bool invert = false;
    const Source s = pick(b, buttons, dirs, invert);
    if (s.kind == Source::Kind::None) return;
    l.active = false;
    b.result = assign(b, l.action, l.slot, l.options.part, s, l.options.on_conflict);
    if (b.result.state == ListenState::Bound && s.kind == Source::Kind::StickAxis) {
        SlotRec& slot = b.actions[l.action].slots[l.slot];
        slot.flags = static_cast<std::uint8_t>(invert ? slot.flags | slot_flag::kInvertX : slot.flags & ~slot_flag::kInvertX);
    }
}

} // namespace detail

void ActionSet::listen(ActionId a, int slot, const ListenOptions& options) {
    detail::SlotRec* s = detail::find_slot(block_, a, slot);
    if (s == nullptr) return;
    const detail::ActionRec& r = block_->actions[a.index];
    const bool part_ok = options.part == Part::Whole || detail::part_fits(r.kind, options.part);
    if ((s->flags & detail::slot_flag::kLocked) != 0 || !part_ok) {
        NNH_LOG_ERROR(actions, "ActionSet: listen() on a locked slot or a part of the wrong kind (\"%s\" slot %d)",
                      r.name, slot);
        NNH_ASSERT(false, "nnh::ActionSet: listen() on a locked slot or a part that doesn't fit the action");
        return;
    }
    const detail::InputView v = detail::current_input_view();
    detail::ListenRec& l = block_->listen;
    l = {};
    l.active = true;
    l.action = a.index;
    l.slot = static_cast<std::uint8_t>(slot);
    l.options = options;
    l.ignored = v.held;
    l.ignored_dirs = detail::dirs_past(v, detail::kCapture);
    block_->result = {};
    block_->result.state = ListenState::Listening;
    block_->result.action = a.index;
    block_->result.slot = static_cast<std::uint8_t>(slot);
    block_->result.part = options.part;
}

void ActionSet::stop_listening() {
    if (block_ != nullptr && block_->listen.active) detail::finish(*block_, ListenState::Cancelled);
}

bool ActionSet::listening() const { return block_ != nullptr && block_->listen.active; }

const RemapResult& ActionSet::listen_result() const {
    static const RemapResult kIdle{};
    return block_ != nullptr ? block_->result : kIdle;
}

void ActionSet::clear_listen_result() {
    if (block_ != nullptr && !block_->listen.active) block_->result = {};
}

} // namespace nnh
