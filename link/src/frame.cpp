#include "nnh_link/frame.hpp"

#include "nnh_link/wire.hpp"

namespace nnh::link {

std::vector<std::uint8_t> encode_frame(Type type, const std::vector<std::uint8_t>& payload) {
    Writer w;
    w.u32(static_cast<std::uint32_t>(1 + payload.size()));
    w.u8(static_cast<std::uint8_t>(type));
    w.raw(payload.data(), payload.size());
    return std::move(w.bytes());
}

void FrameReader::feed(const void* data, std::size_t size) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    buffer_.insert(buffer_.end(), p, p + size);
}

std::optional<Frame> FrameReader::next(bool& error) {
    error = broken_;
    if (broken_ || buffer_.size() < 4) return std::nullopt;
    Reader r(buffer_.data(), 4);
    const std::uint32_t length = r.u32();
    if (length == 0 || length > kMaxFrame) {
        broken_ = error = true;
        return std::nullopt;
    }
    if (buffer_.size() < 4 + std::size_t{length}) return std::nullopt;
    const std::uint8_t type = buffer_[4];
    if (!valid_type(type)) {
        broken_ = error = true;
        return std::nullopt;
    }
    Frame f{static_cast<Type>(type), std::vector<std::uint8_t>(buffer_.begin() + 5, buffer_.begin() + 4 + length)};
    buffer_.erase(buffer_.begin(), buffer_.begin() + 4 + length);
    return f;
}

std::array<std::uint8_t, 16> SecureChannel::mac(Side from, std::uint32_t sequence, Type type, const std::uint8_t* message,
                                                std::size_t size) const {
    Writer w;
    w.u8(static_cast<std::uint8_t>(from));
    w.u32(sequence);
    w.u8(static_cast<std::uint8_t>(type));
    w.raw(message, size);
    const Digest d = hmac_sha256(key_.data(), key_.size(), w.bytes().data(), w.bytes().size());
    std::array<std::uint8_t, 16> out;
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = d[i];
    return out;
}

std::vector<std::uint8_t> SecureChannel::seal(Type type, const std::vector<std::uint8_t>& message) {
    const std::uint32_t sequence = ++sent_;
    Writer w;
    w.u32(sequence);
    w.raw(message.data(), message.size());
    const auto tag = mac(me_, sequence, type, message.data(), message.size());
    w.raw(tag.data(), tag.size());
    return encode_frame(type, w.bytes());
}

std::optional<std::vector<std::uint8_t>> SecureChannel::open(const Frame& frame) {
    const std::vector<std::uint8_t>& p = frame.payload;
    if (p.size() < 4 + 16) return std::nullopt;
    Reader r(p.data(), 4);
    const std::uint32_t sequence = r.u32();
    const std::size_t size = p.size() - 4 - 16;
    const Side them = me_ == Side::Console ? Side::Server : Side::Console;
    const auto expected = mac(them, sequence, frame.type, p.data() + 4, size);
    if (!equal_constant_time(expected.data(), p.data() + 4 + size, 16) || sequence != received_ + 1) return std::nullopt;
    received_ = sequence;
    return std::vector<std::uint8_t>(p.begin() + 4, p.begin() + 4 + static_cast<long>(size));
}

} // namespace nnh::link
