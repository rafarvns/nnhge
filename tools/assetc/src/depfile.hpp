#pragma once
// Ninja/Make depfiles (spec 012 constitution III): the files a converted asset was made from, so editing a shared
// tileset reconverts exactly the maps that read it (add_custom_command(... DEPFILE ...)).

#include <string>
#include <vector>

namespace nnh::assetc {

/// "<target>: <input> <input>...\n", with ' ' and '#' escaped by a backslash and '$' doubled.
std::string depfile_text(const std::string& target, const std::vector<std::string>& inputs);

} // namespace nnh::assetc
