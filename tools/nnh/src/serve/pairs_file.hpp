#pragma once
// The consoles nnh serve is paired with (spec 011 FR-3): "<console id, 32 hex>=<secret, 64 hex>" lines in
// nnh-pairs.ini next to nnh.ini, readable by the owner only where the OS has such permissions. Parsing is
// pure and unit tested in tools/nnh/tests/test_serve.cpp.

#include <string>
#include <utility>
#include <vector>

#include "link_types.hpp"
#include "nnh_link/messages.hpp"

namespace nnh::tool {

using PairList = std::vector<std::pair<Id, Digest>>;

/// Malformed lines are skipped; a later line for the same console wins.
PairList parse_pairs(const std::string& text);
std::string format_pairs(const PairList& pairs);

/// "<folder of settings_file>/nnh-pairs.ini" ("" if settings_file is "").
std::string pairs_path(const std::string& settings_file);

PairList load_pairs(const std::string& path);
/// Writes the file (owner read/write only). @return false on failure.
bool save_pairs(const std::string& path, const PairList& pairs);

} // namespace nnh::tool
