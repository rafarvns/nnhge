// Deterministic mini-fuzz of NNH Link's fuzz targets (fuzz/link_targets.hpp), run on every compiler:
// valid seeds plus pseudo-random mutations. The real fuzzing (libFuzzer + ASan) runs on Linux.

#include <cstdint>
#include <vector>

#include "fuzz/link_targets.hpp"
#include "nnh_link/discovery.hpp"
#include "nnh_link/frame.hpp"
#include "nnh_link/messages.hpp"
#include "test.hpp"

using namespace nnh::link;

namespace {

std::uint32_t next(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u; // LCG: reproducible
    return state;
}

void mutate_and_run(int (*target)(const std::uint8_t*, std::size_t), const std::vector<std::uint8_t>& seed) {
    std::vector<std::uint8_t> buffer;
    std::uint32_t state = 4242;
    for (int i = 0; i < 5000; ++i) {
        buffer = seed;
        const int flips = static_cast<int>(next(state) % 4);
        for (int f = 0; f < flips && !buffer.empty(); ++f) {
            buffer[next(state) % buffer.size()] ^= static_cast<std::uint8_t>(next(state));
        }
        if (next(state) % 3 == 0 && !buffer.empty()) buffer.resize(next(state) % buffer.size()); // cut
        if (next(state) % 5 == 0) buffer.push_back(static_cast<std::uint8_t>(next(state)));    // grow
        target(buffer.data(), buffer.size());
    }
}

} // namespace

NNH_TEST(link_message_decoders_survive_mutated_inputs) {
    Hello h;
    h.id.fill(1);
    const std::vector<std::vector<std::uint8_t>> seeds = {
        encode(h), encode(BuildRequest{"audio", true}), encode(BuildResult{1234, {}}),
        encode(std::vector<GameInfo>{{"hello", "Hello"}, {"audio", "Audio"}}), encode_text("main.cpp:3: error"),
        encode_lines({"a", "b", "c"}), {}};
    for (const auto& seed : seeds) mutate_and_run(fuzz::messages_target, seed);
    CHECK(true);
}

NNH_TEST(link_frame_reader_survives_mutated_streams) {
    Digest key;
    key.fill(0x5A);
    SecureChannel server(key, Side::Server); // same key as the target's console side: some frames open
    std::vector<std::uint8_t> stream = {3};
    for (const auto& f : {server.seal(Type::Games, encode(std::vector<GameInfo>{{"hello", "Hello"}})),
                          server.seal(Type::BuildLine, encode_text("compiling")), encode_frame(Type::ListGames, {})}) {
        stream.insert(stream.end(), f.begin(), f.end());
    }
    mutate_and_run(fuzz::frames_target, stream);
    CHECK(true);
}

NNH_TEST(link_discovery_parser_survives_mutated_answers) {
    Id id;
    id.fill(0xC3);
    const std::string answer = discovery_answer(id, "RAFA-PC");
    mutate_and_run(fuzz::discovery_target, std::vector<std::uint8_t>(answer.begin(), answer.end()));
    const std::string question = kDiscoveryQuestion;
    mutate_and_run(fuzz::discovery_target, std::vector<std::uint8_t>(question.begin(), question.end()));
    CHECK(true);
}
