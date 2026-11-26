// The reload receiver (see reload_receiver.hpp).

#include "reload/reload_receiver.hpp"

#include "assets/asset_path.hpp"
#include "core/crc32.hpp"

namespace nnh::detail::reload {

using remote::Message;
using remote::MessageType;
using remote::ReloadOp;
using remote::ReloadReason;

ReceiverEvent ReloadReceiver::refuse(ReloadReason reason, const ReceiverSink& sink) {
    refusal_.reason = reason;
    if (state_ == State::Open || state_ == State::Staged) sink.discard(sink.context);
    state_ = State::Dropping;
    expect_ = Expect::Nothing;
    file_open_ = false;
    return ReceiverEvent::Refused;
}

ReceiverEvent ReloadReceiver::begin(const Message& m) {
    if (state_ == State::Open || state_ == State::Staged) { // busy: the batch in progress keeps going
        refusal_ = {.batch = remote::reload_batch(m), .reason = ReloadReason::Batch, .file = remote::kNoFile};
        return ReceiverEvent::Refused;
    }
    state_ = State::Open;
    expect_ = Expect::Nothing;
    file_open_ = false;
    batch_ = remote::reload_batch(m);
    files_ = remote::reload_files(m);
    total_ = remote::reload_total(m);
    next_ = 0;
    counted_ = 0;
    refusal_ = {.batch = batch_, .reason = ReloadReason::None, .file = remote::kNoFile};
    return ReceiverEvent::None;
}

ReceiverEvent ReloadReceiver::file(const Message& m, const ReceiverSink& sink) {
    const std::uint16_t index = remote::reload_index(m);
    if (state_ != State::Open) {
        refusal_ = {.batch = state_ == State::Idle ? std::uint16_t{0} : batch_, .reason = ReloadReason::Order,
                    .file = index};
        if (state_ == State::Staged) return refuse(ReloadReason::Order, sink);
        state_ = State::Dropping;
        return ReceiverEvent::Refused;
    }
    refusal_.file = index;
    if (file_open_ || expect_ != Expect::Nothing) return refuse(ReloadReason::Size, sink); // the last put is short
    if (index != next_ || index >= files_) return refuse(ReloadReason::Order, sink);
    op_ = remote::reload_op(m);
    size_ = remote::reload_size(m);
    path_length_ = remote::reload_path_length(m);
    const std::uint64_t counted = std::uint64_t{counted_} + path_length_ + size_;
    if (counted > total_) return refuse(ReloadReason::Total, sink);
    counted_ = static_cast<std::uint32_t>(counted);
    current_ = index;
    path_got_ = 0;
    crc_got_ = 0;
    received_ = 0;
    crc_ = 0;
    expect_ = Expect::Path;
    return ReceiverEvent::None;
}

ReceiverEvent ReloadReceiver::chunk(const Message& m, const ReceiverSink& sink) {
    if (state_ != State::Open) {
        refusal_ = {.batch = state_ == State::Idle ? std::uint16_t{0} : batch_, .reason = ReloadReason::Order,
                    .file = remote::reload_index(m)};
        if (state_ == State::Staged) return refuse(ReloadReason::Order, sink);
        state_ = State::Dropping;
        return ReceiverEvent::Refused;
    }
    refusal_.file = remote::reload_index(m);
    if (!file_open_ || expect_ != Expect::Nothing || remote::reload_index(m) != current_) {
        return refuse(ReloadReason::Order, sink);
    }
    if (remote::reload_offset(m) != received_) return refuse(ReloadReason::Offset, sink);
    piece_left_ = remote::reload_chunk_length(m);
    if (piece_left_ > size_ - received_) return refuse(ReloadReason::Size, sink);
    expect_ = Expect::Chunk;
    return ReceiverEvent::None;
}

ReceiverEvent ReloadReceiver::on_message(const Message& m, const ReceiverSink& sink) {
    switch (m.type) {
    case MessageType::ReloadBegin: return begin(m);
    case MessageType::ReloadFile:
        if (state_ == State::Dropping) return ReceiverEvent::None;
        return file(m, sink);
    case MessageType::ReloadChunk:
        if (state_ == State::Dropping) return ReceiverEvent::None;
        return chunk(m, sink);
    case MessageType::ReloadCancel: {
        const std::uint16_t batch = remote::reload_batch(m);
        if (state_ == State::Idle || (batch != 0 && batch != batch_)) return ReceiverEvent::None;
        const bool had = state_ == State::Open || state_ == State::Staged;
        if (had) sink.discard(sink.context);
        state_ = State::Idle;
        expect_ = Expect::Nothing;
        file_open_ = false;
        refusal_ = {.batch = batch_, .reason = ReloadReason::None, .file = remote::kNoFile};
        return had ? ReceiverEvent::Cancelled : ReceiverEvent::None;
    }
    default: return ReceiverEvent::None;
    }
}

ReceiverEvent ReloadReceiver::file_header_done(const ReceiverSink& sink) {
    path_[path_length_] = '\0';
    for (int i = 0; i < path_length_; ++i) {
        if (path_[i] == '\0') return refuse(ReloadReason::Path, sink);
    }
    if (!valid_asset_path(path_)) return refuse(ReloadReason::Path, sink);
    crc_expected_ = static_cast<std::uint32_t>(crc_bytes_[0]) | (static_cast<std::uint32_t>(crc_bytes_[1]) << 8) |
                    (static_cast<std::uint32_t>(crc_bytes_[2]) << 16) |
                    (static_cast<std::uint32_t>(crc_bytes_[3]) << 24);
    expect_ = Expect::Nothing;
    if (!sink.begin_file(sink.context, current_, path_, op_, op_ == ReloadOp::Put ? size_ : 0)) {
        return refuse(ReloadReason::Write, sink);
    }
    if (op_ == ReloadOp::Put && size_ > 0) {
        file_open_ = true;
        return ReceiverEvent::None;
    }
    return file_done(sink);
}

ReceiverEvent ReloadReceiver::file_done(const ReceiverSink& sink) {
    file_open_ = false;
    if (op_ == ReloadOp::Put && crc_ != crc_expected_) return refuse(ReloadReason::Crc, sink);
    if (!sink.finish(sink.context, current_)) return refuse(ReloadReason::Write, sink);
    next_ = static_cast<std::uint16_t>(current_ + 1);
    if (next_ < files_) return ReceiverEvent::None;
    refusal_.file = remote::kNoFile;
    if (counted_ != total_) return refuse(ReloadReason::Total, sink);
    state_ = State::Staged;
    return ReceiverEvent::Staged;
}

ReceiverEvent ReloadReceiver::on_payload(const std::uint8_t* data, std::size_t size, const ReceiverSink& sink) {
    if (state_ != State::Open) return ReceiverEvent::None; // a dropped batch's bytes (or none expected)
    std::size_t at = 0;
    while (at < size) {
        switch (expect_) {
        case Expect::Nothing: return refuse(ReloadReason::Order, sink);
        case Expect::Path:
            path_[path_got_++] = static_cast<char>(data[at++]);
            if (path_got_ < path_length_) break;
            if (op_ == ReloadOp::Put) {
                expect_ = Expect::Crc;
                break;
            }
            return file_header_done(sink);
        case Expect::Crc:
            crc_bytes_[crc_got_++] = data[at++];
            if (crc_got_ == 4) {
                const ReceiverEvent e = file_header_done(sink);
                if (e != ReceiverEvent::None || at == size) return e;
            }
            break;
        case Expect::Chunk: {
            const std::size_t n = size - at < piece_left_ ? size - at : piece_left_;
            if (!sink.append(sink.context, data + at, n)) return refuse(ReloadReason::Write, sink);
            crc_ = crc32_update(crc_, data + at, n);
            at += n;
            received_ += static_cast<std::uint32_t>(n);
            piece_left_ -= static_cast<std::uint32_t>(n);
            if (piece_left_ == 0) {
                expect_ = Expect::Nothing;
                if (received_ == size_) return file_done(sink);
            }
            break;
        }
        }
    }
    return ReceiverEvent::None;
}

bool ReloadReceiver::take(std::uint16_t batch, std::uint16_t files) {
    if (state_ != State::Staged || batch != batch_ || files != files_) {
        refusal_ = {.batch = batch, .reason = ReloadReason::Batch, .file = remote::kNoFile};
        return false;
    }
    state_ = State::Idle;
    return true;
}

bool ReloadReceiver::detach(const ReceiverSink& sink) {
    const bool had = state_ == State::Open || state_ == State::Staged;
    if (had) sink.discard(sink.context);
    state_ = State::Idle;
    expect_ = Expect::Nothing;
    file_open_ = false;
    return had;
}

} // namespace nnh::detail::reload
