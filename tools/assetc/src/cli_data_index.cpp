// `nnh-assetc data-index` (cli.hpp, cli_internal.hpp): the references sidecars of every data asset and the
// game's asset list, checked together and written as the data index `.nnh_data`.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "cli_internal.hpp"
#include "data_index_build.hpp"
#include "files.hpp"

namespace nnh::assetc {

namespace {

constexpr std::size_t kMaxListBytes = 4 * 1024 * 1024;    ///< the asset list or the list of sidecars
constexpr std::size_t kMaxSidecarBytes = 1024 * 1024;

struct IndexArgs {
    std::string out, schema, assets_list, refs_list, show = "assets";
};

bool parse(int argc, const char* const* argv, IndexArgs& a) {
    for (int i = 2; i < argc; i += 2) {
        if (i + 1 >= argc) return false;
        const char* v = argv[i + 1];
        if (std::strcmp(argv[i], "--out") == 0) a.out = v;
        else if (std::strcmp(argv[i], "--schema") == 0) a.schema = v;
        else if (std::strcmp(argv[i], "--assets-list") == 0) a.assets_list = v;
        else if (std::strcmp(argv[i], "--refs-list") == 0) a.refs_list = v;
        else if (std::strcmp(argv[i], "--show") == 0) a.show = v;
        else return false;
    }
    return !a.out.empty() && !a.schema.empty() && !a.assets_list.empty() && !a.refs_list.empty();
}

/// Reads a file of at most `max` bytes; prints "<path>: can't read the file" otherwise.
bool read_bounded(const std::string& path, std::size_t max, std::string& out, std::FILE* err) {
    if (DiskFiles{}.read(path, max + 1, out) && out.size() <= max) return true;
    std::fprintf(err, "%s: can't read the file\n", path.c_str());
    return false;
}

/// The non-empty lines of `text` (LF or CRLF).
std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t lf = text.find('\n', at);
        if (lf == std::string::npos) lf = text.size();
        std::string line = text.substr(at, lf - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) out.push_back(std::move(line));
        at = lf + 1;
    }
    return out;
}

bool read_inputs(const IndexArgs& a, IndexInput& in, Schema& schema, std::FILE* err) {
    std::string text;
    if (!read_bounded(a.schema, kMaxDescriptionBytes, text, err)) return false;
    std::string problem;
    if (!read_schema(text, schema, problem)) {
        std::fprintf(err, "%s: %s\n", a.schema.c_str(), problem.c_str());
        return false;
    }
    in.schema = &schema;
    if (!read_bounded(a.assets_list, kMaxListBytes, text, err)) return false;
    in.assets = lines_of(text);
    if (!read_bounded(a.refs_list, kMaxListBytes, text, err)) return false;
    // Relative sidecar paths are relative to the list's folder (the build writes them so: devkitPro's MSYS CMake
    // would write absolute paths this native program reads as relative ones).
    const std::filesystem::path base = std::filesystem::path(a.refs_list).parent_path();
    for (const std::string& line : lines_of(text)) {
        const std::filesystem::path p(line);
        const std::string path = p.is_absolute() || base.empty() ? line : (base / p).generic_string();
        if (!read_bounded(path, kMaxSidecarBytes, text, err)) return false;
        RefsFile f;
        if (!read_refs(text, f, problem)) {
            std::fprintf(err, "%s: %s\n", path.c_str(), problem.c_str());
            return false;
        }
        if (f.data) in.data.push_back(std::move(f));
    }
    in.shown = a.show;
    return true;
}

int run_checked(const IndexArgs& a, std::FILE* err) {
    Schema schema;
    IndexInput in;
    if (!read_inputs(a, in, schema, err)) return 1;
    IndexModel model;
    std::string errors;
    if (!build_index(in, model, errors)) {
        std::fputs(errors.c_str(), err);
        return 1;
    }
    std::string problem;
    const std::vector<std::uint8_t> bytes = write_index(model, problem);
    if (bytes.empty()) {
        std::fprintf(err, "%s: %s\n", a.out.c_str(), problem.c_str());
        return 1;
    }
    if (!write_file_if_changed(a.out, bytes.data(), bytes.size())) {
        std::fprintf(err, "%s: can't write the file\n", a.out.c_str());
        return 1;
    }
    return 0;
}

} // namespace

int run_data_index(int argc, const char* const* argv, std::FILE* err) {
    IndexArgs a;
    if (!parse(argc, argv, a)) return 2;
    const int code = run_checked(a, err);
    if (code != 0) std::remove(a.out.c_str()); // no index from a failed build (and the step reruns)
    return code;
}

} // namespace nnh::assetc
