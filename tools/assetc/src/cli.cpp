// The nnh-assetc command line (see cli.hpp).

#include "cli.hpp"

#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "cli_internal.hpp"
#include "convert.hpp"
#include "convert_mesh.hpp"
#include "convert_sprite.hpp"
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
    "       nnh-assetc map <in.tmj> <out.nnhmap> --assets <assets folder> [--depfile <out.d>]\n"
    "       nnh-assetc mesh <in.gltf|in.glb> <out.nnhmesh> --assets <assets folder> [--depfile <out.d>]\n"
    "       nnh-assetc data <in> <out> --schema <schema> --refs <out.refs> --name <asset name> [--show <path>]\n"
    "       nnh-assetc data-index --out <.nnh_data> --schema <schema> --assets-list <file> --refs-list <file>\n"
    "                  [--show <assets folder>]\n"
    "       nnh-assetc font <in.font|in.fnt> <out.nnhfont> --assets <assets folder> --show <asset name>\n"
    "                  [--depfile <out.d>] [--strings <code>=<string file>]...\n";

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

using ConvertFn = bool (*)(FileSource&, const std::string&, const std::string&, Converted&, Problem&);

/// `map` (Tiled) and `sprite` (Aseprite, spec 016): convert, write the output and the depfile.
int run_convert(ConvertFn convert, const MapArgs& a, std::FILE* err) {
    std::remove(a.out.c_str()); // a failed conversion leaves nothing to pack
    DiskFiles files;
    Converted converted;
    Problem problem;
    if (!convert(files, a.in, a.assets, converted, problem)) {
        std::fprintf(err, "%s\n", problem_text(problem).c_str()); // `file: reason` or `file:line: reason`
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

std::vector<std::string> g_mesh_notes; // the last model's ignored features (spec 026)

bool convert_mesh_noting(FileSource& files, const std::string& in, const std::string& assets, Converted& out,
                         Problem& problem) {
    g_mesh_notes.clear();
    return convert_mesh(files, in, assets, out, problem, &g_mesh_notes);
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
    if (argc >= 2 && std::strcmp(argv[1], "map") == 0 && parse_map_args(argc, argv, args)) {
        return run_convert(convert_map, args, err);
    }
    if (argc >= 2 && std::strcmp(argv[1], "sprite") == 0 && parse_map_args(argc, argv, args)) {
        return run_convert(convert_sprite, args, err);
    }
    if (argc >= 2 && std::strcmp(argv[1], "mesh") == 0 && parse_map_args(argc, argv, args)) {
        const int code = run_convert(convert_mesh_noting, args, err);
        for (const std::string& note : g_mesh_notes) std::fprintf(out, "%s: note: %s\n", args.in.c_str(), note.c_str());
        return code;
    }
    if (argc >= 2 && std::strcmp(argv[1], "data") == 0) {
        const int code = run_data(argc, argv, err);
        if (code != 2) return code;
    }
    if (argc >= 2 && std::strcmp(argv[1], "data-index") == 0) {
        const int code = run_data_index(argc, argv, err);
        if (code != 2) return code;
    }
    if (argc >= 2 && std::strcmp(argv[1], "font") == 0) { // spec 029
        const int code = run_font(argc, argv, out, err);
        if (code != 2) return code;
    }
    if (argc >= 2 && std::strcmp(argv[1], "strings") == 0) {
        const int code = run_strings(argc, argv, out, err);
        if (code != 2) return code;
    }
    if (argc >= 2 && std::strcmp(argv[1], "strings-index") == 0) {
        const int code = run_strings_index(argc, argv, out, err);
        if (code != 2) return code;
    }
    std::fputs(kUsage, err);
    return 2;
}

} // namespace nnh::assetc
