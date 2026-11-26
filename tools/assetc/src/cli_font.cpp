// `nnh-assetc font` (cli_internal.hpp; spec 029 FR-1..FR-4): one font asset converted, its output rewritten only when it
// changes (Ninja `restat`), its depfile, the report line and notes on stdout; problems on stderr, the old output removed.

#include <cstdio>
#include <cstring>

#include "cli_internal.hpp"
#include "convert_font.hpp"
#include "depfile.hpp"
#include "files.hpp"

namespace nnh::assetc {

int run_font(int argc, const char* const* argv, std::FILE* out, std::FILE* err) {
    if (argc < 4) return 2;
    FontInput in;
    in.path = argv[2];
    const std::string output = argv[3];
    std::string depfile;
    for (int i = 4; i + 1 < argc; i += 2) {
        const std::string value = argv[i + 1];
        if (std::strcmp(argv[i], "--assets") == 0) in.assets = value;
        else if (std::strcmp(argv[i], "--show") == 0) in.shown = value;
        else if (std::strcmp(argv[i], "--depfile") == 0) depfile = value;
        else if (std::strcmp(argv[i], "--strings") == 0 && value.find('=') != std::string::npos) {
            in.strings.emplace_back(value.substr(0, value.find('=')), value.substr(value.find('=') + 1));
        } else {
            return 2;
        }
    }
    if (in.assets.empty() || (argc - 4) % 2 != 0) return 2;
    if (in.shown.empty()) in.shown = in.path;
    DiskFiles files;
    FontOutput result;
    const std::string problems = convert_font(files, in, result);
    if (!problems.empty()) {
        std::fputs(problems.c_str(), err);
        if (problems.back() != '\n') std::fputc('\n', err);
        std::remove(output.c_str());
        return 1;
    }
    if (!write_file_if_changed(output, result.bytes.data(), result.bytes.size())) {
        std::fprintf(err, "%s: can't be written\n", output.c_str());
        return 1;
    }
    if (!depfile.empty()) {
        const std::string text = depfile_text(from_working_dir(output), from_working_dir(result.inputs));
        write_file_if_changed(depfile, text.data(), text.size());
    }
    for (const std::string& note : result.notes) std::fprintf(out, "%s\n", note.c_str());
    std::fprintf(out, "%s\n", result.report.c_str());
    return 0;
}

} // namespace nnh::assetc
