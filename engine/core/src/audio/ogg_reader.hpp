#pragma once
// Ogg Vorbis reader (spec 008): decodes a file read piece by piece from a ByteSource into interleaved
// 16-bit frames, with sample-exact seeking for loop points. Wraps the vendored stb_vorbis (push-data
// API), whose memory comes from an arena the caller owns: nothing is allocated. Unit tested in
// tests/audio/test_ogg_reader.cpp, fuzzed (tests/fuzz).

#include <cstddef>
#include <cstdint>

#include "nnh/result.hpp"

struct stb_vorbis;

namespace nnh::detail {

/// Random-access bytes: an asset in the game, a memory buffer in tests.
struct ByteSource {
    void* context = nullptr;
    std::uint64_t size = 0;
    /// Reads up to `n` bytes at `offset` (< size). @return bytes read; 0 = failure.
    std::size_t (*read_at)(void* context, std::uint64_t offset, void* out, std::size_t n) = nullptr;
};

class OggDecoder {
public:
    /// Compressed bytes buffered for the decoder: the largest Vorbis packet or Ogg page it can handle.
    static constexpr std::size_t kInputBytes = 16 * 1024;
    /// Arena that fits stb_vorbis's setup and temporary memory for common files (measured in tests).
    static constexpr std::size_t kDefaultArenaBytes = 256 * 1024;

    OggDecoder() = default;
    ~OggDecoder() { close(); }
    OggDecoder(const OggDecoder&) = delete;
    OggDecoder& operator=(const OggDecoder&) = delete;

    /// Opens the file. `arena` (`arena_size` bytes, 16-byte aligned) holds all the decoder's memory and
    /// must outlive it. Errors: Corrupted (not Ogg Vorbis, headers cut off), Unsupported (more than 2
    /// channels, rate outside 8000–48000 Hz), OutOfMemory (arena too small), IoError.
    Result<void> open(const ByteSource& source, void* arena, std::size_t arena_size);
    void close();
    bool is_open() const { return vorbis_ != nullptr; }

    int channels() const { return channels_; }
    std::uint32_t rate() const { return rate_; }
    /// Frames in the file (from its last complete page).
    std::uint64_t length() const { return length_; }
    /// The LOOPSTART comment tag, in frames, when the file has a valid one.
    bool has_loop_start() const { return has_loop_start_; }
    std::uint64_t loop_start() const { return loop_start_; }
    /// Index of the frame the next read() returns.
    std::uint64_t position() const { return position_; }
    /// Arena bytes stb_vorbis needs for this file.
    std::size_t arena_used() const { return arena_used_; }

    /// Decodes up to `capacity` frames into `out` (interleaved, `capacity * channels()` values).
    /// @return frames written; 0 at the end of the file. Errors: IoError, Corrupted, InvalidArgument.
    Result<std::uint32_t> read(std::int16_t* out, std::uint32_t capacity);

    /// The next read() starts at `frame` (<= length()). Errors: InvalidArgument, IoError, Corrupted.
    Result<void> seek(std::uint64_t frame);

private:
    Result<void> start_decoder();
    Result<bool> fill_input();
    Result<bool> decode_packet();
    Result<void> skip_frames(std::uint64_t count);
    Result<std::uint64_t> find_page_before(std::uint64_t frame, std::uint64_t& granule);
    void read_tags();

    ByteSource source_{};
    unsigned char* arena_ = nullptr;
    std::size_t arena_size_ = 0;
    stb_vorbis* vorbis_ = nullptr;
    int channels_ = 0;
    std::uint32_t rate_ = 0;
    std::uint64_t length_ = 0;
    std::uint64_t loop_start_ = 0;
    bool has_loop_start_ = false;
    std::size_t arena_used_ = 0;

    std::uint64_t position_ = 0;  ///< next frame read() returns
    std::uint64_t file_at_ = 0;   ///< file offset of input_[input_size_]
    std::uint64_t audio_start_ = 0; ///< file offset of the first audio page
    unsigned char input_[kInputBytes];
    std::size_t input_used_ = 0;  ///< bytes at the front of input_ already consumed
    std::size_t input_size_ = 0;
    bool source_done_ = false;

    float** pending_ = nullptr;   ///< decoded frames not returned yet (stb's buffers)
    int pending_count_ = 0;
    int pending_at_ = 0;

    std::uint64_t cached_frame_ = 0; ///< last page search (a loop point is sought again and again)
    std::uint64_t cached_offset_ = 0;
    std::uint64_t cached_granule_ = 0;
    bool has_cache_ = false;
};

} // namespace nnh::detail
