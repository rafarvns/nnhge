// The localization example's self-check on the fake platform (spec 029 T7.2, AC-2): the example's own converted assets
// (fonts, tables, manifest) read from its build folder, every language switched to like the example does, and each
// running sum equal to the checkpoints the example compares on the PC and the console.

#include <cstdio>
#include <string>
#include <vector>

#include "../../examples/localization/self_check.hpp"
#include "../loc/loc_fixture.hpp"
#include "nnh/localization.hpp"
#include "test.hpp"

using namespace loc_test;
using namespace font_test;

namespace {

const std::string kAssets = NNH_LOC_EXAMPLE_ASSETS;

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::vector<std::uint8_t> out;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return out;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return out;
}

/// Every asset of the example's index, in the fake's assets.
bool mount_example() {
    fresh_engine();
    const std::vector<std::uint8_t> index = read_file(kAssets + "/.nnh_assets");
    if (index.empty()) return false;
    std::string line;
    for (const std::uint8_t c : index) {
        if (c != '\n' && c != '\r') {
            line += static_cast<char>(c);
            continue;
        }
        if (!line.empty()) test_assets().push_back({line, &keep(read_file(kAssets + "/" + line))});
        line.clear();
    }
    remount();
    return true;
}

} // namespace

NNH_TEST(loc_example_self_check_matches_its_checkpoints) {
    const StopAtEnd stop_at_end;
    CHECK(mount_example());
    nnh::platform_fake::set_system_language("en");
    nnh::loc::use_fonts();
    CHECK(nnh::loc::start().ok());
    std::uint32_t hash = 2166136261u;
    for (int i = 0; i < loc_check::kLanguages; ++i) {
        CHECK(nnh::loc::set_language(loc_check::kCodes[i]).ok());
        CHECK(nnh::frame()); // the switch at the start of the frame, the role fonts with it
        CHECK(std::string(nnh::loc::code()) == loc_check::kCodes[i]);
        hash = loc_check::language_sum(hash);
        if (hash != loc_check::kCheckpoints[i]) std::printf("    %s sum %08X\n", loc_check::kCodes[i], static_cast<unsigned>(hash));
        CHECK(hash == loc_check::kCheckpoints[i]);
    }
    CHECK(hash == loc_check::kLocCheckGolden);
}
