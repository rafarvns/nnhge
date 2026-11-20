// What NNH Link remembers (apps/nnh-link/src/storage.cpp, spec 011 FR-2, FR-3). The bytes come from the
// SD card: decoding must refuse anything malformed and change nothing then.

#include "storage.hpp"
#include "test.hpp"

using link_app::Storage;
using nnh::link::Digest;
using nnh::link::Id;
using nnh::link::Ipv4;

namespace {

Id id_of(std::uint8_t n) {
    Id id{};
    id[0] = n;
    return id;
}

Digest secret_of(std::uint8_t n) {
    Digest d{};
    d.fill(n);
    return d;
}

Storage filled() {
    Storage s;
    s.set_console_id(id_of(9));
    s.remember(id_of(1), secret_of(1));
    s.remember(id_of(2), secret_of(2));
    s.set_last_server(id_of(2), Ipv4{{192, 168, 10, 2}});
    s.set_last_game("audio");
    return s;
}

} // namespace

NNH_TEST(storage_round_trips) {
    const Storage s = filled();
    Storage t;
    CHECK(t.decode(s.encode()));
    CHECK(t.console_id() == id_of(9));
    CHECK(t.pair_count() == 2);
    CHECK(t.secret_of(id_of(2)) == secret_of(2));
    CHECK(t.last_server()->address == (Ipv4{{192, 168, 10, 2}}));
    CHECK(t.last_game() == "audio");
    CHECK(!t.dirty());
}

NNH_TEST(storage_empty_round_trips) {
    Storage t = filled();
    CHECK(t.decode(Storage().encode()));
    CHECK(!t.has_console_id());
    CHECK(t.pair_count() == 0);
    CHECK(!t.last_server().has_value());
    CHECK(t.last_game().empty());
}

NNH_TEST(storage_remembering_again_replaces_the_secret) {
    Storage s = filled();
    s.remember(id_of(1), secret_of(5));
    CHECK(s.pair_count() == 2);
    CHECK(s.secret_of(id_of(1)) == secret_of(5));
}

NNH_TEST(storage_drops_the_oldest_pairing_when_full) {
    Storage s;
    for (std::uint8_t i = 1; i <= Storage::kMaxPairs + 1; ++i) s.remember(id_of(i), secret_of(i));
    CHECK(s.pair_count() == Storage::kMaxPairs);
    CHECK(!s.paired(id_of(1)));
    CHECK(s.paired(id_of(Storage::kMaxPairs + 1)));
}

NNH_TEST(storage_forgets_a_pairing) {
    Storage s = filled();
    s.clear_dirty();
    s.forget(id_of(1));
    CHECK(!s.paired(id_of(1)));
    CHECK(s.dirty());
    s.clear_dirty();
    s.forget(id_of(1)); // nothing to forget: nothing to save
    CHECK(!s.dirty());
}

NNH_TEST(storage_marks_changes_dirty_only_when_something_changed) {
    Storage s = filled();
    s.clear_dirty();
    s.set_last_game("audio");
    s.set_last_server(id_of(2), Ipv4{{192, 168, 10, 2}});
    CHECK(!s.dirty());
    s.set_last_game("bad name!");
    CHECK(!s.dirty());
    CHECK(s.last_game() == "audio");
    s.set_last_game("hello");
    CHECK(s.dirty());
}

NNH_TEST(storage_refuses_every_truncation_and_extra_bytes) {
    const std::vector<std::uint8_t> good = filled().encode();
    for (std::size_t n = 0; n < good.size(); ++n) {
        Storage t = filled();
        t.set_last_game("hello");
        CHECK(!t.decode(std::vector<std::uint8_t>(good.begin(), good.begin() + static_cast<long>(n))));
        CHECK(t.last_game() == "hello"); // unchanged
    }
    std::vector<std::uint8_t> longer = good;
    longer.push_back(0);
    Storage t;
    CHECK(!t.decode(longer));
}

NNH_TEST(storage_refuses_bad_counts_flags_and_names) {
    const std::vector<std::uint8_t> good = filled().encode();
    Storage t;
    std::vector<std::uint8_t> b = good;
    b[16] = Storage::kMaxPairs + 1; // pair count
    CHECK(!t.decode(b));
    b = good;
    const std::size_t flag = 16 + 1 + 2 * 48;
    b[flag] = 2; // has-last-server flag
    CHECK(!t.decode(b));
    b = good;
    b[b.size() - 1] = '!'; // last game "audi!"
    CHECK(!t.decode(b));
    b = good;
    for (std::size_t i = 0; i < 16; ++i) b[16 + 1 + 48 + i] = b[16 + 1 + i]; // the same server twice
    CHECK(!t.decode(b));
}
