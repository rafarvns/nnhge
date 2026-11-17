// Path validation (nnh::fs::validate_path): the only paths the engine will touch.

#include "nnh/fs.hpp"
#include "test.hpp"

using nnh::fs::validate_path;

NNH_TEST(simple_relative_paths_are_accepted) {
    const char* ok[] = {"save.bin", "saves/slot1.bin", "a/b/c/d.txt", "log.prev.txt", "x-y_z.9"};
    for (const char* p : ok) CHECK(validate_path(p).ok());
}

NNH_TEST(paths_escaping_or_naming_devices_are_rejected) {
    const char* bad[] = {"",           "/save.bin",  "../save.bin", "saves/../../x", "sdmc:/x",
                         "C:\\x",      "a\\b",       "a//b",        "./a",           "a/./b",
                         "saves/",     "..",         ".",           "a/b/c/d/e/f",   "with space",
                         "tab\tname",  "uni\xC3\xA9", "a:b"};
    for (const char* p : bad) CHECK(!validate_path(p).ok());
}

NNH_TEST(too_long_paths_are_rejected) {
    char path[nnh::fs::kMaxPathLength + 8];
    for (char& c : path) c = 'a';
    path[sizeof path - 1] = '\0';
    CHECK(!validate_path(path).ok());
}

NNH_TEST(rejection_explains_why) {
    const auto r = validate_path("../x");
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::InvalidArgument);
}
