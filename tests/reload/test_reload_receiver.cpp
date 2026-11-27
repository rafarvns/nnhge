// The reload receiver (spec 031 T1.3, FR-4, FR-18, X): a complete batch of puts and removes → staged with the exact sink
// calls; every refusal and its reason (out of order, wrong offset, short or long file, total mismatch, bad CRC, a bad
// path, Begin while busy, Apply for another batch or count, cancel and detach mid-file → discard); limits.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "core/crc32.hpp"
#include "reload/reload_receiver.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;
using nnh::detail::reload::ReceiverEvent;
using nnh::detail::reload::ReceiverSink;
using nnh::detail::reload::ReloadReceiver;

namespace {

struct Recorder {
    char log[2048] = {};
    std::size_t bytes = 0;
    bool fail_begin = false, fail_append = false, fail_finish = false;

    void add(const char* text) { std::strncat(log, text, sizeof log - std::strlen(log) - 1); }
};

bool rec_begin(void* c, std::uint16_t index, const char* path, ReloadOp op, std::uint32_t size) {
    auto* r = static_cast<Recorder*>(c);
    char line[300];
    std::snprintf(line, sizeof line, "begin %u %s %s %u;", index, path, op == ReloadOp::Put ? "put" : "remove",
                  static_cast<unsigned>(size));
    r->add(line);
    return !r->fail_begin;
}
bool rec_append(void* c, const std::uint8_t*, std::size_t size) {
    auto* r = static_cast<Recorder*>(c);
    char line[32];
    std::snprintf(line, sizeof line, "append %u;", static_cast<unsigned>(size));
    r->add(line);
    r->bytes += size;
    return !r->fail_append;
}
bool rec_finish(void* c, std::uint16_t index) {
    auto* r = static_cast<Recorder*>(c);
    char line[32];
    std::snprintf(line, sizeof line, "finish %u;", index);
    r->add(line);
    return !r->fail_finish;
}
void rec_discard(void* c) { static_cast<Recorder*>(c)->add("discard;"); }

struct Fixture {
    Recorder rec;
    ReceiverSink sink{&rec, rec_begin, rec_append, rec_finish, rec_discard};
    ReloadReceiver r;
    ReceiverEvent last = ReceiverEvent::None;

    ReceiverEvent msg(const Message& m) { return last = r.on_message(m, sink); }
    ReceiverEvent bytes(const void* data, std::size_t n) {
        return last = r.on_payload(static_cast<const std::uint8_t*>(data), n, sink);
    }
    /// A put's File message, path and CRC of `content`.
    ReceiverEvent put(std::uint16_t index, const char* path, const char* content, std::uint32_t crc_delta = 0) {
        const auto len = static_cast<std::uint8_t>(std::strlen(path));
        const auto size = static_cast<std::uint32_t>(std::strlen(content));
        if (msg(reload_file_message(index, ReloadOp::Put, len, size)) != ReceiverEvent::None) return last;
        if (bytes(path, len) != ReceiverEvent::None) return last;
        const std::uint32_t crc = nnh::detail::crc32(content, size) + crc_delta;
        const std::uint8_t c[4] = {static_cast<std::uint8_t>(crc), static_cast<std::uint8_t>(crc >> 8),
                                   static_cast<std::uint8_t>(crc >> 16), static_cast<std::uint8_t>(crc >> 24)};
        return bytes(c, 4);
    }
    ReceiverEvent chunk(std::uint16_t index, std::uint32_t offset, const char* content) {
        const auto n = static_cast<std::uint16_t>(std::strlen(content));
        if (msg(reload_chunk_message(index, n, offset)) != ReceiverEvent::None) return last;
        return bytes(content, n);
    }
    ReceiverEvent remove(std::uint16_t index, const char* path) {
        const auto len = static_cast<std::uint8_t>(std::strlen(path));
        if (msg(reload_file_message(index, ReloadOp::Remove, len, 0)) != ReceiverEvent::None) return last;
        return bytes(path, len);
    }
};

constexpr std::uint32_t total_of(std::uint32_t paths, std::uint32_t sizes) { return paths + sizes; }

} // namespace

NNH_TEST(reload_receiver_stages_a_batch_of_puts_and_a_remove) {
    Fixture f;
    // art/a.png (9) "hello" (5), b.txt (5) "" (0), old.png (7) removed
    CHECK(f.msg(reload_begin_message(4, 3, total_of(9 + 5 + 7, 5))) == ReceiverEvent::None);
    CHECK(f.put(0, "art/a.png", "hello") == ReceiverEvent::None);
    CHECK(f.chunk(0, 0, "hel") == ReceiverEvent::None);
    CHECK(f.chunk(0, 3, "lo") == ReceiverEvent::None);
    CHECK(f.put(1, "b.txt", "") == ReceiverEvent::None);
    CHECK(f.remove(2, "old.png") == ReceiverEvent::Staged);
    CHECK(std::strcmp(f.rec.log, "begin 0 art/a.png put 5;append 3;append 2;finish 0;begin 1 b.txt put 0;finish 1;"
                                 "begin 2 old.png remove 0;finish 2;") == 0);
    CHECK(f.r.state() == ReloadReceiver::State::Staged);
    CHECK(f.r.batch() == 4);
    CHECK(f.r.files() == 3);
}

NNH_TEST(reload_receiver_path_and_crc_split_into_pieces) {
    Fixture f;
    f.msg(reload_begin_message(1, 1, 9 + 2));
    f.msg(reload_file_message(0, ReloadOp::Put, 9, 2));
    const std::uint32_t crc = nnh::detail::crc32("xy", 2);
    std::uint8_t tail[13];
    std::memcpy(tail, "art/a.png", 9);
    for (int i = 0; i < 4; ++i) tail[9 + i] = static_cast<std::uint8_t>(crc >> (8 * i));
    for (int i = 0; i < 13; ++i) CHECK(f.bytes(tail + i, 1) == ReceiverEvent::None);
    f.msg(reload_chunk_message(0, 2, 0));
    CHECK(f.bytes("x", 1) == ReceiverEvent::None);
    CHECK(f.bytes("y", 1) == ReceiverEvent::Staged);
}

NNH_TEST(reload_receiver_take_hands_the_staged_batch_over) {
    Fixture f;
    f.msg(reload_begin_message(7, 1, 5 + 1));
    f.put(0, "a.txt", "z");
    f.chunk(0, 0, "z");
    CHECK(!f.r.take(8, 1));
    CHECK(f.r.refusal().reason == ReloadReason::Batch);
    CHECK(!f.r.take(7, 2));
    CHECK(f.r.state() == ReloadReceiver::State::Staged);
    CHECK(f.r.take(7, 1));
    CHECK(f.r.state() == ReloadReceiver::State::Idle);
    CHECK(!f.r.take(7, 1)); // nothing staged any more
}

NNH_TEST(reload_receiver_refuses_a_file_out_of_order) {
    Fixture f;
    f.msg(reload_begin_message(1, 2, 100));
    CHECK(f.msg(reload_file_message(1, ReloadOp::Put, 5, 1)) == ReceiverEvent::Refused);
    CHECK(f.r.refusal().reason == ReloadReason::Order);
    CHECK(f.r.refusal().file == 1);
    CHECK(std::strcmp(f.rec.log, "discard;") == 0);
    // the rest of the batch is ignored, its payload too
    CHECK(f.bytes("aaaaa", 5) == ReceiverEvent::None);
    CHECK(f.msg(reload_chunk_message(1, 1, 0)) == ReceiverEvent::None);
    CHECK(f.r.state() == ReloadReceiver::State::Dropping);
}

NNH_TEST(reload_receiver_refuses_a_total_that_is_not_reached) {
    Fixture f;
    f.msg(reload_begin_message(1, 1, 100));
    f.put(0, "a.txt", "1");
    f.chunk(0, 0, "1");
    CHECK(f.last == ReceiverEvent::Refused); // total 100 ≠ 6: refused at the last file
    CHECK(f.r.refusal().reason == ReloadReason::Total);
}

NNH_TEST(reload_receiver_refuses_a_file_index_past_the_batch) {
    Fixture f;
    f.msg(reload_begin_message(1, 2, 100));
    f.remove(0, "a.txt");
    f.remove(1, "b.txt"); // the last file: the total (100) isn't reached → refused, then ignored
    Fixture g;
    g.msg(reload_begin_message(1, 1, 100));
    CHECK(g.msg(reload_file_message(1, ReloadOp::Remove, 1, 0)) == ReceiverEvent::Refused);
    CHECK(g.r.refusal().reason == ReloadReason::Order);
    CHECK(f.last == ReceiverEvent::Refused);
    CHECK(f.msg(reload_file_message(2, ReloadOp::Remove, 1, 0)) == ReceiverEvent::None);
    CHECK(f.r.refusal().reason == ReloadReason::Total);
}

NNH_TEST(reload_receiver_refuses_a_chunk_at_the_wrong_offset) {
    Fixture f;
    f.msg(reload_begin_message(1, 1, 5 + 4));
    f.put(0, "a.txt", "abcd");
    f.chunk(0, 0, "ab");
    CHECK(f.chunk(0, 1, "cd") == ReceiverEvent::Refused);
    CHECK(f.r.refusal().reason == ReloadReason::Offset);
    CHECK(f.r.refusal().file == 0);
}

NNH_TEST(reload_receiver_refuses_a_chunk_for_another_file_or_none) {
    Fixture f;
    f.msg(reload_begin_message(1, 2, 100));
    CHECK(f.chunk(0, 0, "x") == ReceiverEvent::Refused); // no file open
    CHECK(f.r.refusal().reason == ReloadReason::Order);
    Fixture g;
    g.msg(reload_begin_message(1, 2, 100));
    g.put(0, "a.txt", "ab");
    CHECK(g.chunk(1, 0, "ab") == ReceiverEvent::Refused);
    CHECK(g.r.refusal().reason == ReloadReason::Order);
}

NNH_TEST(reload_receiver_refuses_a_long_file) {
    Fixture f;
    f.msg(reload_begin_message(1, 1, 5 + 2));
    f.put(0, "a.txt", "ab");
    CHECK(f.chunk(0, 0, "abc") == ReceiverEvent::Refused);
    CHECK(f.r.refusal().reason == ReloadReason::Size);
}

NNH_TEST(reload_receiver_refuses_a_short_file) {
    Fixture f;
    f.msg(reload_begin_message(1, 2, 100));
    f.put(0, "a.txt", "abcd");
    f.chunk(0, 0, "ab");
    CHECK(f.msg(reload_file_message(1, ReloadOp::Remove, 3, 0)) == ReceiverEvent::Refused); // the put isn't complete
    CHECK(f.r.refusal().reason == ReloadReason::Size);
}

NNH_TEST(reload_receiver_refuses_a_total_mismatch_early) {
    Fixture f;
    f.msg(reload_begin_message(1, 2, 10));
    CHECK(f.put(0, "a.txt", "0123456789") == ReceiverEvent::Refused); // 5 + 10 > 10: known before any byte
    CHECK(f.r.refusal().reason == ReloadReason::Total);
}

NNH_TEST(reload_receiver_refuses_a_bad_crc) {
    Fixture f;
    f.msg(reload_begin_message(1, 1, 5 + 3));
    f.put(0, "a.txt", "abc", 1);
    CHECK(f.chunk(0, 0, "abc") == ReceiverEvent::Refused);
    CHECK(f.r.refusal().reason == ReloadReason::Crc);
    CHECK(std::strstr(f.rec.log, "finish") == nullptr);
    CHECK(std::strstr(f.rec.log, "discard;") != nullptr);
}

NNH_TEST(reload_receiver_refuses_a_bad_path) {
    const char* bad[] = {"../a.png", "a//b", "/abs", "a/./b", "a\\b", "x?"};
    for (const char* path : bad) {
        Fixture f;
        f.msg(reload_begin_message(1, 1, 100));
        CHECK(f.put(0, path, "") == ReceiverEvent::Refused);
        CHECK(f.r.refusal().reason == ReloadReason::Path);
    }
    Fixture nul;
    nul.msg(reload_begin_message(1, 1, 100));
    nul.msg(reload_file_message(0, ReloadOp::Remove, 3, 0));
    CHECK(nul.bytes("a\0b", 3) == ReceiverEvent::Refused); // an interior NUL
}

NNH_TEST(reload_receiver_refuses_begin_while_busy) {
    Fixture f;
    f.msg(reload_begin_message(1, 1, 100));
    CHECK(f.msg(reload_begin_message(2, 1, 100)) == ReceiverEvent::Refused);
    CHECK(f.r.refusal().reason == ReloadReason::Batch);
    CHECK(f.r.refusal().batch == 2);
    CHECK(f.r.state() == ReloadReceiver::State::Open); // the open batch keeps going
    Fixture g;
    g.msg(reload_begin_message(1, 1, 5));
    g.remove(0, "a.txt");
    CHECK(g.r.state() == ReloadReceiver::State::Staged);
    CHECK(g.msg(reload_begin_message(2, 1, 100)) == ReceiverEvent::Refused);
    CHECK(g.r.state() == ReloadReceiver::State::Staged);
}

NNH_TEST(reload_receiver_cancel_mid_file_discards) {
    Fixture f;
    f.msg(reload_begin_message(3, 1, 100));
    f.put(0, "a.txt", "abcd");
    f.chunk(0, 0, "ab");
    CHECK(f.msg(reload_cancel_message(9)) == ReceiverEvent::None); // another batch: ignored
    CHECK(f.msg(reload_cancel_message(3)) == ReceiverEvent::Cancelled);
    CHECK(f.r.state() == ReloadReceiver::State::Idle);
    CHECK(std::strstr(f.rec.log, "discard;") != nullptr);
    CHECK(f.msg(reload_begin_message(4, 1, 5)) == ReceiverEvent::None); // a new batch is welcome
}

NNH_TEST(reload_receiver_cancel_zero_drops_a_staged_batch) {
    Fixture f;
    f.msg(reload_begin_message(3, 1, 5));
    f.remove(0, "a.txt");
    CHECK(f.msg(reload_cancel_message(0)) == ReceiverEvent::Cancelled);
    CHECK(f.r.state() == ReloadReceiver::State::Idle);
    CHECK(f.msg(reload_cancel_message(0)) == ReceiverEvent::None); // nothing left
}

NNH_TEST(reload_receiver_detach_mid_file_discards) {
    Fixture f;
    f.msg(reload_begin_message(3, 1, 100));
    f.put(0, "a.txt", "abcd");
    CHECK(f.r.detach(f.sink));
    CHECK(std::strstr(f.rec.log, "discard;") != nullptr);
    CHECK(f.r.state() == ReloadReceiver::State::Idle);
    CHECK(!f.r.detach(f.sink));
}

NNH_TEST(reload_receiver_a_write_failure_refuses) {
    Fixture f;
    f.rec.fail_append = true;
    f.msg(reload_begin_message(1, 1, 5 + 2));
    f.put(0, "a.txt", "ab");
    CHECK(f.chunk(0, 0, "ab") == ReceiverEvent::Refused);
    CHECK(f.r.refusal().reason == ReloadReason::Write);
    Fixture g;
    g.rec.fail_begin = true;
    g.msg(reload_begin_message(1, 1, 5));
    CHECK(g.remove(0, "a.txt") == ReceiverEvent::Refused);
    CHECK(g.r.refusal().reason == ReloadReason::Write);
}

NNH_TEST(reload_receiver_file_and_chunk_without_begin_are_ignored_quietly) {
    Fixture f;
    CHECK(f.msg(reload_file_message(0, ReloadOp::Put, 5, 1)) == ReceiverEvent::Refused);
    CHECK(f.r.refusal().reason == ReloadReason::Order);
    CHECK(f.r.refusal().batch == 0);
    CHECK(f.bytes("a.txt", 5) == ReceiverEvent::None);
}

NNH_TEST(reload_receiver_limits) {
    Fixture f; // 256 removes of one-letter paths
    CHECK(f.msg(reload_begin_message(1, 256, 256)) == ReceiverEvent::None);
    ReceiverEvent e = ReceiverEvent::None;
    for (int i = 0; i < 256; ++i) e = f.remove(static_cast<std::uint16_t>(i), "a");
    CHECK(e == ReceiverEvent::Staged);
    Fixture g; // a 255-byte path
    char path[256];
    std::memset(path, 'p', 255);
    path[255] = 0;
    g.msg(reload_begin_message(1, 1, 255));
    CHECK(g.remove(0, path) == ReceiverEvent::Staged);
}
