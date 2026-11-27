// PC platform only (spec 031, Linux finding): the Debug/Development data folder sits next to the program, so it must
// never share a name with it. On Linux the `data` example's program is the file `<exe dir>/data`, and a data folder
// `<exe dir>/data/data/` could not be created: hot reload's `.nnh_reload/<n>.bin` failed ("could not write the file").
// Linked into nnh_conformance_pc.

#include <cstdio>
#include <cstring>
#include <filesystem>

#include "data_root_pc.hpp"
#include "nnh/platform/fs.hpp"
#include "stdio_fs.hpp"
#include "test.hpp"

namespace fs = std::filesystem;

NNH_TEST(pc_data_root_never_is_the_programs_own_name) {
    char root[256];
    CHECK(nnh::platform::pc::dev_data_root("/games/build/", "data", root, sizeof root));
    CHECK(std::strncmp(root, "/games/build/data/", 18) != 0); // `/games/build/data` is the program on Linux
    CHECK(std::strstr(root, "/data/") != nullptr);           // the folder is still named after the game
    CHECK(std::strstr(root, "/nnh_data/") == nullptr); // the build's own folder (data assets' refs, schema programs)
    CHECK(!nnh::platform::pc::dev_data_root("/games/build/", "data", root, 8)); // too long: refused, not cut
}

NNH_TEST(pc_received_files_can_be_created_next_to_a_program_named_like_its_data_folder) {
    std::error_code ec;
    const fs::path base = fs::current_path(ec) / "data_root_test";
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    std::FILE* program = std::fopen((base / "data").string().c_str(), "wb"); // the Linux executable `data`
    CHECK(program != nullptr);
    if (program != nullptr) std::fclose(program);
    char root[256];
    const std::string prefix = base.generic_string() + "/";
    CHECK(nnh::platform::pc::dev_data_root(prefix.c_str(), "data", root, sizeof root));
    CHECK(nnh::platform::shared::stdio_fs_set_root(root));
    CHECK(nnh::platform::fs_write(".nnh_reload/1.bin", nullptr, 0)); // what a reload's first file does
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == 0);
    (void)nnh::platform::shared::stdio_fs_set_root(""); // the working folder again (returns false: nothing to make)
    fs::remove_all(base, ec);
}
