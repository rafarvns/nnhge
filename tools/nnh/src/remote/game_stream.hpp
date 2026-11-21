#pragma once
// The byte stream a Development game sends to nnh (spec 013, D28): log lines with records in between.
// Bytes arrive in arbitrary chunks; the splitter returns whole lines and whole records in stream order.
// Untrusted: anything that isn't a valid record header (core's decode_record_header) is text, and both a
// line and a record are capped (kMaxLine, wire::kMaxRecordPayload), so memory stays bounded.
// Pure; tested in tools/nnh/tests/test_game_stream.cpp (+ mini-fuzz in test_fuzz_tool.cpp).

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "remote/game_records.hpp"

namespace nnh::tool::remote {

/// A line longer than this is cut: its first kMaxLine bytes are returned as one line.
inline constexpr std::size_t kMaxLine = 1u << 20;

struct StreamItem {
    bool is_record = false;
    std::string line; ///< text without '\n' and without any '\r'
    Record record;
};

class GameStream {
public:
    /// Splits `size` more bytes, appending what they complete to `out`. Text before a record that is not
    /// yet ended by '\n' is returned as a line first, so lines and records keep the stream's order.
    void feed(const std::uint8_t* data, std::size_t size, std::vector<StreamItem>& out);

    /// The stream ended: anything pending (a last line without '\n', or the bytes of an unfinished
    /// record, shown as text) is appended to `out`.
    void finish(std::vector<StreamItem>& out);

private:
    void take(std::uint8_t byte, std::vector<StreamItem>& out);
    void text_byte(std::uint8_t byte, std::vector<StreamItem>& out);
    void end_line(std::vector<StreamItem>& out);
    void header_complete(std::vector<StreamItem>& out);
    void record_complete(std::vector<StreamItem>& out);

    enum class Mode { Text, Header, Payload };
    Mode mode_ = Mode::Text;
    std::string line_;
    std::uint8_t header_[wire::kRecordHeaderSize] = {};
    std::size_t header_size_ = 0;
    wire::RecordHeader record_;
    std::vector<std::uint8_t> payload_;
};

} // namespace nnh::tool::remote
