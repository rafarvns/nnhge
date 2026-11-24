// Console remote input (see remote_input.hpp).

#include "remote/remote_input.hpp"

#include "input/stick.hpp"
#include "nnh/input.hpp"

namespace nnh::detail::remote {

// The wire's button mask is exactly the nnh::Button bits (A = bit 0 … Right = bit 13); remote_wire.hpp is pure
// and can't include <nnh/input.hpp>, so the link is checked here.
static_assert(static_cast<std::uint32_t>(Button::A) == 1u, "the wire's bit 0 is Button::A");
static_assert(kButtonMask == (static_cast<std::uint32_t>(Button::Right) << 1) - 1u,
              "the wire's button mask covers every nnh::Button, Right being the highest bit");

namespace {

float axis(std::int16_t thousandths) { return static_cast<float>(thousandths) / 1000.0f; }

/// Replaces the real (x, y) with the remote one when the remote stick is pushed further.
void merge_stick(float& x, float& y, std::int16_t remote_x, std::int16_t remote_y) {
    const float rx = axis(remote_x), ry = axis(remote_y);
    if (rx * rx + ry * ry > x * x + y * y) {
        x = rx;
        y = ry;
    }
}

} // namespace

void RemoteInput::apply(const Message& m) {
    switch (m.type) {
    case MessageType::Buttons: buttons = (buttons | m.press) & ~m.release; break;
    case MessageType::Stick:
        (m.stick == 0 ? circle_x : cstick_x) = m.x;
        (m.stick == 0 ? circle_y : cstick_y) = m.y;
        break;
    case MessageType::Touch:
        touching = m.down;
        touch_x = m.x;
        touch_y = m.y;
        break;
    default: break;
    }
}

void RemoteInput::merge_into(platform::InputSample& sample) const {
    sample.buttons |= buttons;
    merge_stick(sample.circle_x, sample.circle_y, circle_x, circle_y);
    merge_stick(sample.cstick_x, sample.cstick_y, cstick_x, cstick_y);
    if (touching) {
        sample.touching = true;
        sample.touch_x = touch_x;
        sample.touch_y = touch_y;
    }
}

void RemoteInput::replace_into(platform::InputSample& sample) const {
    sample = platform::InputSample{};
    merge_into(sample); // over an idle reading the merge is the remote controls themselves
}

RestReport rest_report(const platform::InputSample& physical) {
    const Stick circle = shape_stick(physical.circle_x, physical.circle_y);
    const Stick cstick = shape_stick(physical.cstick_x, physical.cstick_y);
    RestReport report;
    report.circle_moved = circle.x != 0.0f || circle.y != 0.0f;
    report.cstick_moved = cstick.x != 0.0f || cstick.y != 0.0f;
    report.buttons = physical.buttons;
    report.touching = physical.touching;
    return report;
}

} // namespace nnh::detail::remote
