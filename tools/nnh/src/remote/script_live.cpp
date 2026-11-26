// The live value lines of a script (spec 030 FR-12, FR-16; plan "Script commands"): set, reset, action, dump, pause,
// resume, step [N], expect-value NAME OP VALUE [~ E] [within N], and the paused-section rules (a command that moves
// the clock while the game is paused is an error naming its line). Names are checked by the game's rules here; whether
// the game declares them is known only when their segment is sent (the runner). Pure; tested in
// tools/nnh/tests/test_script_live.cpp.

#include <charconv>
#include <cmath>
#include <string>

#include "live/live_names.hpp"
#include "remote/live_wire.hpp"
#include "remote/script.hpp"
#include "remote/script_live.hpp"
#include "remote/script_tokens.hpp"

namespace nnh::tool::remote {
namespace {

std::string name_argument(const Token& t, std::string& out) {
    if (t.quoted) return "NAME isn't quoted";
    const auto e = detail::live::check_name(t.text.data(), t.text.size());
    if (e != detail::live_name::Error::None) return "name \"" + t.text + "\" " + detail::live::name_error_text(e);
    out = t.text;
    return "";
}

bool compare_op(const std::string& s, std::uint8_t& op) {
    static constexpr const char* kOps[] = {"==", "!=", "<", "<=", ">", ">="};
    for (std::uint8_t i = 0; i < 6; ++i) {
        if (s == kOps[i]) {
            op = i;
            return true;
        }
    }
    return false;
}

bool parse_double(const std::string& s, double& out) {
    const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size() && !s.empty();
}

std::string expect_value(const std::vector<Token>& a, Step& step) {
    std::string e = name_argument(a[1], step.text);
    if (!e.empty()) return e;
    if (a[2].quoted || !compare_op(a[2].text, step.compare)) return "OP is one of == != < <= > >=";
    step.value = a[3].text;
    for (std::size_t i = 4; i < a.size();) {
        if (a[i].text == "~" && i + 1 < a.size()) {
            if (!parse_double(a[i + 1].text, step.tolerance) || !std::isfinite(step.tolerance) || step.tolerance < 0) {
                return "the tolerance after ~ is a number 0 or more";
            }
            i += 2;
        } else if (a[i].text == "within" && i + 1 < a.size()) {
            if (!parse_frames(a[i + 1].text, step.frames)) return "frames must be a whole number 1 to 216000";
            i += 2;
        } else {
            return "usage: expect-value NAME OP VALUE [~ E] [within N]";
        }
    }
    return "";
}

} // namespace

bool is_live_command(Command c) {
    return c == Command::Set || c == Command::Reset || c == Command::Action || c == Command::Dump ||
           c == Command::Pause || c == Command::Resume || c == Command::StepFrames || c == Command::ExpectValue;
}

std::string read_live_arguments(const std::vector<Token>& a, Step& step) {
    switch (step.command) {
    case Command::Set: {
        const std::string e = name_argument(a[1], step.text);
        if (!e.empty()) return e;
        step.value = a[2].text; // quoted or not: a colour must be quoted, `#` starts a comment
        return "";
    }
    case Command::Reset:
        if (a[1].text == "all") {
            step.text = "all";
            return "";
        }
        return name_argument(a[1], step.text);
    case Command::Action: return name_argument(a[1], step.text);
    case Command::StepFrames: {
        if (a.size() == 1) {
            step.frames = 1;
            return "";
        }
        return parse_frames(a[1].text, step.frames) && step.frames <= detail::remote::kMaxStepFrames
                   ? ""
                   : "step is 1 to 3600 frames";
    }
    case Command::ExpectValue: return expect_value(a, step);
    default: return "";
    }
}

std::string paused_rule(const ScriptParser& parser, const Step& step) {
    if (!parser.paused) return "";
    const char* what = nullptr;
    switch (step.command) {
    case Command::Wait: what = "wait"; break;
    case Command::Hold: what = "hold"; break;
    case Command::Tap: what = "tap"; break;
    case Command::Drag: what = "drag"; break;
    case Command::Expect: what = "expect"; break;
    case Command::ExpectValue:
        if (step.frames > 0) what = "expect-value ... within";
        break;
    default: break;
    }
    return what == nullptr ? "" : std::string(what) + " while paused: step or resume first";
}

void remember_live(ScriptParser& parser, const Step& step) {
    if (step.command == Command::Pause || step.command == Command::StepFrames) parser.paused = true;
    if (step.command == Command::Resume) parser.paused = false;
}

} // namespace nnh::tool::remote
