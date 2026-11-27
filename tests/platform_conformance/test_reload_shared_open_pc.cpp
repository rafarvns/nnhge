// PC platform only (spec 031 T2.2, AC-9): a received file opened with reload_stream_open() stays readable while the
// game replaces it (the atomic save renames over it) and removes it — Windows' default file sharing would refuse both
// while a stream reads it. Linked into nnh_conformance_pc; the file lives in the data folder (fs_root(), the working
// folder when the platform isn't started).

#include <cstring>

#include "nnh/platform/assets.hpp"
#include "nnh/platform/fs.hpp"
#include "nnh/platform/reload.hpp"
#include "test.hpp"

namespace platform = nnh::platform;

namespace {

bool write(const char* path, const char* text) {
    const platform::FsChunk chunk{text, std::strlen(text)};
    return platform::fs_write(path, &chunk, 1);
}

} // namespace

NNH_TEST(pc_reload_stream_survives_a_replace_and_a_remove) {
    const char* path = "reload_shared_open_test.bin";
    CHECK(write(path, "old bytes"));
    platform::AssetStream stream = 0;
    CHECK(platform::reload_stream_open(path, stream));
    if (stream == 0) return;
    char head[4] = {};
    std::size_t read = 0;
    CHECK(platform::asset_read_at(stream, 0, head, 3, read) && read == 3 && std::memcmp(head, "old", 3) == 0);
    CHECK(write(path, "new bytes!")); // the atomic save: a temporary file renamed over the open one
    char again[4] = {};
    CHECK(platform::asset_read_at(stream, 4, again, 3, read) && read == 3 && std::memcmp(again, "byt", 3) == 0);
    CHECK(platform::fs_size(path) == 10);
    CHECK(platform::fs_remove(path));
    platform::asset_close(stream);
    CHECK(platform::fs_size(path) < 0);
}

NNH_TEST(pc_reload_stream_open_of_a_missing_file_fails) {
    platform::AssetStream stream = 1;
    CHECK(!platform::reload_stream_open("reload_shared_open_missing.bin", stream));
    CHECK(stream == 0);
}
