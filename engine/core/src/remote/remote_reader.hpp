#pragma once
// Console remote reader (spec 013 FR-8): the bytes the PC sent → validated messages. Handles messages split
// across reads, requires Hello first, keeps frames in order, and turns itself off after kMaxBadInARow invalid
// messages in a row. Pure, fixed size, no heap; a fuzz target (tests/fuzz). Tests: tests/remote/test_remote_reader.cpp.

#include <cstddef>
#include <cstdint>

#include "remote/remote_wire.hpp"

namespace nnh::detail::remote {

/// Invalid messages in a row that switch the remote off until the PC reconnects (reset()).
inline constexpr int kMaxBadInARow = 8;

/// One complete message read: accepted (`reason` None, frame resolved) or rejected (only `message.seq` is
/// meaningful: the reply is Rejected{seq, reason}).
struct ReadItem {
    Reject reason = Reject::None;
    Message message;
};

struct FeedResult {
    std::size_t items = 0;    ///< entries written to `out`
    std::size_t consumed = 0; ///< bytes used; less than `size` only when `out` filled up
};

class RemoteReader {
public:
    /// How many bytes to read now so that at most `room` complete messages come out (counts the bytes of a
    /// message already partly read). 0 when `room` is 0.
    std::size_t bytes_wanted(std::size_t room) const;

    /// Takes the next bytes of the stream at the start of frame `now`. Writes one item per complete message, at
    /// most `capacity`, and stops there (FeedResult::consumed tells how far it got). Rules, in order: while
    /// off → TooMany (anything); invalid bytes → their decode reason; Hello → accepted (attaches; its frame isn't ordered);
    /// before Hello → NotAttached; a resolved frame lower than the last accepted one → Order; Bye → accepted and
    /// detaches. kMaxBadInARow rejections in a row switch the reader off. Never allocates.
    FeedResult feed(const std::uint8_t* data, std::size_t size, std::uint32_t now, ReadItem* out,
                    std::size_t capacity);

    bool attached() const { return attached_; }
    /// True after kMaxBadInARow invalid messages in a row ("remote input off"), until reset().
    bool off() const { return off_; }
    /// A new connection: forget everything.
    void reset() { *this = RemoteReader{}; }

private:
    ReadItem judge(const std::uint8_t* bytes, std::uint32_t now);

    std::uint8_t partial_[kMessageSize] = {};
    std::size_t partial_size_ = 0;
    std::uint32_t last_frame_ = 0;
    int bad_in_row_ = 0;
    bool attached_ = false;
    bool off_ = false;
};

} // namespace nnh::detail::remote
