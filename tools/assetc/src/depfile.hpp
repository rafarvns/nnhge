#pragma once
// Ninja/Make depfiles (spec 012 constitution III): the files a converted asset was made from, so editing a shared
// tileset reconverts exactly the maps that read it (add_custom_command(... DEPFILE ...)).

#include <string>
#include <vector>

namespace nnh::assetc {

/// `path` relative to the working directory, where CMake reads depfile paths from: devkitPro's MSYS CMake/Ninja would
/// take this native program's absolute "E:/..." paths for relative ones. Unchanged if it can't be known.
std::string from_working_dir(const std::string& path);
std::vector<std::string> from_working_dir(const std::vector<std::string>& paths);

/// "<target>: <input> <input>...\n", with ' ' and '#' escaped by a backslash and '$' doubled.
std::string depfile_text(const std::string& target, const std::vector<std::string>& inputs);

} // namespace nnh::assetc
