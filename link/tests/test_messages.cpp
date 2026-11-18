// NNH Link's wire format (wire.hpp, messages.hpp, frame.hpp, discovery.hpp; spec 011): round trips,
// rejection of malformed input, frame splitting, and the sealed channel against tampering, replay,
// reordering, reflection and a wrong key.

#include <cstring>
#include <string>
#include <vector>

#include "nnh_link/discovery.hpp"
#include "nnh_link/frame.hpp"
#include "nnh_link/messages.hpp"
#include "nnh_link/wire.hpp"
#include "test.hpp"

using namespace nnh::link;

namespace {

Digest key_of(std::uint8_t v) {
    Digest d;
    d.fill(v);
    return d;
}

/// The frame inside encoded bytes.
Frame frame_of(const std::vector<std::uint8_t>& bytes) {
    FrameReader r;
    r.feed(bytes.data(), bytes.size());
    bool error = false;
    auto f = r.next(error);
    return f ? *f : Frame{};
}

} // namespace

NNH_TEST(messages_round_trip) {
    Hello h;
    h.id.fill(7);
    h.nonce.fill(9);
    Hello h2;
    CHECK(decode(encode(h), h2) && h2.version == kProtocolVersion && h2.id == h.id && h2.nonce == h.nonce);
    const std::vector<GameInfo> games = {{"hello", "Hello NNHGE"}, {"audio", "NNHGE Audio"}};
    std::vector<GameInfo> games2;
    CHECK(decode(encode(games), games2) && games2.size() == 2 && games2[1].name == "audio" && games2[1].title == "NNHGE Audio");
    BuildRequest b{"audio", true}, b2;
    CHECK(decode(encode(b), b2) && b2.game == "audio" && b2.release);
    BuildResult res{123456, key_of(3)}, res2;
    CHECK(decode(encode(res), res2) && res2.size == 123456 && res2.sha256 == res.sha256);
    std::string text;
    CHECK(decode_text(encode_text("main.cpp:3: error"), text) && text == "main.cpp:3: error");
    std::vector<std::string> lines;
    CHECK(decode_lines(encode_lines({"a", "b"}), lines) && lines.size() == 2 && lines[1] == "b");
    Digest d;
    CHECK(decode_digest(encode_digest(key_of(5)), d) && d == key_of(5));
}

NNH_TEST(error_lines_keep_the_last_twenty) {
    std::vector<std::string> many;
    for (int i = 0; i < 30; ++i) many.push_back("line " + std::to_string(i));
    std::vector<std::string> lines;
    CHECK(decode_lines(encode_lines(many), lines) && lines.size() == kMaxErrorLines && lines.back() == "line 29");
}

NNH_TEST(malformed_messages_are_rejected) {
    Hello h;
    auto bytes = encode(h);
    bytes.push_back(0); // left over
    CHECK(!decode(bytes, h));
    bytes.resize(10); // cut short
    CHECK(!decode(bytes, h));
    BuildRequest b;
    CHECK(!decode(encode(BuildRequest{"../x", false}), b));
    CHECK(!decode(encode(BuildRequest{"", false}), b));
    auto release2 = encode(BuildRequest{"x", false});
    release2.back() = 2;
    CHECK(!decode(release2, b));
    std::string text;
    CHECK(!decode_text(encode_text("bell\x07"), text));
    Writer w;
    w.str(std::string(300, 'a')); // over kMaxText
    CHECK(!decode_text(w.bytes(), text));
    std::vector<GameInfo> games;
    Writer too_many;
    too_many.u16(static_cast<std::uint16_t>(kMaxGames + 1));
    CHECK(!decode(too_many.bytes(), games));
    BuildResult res;
    CHECK(!decode(encode(BuildResult{1ull << 40, {}}), res)); // absurd size
}

NNH_TEST(frames_split_however_the_bytes_arrive) {
    std::vector<std::uint8_t> stream = encode_frame(Type::ListGames, {});
    const auto second = encode_frame(Type::BuildLine, encode_text("compiling"));
    stream.insert(stream.end(), second.begin(), second.end());
    FrameReader r;
    std::vector<Frame> got;
    bool error = false;
    for (std::uint8_t byte : stream) { // one byte at a time
        r.feed(&byte, 1);
        while (auto f = r.next(error)) got.push_back(*f);
    }
    CHECK(!error && got.size() == 2 && got[0].type == Type::ListGames && got[1].type == Type::BuildLine);
    std::string text;
    CHECK(decode_text(got[1].payload, text) && text == "compiling");
}

NNH_TEST(bad_frames_break_the_reader) {
    const std::uint8_t huge[] = {0xFF, 0xFF, 0x00, 0x00, 1};
    FrameReader r;
    r.feed(huge, sizeof huge);
    bool error = false;
    CHECK(!r.next(error) && error);
    const std::uint8_t unknown_type[] = {1, 0, 0, 0, 99};
    FrameReader r2;
    r2.feed(unknown_type, sizeof unknown_type);
    CHECK(!r2.next(error) && error);
    const std::uint8_t empty[] = {0, 0, 0, 0};
    FrameReader r3;
    r3.feed(empty, sizeof empty);
    CHECK(!r3.next(error) && error);
    CHECK(!r3.next(error) && error); // stays broken
}

NNH_TEST(sealed_frames_open_only_once_in_order_from_the_other_side) {
    SecureChannel console(key_of(1), Side::Console);
    SecureChannel server(key_of(1), Side::Server);
    const auto first = frame_of(console.seal(Type::ListGames, {}));
    const auto second = frame_of(console.seal(Type::Build, encode(BuildRequest{"hello", false})));
    const auto opened = server.open(first);
    CHECK(opened && opened->empty());
    CHECK(!server.open(first)); // replayed
    const auto build = server.open(second);
    BuildRequest b;
    CHECK(build && decode(*build, b) && b.game == "hello");
    // Reordering: the third sent first is refused.
    const auto third = frame_of(console.seal(Type::Launching, encode_text("hello")));
    const auto fourth = frame_of(console.seal(Type::Launching, encode_text("again")));
    CHECK(!server.open(fourth));
    CHECK(server.open(third).has_value());
}

NNH_TEST(sealed_frames_reject_tampering_reflection_and_wrong_keys) {
    SecureChannel console(key_of(1), Side::Console);
    SecureChannel server(key_of(1), Side::Server);
    Frame f = frame_of(console.seal(Type::Build, encode(BuildRequest{"hello", false})));
    Frame tampered = f;
    tampered.payload[6] ^= 1;
    CHECK(!server.open(tampered));
    Frame retyped = f;
    retyped.type = Type::Launching; // the type is covered by the MAC too
    CHECK(!server.open(retyped));
    CHECK(!console.open(f)); // reflected back to its sender
    SecureChannel stranger(key_of(2), Side::Server);
    CHECK(!stranger.open(f));
    CHECK(server.open(f).has_value()); // the untouched frame still opens
    CHECK(!server.open(Frame{Type::Build, {1, 2, 3}})); // too short to be sealed
}

NNH_TEST(discovery_answers_round_trip_and_reject_junk) {
    Id id;
    for (std::size_t i = 0; i < id.size(); ++i) id[i] = static_cast<std::uint8_t>(i * 17);
    const std::string answer = discovery_answer(id, "RAFA-PC");
    Id got;
    std::string name;
    CHECK(parse_discovery_answer(answer.data(), answer.size(), got, name) && got == id && name == "RAFA-PC");
    const std::string odd = discovery_answer(id, "caf\xC3\xA9\n" + std::string(40, 'x'));
    CHECK(parse_discovery_answer(odd.data(), odd.size(), got, name) && name.size() == kMaxServerName && name.find('?') != std::string::npos);
    for (const std::string& bad : {std::string("NNHSERVE1 "), std::string("NNHSERVE2 ") + answer.substr(10),
                                   answer.substr(0, answer.size() - 8) + "\x01", answer.substr(0, 41) + "zz" + answer.substr(43)}) {
        CHECK(!parse_discovery_answer(bad.data(), bad.size(), got, name));
    }
    CHECK(is_discovery_question("NNHLINK?1", 9) && !is_discovery_question("NNHLINK?2", 9));
}
