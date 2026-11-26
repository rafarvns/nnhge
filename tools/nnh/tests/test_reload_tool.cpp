// The `nnh` side of hot reload (spec 031 T3.1–T3.4, FR-1, FR-2, FR-4, FR-14, FR-18): the output mirror (changed,
// unchanged, added, removed, the batch order, the mirror moving only on accept), the source watcher (one save, a
// temporary file + rename, a map + tileset burst, a change during the quiet period), Ninja dry-run parsing, the asset
// build's retry, and a batch's exact bytes with the local limits.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "cli.hpp"
#include "core/crc32.hpp"
#include "reload/asset_build.hpp"
#include "reload/batch_sender.hpp"
#include "reload/output_snapshot.hpp"
#include "reload/reload_setup.hpp"
#include "reload/reload_wire.hpp"
#include "reload/source_watcher.hpp"
#include "test.hpp"

using namespace nnh::tool::reload;
namespace fs = std::filesystem;
namespace wire = nnh::detail::remote;

namespace {

std::string temp_dir(const char* name) {
    const std::string d = std::string(NNH_TOOL_TEST_TMP) + "/reload_" + name;
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

void write(const std::string& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

std::string paths_of(const std::vector<OutputChange>& v) {
    std::string s;
    for (const OutputChange& c : v) s += (c.removed ? "-" : "+") + c.path + " ";
    return s;
}

SourceListing listing(std::initializer_list<std::pair<const char*, int>> files) {
    SourceListing l;
    for (const auto& [p, t] : files) l[p] = SourceStamp{1, t};
    return l;
}

} // namespace

NNH_TEST(reload_snapshot_diffs_changed_added_removed_in_batch_order) {
    const std::string d = temp_dir("snap");
    write(d + "/art/hero.png", "hero");
    write(d + "/levels/room.nnhmap", "room");
    write(d + "/.nnh_assets", "art/hero.png\nlevels/room.nnhmap\n");
    write(d + "/same.txt", "same");
    OutputSnapshot s;
    s.scan(d);
    CHECK(s.diff(d).empty());
    write(d + "/art/hero.png", "hero2");     // changed
    write(d + "/same.txt", "same");          // rewritten, same bytes: not sent
    write(d + "/art/new.png", "new");        // added
    fs::remove(d + "/levels/room.nnhmap");   // removed
    write(d + "/.nnh_assets", "art/hero.png\nart/new.png\n");
    write(d + "/.nnh_data", "d");
    const std::vector<OutputChange> c = s.diff(d);
    CHECK(paths_of(c) == "+art/hero.png +art/new.png -levels/room.nnhmap +.nnh_data +.nnh_assets ");
    CHECK(s.diff(d).size() == 5); // the mirror didn't move
    s.accept(c);
    CHECK(s.diff(d).empty());
    s.forget("art/hero.png"); // `reload art/hero.png`: sent again though unchanged
    CHECK(paths_of(s.diff(d)) == "+art/hero.png ");
}

NNH_TEST(reload_snapshot_reports_path_hash_collisions) {
    const std::string d = temp_dir("collide");
    write(d + "/a.txt", "x");
    OutputSnapshot s;
    s.scan(d);
    CHECK(s.hash_collisions().empty());
}

NNH_TEST(reload_watcher_one_save_after_the_quiet_period) {
    SourceWatcher w;
    w.reset(listing({{"art/hero.png", 1}}));
    CHECK(!w.poll(listing({{"art/hero.png", 1}}), 100));
    CHECK(!w.poll(listing({{"art/hero.png", 2}}), 200)); // saved: the quiet period starts
    CHECK(w.gathering());
    CHECK(!w.poll(listing({{"art/hero.png", 2}}), 300));
    CHECK(!w.poll(listing({{"art/hero.png", 2}}), 449));
    CHECK(w.poll(listing({{"art/hero.png", 2}}), 450)); // 250 ms without a change
    CHECK(w.changed().size() == 1 && w.changed()[0] == "art/hero.png");
    CHECK(!w.poll(listing({{"art/hero.png", 2}}), 1000)); // once per burst
}

NNH_TEST(reload_watcher_a_temporary_file_and_rename_is_one_reload) {
    SourceWatcher w;
    w.reset(listing({{"hero.png", 1}}));
    CHECK(!w.poll(listing({{"hero.png", 1}, {"hero.png~tmp", 5}}), 100));
    CHECK(!w.poll(listing({{"hero.png", 6}}), 200)); // renamed over: a change during the quiet period restarts it
    CHECK(!w.poll(listing({{"hero.png", 6}}), 400));
    CHECK(w.poll(listing({{"hero.png", 6}}), 450));
    CHECK(w.changed().size() == 2); // the temporary file too: harmless, the build sees only the result
}

NNH_TEST(reload_watcher_a_map_and_tileset_burst_add_and_remove) {
    SourceWatcher w;
    w.reset(listing({{"levels/room.tmj", 1}, {"levels/tiles.tsj", 1}, {"old.png", 1}}));
    CHECK(!w.poll(listing({{"levels/room.tmj", 2}, {"levels/tiles.tsj", 1}, {"old.png", 1}}), 100));
    CHECK(!w.poll(listing({{"levels/room.tmj", 2}, {"levels/tiles.tsj", 2}, {"new.png", 1}}), 300));
    CHECK(w.poll(listing({{"levels/room.tmj", 2}, {"levels/tiles.tsj", 2}, {"new.png", 1}}), 550));
    CHECK(w.changed().size() == 4);
    CHECK(w.changed()[0] == "levels/room.tmj" && w.changed()[3] == "old.png");
    CHECK(!w.poll(listing({{"levels/room.tmj", 2}, {"levels/tiles.tsj", 2}, {"new.png", 1}}), 900)); // no touch
}

NNH_TEST(reload_build_commands_per_preset) {
    nnh::tool::Shell pc;
    const BuildCommands c = build_commands("E:/repo", "pc-debug", "data", pc);
    CHECK(c.assets.find("--build --preset pc-debug --target data_assets") != std::string::npos);
    CHECK(c.dry_run.find("--target data -- -n") != std::string::npos);
    nnh::tool::Shell dkp;
    dkp.windows = true;
    dkp.devkitpro_bash = "C:/devkitPro/msys2/usr/bin/bash.exe";
    const BuildCommands d = build_commands("E:/repo", "3ds-dev", "data", dkp);
    CHECK(d.assets.find("bash.exe") != std::string::npos);
    CHECK(d.assets.find("--target data_assets") != std::string::npos);
}

NNH_TEST(reload_build_retries_once_and_keeps_the_lines) {
    int calls = 0;
    const CommandRunner fails_once = [&](const std::string&, std::vector<std::string>& lines) {
        lines.push_back("assets/levels/room.tmj:12: unknown layer type");
        return ++calls == 1 ? 1 : 0;
    };
    const BuildResult r = build_assets(BuildCommands{"a", "b"}, fails_once, 0);
    CHECK(r.ok && r.attempts == 2 && calls == 2);
    calls = 0;
    const CommandRunner always = [&](const std::string&, std::vector<std::string>& lines) {
        lines.push_back("assets/levels/room.tmj:12: unknown layer type");
        ++calls;
        return 1;
    };
    const BuildResult f = build_assets(BuildCommands{"a", "b"}, always, 0);
    CHECK(!f.ok && f.attempts == 2 && f.lines.size() == 1);
    CHECK(f.lines[0] == "assets/levels/room.tmj:12: unknown layer type");
}

NNH_TEST(reload_dry_run_first_command) {
    CHECK(first_dry_run_command({"ninja: no work to do."}).empty());
    CHECK(first_dry_run_command({}).empty());
    CHECK(first_dry_run_command({"[1/3] Building CXX object examples/data/CMakeFiles/data.dir/data_types.cpp.obj",
                                 "[2/3] Linking CXX executable examples/data/data.exe"}) ==
          "Building CXX object examples/data/CMakeFiles/data.dir/data_types.cpp.obj");
    CHECK(first_dry_run_command({"ninja: Entering directory `build'", "[1/1] Re-running CMake..."}).empty());
    CHECK(first_dry_run_command({"[x/1] nope", "[] nope"}).empty());
    CHECK(first_dry_run_command({"[0/2] Re-checking globbed directories...", "ninja: no work to do."}).empty());
}

NNH_TEST(reload_batch_bytes_exactly) {
    BatchFileData put{"a/b.png", false, {'h', 'i'}};
    BatchFileData gone{"c.txt", true, {}};
    const EncodedBatch e = encode_batch(5, {put, gone});
    CHECK(e.error.empty());
    CHECK(e.files == 2 && e.total == 7 + 2 + 5);
    // Begin, File + path + CRC, Chunk + 2 bytes, File + path
    CHECK(e.bytes.size() == 16 + 16 + 7 + 4 + 16 + 2 + 16 + 5);
    wire::Message m;
    CHECK(wire::decode_message(e.bytes.data(), m) == wire::Reject::None);
    CHECK(m.type == wire::MessageType::ReloadBegin && wire::reload_batch(m) == 5 && wire::reload_files(m) == 2);
    CHECK(std::memcmp(e.bytes.data() + 32, "a/b.png", 7) == 0);
    const std::uint32_t crc = nnh::detail::crc32("hi", 2);
    CHECK(e.bytes[39] == static_cast<std::uint8_t>(crc) && e.bytes[42] == static_cast<std::uint8_t>(crc >> 24));
    CHECK(wire::decode_message(e.bytes.data() + 43, m) == wire::Reject::None && m.type == wire::MessageType::ReloadChunk);
    CHECK(e.bytes[59] == 'h' && e.bytes[60] == 'i');
    const std::vector<std::uint8_t> apply = encode_apply(5, 2, 100, 9);
    CHECK(wire::decode_message(apply.data(), m) == wire::Reject::None && m.frame == 100 && m.seq == 9);
}

NNH_TEST(reload_batch_limits_are_refused_locally) {
    std::vector<BatchFileData> many(257, BatchFileData{"a", true, {}});
    CHECK(encode_batch(1, many).error == "too many changes for one reload: restart");
    BatchFileData big{"big.bin", false, std::vector<std::uint8_t>(wire::kReloadMaxFileBytes + 1)};
    CHECK(encode_batch(1, {big}).error == "too many changes for one reload: restart");
    CHECK(!encode_batch(1, {}).error.empty());
    const std::vector<std::uint8_t> notice = encode_notice(true, "data_types.cpp", 0);
    wire::Message m;
    CHECK(wire::decode_message(notice.data(), m) == wire::Reject::None);
    CHECK(wire::notice_reason(m) == wire::NoticeReason::NeedsRestart && notice.size() == 16 + 14);
}

NNH_TEST(reload_paths_file_parses_its_three_lines) {
    const auto p = parse_reload_paths("assets=E:/g/assets\r\ngame=E:/g\nout=E:/b/g/assets\n");
    CHECK(p.has_value());
    CHECK(p->assets == "E:/g/assets" && p->game == "E:/g" && p->out == "E:/b/g/assets");
    const auto msys = parse_reload_paths("assets=/e/g/assets\ngame=/e/g\nout=/e/b/assets\n"); // devkitPro's CMake
    CHECK(msys.has_value() && msys->assets == "E:/g/assets");
    CHECK(!parse_reload_paths("assets=a\ngame=b\n").has_value());
}

NNH_TEST(reload_cli_no_reload_is_for_run) {
    nnh::tool::Options o;
    std::string e;
    CHECK(nnh::tool::parse_command_line({"run", "data", "--pc", "--no-reload"}, o, e) && o.no_reload);
    nnh::tool::Options b;
    CHECK(!nnh::tool::parse_command_line({"build", "data", "--no-reload"}, b, e));
    CHECK(e == "--no-reload is for nnh run");
}
