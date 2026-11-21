// The game's stream on the PC (see game_stream.hpp). A byte 0x1E may start a record: the next 7 bytes
// decide. A valid header switches to the payload; anything else makes the 0x1E plain text and the other
// bytes are looked at again (one of them may start the real record).

#include "remote/game_stream.hpp"

namespace nnh::tool::remote {

void GameStream::feed(const std::uint8_t* data, std::size_t size, std::vector<StreamItem>& out) {
    std::size_t at = 0;
    while (at < size) {
        if (mode_ == Mode::Payload) { // bulk copy: a screen is ~300 KB
            const std::size_t want = record_.length - payload_.size();
            const std::size_t n = size - at < want ? size - at : want;
            payload_.insert(payload_.end(), data + at, data + at + n);
            at += n;
            if (payload_.size() == record_.length) record_complete(out);
        } else {
            take(data[at++], out);
        }
    }
}

void GameStream::finish(std::vector<StreamItem>& out) {
    if (mode_ == Mode::Header) {
        for (std::size_t i = 0; i < header_size_; ++i) text_byte(header_[i], out);
    } else if (mode_ == Mode::Payload) {
        for (std::uint8_t b : header_) text_byte(b, out);
        for (std::uint8_t b : payload_) text_byte(b, out);
    }
    mode_ = Mode::Text;
    header_size_ = 0;
    payload_.clear();
    if (!line_.empty()) end_line(out);
}

void GameStream::take(std::uint8_t byte, std::vector<StreamItem>& out) {
    if (mode_ == Mode::Text) {
        if (byte == wire::kRecordMark[0]) {
            mode_ = Mode::Header;
            header_[0] = byte;
            header_size_ = 1;
        } else {
            text_byte(byte, out);
        }
        return;
    }
    header_[header_size_++] = byte;
    if (header_size_ == wire::kRecordHeaderSize) header_complete(out);
}

void GameStream::header_complete(std::vector<StreamItem>& out) {
    header_size_ = 0;
    if (wire::decode_record_header(header_, record_)) {
        if (!line_.empty()) end_line(out); // the partial line came first
        mode_ = Mode::Payload;
        payload_.reserve(record_.length);
        if (record_.length == 0) record_complete(out);
        return;
    }
    mode_ = Mode::Text;
    text_byte(header_[0], out);
    std::uint8_t rest[wire::kRecordHeaderSize - 1];
    for (std::size_t i = 0; i < sizeof rest; ++i) rest[i] = header_[i + 1];
    for (std::uint8_t b : rest) take(b, out);
}

void GameStream::record_complete(std::vector<StreamItem>& out) {
    StreamItem item;
    item.is_record = true;
    item.record = decode_record(record_, payload_.data());
    out.push_back(std::move(item));
    payload_.clear();
    mode_ = Mode::Text;
}

void GameStream::text_byte(std::uint8_t byte, std::vector<StreamItem>& out) {
    if (byte == '\r') return;
    if (byte == '\n') {
        end_line(out);
        return;
    }
    line_.push_back(static_cast<char>(byte));
    if (line_.size() == kMaxLine) end_line(out);
}

void GameStream::end_line(std::vector<StreamItem>& out) {
    StreamItem item;
    item.line = std::move(line_);
    out.push_back(std::move(item));
    line_.clear();
}

} // namespace nnh::tool::remote
