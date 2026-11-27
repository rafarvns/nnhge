#include "text_file.hpp"

#include <cstdio>

namespace nnh::tool {

std::string read_text_file(const std::string& path, std::size_t limit) {
    std::string text;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return text;
    char buffer[4096];
    std::size_t n = 0;
    while (text.size() <= limit && (n = std::fread(buffer, 1, sizeof buffer, f)) > 0) text.append(buffer, n);
    std::fclose(f);
    return text;
}

} // namespace nnh::tool
