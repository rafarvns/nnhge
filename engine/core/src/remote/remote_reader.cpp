// Console remote reader (see remote_reader.hpp).

#include "remote/remote_reader.hpp"

namespace nnh::detail::remote {

std::size_t RemoteReader::bytes_wanted(std::size_t room) const {
    return room == 0 ? 0 : room * kMessageSize - partial_size_;
}

FeedResult RemoteReader::feed(const std::uint8_t* data, std::size_t size, std::uint32_t now, ReadItem* out,
                              std::size_t capacity) {
    FeedResult result;
    while (result.consumed < size) {
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
    } else if (item.message.type == MessageType::Hello) {
        attached_ = true;
    } else if (!attached_) {
        item.reason = Reject::NotAttached;
    } else {
        item.message.frame = resolve_frame(item.message.frame, now);
        if (item.message.frame < last_frame_) {
            item.reason = Reject::Order;
        } else {
            last_frame_ = item.message.frame;
            if (item.message.type == MessageType::Bye) attached_ = false;
        }
    }
    bad_in_row_ = item.reason == Reject::None ? 0 : bad_in_row_ + 1;
    if (bad_in_row_ >= kMaxBadInARow) off_ = true;
    return item;
}

} // namespace nnh::detail::remote
