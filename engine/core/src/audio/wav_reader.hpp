#pragma once
// WAV reader (spec 008): finds the format and the sample bytes of a RIFF/WAVE file. Accepts PCM
// (plain or WAVE_FORMAT_EXTENSIBLE), 8 or 16 bits, mono or stereo, 8–48 kHz. Pure and bounded: every
// size and offset is checked; nothing is copied. Unit tested in tests/audio/test_wav_reader.cpp, fuzzed.

#include <cstddef>
#include <cstdint>

#include "nnh/result.hpp"

namespace nnh::detail {

struct WavFormat {
    int channels = 0;           ///< 1 or 2
    int bits = 0;               ///< 8 (unsigned samples) or 16 (signed)
    std::uint32_t rate = 0;     ///< samples per second
    std::size_t data_offset = 0; ///< where the samples start, from the start of the file
    std::size_t data_size = 0;   ///< bytes of whole frames

    std::uint32_t frames() const {
        return static_cast<std::uint32_t>(data_size / static_cast<std::size_t>(channels * bits / 8));
    }
};

/// Parses the start of a WAV file. `bytes` holds the first `size` bytes of a file of `file_size` bytes
/// (the whole file for effects; just its start when streaming): the `data` chunk header must lie in
/// `bytes`, its samples may lie beyond them but not beyond `file_size`.
/// Errors: Corrupted (not a WAV, broken structure, no samples), Unsupported (another encoding, bit depth,
/// channel count or rate), InvalidArgument (`size` > `file_size`).
Result<WavFormat> parse_wav(const void* bytes, std::size_t size, std::size_t file_size);

} // namespace nnh::detail
