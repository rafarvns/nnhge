// Spec 027 changes nothing of spec 002's raw saves or spec 017's bindings (T0.2, T1.3, AC-7, FR-20): the bytes
// nnh::save::write produces, an NNHB blob, the fs call sequences of a raw write and read, link.sav's format, and
// detail::save_read_one behaving as save::read's own read of one file. Values pinned before any spec 027 code.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "../actions/actions_fixture.hpp"
#include "core/crc32.hpp"
#include "fs/save_format.hpp"
#include "nnh/fs.hpp"

namespace fake = nnh::platform_fake;
using actions_test::btn;
using actions_test::restart;
using nnh::Button;

namespace {

enum class Act { Jump, Attack, Move, Pause };

// Pinned at T0.2 (main at 668e1ae, before spec 027).
constexpr std::size_t kRawFileSize = 116;
constexpr std::uint32_t kRawFileCrc = 0x7E97C64Du;
constexpr std::size_t kBindingsSize = 28;
constexpr std::uint32_t kBindingsCrc = 0xE82EA36Eu;

void fresh() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
}

struct Blob {
    std::uint8_t b[100];
    Blob() {
        for (int i = 0; i < 100; ++i) b[i] = static_cast<std::uint8_t>(i * 7 + 3);
    }
};

bool pinned(const char* what, std::uint32_t got, std::uint32_t expected) {
    if (got != expected) std::printf("    %s: 0x%08X (pinned 0x%08X)\n", what, static_cast<unsigned>(got), static_cast<unsigned>(expected));
    return got == expected;
}

} // namespace

NNH_TEST(raw_save_write_bytes_are_unchanged_by_spec_027) {
    fresh();
    const Blob blob;
    CHECK(nnh::save::write("raw.sav", blob.b, sizeof blob.b, 7).ok());
    std::uint8_t file[256];
    const auto r = nnh::fs::read_file("raw.sav", file, sizeof file);
    CHECK(r.ok() && r.value() == kRawFileSize);
    CHECK(pinned("raw file crc", nnh::detail::crc32(file, r.value_or(0)), kRawFileCrc));
    nnh::shutdown();
}

NNH_TEST(raw_save_call_sequences_are_unchanged_by_spec_027) {
    fresh();
    const Blob blob;
    CHECK(nnh::save::write("raw.sav", blob.b, sizeof blob.b, 7).ok());
    fake::clear_fs_log();
    CHECK(nnh::save::write("raw.sav", blob.b, sizeof blob.b, 7).ok()); // over an existing save
    CHECK(std::string_view(fake::fs_log()) == "WSXNN");
    fake::clear_fs_log();
    std::uint8_t out[128];
    CHECK(nnh::save::read("raw.sav", out, sizeof out, nullptr).ok());
    CHECK(std::string_view(fake::fs_log()) == "SRR");
    nnh::shutdown();
}

NNH_TEST(bindings_blob_bytes_are_unchanged_by_spec_027) {
    restart();
    nnh::ActionSet set{{.actions = 4}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A), btn(Button::Up)}}));
    CHECK(set.add(Act::Attack, {.name = "attack", .bindings = {btn(Button::B)}}));
    CHECK(set.add(Act::Move, {.name = "move", .kind = nnh::ActionKind::Vector,
                              .bindings = {{nnh::Source::stick(nnh::AnalogStick::CirclePad)}}}));
    CHECK(set.add(Act::Pause, {.name = "pause", .bindings = {btn(Button::Start)}}));
    set.set_source(Act::Jump, 0, nnh::Source::button(Button::Y));
    std::uint8_t blob[nnh::kMaxBindingsSize];
    const auto r = set.save_to(blob, sizeof blob, 5);
    CHECK(r.ok());
    CHECK(pinned("bindings size", static_cast<std::uint32_t>(r.value_or(0)), kBindingsSize));
    CHECK(pinned("bindings crc", nnh::detail::crc32(blob, r.value_or(0)), kBindingsCrc));
    nnh::shutdown();
}

NNH_TEST(save_read_one_reads_one_file_like_save_read) {
    fresh();
    const Blob blob;
    std::uint8_t out[128];
    std::uint16_t version = 0;
    auto missing = nnh::detail::save_read_one("none.sav", out, sizeof out, &version);
    CHECK(!missing.ok() && missing.error().code == nnh::ErrorCode::NotFound);
    CHECK(nnh::save::write("one.sav", blob.b, sizeof blob.b, 7).ok());
    auto ok = nnh::detail::save_read_one("one.sav", out, sizeof out, &version);
    CHECK(ok.ok() && ok.value() == 100 && version == 7 && std::memcmp(out, blob.b, 100) == 0);
    auto small = nnh::detail::save_read_one("one.sav", out, 99, nullptr);
    CHECK(!small.ok() && small.error().code == nnh::ErrorCode::LimitReached);
    fake::corrupt_file("one.sav", 40);
    auto bad = nnh::detail::save_read_one("one.sav", out, sizeof out, nullptr);
    CHECK(!bad.ok() && bad.error().code == nnh::ErrorCode::Corrupted);
    CHECK(nnh::save::write("two.sav", blob.b, sizeof blob.b, 7).ok());
    fake::truncate_file("two.sav", 10);
    auto shortened = nnh::detail::save_read_one("two.sav", out, sizeof out, nullptr);
    CHECK(!shortened.ok() && shortened.error().code == nnh::ErrorCode::Corrupted);
    nnh::shutdown();
}

NNH_TEST(save_read_one_never_falls_back_to_the_backup) {
    fresh();
    const Blob blob;
    CHECK(nnh::save::write("pair.sav", blob.b, sizeof blob.b, 1).ok());
    CHECK(nnh::save::write("pair.sav", blob.b, sizeof blob.b, 2).ok()); // the first is pair.sav.bak
    fake::corrupt_file("pair.sav", 30);
    std::uint8_t out[128];
    CHECK(!nnh::detail::save_read_one("pair.sav", out, sizeof out, nullptr).ok());
    CHECK(nnh::save::read("pair.sav", out, sizeof out, nullptr).ok()); // save::read does
    nnh::shutdown();
}
