// main() of a game's data schema program (spec 014 FR-1, FR-3, D29; cmake/nnh_data.cmake). The program is the
// game's schema sources (their NNH_DATA_* tables register themselves before main) plus this file and core's pure
// registry files, compiled for the host and never linked against the engine.
//
//   <game>_data_schema <out.nnhschema>
//
// Checks the whole schema (duplicate keys, unknown nested types, depth, counts: data/schema_check.hpp); on a
// problem prints every one as "<schema source>: <reason>" and exits with 1, leaving the old description alone
// (the build stops anyway). Otherwise writes the schema description (data/schema_write.hpp) — only if its text
// changed, so editing a schema source without changing the schema (a comment, a default value) reconverts no
// data file (Ninja `restat`). Usage errors exit with 2. The tests' program is checked by ctest
// data_schema_program_*.

#include <cstdio>
#include <cstring>
#include <string>

#include "data/schema_check.hpp"
#include "data/schema_write.hpp"

namespace {

void print_issue(void*, const char* message) {
    std::fprintf(stderr, "%s\n", message);
}

void append(void* context, const char* text, std::size_t size) {
    static_cast<std::string*>(context)->append(text, size);
}

/// The file's whole content, or "" (and false) when it can't be read.
bool read_all(const char* path, std::string& out) {
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return false;
    char buffer[4096];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0)
        out.append(buffer, got);
    std::fclose(f);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fputs("usage: <game>_data_schema <out.nnhschema>\n", stderr);
        return 2;
    }
    const nnh::detail::DataRegistry& registry = nnh::detail::data_registry();
    if (nnh::detail::check_schema(registry, print_issue, nullptr) != 0) return 1;
    std::string text;
    nnh::detail::write_schema(registry, append, &text);
    std::string old;
    if (read_all(argv[1], old) && old == text) return 0;
    std::FILE* f = std::fopen(argv[1], "wb");
    const bool written = f != nullptr && std::fwrite(text.data(), 1, text.size(), f) == text.size();
    const bool closed = f != nullptr && std::fclose(f) == 0;
    if (!written || !closed) {
        std::fprintf(stderr, "%s: can't write the file\n", argv[1]);
        return 1;
    }
    return 0;
}
