#pragma once
// A WAV file read in pieces from a ByteSource (streamed music, spec 008), as 16-bit frames. Its header
// (up to the data chunk) must fit in the first 4 KB. Unit tested in tests/audio/test_music_track.cpp.

#include <cstdint>

#include "audio/ogg_reader.hpp"
#include "audio/wav_reader.hpp"
#include "nnh/result.hpp"

namespace nnh::detail {

class WavStream {
public:
    Result<void> open(const ByteSource& source);
    int channels() const { return format_.channels; }
    std::uint32_t rate() const { return format_.rate; }
    std::uint64_t length() const { return length_; }
    std::uint64_t position() const { return position_; }

    /// Up to `capacity` frames into `out` (interleaved). @return frames; 0 at the end. Errors: IoError.
    Result<std::uint32_t> read(std::int16_t* out, std::uint32_t capacity);
    /// Errors: InvalidArgument (past the end).
    Result<void> seek(std::uint64_t frame);

private:
    ByteSource source_{};
    WavFormat format_{};
    std::uint64_t length_ = 0;
    std::uint64_t position_ = 0;
    std::uint8_t buffer_[4096];
};

} // namespace nnh::detail
