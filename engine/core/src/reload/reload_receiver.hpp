#pragma once
// The reload receiver (spec 031 D54, FR-4, FR-18): the transfer messages of one batch and their payload pieces → calls on
// a sink that writes files. One batch at a time; files in index order; a put's chunks contiguous from offset 0 to its
// size; paths valid asset paths; sizes and paths adding up to the batch's total; a put's CRC-32 checked after its last
// chunk. Any refusal discards the batch (sink.discard) and ignores the rest of it until the next Begin or Cancel.
// Pure (sink of function pointers), fixed size, no heap; fuzzed. Tests: tests/reload/test_reload_receiver.cpp.

#include <cstddef>
#include <cstdint>

#include "reload/reload_wire.hpp"

namespace nnh::detail::reload {

/// Where a batch's files go. Every function returns false on a write failure (the batch is then refused).
struct ReceiverSink {
    void* context = nullptr;
    /// A new file of the batch: `path` (NUL-terminated, valid), a put of `size` bytes or a remove (size 0).
    bool (*begin_file)(void* context, std::uint16_t index, const char* path, remote::ReloadOp op, std::uint32_t size) =
        nullptr;
    /// The next bytes of the open put.
    bool (*append)(void* context, const std::uint8_t* data, std::size_t size) = nullptr;
    /// The open file is complete (and its CRC matched).
    bool (*finish)(void* context, std::uint16_t index) = nullptr;
    /// Forget every file of the open or staged batch.
    void (*discard)(void* context) = nullptr;
};

/// What a call changed.
enum class ReceiverEvent : std::uint8_t {
    None,      ///< nothing to report
    Staged,    ///< the batch's last file was received and checked: ReloadStaged
    Refused,   ///< the batch was dropped: ReloadRefused with refusal()
    Cancelled, ///< a Cancel dropped the batch
};

struct ReceiverRefusal {
    std::uint16_t batch = 0;
    remote::ReloadReason reason = remote::ReloadReason::None;
    std::uint16_t file = remote::kNoFile;
};

class ReloadReceiver {
public:
    enum class State : std::uint8_t { Idle, Open, Staged, Dropping };

    /// A decoded, accepted transfer message (Begin, File, Chunk, Cancel). Notices are not the receiver's.
    ReceiverEvent on_message(const remote::Message& m, const ReceiverSink& sink);
    /// A piece of the payload of the last message given to on_message().
    ReceiverEvent on_payload(const std::uint8_t* data, std::size_t size, const ReceiverSink& sink);
    /// Apply for `batch` with `files`: true and back to Idle when that batch is staged (its files now belong to the
    /// caller); false otherwise (refusal() says why; a staged batch stays staged).
    bool take(std::uint16_t batch, std::uint16_t files);
    /// The connection ended: an open or staged batch is discarded. @return true if there was one.
    bool detach(const ReceiverSink& sink);

    State state() const { return state_; }
    std::uint16_t batch() const { return batch_; }
    std::uint16_t files() const { return files_; }
    const ReceiverRefusal& refusal() const { return refusal_; }

private:
    ReceiverEvent refuse(remote::ReloadReason reason, const ReceiverSink& sink);
    ReceiverEvent begin(const remote::Message& m);
    ReceiverEvent file(const remote::Message& m, const ReceiverSink& sink);
    ReceiverEvent chunk(const remote::Message& m, const ReceiverSink& sink);
    ReceiverEvent file_header_done(const ReceiverSink& sink);
    ReceiverEvent file_done(const ReceiverSink& sink);

    enum class Expect : std::uint8_t { Nothing, Path, Crc, Chunk };

    State state_ = State::Idle;
    Expect expect_ = Expect::Nothing;
    bool file_open_ = false;   // a put whose bytes are still arriving
    remote::ReloadOp op_ = remote::ReloadOp::Put;
    std::uint16_t batch_ = 0, files_ = 0, next_ = 0, current_ = 0;
    std::uint32_t total_ = 0, counted_ = 0;   // announced, and paths + sizes so far
    std::uint32_t size_ = 0, received_ = 0;   // the current put
    std::uint32_t crc_ = 0, crc_expected_ = 0;
    std::uint32_t piece_left_ = 0;            // bytes of the current payload still expected
    std::uint8_t path_length_ = 0, path_got_ = 0, crc_got_ = 0;
    std::uint8_t crc_bytes_[4] = {};
    char path_[remote::kReloadMaxPath + 1] = {};
    ReceiverRefusal refusal_;
};

} // namespace nnh::detail::reload
