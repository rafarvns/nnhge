// Writes the golden save files of spec 027 (AC-2): tests/data/saves/v1.sav, v2.sav, v3.sav (whole NNHS files: spec
// 002's header, then the structured payload) from examples/saves' reference values, and their dumps v1.txt, v2.txt,
// v3.txt (read with the v3 schema: retired and unknown ids by number). Built by hand, in a Debug or Development
// preset (the dump):
//   cmake --build --preset pc-debug --target make_saves_golden
//   build/pc-debug/tests/make_saves_golden tests/data/saves
// then copy the .sav files into examples/saves/assets/golden/ (ctest saves_golden_copies_match compares them).
// Regenerate only with a reason in the commit message (tests/data/saves/README.md).

#include <cstdio>
#include <string>

#include "core/crc32.hpp"
#include "fs/save_format.hpp"
#include "saves/progress_sum.hpp"
#include "saves/reference.hpp"

namespace {

std::uint8_t g_payload[8192];

void add_line(void* context, const char* line) {
    std::string& text = *static_cast<std::string*>(context);
    text += line;
    text += '\n';
}

template <class T> bool write_golden(const std::string& dir, int version, const T& value) {
    const auto encoded = nnh::save::encode(value, g_payload, sizeof g_payload);
    if (!encoded) return false;
    const std::size_t size = encoded.value();
    std::uint8_t header[nnh::detail::kSaveHeaderSize];
    nnh::detail::build_save_header(header, static_cast<std::uint32_t>(size), nnh::save::kFormatVersion,
                                   nnh::detail::crc32(g_payload, size));
    const std::string base = dir + "/v" + std::to_string(version);
    std::FILE* f = std::fopen((base + ".sav").c_str(), "wb");
    if (f == nullptr) return false;
    const bool ok = std::fwrite(header, 1, sizeof header, f) == sizeof header &&
                    std::fwrite(g_payload, 1, size, f) == size;
    std::fclose(f);
    std::string text;
    if (!ok || !nnh::save::dump_bytes<saves_example::Progress>(g_payload, size, add_line, &text)) return false;
    std::FILE* t = std::fopen((base + ".txt").c_str(), "wb");
    if (t == nullptr) return false;
    const bool written = std::fwrite(text.data(), 1, text.size(), t) == text.size();
    std::fclose(t);
    std::printf("v%d.sav: %u bytes (payload %u), crc32 %08X\n", version, static_cast<unsigned>(16 + size),
                static_cast<unsigned>(size), static_cast<unsigned>(nnh::detail::crc32(g_payload, size)));
    return written;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::printf("usage: make_saves_golden <tests/data/saves>\n");
        return 2;
    }
    const std::string dir = argv[1];
    const bool ok = write_golden(dir, 1, saves_example::reference_v1()) &&
                    write_golden(dir, 2, saves_example::reference_v2()) &&
                    write_golden(dir, 3, saves_example::reference_v3());
    std::printf("%s\n", ok ? "written" : "FAILED");
    return ok ? 0 : 1;
}
