// Ninja/Make depfiles (see depfile.hpp).

#include "depfile.hpp"

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

std::string depfile_text(const std::string& target, const std::vector<std::string>& inputs) {
    std::string text = escaped(target) + ":";
    for (const std::string& input : inputs) text += " " + escaped(input);
    return text + "\n";
}

} // namespace nnh::assetc
