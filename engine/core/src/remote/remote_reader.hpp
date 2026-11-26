#pragma once
// Console remote reader (spec 013 FR-8): the bytes the PC sent → validated messages. Handles messages split
// across reads, requires Hello first, keeps frames in order, and turns itself off after kMaxBadInARow invalid
// messages in a row. Pure, fixed size, no heap; a fuzz target (tests/fuzz). Tests: tests/remote/test_remote_reader.cpp.
// Payload mode (spec 031, D54): a decoded message may announce raw bytes after it (payload_length()); they come out as
// pieces pointing into the caller's buffer, then decoding resumes (tests/remote/test_remote_reader_payload.cpp).

#include <cstddef>
#include <cstdint>

#include "remote/remote_wire.hpp"

namespace nnh::detail::remote {

/// Invalid messages in a row that switch the remote off until the PC reconnects (reset()).
inline constexpr int kMaxBadInARow = 8;

/// One complete message read: accepted (`reason` None, frame resolved) or rejected (only `message.seq` is
/// meaningful: the reply is Rejected{seq, reason}) — or, when `payload_size` > 0, a piece of the payload of the last
/// accepted message (`payload` points into the bytes given to feed(); `payload_end` on its last piece).
struct ReadItem {
    Reject reason = Reject::None;
    Message message;
    const std::uint8_t* payload = nullptr;
    std::uint32_t payload_size = 0;
    bool payload_end = false;
};

struct FeedResult {
    std::size_t items = 0;    ///< entries written to `out`
    std::size_t consumed = 0; ///< bytes used; less than `size` only when `out` filled up
};

class RemoteReader {
public:
    /// How many bytes to read now so that at most `room` complete messages come out (counts the bytes of a
    /// message already partly read). 0 when `room` is 0. In payload mode: the payload bytes left (the caller caps it).
    /// A read may still yield more items than `room` (a message's payload piece is an item too): feed() stops when
    /// `out` is full, and the caller feeds the rest of the bytes after handling those items.
    std::size_t bytes_wanted(std::size_t room) const;

    /// Takes the next bytes of the stream at the start of frame `now`. Writes one item per complete message, at
    /// most `capacity`, and stops there (FeedResult::consumed tells how far it got). Rules, in order: while
    /// off → TooMany (anything); invalid bytes → their decode reason; Hello → accepted (attaches; its frame isn't ordered);
    /// before Hello → NotAttached; a resolved frame lower than the last accepted one → Order (not for the reload
    /// transfer messages, handled at receipt); Bye → accepted and detaches. kMaxBadInARow rejections in a row switch
    /// the reader off. A decoded message's payload follows it: pieces for an accepted message, skipped for a refused
    /// one (a decode failure has none: its length is never trusted). Never allocates.
    FeedResult feed(const std::uint8_t* data, std::size_t size, std::uint32_t now, ReadItem* out,
                    std::size_t capacity);

    bool attached() const { return attached_; }
    /// Payload bytes still expected after the last message (0: decoding messages).
    std::uint32_t payload_left() const { return payload_left_; }
    /// True after kMaxBadInARow invalid messages in a row ("remote input off"), until reset().
    bool off() const { return off_; }
    /// A new connection: forget everything.
    void reset() { *this = RemoteReader{}; }

private:
    ReadItem judge(const std::uint8_t* bytes, std::uint32_t now);

    std::uint8_t partial_[kMessageSize] = {};
    std::size_t partial_size_ = 0;
    std::uint32_t last_frame_ = 0;
    std::uint32_t payload_left_ = 0;
    bool payload_skip_ = false; // the payload of a refused message: consumed, not handed out
    int bad_in_row_ = 0;
    bool attached_ = false;
    bool off_ = false;
};

} // namespace nnh::detail::remote
