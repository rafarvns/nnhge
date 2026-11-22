// Path helpers of nnh-assetc and the PNG header reader.

#include <string>

#include "files.hpp"
#include "paths.hpp"
#include "png_size.hpp"
#include "test.hpp"
#include "tiled_fixture.hpp"

using namespace nnh::assetc;

NNH_TEST(paths_normalize_separators_dots_and_parents) {
    CHECK(normalize_path("a\\b//c/./d") == "a/b/c/d");
    CHECK(normalize_path("a/b/../c") == "a/c");
    CHECK(normalize_path("../a/../../b") == "../../b");
    CHECK(normalize_path("/x/../../y") == "/y");
    CHECK(normalize_path("C:\\game\\assets\\..\\x") == "C:/game/x");
    CHECK(normalize_path("C:/../x") == "C:/x");
    CHECK(normalize_path("a/") == "a");
    CHECK(normalize_path("./") == "");
}

NNH_TEST(paths_know_absolute_paths) {
    CHECK(is_absolute_path("/a"));
    CHECK(is_absolute_path("C:/a"));
    CHECK(is_absolute_path("c:\\a"));
    CHECK(!is_absolute_path("a/b"));
    CHECK(!is_absolute_path("C:a"));
    CHECK(!is_absolute_path(""));
}

NNH_TEST(paths_join_relative_and_absolute) {
    CHECK(parent_path("a/b/c.tmj") == "a/b");
    CHECK(parent_path("c.tmj") == "");
    CHECK(parent_path("/c.tmj") == "/");
    CHECK(join_path("/g/assets/levels", "../tiles/w.tsj") == "/g/assets/tiles/w.tsj");
    CHECK(join_path("/g/assets/levels", "/other/w.tsj") == "/other/w.tsj");
    CHECK(join_path("/g/assets/levels", "E:\\x\\w.tsj") == "E:/x/w.tsj");
    CHECK(join_path("", "w.tsj") == "w.tsj");
}

NNH_TEST(paths_inside_a_root_only_strictly) {
    std::string inside;
    CHECK(path_inside("/g/assets", "/g/assets/tiles/w.png", inside) && inside == "tiles/w.png");
    CHECK(!path_inside("/g/assets", "/g/assets", inside));
    CHECK(!path_inside("/g/assets", "/g/assets2/w.png", inside));
    CHECK(!path_inside("/g/assets", "/g/w.png", inside));
    CHECK(path_inside("C:/g", "C:/g/w.png", inside) && inside == "w.png");
}

NNH_TEST(paths_relative_from_a_folder) {
    CHECK(relative_path("/g/build/x", "/g/build/x/a.png") == "a.png");
    CHECK(relative_path("/g/build/x", "/g/assets/t/w.tsj") == "../../assets/t/w.tsj");
    CHECK(relative_path("/g/build", "/g/buildings/a") == "../buildings/a");
    CHECK(relative_path("/", "/a/b") == "a/b");
    CHECK(relative_path("E:\\g\\build", "e:/g/assets/w.png") == "../assets/w.png"); // drive letters: any case
    CHECK(relative_path("C:/g", "E:/g/w.png") == "E:/g/w.png");                     // another drive: unchanged
    CHECK(relative_path("/g/x", "/g/x") == ".");
}

NNH_TEST(png_size_reads_the_ihdr) {
    int w = 0, h = 0;
    CHECK(png_size(tiled_fixture::png(256, 64), w, h) && w == 256 && h == 64);
    CHECK(png_size(tiled_fixture::png(1, 1).substr(0, kPngHeaderBytes), w, h) && w == 1 && h == 1);
}

NNH_TEST(png_size_refuses_other_bytes) {
    int w = 0, h = 0;
    const std::string good = tiled_fixture::png(16, 16);
    CHECK(!png_size(good.substr(0, kPngHeaderBytes - 1), w, h));
    CHECK(!png_size("GIF89a................................", w, h));
    std::string not_ihdr = good;
    not_ihdr[12] = 'X';
    CHECK(!png_size(not_ihdr, w, h));
    std::string short_chunk = good;
    short_chunk[11] = 12;
    CHECK(!png_size(short_chunk, w, h));
    CHECK(!png_size(tiled_fixture::png(0, 16), w, h));
    CHECK(!png_size(tiled_fixture::png(16, 0), w, h));
    CHECK(!png_size(tiled_fixture::png(0x80000000u, 16), w, h));
    CHECK(!png_size(tiled_fixture::png(16, 0x80000000u), w, h));
}

NNH_TEST(memory_files_read_a_prefix_or_fail) {
    MemoryFiles files;
    files.add("a.txt", "hello");
    std::string out;
    CHECK(files.read("a.txt", 3, out) && out == "hel");
    CHECK(files.read("a.txt", 100, out) && out == "hello");
    CHECK(!files.read("b.txt", 100, out));
}

NNH_TEST(disk_files_read_a_prefix_or_fail) {
    DiskFiles files;
    std::string out;
    CHECK(!files.read(NNH_ASSETC_TEST_TMP "/no_such_file.bin", 10, out));
    CHECK(tiled_fixture::write_file(NNH_ASSETC_TEST_TMP "/disk_files.bin", std::string("ab\0cd", 5)));
    CHECK(files.read(NNH_ASSETC_TEST_TMP "/disk_files.bin", 100, out) && out == std::string("ab\0cd", 5));
    CHECK(files.read(NNH_ASSETC_TEST_TMP "/disk_files.bin", 2, out) && out == "ab");
}
