#include "link_targets.hpp"

#include <cstdlib>
#include <string>
#include <vector>

#include "nnh_link/discovery.hpp"
#include "nnh_link/frame.hpp"
#include "nnh_link/messages.hpp"

namespace nnh::link::fuzz {

namespace {

/// A decoded message must survive encode → decode.
template <typename T>
void round_trip(const std::vector<std::uint8_t>& p) {
    T m{};
    if (!decode(p, m)) return;
    T again{};
    if (!decode(encode(m), again)) std::abort();
}

} // namespace

int messages_target(const std::uint8_t* data, std::size_t size) {
    const std::vector<std::uint8_t> p(data, data + size);
    round_trip<Hello>(p);
    round_trip<BuildRequest>(p);
    round_trip<BuildResult>(p);
    round_trip<std::vector<GameInfo>>(p);
    std::string text;
    if (decode_text(p, text) && !decode_text(encode_text(text), text)) std::abort();
    std::vector<std::string> lines;
    if (decode_lines(p, lines) && !decode_lines(encode_lines(lines), lines)) std::abort();
    Digest d;
    decode_digest(p, d);
    return 0;
}

int frames_target(const std::uint8_t* data, std::size_t size) {
    // The first byte picks how the stream is cut into reads, the rest is the stream.
    if (size == 0) return 0;
    const std::size_t step = 1 + data[0] % 17;
    FrameReader reader;
    Digest key;
    key.fill(0x5A);
    SecureChannel channel(key, Side::Console);
    bool error = false;
    for (std::size_t at = 1; at < size && !error; at += step) {
        const std::size_t n = at + step <= size ? step : size - at;
        reader.feed(data + at, n);
        while (auto f = reader.next(error)) {
            if (f->payload.size() > kMaxFrame) std::abort();
            channel.open(*f);
        }
    }
    return 0;
}

int discovery_target(const std::uint8_t* data, std::size_t size) {
    const char* text = reinterpret_cast<const char*>(data);
    is_discovery_question(text, size);
    Id id;
    std::string name;
    if (parse_discovery_answer(text, size, id, name)) {
        const std::string again = discovery_answer(id, name);
        Id id2;
        std::string name2;
        if (!parse_discovery_answer(again.data(), again.size(), id2, name2) || id2 != id || name2 != name) std::abort();
    }
    return 0;
}

} // namespace nnh::link::fuzz
