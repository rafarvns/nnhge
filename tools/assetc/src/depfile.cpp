// Ninja/Make depfiles (see depfile.hpp).

#include "depfile.hpp"

#include <filesystem>
#include <system_error>

#include "paths.hpp"

namespace nnh::assetc {

namespace {

std::string escaped(const std::string& path) {
    std::string out;
    for (const char c : path) {
        if (c == ' ' || c == '#') out += '\\';
        if (c == '$') out += '$';
        out += c;
    }
    return out;
}

} // namespace

std::string from_working_dir(const std::string& path) {
    std::error_code error;
    const std::filesystem::path cwd = std::filesystem::current_path(error);
    if (error || !std::filesystem::path(path).is_absolute()) return path;
    return relative_path(cwd.generic_string(), path);
}

std::vector<std::string> from_working_dir(const std::vector<std::string>& paths) {
    std::vector<std::string> out;
    for (const std::string& p : paths) out.push_back(from_working_dir(p));
    return out;
}

std::string depfile_text(const std::string& target, const std::vector<std::string>& inputs) {
    std::string text = escaped(target) + ":";
    for (const std::string& input : inputs) text += " " + escaped(input);
    return text + "\n";
}

} // namespace nnh::assetc
