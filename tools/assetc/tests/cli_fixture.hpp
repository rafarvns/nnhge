#pragma once
// Shared by the command-line tests: runs nnh-assetc's run_cli() in-process with its output captured.

#include <cstdio>
#include <string>
#include <vector>

#include "cli.hpp"
#include "files.hpp"

namespace cli_test {

struct Run {
    int code = 0;
    std::string out, err;
};

inline std::string read_back(std::FILE* f) {
    std::string text;
    std::rewind(f);
    char buffer[512];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0)
        text.append(buffer, got);
    std::fclose(f);
    return text;
}

inline Run run(const std::vector<std::string>& args) {
    std::vector<const char*> argv{"nnh-assetc"};
    for (const std::string& a : args)
        argv.push_back(a.c_str());
    std::FILE* out = std::tmpfile();
    std::FILE* err = std::tmpfile();
    Run r;
    r.code = nnh::assetc::run_cli(static_cast<int>(argv.size()), argv.data(), out, err);
    r.out = read_back(out);
    r.err = read_back(err);
    return r;
}

inline bool exists(const std::string& path) {
    std::string bytes;
    return nnh::assetc::DiskFiles{}.read(path, 1, bytes);
}

inline std::string read(const std::string& path) {
    std::string bytes;
    nnh::assetc::DiskFiles{}.read(path, 1 << 20, bytes);
    return bytes;
}

} // namespace cli_test
