#pragma once
// The message when a game or NNH Link speaks another protocol version than this nnh (spec 033 FR-22, D56 item 7): it
// names both versions, this nnh's engine version and the project's, and the fix (the nnh of the same release, or the
// pin). One builder for nnh run (script and typed modes, PC and console) and nnh serve. Pure; tested in
// tools/nnh/tests/test_protocol_mismatch.cpp.

#include <optional>
#include <string>

namespace nnh::tool {

enum class ProtocolSide { Remote, Link };

/// `theirs`: the version the game (or NNH Link) speaks, nullopt when it only refused ours; `ours`: this nnh's;
/// `nnh_version`: this nnh's engine version; `project_engine`: the project's engine version ("" = unknown; unused for
/// Link).
std::string protocol_mismatch_message(ProtocolSide side, std::optional<int> theirs, int ours,
                                      const std::string& nnh_version, const std::string& project_engine);

/// nnh serve's line after a console's session (link/'s serve_console): a refusal for the link protocol version
/// ("the console speaks link protocol version N") becomes the Link message above; anything else is kept.
std::string serve_end_text(const std::string& end, const std::string& nnh_version);

} // namespace nnh::tool
