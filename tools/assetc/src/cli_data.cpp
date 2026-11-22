// `nnh-assetc data` (cli.hpp): one asset of a game with a data schema — converted if its extension is a
// declared key, copied otherwise — plus its references sidecar.

#include <cstdio>
#include <cstring>
#include <string>

#include "cli_internal.hpp"
#include "data/data_format.hpp"
#include "data_convert.hpp"
#include "data_refs.hpp"
#include "files.hpp"
#include "toml.hpp"

namespace nnh::assetc {

namespace {

struct DataArgs {
    std::string in, out, schema, refs, name, show;
};

bool parse(int argc, const char* const* argv, DataArgs& a) {
    if (argc < 4) return false;
    a.in = argv[2];
    a.out = argv[3];
    for (int i = 4; i < argc; i += 2) {
        if (i + 1 >= argc) return false;
        const char* v = argv[i + 1];
        if (std::strcmp(argv[i], "--schema") == 0)
            a.schema = v;
        else if (std::strcmp(argv[i], "--refs") == 0)
            a.refs = v;
        else if (std::strcmp(argv[i], "--name") == 0)
            a.name = v;
        else if (std::strcmp(argv[i], "--show") == 0)
            a.show = v;
        else
            return false;
    }
    if (a.show.empty()) a.show = a.in;
    return !a.schema.empty() && !a.refs.empty() && !a.name.empty();
}

/// The extension of the file name `path` ends with ("" if none).
std::string extension(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.rfind('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "";
    return path.substr(dot + 1);
}

/// Both outputs, or neither (run_data removes them on failure). Unchanged bytes keep their file times.
int write_outputs(const DataArgs& a, const std::string& bytes, const std::string& refs, std::FILE* err) {
    const char* failed = nullptr;
    if (!write_file_if_changed(a.out, bytes.data(), bytes.size()))
        failed = a.out.c_str();
    else if (!write_file_if_changed(a.refs, refs.data(), refs.size()))
        failed = a.refs.c_str();
    if (failed == nullptr) return 0;
    std::fprintf(err, "%s: can't write the file\n", failed);
    return 1;
}

int convert(const DataArgs& a, const Schema& schema, const detail::TypeInfo& type, const std::string& text,
            std::FILE* err) {
    ConvertedData converted;
    Diagnostics problems;
    if (!convert_data(schema, type, text, converted, problems)) {
        std::fputs(problems.text(a.show).c_str(), err);
        return 1;
    }
    std::string name = a.name;
    const std::string ext = "." + std::string(type.key);
    if (name.size() > ext.size() && name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
        name.resize(name.size() - ext.size());
    }
    const std::string bytes(converted.bytes.begin(), converted.bytes.end());
    return write_outputs(a, bytes, refs_text(name, type.key, bytes.size(), converted.references), err);
}

int run_checked(const DataArgs& a, std::FILE* err) {
    DiskFiles files;
    std::string description;
    if (!files.read(a.schema, kMaxDescriptionBytes + 1, description)) {
        std::fprintf(err, "%s: can't read the file\n", a.schema.c_str());
        return 1;
    }
    Schema schema;
    std::string problem;
    if (!read_schema(description, schema, problem)) {
        std::fprintf(err, "%s: %s\n", a.schema.c_str(), problem.c_str());
        return 1;
    }
    const detail::TypeInfo* type = schema.type_by_key(extension(a.in));
    // Data files are bounded by the reader (64 KiB); other assets are copied whole.
    const std::size_t limit = type != nullptr ? toml::kMaxFileBytes + 1 : static_cast<std::size_t>(-1);
    std::string text;
    if (!files.read(a.in, limit, text)) {
        std::fprintf(err, "%s: can't read the file\n", a.in.c_str());
        return 1;
    }
    if (type == nullptr) return write_outputs(a, text, empty_refs_text(), err);
    return convert(a, schema, *type, text, err);
}

} // namespace

int run_data(int argc, const char* const* argv, std::FILE* err) {
    DataArgs a;
    if (!parse(argc, argv, a)) return 2;
    const int code = run_checked(a, err);
    if (code != 0) { // a failed conversion leaves nothing to pack (and reruns next build)
        std::remove(a.out.c_str());
        std::remove(a.refs.c_str());
    }
    return code;
}

} // namespace nnh::assetc
