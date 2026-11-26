// Script parser (see script.hpp). Words come from script_tokens.cpp; this file knows the commands.

#include <string>

#include "remote/remote_wire.hpp"
#include "remote/script.hpp"
#include "remote/script_tokens.hpp"

namespace nnh::tool::remote {
namespace {

struct CommandInfo {
    const char* name;
    Command command;
    std::size_t args;     ///< at least
    std::size_t max_args; ///< at most
    const char* usage;
};

constexpr CommandInfo kCommands[] = {
    {"wait", Command::Wait, 1, 1, "wait N"},
    {"press", Command::Press, 1, 1, "press B[+B...]"},
    {"release", Command::Release, 1, 1, "release B[+B...] | release all"},
    {"hold", Command::Hold, 2, 2, "hold B[+B...] N"},
    {"tap", Command::Tap, 1, 1, "tap B[+B...]"},
    {"stick", Command::Stick, 3, 3, "stick circle|cstick X Y"},
    {"touch", Command::Touch, 2, 2, "touch X Y"},
    {"drag", Command::Drag, 3, 3, "drag X Y N"},
    {"untouch", Command::Untouch, 0, 0, "untouch"},
    {"screenshot", Command::Screenshot, 1, 2, "screenshot NAME [eyes]"},
    {"expect", Command::Expect, 2, 2, "expect \"TEXT\" N"},
    {"fail-on", Command::FailOn, 1, 1, "fail-on \"TEXT\""},
    {"end", Command::End, 0, 0, "end"},
    {"slider", Command::Slider, 1, 1, "slider V|off"},
    {"language", Command::Language, 1, 1, "language CODE|off"},
};

const CommandInfo* find_command(const std::string& name) {
    for (const CommandInfo& c : kCommands) {
        if (name == c.name) return &c;
    }
    return nullptr;
}

bool is_input(Command c) {
    return c != Command::Wait && c != Command::Expect && c != Command::FailOn && c != Command::End;
}

const char* const kFramesError = "frames must be a whole number 1 to 216000";
const char* const kTouchError = "touch is X 0 to 319, Y 0 to 239";
const char* const kStickError = "stick values are -1 to 1 (at most 3 decimals)";

/// Reads a TEXT argument. @return "" or the error.
std::string text_argument(const Token& t, std::string& out) {
    if (!t.quoted) return "TEXT must be in double quotes";
    if (t.text.empty()) return "TEXT is empty";
    if (t.text.size() > kMaxTextChars) return "TEXT is longer than 128 characters";
    out = t.text;
    return "";
}

/// Fills `step` from the arguments of its command (count already checked). @return "" or the error.
std::string read_arguments(const std::vector<Token>& a, Step& step) {
    for (std::size_t i = 1; i < a.size(); ++i) {
        const bool text_slot = (step.command == Command::Expect || step.command == Command::FailOn) && i == 1;
        if (a[i].quoted && !text_slot) return "only TEXT goes in double quotes";
    }
    switch (step.command) {
    case Command::Wait: return parse_frames(a[1].text, step.frames) ? "" : kFramesError;
    case Command::Release:
        if (lower(a[1].text) == "all") {
            step.command = Command::ReleaseAll;
            return "";
        }
        return parse_buttons(a[1].text, step.buttons);
    case Command::Press:
    case Command::Tap: return parse_buttons(a[1].text, step.buttons);
    case Command::Hold: {
        const std::string e = parse_buttons(a[1].text, step.buttons);
        if (!e.empty()) return e;
        return parse_frames(a[2].text, step.frames) ? "" : kFramesError;
    }
    case Command::Stick: {
        const std::string which = lower(a[1].text);
        if (which != "circle" && which != "cstick") return "stick is 'circle' or 'cstick'";
        step.stick = which == "cstick" ? 1 : 0;
        const bool ok = parse_stick_value(a[2].text, step.x) && parse_stick_value(a[3].text, step.y);
        return ok ? "" : kStickError;
    }
    case Command::Touch:
    case Command::Drag: {
        if (!parse_index(a[1].text, 319, step.x) || !parse_index(a[2].text, 239, step.y)) return kTouchError;
        if (step.command == Command::Drag && !parse_frames(a[3].text, step.frames)) return kFramesError;
        return "";
    }
    case Command::Screenshot:
        step.text = a[1].text;
        if (!valid_name(a[1].text)) return "NAME is 1 to 32 of A-Z a-z 0-9 _ -";
        step.eyes = a.size() == 3;
        if (step.eyes && lower(a[2].text) != "eyes") return "the only option of screenshot is 'eyes' (both eyes of the top screen)";
        return "";
    case Command::Slider:
        if (lower(a[1].text) == "off") {
            step.command = Command::SliderOff;
            return "";
        }
        return a[1].text[0] != '-' && parse_stick_value(a[1].text, step.x) ? ""
                                                                         : "slider is 0 to 1 (at most 3 decimals) or off";
    case Command::Language: {
        if (lower(a[1].text) == "off") {
            step.command = Command::LanguageOff;
            return "";
        }
        detail::remote::Message probe; // the code must fit one Language message (well-formed, at most 8 bytes)
        step.text = a[1].text;
        return detail::remote::pack_language(step.text.c_str(), probe)
                   ? ""
                   : "language is a code like ja, pt-BR or zh-Hant (at most 8 characters) or off";
    }
    case Command::Expect: {
        const std::string e = text_argument(a[1], step.text);
        if (!e.empty()) return e;
        return parse_frames(a[2].text, step.frames) ? "" : kFramesError;
    }
    case Command::FailOn: return text_argument(a[1], step.text);
    default: return "";
    }
}

/// Updates what the parser remembers after `step` was accepted; may add the late-input warning.
void remember(ScriptParser& parser, const Step& step, Script& script) {
    if (is_input(step.command) && parser.expect_line > 0 && !parser.warned &&
        parser.since_expect < kExpectMargin) {
        parser.warned = true;
        script.warnings.push_back(parser.file + ":" + std::to_string(step.line) + ": warning: input " +
                                  std::to_string(parser.since_expect) + " frames after the expect on line " +
                                  std::to_string(parser.expect_line) +
                                  " will probably reach the console late; wait 10 frames or more first");
    }
    // Only `wait` can come between an expect and its first input (any input gets the check itself).
    if (step.command == Command::Wait) parser.since_expect += step.frames;
    switch (step.command) {
    case Command::Touch: parser.touching = true; break;
    case Command::Untouch:
    case Command::ReleaseAll: parser.touching = false; break;
    case Command::Expect:
        parser.expect_line = step.line;
        parser.since_expect = 0;
        parser.warned = false;
        break;
    case Command::End: parser.ended = true; break;
    default: break;
    }
}

/// @return "" and fills `step`, or the error of this line.
std::string parse_command(const ScriptParser& parser, const std::vector<Token>& tokens, Step& step) {
    if (parser.ended) return "nothing may follow end";
    const std::string name = lower(tokens[0].text);
    const CommandInfo* info = tokens[0].quoted ? nullptr : find_command(name);
    if (info == nullptr) return "unknown command '" + tokens[0].text + "'";
    if (tokens.size() < info->args + 1 || tokens.size() > info->max_args + 1) {
        return std::string("wrong number of arguments; usage: ") + info->usage;
    }
    step.command = info->command;
    const std::string error = read_arguments(tokens, step);
    if (!error.empty()) return error;
    if (step.command == Command::Drag && !parser.touching) return "drag needs a touch down first (touch X Y)";
    return "";
}

} // namespace

void parse_script_line(ScriptParser& parser, std::string_view text, int line, Script& script,
                       std::vector<std::string>& errors) {
    const std::string where = parser.file + ":" + std::to_string(line) + ": ";
    if (text.size() > kMaxLineChars) {
        errors.push_back(where + "the line is longer than 256 characters");
        return;
    }
    std::vector<Token> tokens;
    std::string error = tokenize(text, tokens);
    if (error.empty() && tokens.empty()) return;
    Step step;
    step.line = line;
    if (error.empty()) error = parse_command(parser, tokens, step);
    if (!error.empty()) {
        errors.push_back(where + error);
        return;
    }
    remember(parser, step, script);
    script.steps.push_back(std::move(step));
}

ParsedScript parse_script(std::string_view text, const std::string& file) {
    ParsedScript out;
    out.script.file = file;
    if (text.size() > kMaxScriptBytes) {
        out.errors.push_back(file + ": the script is larger than 64 KB");
        return out;
    }
    std::vector<std::string_view> lines;
    for (std::size_t start = 0; start < text.size();) {
        std::size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) nl = text.size();
        std::string_view l = text.substr(start, nl - start);
        if (!l.empty() && l.back() == '\r') l.remove_suffix(1);
        lines.push_back(l);
        start = nl + 1;
    }
    if (lines.size() > static_cast<std::size_t>(kMaxScriptLines)) {
        out.errors.push_back(file + ":" + std::to_string(kMaxScriptLines + 1) +
                             ": the script has more than 4096 lines");
        return out;
    }
    out.script.lines = static_cast<int>(lines.size());
    ScriptParser parser{.file = file};
    for (std::size_t i = 0; i < lines.size(); ++i) {
        parse_script_line(parser, lines[i], static_cast<int>(i + 1), out.script, out.errors);
    }
    if (!parser.ended) {
        Step end;
        end.line = out.script.lines > 0 ? out.script.lines : 1;
        out.script.steps.push_back(end);
    }
    return out;
}

} // namespace nnh::tool::remote
