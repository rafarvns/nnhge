// Event ring buffer (see event_queue.hpp).
//
// Layout: records of [Header | payload], each padded to 8 bytes. When a record doesn't fit before the
// end of the buffer, the writer leaves a "wrap" header (type == nullptr) — or nothing, if not even a
// header fits — and continues at offset 0. The reader skips the same way.

#include "events/event_queue.hpp"

#include <cstring>

namespace nnh::detail {
namespace {

constexpr std::size_t kAlign = 8;

struct Header {
    TypeId type; // nullptr marks "wrap to the start"
    std::uint32_t sequence;
    std::uint16_t size;
    std::uint16_t depth;
};

constexpr std::size_t align_up(std::size_t n) { return (n + kAlign - 1) & ~(kAlign - 1); }
constexpr std::size_t kHeaderBytes = align_up(sizeof(Header));
constexpr std::size_t record_bytes(std::size_t payload) { return kHeaderBytes + align_up(payload); }

} // namespace

void EventQueue::reset(void* buffer, std::size_t capacity) {
    buffer_ = static_cast<unsigned char*>(buffer);
    capacity_ = capacity & ~(kAlign - 1);
    head_ = tail_ = count_ = 0;
    next_sequence_ = 0;
}

bool EventQueue::push(TypeId type, const void* data, std::size_t size, std::uint16_t depth) {
    const std::size_t need = record_bytes(size);
    if (count_ == 0) head_ = tail_ = 0;
    std::size_t at;
    if (count_ > 0 && tail_ == head_) {
        return false; // completely full
    } else if (tail_ >= head_) {
        // Free space: [tail, capacity) then [0, head).
        if (capacity_ - tail_ >= need) {
            at = tail_;
        } else if (need < head_ || (count_ == 0 && need <= capacity_)) {
            if (capacity_ - tail_ >= kHeaderBytes) {
                Header wrap{nullptr, 0, 0, 0};
                std::memcpy(buffer_ + tail_, &wrap, sizeof wrap);
            }
            at = 0;
        } else {
            return false;
        }
    } else {
        // Free space: [tail, head). Filling it exactly makes tail == head, which (with count > 0) means full.
        if (head_ - tail_ < need) return false;
        at = tail_;
    }

    const Header header{type, next_sequence_++, static_cast<std::uint16_t>(size), depth};
    std::memcpy(buffer_ + at, &header, sizeof header);
    if (size > 0) std::memcpy(buffer_ + at + kHeaderBytes, data, size);
    tail_ = at + need;
    ++count_;
    return true;
}

void EventQueue::skip_wrap_at_head() {
    if (capacity_ - head_ < kHeaderBytes) {
        head_ = 0;
        return;
    }
    Header header;
    std::memcpy(&header, buffer_ + head_, sizeof header);
    if (header.type == nullptr) head_ = 0;
}

bool EventQueue::front(EventRecord& out) {
    if (count_ == 0) return false;
    skip_wrap_at_head();
    Header header;
    std::memcpy(&header, buffer_ + head_, sizeof header);
    out = EventRecord{header.type, header.sequence, header.size, header.depth, buffer_ + head_ + kHeaderBytes};
    return true;
}

void EventQueue::pop() {
    if (count_ == 0) return;
    skip_wrap_at_head();
    Header header;
    std::memcpy(&header, buffer_ + head_, sizeof header);
    head_ += record_bytes(header.size);
    if (--count_ == 0) head_ = tail_ = 0;
}

} // namespace nnh::detail
