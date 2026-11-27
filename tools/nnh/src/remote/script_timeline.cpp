// Script timeline (see script_timeline.hpp).

#include "remote/script_timeline.hpp"

#include "nnh/detail/name_hash.hpp"
#include "remote/live_wire.hpp"

namespace nnh::tool::remote {
namespace {

wire::Message buttons(std::uint32_t press, std::uint32_t release) {
    wire::Message m;
    m.type = wire::MessageType::Buttons;
    m.press = press;
    m.release = release;
    return m;
}

wire::Message stick(std::uint8_t which, std::int16_t x, std::int16_t y) {
    wire::Message m;
    m.type = wire::MessageType::Stick;
    m.stick = which;
    m.x = x;
    m.y = y;
    return m;
}

wire::Message touch(bool down, std::int16_t x, std::int16_t y) {
    wire::Message m;
    m.type = wire::MessageType::Touch;
    m.down = down;
    m.x = x;
    m.y = y;
    return m;
}

/// The point `step` of `steps` on the way from `from` to `to` (the last step is exactly `to`).
std::int16_t between(std::int16_t from, std::int16_t to, std::uint32_t step, std::uint32_t steps) {
    const long long d = static_cast<long long>(to - from) * step;
    return static_cast<std::int16_t>(from + d / static_cast<long long>(steps));
}

} // namespace

void Timeline::send(const wire::Message& message, std::uint32_t offset, int line, std::string text) {
    TimedEvent e;
    e.offset = offset;
    e.line = line;
    e.message = message;
    e.text = std::move(text);
    open_.events.push_back(std::move(e));
}

void Timeline::add(const Step& step) {
    const std::uint32_t now = open_.length;
    const int line = step.line;
    switch (step.command) {
    case Command::Wait: open_.length += step.frames; break;
    case Command::Press: send(buttons(step.buttons, 0), now, line); break;
    case Command::Release: send(buttons(0, step.buttons), now, line); break;
    case Command::ReleaseAll:
        send(buttons(0, wire::kButtonMask), now, line);
        send(stick(0, 0, 0), now, line);
        send(stick(1, 0, 0), now, line);
        send(touch(false, touch_x_, touch_y_), now, line);
        break;
    case Command::Hold:
        send(buttons(step.buttons, 0), now, line);
        send(buttons(0, step.buttons), now + step.frames, line);
        open_.length += step.frames;
        break;
    case Command::Tap:
        send(buttons(step.buttons, 0), now, line);
        send(buttons(0, step.buttons), now + 1, line);
        open_.length += 2;
        break;
    case Command::Stick: send(stick(step.stick, step.x, step.y), now, line); break;
    case Command::Touch:
        touch_x_ = step.x;
        touch_y_ = step.y;
        send(touch(true, step.x, step.y), now, line);
        break;
    case Command::Drag:
        for (std::uint32_t i = 1; i <= step.frames; ++i) {
            send(touch(true, between(touch_x_, step.x, i, step.frames), between(touch_y_, step.y, i, step.frames)),
                 now + i, line);
        }
        touch_x_ = step.x;
        touch_y_ = step.y;
        open_.length += step.frames + 1; // the target is held for a frame before what follows
        break;
    case Command::Untouch: send(touch(false, touch_x_, touch_y_), now, line); break;
    case Command::Screenshot: {
        wire::Message m;
        m.type = wire::MessageType::Screenshot;
        m.screens = wire::kScreenTop | wire::kScreenBottom | (step.eyes ? wire::kScreenTopRight : 0);
        send(m, now, line, step.text);
        if (!step.paused) open_.length += 1; // what follows lands after the captured frame (paused: no frame runs)
        break;
    }
    case Command::Slider:
    case Command::SliderOff: {
        wire::Message m;
        m.type = wire::MessageType::Slider;
        m.slider = step.command == Command::SliderOff ? wire::kSliderRelease : static_cast<std::uint16_t>(step.x);
        send(m, now, line);
        break;
    }
    case Command::Language:
    case Command::LanguageOff: {
        wire::Message m;
        m.type = wire::MessageType::Language;
        wire::pack_language(step.command == Command::Language ? step.text.c_str() : nullptr, m); // checked by the parser
        send(m, now, line);
        break;
    }
    case Command::FailOn:
        send(wire::Message{}, now, line, step.text);
        open_.events.back().kind = TimedEvent::Kind::FailOn;
        break;
    case Command::Set:
    case Command::Reset:
    case Command::Action:
    case Command::Dump: {
        const std::uint32_t hash = step.text.empty() || step.text == "all" ? 0 : detail::fnv1a32(step.text.data(), step.text.size());
        wire::Message m = step.command == Command::Set      ? wire::live_set_message(hash, 0)
                          : step.command == Command::Action ? wire::live_message(wire::LiveOp::Action, 0, 0, hash)
                          : step.command == Command::Dump   ? wire::live_message(wire::LiveOp::Dump, 0, 0, 0)
                          : wire::live_message(wire::LiveOp::Reset,
                                               static_cast<std::uint8_t>(hash == 0 ? wire::ResetScope::All
                                                                                   : wire::ResetScope::One),
                                               0, hash);
        send(m, now, line, step.text);
        TimedEvent& e = open_.events.back();
        e.live = true;
        e.live_command = step.command;
        e.value = step.value;
        e.origin = step.origin;
        break;
    }
    case Command::Pause: send(wire::pause_message(wire::PauseOp::Pause, 0), now, line); break;
    case Command::Resume: send(wire::pause_message(wire::PauseOp::Resume, 0), now, line); break;
    case Command::StepFrames:
        send(wire::pause_message(wire::PauseOp::Step, static_cast<std::uint16_t>(step.frames)), now, line);
        open_.length += step.frames;
        break;
    case Command::ExpectValue:
    case Command::Expect:
    case Command::SwapAsset: // spec 031: barriers the runner carries out (script_runner_reload.cpp)
    case Command::RestoreAsset:
    case Command::ExpectReload:
    case Command::End:
        open_.until = step;
        closed_.push_back(std::move(open_));
        open_ = Segment{};
        break;
    }
}

Segment Timeline::take_open() {
    Segment out = std::move(open_);
    open_ = Segment{};
    return out;
}

std::vector<Segment> build_timeline(const Script& script) {
    Timeline t;
    for (const Step& step : script.steps) t.add(step);
    return t.segments();
}

} // namespace nnh::tool::remote
