#pragma once
// The live value lines of a script (script_live.cpp): argument reading and the paused-section rules for
// script_parser.cpp.

#include <string>
#include <vector>

#include "remote/script.hpp"
#include "remote/script_tokens.hpp"

namespace nnh::tool::remote {

bool is_live_command(Command c);
/// Fills `step` from the arguments of a live command (count already checked). @return "" or the error.
std::string read_live_arguments(const std::vector<Token>& a, Step& step);
/// "" or the error of a command that moves the clock while the script has the game paused.
std::string paused_rule(const ScriptParser& parser, const Step& step);
/// Updates the parser's paused state after `step`.
void remember_live(ScriptParser& parser, const Step& step);

} // namespace nnh::tool::remote
