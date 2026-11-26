// Console remote reader (see remote_reader.hpp).

#include "remote/remote_reader.hpp"

#include "reload/reload_wire.hpp"

namespace nnh::detail::remote {

std::size_t RemoteReader::bytes_wanted(std::size_t room) const {
    if (room == 0) return 0;
    if (payload_left_ > 0) return payload_left_; // one piece: one output slot
    return room * kMessageSize - partial_size_;
}

FeedResult RemoteReader::feed(const std::uint8_t* data, std::size_t size, std::uint32_t now, ReadItem* out,
                              std::size_t capacity) {
    FeedResult result;
    while (result.consumed < size) {
        if (payload_left_ > 0) { // payload mode: hand the bytes out (or skip them) as one piece
            const std::size_t available = size - result.consumed;
            const std::uint32_t n = available < payload_left_ ? static_cast<std::uint32_t>(available) : payload_left_;
            if (!payload_skip_) {
                if (result.items == capacity) break;
                ReadItem& piece = out[result.items++];
                piece = ReadItem{};
                piece.payload = data + result.consumed;
                piece.payload_size = n;
                piece.payload_end = n == payload_left_;
            }
            payload_left_ -= n;
            result.consumed += n;
            continue;
        }
        // The byte that completes a message needs a free output slot.
        if (partial_size_ == kMessageSize - 1 && result.items == capacity) break;
        partial_[partial_size_++] = data[result.consumed++];
        if (partial_size_ < kMessageSize) continue;
        partial_size_ = 0;
        out[result.items++] = judge(partial_, now);
    }
    return result;
}

ReadItem RemoteReader::judge(const std::uint8_t* bytes, std::uint32_t now) {
    ReadItem item;
    item.reason = off_ ? Reject::TooMany : decode_message(bytes, item.message);
    if (item.reason != Reject::None) {
        item.message = Message{};
        item.message.seq = message_seq(bytes);
        if (off_) return item;
    } else {
        payload_left_ = payload_length(item.message); // a decoded message's payload follows it, accepted or not
        payload_skip_ = false;
    }
    if (item.reason != Reject::None) {
        // counted below
    } else if (item.message.type == MessageType::Hello) {
        attached_ = true;
    } else if (!attached_) {
        item.reason = Reject::NotAttached;
    } else if (is_reload_transfer(item.message.type)) {
        item.message.frame = now; // handled at receipt: never scheduled, never ordered
    } else {
        item.message.frame = resolve_frame(item.message.frame, now);
        if (item.message.frame < last_frame_) {
            item.reason = Reject::Order;
        } else {
            last_frame_ = item.message.frame;
            if (item.message.type == MessageType::Bye) attached_ = false;
        }
    }
    if (item.reason != Reject::None) payload_skip_ = true;
    bad_in_row_ = item.reason == Reject::None ? 0 : bad_in_row_ + 1;
    if (bad_in_row_ >= kMaxBadInARow) off_ = true;
    return item;
}

} // namespace nnh::detail::remote
