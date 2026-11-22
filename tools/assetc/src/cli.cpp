// The nnh-assetc command line (see cli.hpp).

#include "cli.hpp"

#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "cli_internal.hpp"
#include "convert.hpp"
#include "core/crc32.hpp" // shared with core (pure file): the version line proves which CRC the maps get
#include "depfile.hpp"
#include "files.hpp"
#include "paths.hpp"

#ifndef NNH_ASSETC_VERSION
#error "NNH_ASSETC_VERSION is set by tools/assetc/CMakeLists.txt"
#endif

namespace nnh::assetc {

namespace {

constexpr const char* kUsage =
    "usage: nnh-assetc --version\n"
    "       nnh-assetc map <in.tmj> <out.nnhmap> --assets <assets folder> [--depfile <out.d>]\n";

struct MapArgs {
    std::string in, out, assets, depfile;
};

bool parse_map_args(int argc, const char* const* argv, MapArgs& a) {
    if (argc < 4) return false;
    a.in = argv[2];
    a.out = argv[3];
    for (int i = 4; i < argc; i += 2) {
        if (i + 1 >= argc) return false;
        if (std::strcmp(argv[i], "--assets") == 0) a.assets = argv[i + 1];
        else if (std::strcmp(argv[i], "--depfile") == 0) a.depfile = argv[i + 1];
        else return false;
    }
    return !a.assets.empty();
}

/// Depfile paths are relative to the working directory, where CMake reads them from: devkitPro's MSYS CMake
/// would take this native program's absolute "E:/..." paths for relative ones. Unchanged if it can't be known.
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

int run_map(const MapArgs& a, std::FILE* err) {
    std::remove(a.out.c_str()); // a failed conversion leaves nothing to pack
    DiskFiles files;
    Converted converted;
    Problem problem;
    if (!convert_map(files, a.in, a.assets, converted, problem)) {
        std::fprintf(err, "%s: %s\n", problem.file.c_str(), problem.reason.c_str());
        return 1;
    }
    const char* failed = nullptr;
    if (!write_file(a.out, converted.bytes.data(), converted.bytes.size())) {
        failed = a.out.c_str();
    } else if (!a.depfile.empty()) {
        const std::string text = depfile_text(from_working_dir(a.out), from_working_dir(converted.dependencies));
        if (!write_file(a.depfile, text.data(), text.size())) failed = a.depfile.c_str();
    }
    if (failed != nullptr) {
        std::remove(a.out.c_str());
        std::fprintf(err, "%s: can't write the file\n", failed);
        return 1;
    }
    return 0;
}

} // namespace

int run_cli(int argc, const char* const* argv, std::FILE* out, std::FILE* err) {
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
        // The check value proves the shared CRC-32 is the one core uses (0xCBF43926).
        std::fprintf(out, "nnh-assetc %s (crc32 %08X)\n", NNH_ASSETC_VERSION,
                     static_cast<unsigned>(nnh::detail::crc32("123456789", 9)));
        return 0;
    }
    MapArgs args;
    if (argc >= 2 && std::strcmp(argv[1], "map") == 0 && parse_map_args(argc, argv, args)) return run_map(args, err);
    if (argc >= 2 && std::strcmp(argv[1], "data") == 0) {
        const int code = run_data(argc, argv, err);
        if (code != 2) return code;
    }
    std::fputs(kUsage, err);
    return 2;
}

} // namespace nnh::assetc
