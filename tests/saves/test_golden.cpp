// Golden files (spec 027 T6.2, AC-2, FR-4, FR-18): tests/data/saves/v1.sav, v2.sav, v3.sav were written by the three
// versions of the reference save (make_saves_golden). The v3 code loads each to its recorded checksum and report, the
// reference values encode to v3.sav's payload byte for byte, and each file dumps to its committed text. The same on
// every host compiler (and the console: the check mode's lines).

#include <cstdio>
#include <string>

#include "fs/save_format.hpp"
#include "saves/progress_sum.hpp"
#include "saves/reference.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;

namespace ex = saves_example;

namespace {

// Recorded at stage 6 from pc-debug (WinLibs GCC), checked on every compiler since.
struct Golden {
    int version;
    std::uint32_t sum;   ///< progress_sum() of the file loaded by the v3 code
    int migrated;
    std::size_t file_size;
    std::uint32_t crc;   ///< the NNHS header's CRC-32 of the payload
};
constexpr Golden kGolden[3] = {
    {1, 0x0F3BA8C8u, 2, 1785, 0x692F89F1u},
    {2, 0x1EFDB382u, 1, 3606, 0xBB78EEE1u},
    {3, 0xD15774A7u, 0, 3784, 0x11363B5Cu},
};

Bytes read_file(const std::string& name) {
    Bytes b;
    std::FILE* f = std::fopen((std::string(NNH_TEST_DATA_DIR) + "/saves/" + name).c_str(), "rb");
    if (f == nullptr) return b;
    std::uint8_t chunk[1024];
    std::size_t n = 0;
    while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0) b.insert(b.end(), chunk, chunk + n);
    std::fclose(f);
    return b;
}

/// The payload of a golden file (its NNHS header checked).
Bytes payload_of(int version) {
    const Bytes file = read_file("v" + std::to_string(version) + ".sav");
    const auto view = nnh::detail::validate_save(file.data(), file.size());
    if (!view) return {};
    CHECK(view.value().version == 1);
    return Bytes(view.value().payload, view.value().payload + view.value().size);
}

void add_line(void* context, const char* line) {
    std::string& text = *static_cast<std::string*>(context);
    text += line;
    text += '\n';
}

} // namespace

NNH_TEST(save_golden_files_load_to_their_recorded_sums) {
    for (const Golden& g : kGolden) {
        const Bytes file = read_file("v" + std::to_string(g.version) + ".sav");
        CHECK(file.size() == g.file_size);
        const Bytes payload = payload_of(g.version);
        CHECK(!payload.empty() && nnh::detail::crc32(payload.data(), payload.size()) == g.crc);
        static ex::Progress p;
        const auto r = nnh::save::decode(payload.data(), payload.size(), p);
        CHECK(r.ok());
        if (!r.ok()) continue;
        const std::uint32_t sum = ex::progress_sum(p);
        if (sum != g.sum) std::printf("    v%d sum %08X (recorded %08X)\n", g.version, sum, g.sum);
        CHECK(sum == g.sum && r.value().version == static_cast<std::uint32_t>(g.version));
        CHECK(r.value().migrated == g.migrated && r.value().unknown == 0 && r.value().dropped == 0);
        CHECK(r.value().adjusted == 0 && !r.value().used_backup);
    }
}

NNH_TEST(save_golden_v3_is_the_reference_encoded_byte_for_byte) {
    CHECK(encoded(ex::reference_v3()) == payload_of(3));
    CHECK(encoded(ex::reference_v2()) == payload_of(2));
    CHECK(encoded(ex::reference_v1()) == payload_of(1));
}

NNH_TEST(save_golden_v1_and_v2_loads_equal_the_migrations_by_hand) {
    static ex::Progress p;
    const Bytes v1 = payload_of(1);
    CHECK(nnh::save::decode(v1.data(), v1.size(), p).ok());
    CHECK(ex::progress_sum(p) == ex::progress_sum(ex::reference_v1_migrated()));
}

#if !defined(NNH_FLAVOR_RELEASE)
NNH_TEST(save_golden_files_dump_to_their_committed_text) {
    for (int version = 1; version <= 3; ++version) {
        const Bytes payload = payload_of(version);
        std::string text;
        CHECK(nnh::save::dump_bytes<ex::Progress>(payload.data(), payload.size(), add_line, &text).ok());
        const Bytes committed = read_file("v" + std::to_string(version) + ".txt");
        CHECK(!committed.empty() && text == std::string(committed.begin(), committed.end()));
    }
}
#endif
