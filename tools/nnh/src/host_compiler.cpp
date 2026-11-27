#include "host_compiler.hpp"

#include <cstdlib>
#include <filesystem>

namespace nnh::tool {

std::string find_host_cxx(const char* env_value, const std::function<std::string(const char*)>& find_on_path) {
    if (env_value != nullptr && *env_value != 0) return env_value;
    for (const char* name : {"g++", "c++", "clang++"}) {
        std::string path = find_on_path(name);
        if (!path.empty()) return path;
    }
    return {};
}

std::string find_on_path(const char* name) {
    const char* path = std::getenv("PATH");
    if (path == nullptr) return {};
#ifdef _WIN32
    const char separator = ';';
    const std::string file = std::string(name) + ".exe";
#else
    const char separator = ':';
    const std::string file = name;
#endif
    const std::string list = path;
    std::error_code ec;
    for (std::size_t start = 0; start <= list.size();) {
        std::size_t end = list.find(separator, start);
        if (end == std::string::npos) end = list.size();
        const std::string dir = list.substr(start, end - start);
        start = end + 1;
        if (dir.empty()) continue;
        const std::filesystem::path candidate = std::filesystem::path(dir) / file;
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate.generic_string();
    }
    return {};
}

ConfigureExtras configure_extras(bool game, bool devkitpro_shell, const char* engine_env, const char* host_env,
                                 const std::function<std::string(const char*)>& find_on_path) {
    ConfigureExtras extras;
    if (!devkitpro_shell) return extras; // PC presets map NNHGE_ENGINE_DIR themselves; Linux CMake finds a compiler
    extras.host_cxx = find_host_cxx(host_env, find_on_path);
    if (game && engine_env != nullptr) extras.engine_dir = engine_env;
    return extras;
}

} // namespace nnh::tool
