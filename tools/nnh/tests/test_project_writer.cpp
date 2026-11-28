// Writing a new project: never overwrite, all or nothing (new/project_writer.hpp, spec 033 FR-5, FR-6).

#include "new/project_writer.hpp"
#include "test.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

using namespace nnh::tool;

namespace {

namespace fs = std::filesystem;

fs::path u8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

std::string fresh(const char* name) {
    const fs::path p = fs::path(NNH_TOOL_TEST_TMP) / "writer" / name;
    std::error_code ec;
    fs::remove_all(p, ec);
    fs::create_directories(p);
    return p.generic_string();
}

std::vector<OutputFile> project() {
    return {{"CMakeLists.txt", "project(demo)\n"},
            {"src/main.cpp", "int main() {}\n"},
            {"src/scenes/play_scene.cpp", "// scene\n"},
            {"assets/art/player.png", std::string("\x89PNG\r\n\x1a\n\0\0", 10)},
            {".gitignore", "build/\n"}};
}

std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

/// Names, sizes and times of everything below `root` (the AC-1 listing).
std::string listing(const fs::path& root) {
    std::string out;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator();
         ++it) {
        const auto& e = *it;
        out += e.path().generic_string() + "|" +
               (e.is_regular_file() ? std::to_string(e.file_size()) : "dir") + "|" +
               std::to_string(e.last_write_time().time_since_epoch().count()) + "\n";
    }
    return out;
}

bool has(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

/// The real file system with create_file failing on call `fail_at` (0-based) and create_folder on
/// `fail_folder_at`.
NewFs failing_fs(int fail_at, int fail_folder_at = -1) {
    NewFs real = system_new_fs();
    auto files = std::make_shared<int>(0);
    auto folders = std::make_shared<int>(0);
    NewFs f = real;
    f.create_file = [real, files, fail_at](const std::string& p, const std::string& b) {
        return (*files)++ == fail_at ? false : real.create_file(p, b);
    };
    f.create_folder = [real, folders, fail_folder_at](const std::string& p) {
        return (*folders)++ == fail_folder_at ? false : real.create_folder(p);
    };
    return f;
}

} // namespace

NNH_TEST(a_new_folder_gets_every_file_and_sub_folder) {
    const std::string base = fresh("new");
    const WriteResult r = write_project(base + "/demo", project(), system_new_fs());
    CHECK(r.ok && r.error.empty());
    for (const OutputFile& f : project())
        CHECK(read(fs::path(base) / "demo" / f.path) == f.bytes);
}

NNH_TEST(an_existing_empty_folder_is_filled) {
    const std::string base = fresh("empty");
    fs::create_directories(fs::path(base) / "demo");
    const WriteResult r = write_project(base + "/demo", project(), system_new_fs());
    CHECK(r.ok);
    CHECK(read(fs::path(base) / "demo" / "src" / "main.cpp") == "int main() {}\n");
}

NNH_TEST(an_existing_file_or_non_empty_folder_is_refused_unchanged) {
    const std::string base = fresh("refused");
    std::ofstream(fs::path(base) / "file") << "mine";
    fs::create_directories(fs::path(base) / "full" / "sub");
    std::ofstream(fs::path(base) / "full" / "keep.txt") << "keep";
    const std::string before = listing(base);
    WriteResult r = write_project(base + "/file", project(), system_new_fs());
    CHECK(!r.ok && has(r.error, "/file exists and is a file") && has(r.error, "nothing was changed"));
    r = write_project(base + "/full", project(), system_new_fs());
    CHECK(!r.ok && has(r.error, "/full exists and is not empty") && has(r.error, "nothing was changed"));
    CHECK(listing(base) == before);
}

NNH_TEST(a_link_is_refused_unchanged) {
    NewFs f = system_new_fs();
    f.kind = [](const std::string&) { return PathKind::Link; }; // the rule, on any system
    const std::string base = fresh("link");
    WriteResult r = write_project(base + "/demo", project(), f);
    CHECK(!r.ok && has(r.error, "is a link") && listing(base).empty());
    // A real link where the system lets us make one (Windows needs developer mode for symbolic links).
    std::error_code ec;
    fs::create_directories(fs::path(base) / "target");
    fs::create_directory_symlink(fs::path(base) / "target", fs::path(base) / "demo", ec);
    if (ec) {
        std::printf("     (no symbolic link made here: %s; the rule was checked with a fake)\n",
                    ec.message().c_str());
        return;
    }
    const std::string before = listing(base);
    r = write_project(base + "/demo", project(), system_new_fs());
    CHECK(!r.ok && has(r.error, "is a link") && listing(base) == before);
}

NNH_TEST(a_failing_file_at_any_index_leaves_nothing_behind) {
    const std::size_t n = project().size();
    for (int at = 0; at < static_cast<int>(n); ++at) {
        const std::string base = fresh("fail_new");
        const WriteResult r = write_project(base + "/demo", project(), failing_fs(at));
        CHECK(!r.ok && has(r.error, project()[static_cast<std::size_t>(at)].path.c_str()));
        CHECK(has(r.error, "removed"));
        CHECK(!fs::exists(fs::path(base) / "demo")); // the folder nnh made is gone too
    }
}

NNH_TEST(a_failure_in_an_existing_empty_folder_leaves_it_empty) {
    for (int at = 0; at < 5; ++at) {
        const std::string base = fresh("fail_empty");
        fs::create_directories(fs::path(base) / "demo");
        const WriteResult r = write_project(base + "/demo", project(), failing_fs(at));
        CHECK(!r.ok && fs::is_directory(fs::path(base) / "demo") && fs::is_empty(fs::path(base) / "demo"));
    }
}

NNH_TEST(a_failing_sub_folder_leaves_nothing_behind) {
    for (int at = 0; at < 4; ++at) { // the project folder, src, src/scenes, assets ...
        const std::string base = fresh("fail_folder");
        const WriteResult r = write_project(base + "/demo", project(), failing_fs(-1, at));
        CHECK(!r.ok && !fs::exists(fs::path(base) / "demo"));
    }
}

NNH_TEST(the_system_create_is_exclusive) {
    const std::string base = fresh("exclusive");
    const NewFs f = system_new_fs();
    CHECK(f.create_file(base + "/a.txt", "first"));
    CHECK(!f.create_file(base + "/a.txt", "second"));
    CHECK(read(fs::path(base) / "a.txt") == "first");
    CHECK(f.create_folder(base + "/d") && !f.create_folder(base + "/d"));
    CHECK(f.kind(base + "/d") == PathKind::EmptyFolder && f.kind(base + "/a.txt") == PathKind::File);
    CHECK(f.kind(base + "/none") == PathKind::Missing && f.kind(base) == PathKind::Folder);
    CHECK(f.remove_file(base + "/a.txt") && f.remove_folder(base + "/d"));
    CHECK(!fs::exists(fs::path(base) / "a.txt") && !fs::exists(fs::path(base) / "d"));
}

NNH_TEST(a_non_ascii_parent_folder_works) {
    const std::string base = fresh("utf8") + "/pasta-\xc3\xa7\xc3\xa3o"; // "pasta-ção"
    fs::create_directories(u8(base));
    const WriteResult r = write_project(base + "/demo", project(), system_new_fs());
    CHECK(r.ok);
    CHECK(read(u8(base) / "demo" / "src" / "main.cpp") == "int main() {}\n");
}

NNH_TEST(a_real_write_failure_after_the_open_leaves_no_file) { // review 033: the partial file is removed too
    const std::string base = fresh("partial");
    CHECK(!create_file_exclusive(base + "/a.txt", "hello world", 3)); // the write fails after 3 bytes
    CHECK(!fs::exists(fs::path(base) / "a.txt"));
    CHECK(create_file_exclusive(base + "/b.txt", "hello", static_cast<std::size_t>(-1)));
    CHECK(read(fs::path(base) / "b.txt") == "hello");
}

NNH_TEST(a_write_failing_mid_file_leaves_nothing_behind) {
    const std::string base = fresh("partial_project");
    NewFs f = system_new_fs();
    auto calls = std::make_shared<int>(0);
    f.create_file = [calls](const std::string& p, const std::string& b) {
        return create_file_exclusive(p, b, (*calls)++ == 2 ? 2 : static_cast<std::size_t>(-1)); // the 3rd file
    };
    const WriteResult r = write_project(base + "/demo", project(), f);
    CHECK(!r.ok && has(r.error, "src/scenes/play_scene.cpp"));
    CHECK(!fs::exists(fs::path(base) / "demo"));
}

NNH_TEST(what_the_rollback_cannot_remove_is_named) {
    const std::string base = fresh("stuck");
    NewFs f = failing_fs(3); // the 4th file fails
    f.remove_file = [](const std::string& p) { return p.find("main.cpp") == std::string::npos && system_new_fs().remove_file(p); };
    const WriteResult r = write_project(base + "/demo", project(), f);
    CHECK(!r.ok && has(r.error, "could not remove") && has(r.error, "demo/src/main.cpp"));
    CHECK(has(r.error, "demo/src")); // its folder couldn't go either
    CHECK(!has(r.error, "everything nnh new created was removed"));
}
