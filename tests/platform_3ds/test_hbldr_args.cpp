// Arguments for Luma3DS's homebrew loader (engine/platforms/3ds/src/hbldr_args.hpp, spec 011 FR-4).
// The block layout is the Homebrew Launcher's: a 32-bit argument count, then the NUL-terminated strings.

#include <cstdint>
#include <cstring>
#include <string>

#include "hbldr_args.hpp"
#include "test.hpp"

using nnh::platform::ctr::build_hbldr_argv;
using nnh::platform::ctr::HbldrArgv;
using nnh::platform::ctr::hbldr_target;
using nnh::platform::ctr::kHbldrArgvSize;
using nnh::platform::ctr::return_path;

namespace {

std::uint32_t count_of(const HbldrArgv& a) { return a.words[0]; }

/// The i-th string of the block.
std::string arg_at(const HbldrArgv& a, int i) {
    const char* p = reinterpret_cast<const char*>(a.words) + 4;
    for (int k = 0; k < i; ++k) p += std::strlen(p) + 1;
    return p;
}

} // namespace

NNH_TEST(hbldr_argv_starts_with_the_path_then_the_arguments) {
    const char* args[] = {"--nnh-return=sdmc:/3ds/nnh-link.3dsx", "0A0BA8C0_3DSLINK_"};
    HbldrArgv a;
    CHECK(build_hbldr_argv("sdmc:/3ds/nnh/audio.3dsx", args, 2, a));
    CHECK(count_of(a) == 3u);
    CHECK(arg_at(a, 0) == "sdmc:/3ds/nnh/audio.3dsx");
    CHECK(arg_at(a, 1) == "--nnh-return=sdmc:/3ds/nnh-link.3dsx");
    CHECK(arg_at(a, 2) == "0A0BA8C0_3DSLINK_"); // libctru only reads the marker as the last argument
}

NNH_TEST(hbldr_argv_with_no_arguments_has_only_the_path) {
    HbldrArgv a;
    CHECK(build_hbldr_argv("sdmc:/3ds/nnh-link.3dsx", nullptr, 0, a));
    CHECK(count_of(a) == 1u);
    CHECK(arg_at(a, 0) == "sdmc:/3ds/nnh-link.3dsx");
}

NNH_TEST(hbldr_argv_block_is_the_size_the_loader_copies) {
    CHECK(sizeof(HbldrArgv) == kHbldrArgvSize);
    CHECK(kHbldrArgvSize == std::size_t{0x400});
}

NNH_TEST(hbldr_argv_that_does_not_fit_is_refused) {
    const std::string big(kHbldrArgvSize - 4 - 6, 'x'); // with "sdmc:" and the NULs: one byte too many
    const char* args[] = {big.c_str()};
    HbldrArgv a;
    CHECK(!build_hbldr_argv("sdmc:", args, 1, a));
    const std::string fits(kHbldrArgvSize - 4 - 6 - 1, 'x');
    const char* ok[] = {fits.c_str()};
    CHECK(build_hbldr_argv("sdmc:", ok, 1, a));
    CHECK(count_of(a) == 2u);
}

NNH_TEST(hbldr_argv_rejects_a_negative_count_or_null_argument) {
    HbldrArgv a;
    CHECK(!build_hbldr_argv("sdmc:/a.3dsx", nullptr, -1, a));
    const char* args[] = {nullptr};
    CHECK(!build_hbldr_argv("sdmc:/a.3dsx", args, 1, a));
    CHECK(!build_hbldr_argv(nullptr, nullptr, 0, a));
}

NNH_TEST(hbldr_target_drops_the_sd_prefix) {
    CHECK(std::string(hbldr_target("sdmc:/3ds/nnh/audio.3dsx")) == "/3ds/nnh/audio.3dsx");
}

NNH_TEST(hbldr_target_refuses_what_is_not_a_3dsx_on_the_sd_card) {
    CHECK(hbldr_target(nullptr) == nullptr);
    CHECK(hbldr_target("") == nullptr);
    CHECK(hbldr_target("/3ds/a.3dsx") == nullptr);      // no sdmc: prefix
    CHECK(hbldr_target("romfs:/a.3dsx") == nullptr);
    CHECK(hbldr_target("sdmc:/3ds/a.cia") == nullptr);
    CHECK(hbldr_target("sdmc:/.3dsx") == nullptr);      // no name
    CHECK(hbldr_target("sdmc:/3ds/../a.3dsx") == nullptr);
    CHECK(hbldr_target("sdmc:/3ds//a.3dsx") == nullptr);
    const std::string long_path = "sdmc:/" + std::string(250, 'a') + ".3dsx";
    CHECK(hbldr_target(long_path.c_str()) == nullptr);
}

NNH_TEST(return_path_is_found_in_the_arguments) {
    char a0[] = "sdmc:/3ds/nnh/audio.3dsx";
    char a1[] = "--nnh-return=sdmc:/3ds/nnh-link.3dsx";
    char* argv[] = {a0, a1, nullptr};
    CHECK(std::string(return_path(2, argv)) == "sdmc:/3ds/nnh-link.3dsx");
}

NNH_TEST(return_path_is_null_without_the_argument_or_with_a_bad_path) {
    char a0[] = "sdmc:/3ds/nnh/audio.3dsx";
    char* only_path[] = {a0, nullptr};
    CHECK(return_path(1, only_path) == nullptr);
    CHECK(return_path(0, nullptr) == nullptr);
    char bad[] = "--nnh-return=sdmc:/3ds/notes.txt";
    char* with_bad[] = {a0, bad, nullptr};
    CHECK(return_path(2, with_bad) == nullptr);
    char empty[] = "--nnh-return=";
    char* with_empty[] = {a0, empty, nullptr};
    CHECK(return_path(2, with_empty) == nullptr);
}

NNH_TEST(return_path_ignores_argv0) {
    char a0[] = "--nnh-return=sdmc:/3ds/nnh-link.3dsx";
    char* argv[] = {a0, nullptr};
    CHECK(return_path(1, argv) == nullptr);
}
