#pragma once
// The hot reload lines of a script (spec 031 FR-22, Q11; script_reload.cpp), for script_parser.cpp:
//   swap-asset SOURCE FIXTURE [unheld]   SOURCE relative to the game's folder, FIXTURE to the script's
//   restore-asset SOURCE
//   expect-reload [PATH] [refused] [within N]
// Held swaps and expect-reload without `within` take no frame of the script clock; `unheld` swaps and `within` wait.
// None of them is allowed while the script has the game paused (`resume` first).

#include <string>
#include <vector>

#include "remote/script.hpp"
#include "remote/script_tokens.hpp"

namespace nnh::tool::remote {

bool is_reload_command(Command c);
/// Fills `step` from the arguments of a reload command (count already checked). @return "" or the error.
std::string read_reload_arguments(const std::vector<Token>& a, Step& step);

} // namespace nnh::tool::remote
