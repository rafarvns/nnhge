#pragma once
// Decoding a whole sound file into 16-bit frames (spec 008): WAV samples converted, Ogg Vorbis decoded.
// Used by load_sound (sounds.cpp).

#include <cstdint>

#include "audio/ogg_reader.hpp"
#include "audio/wav_reader.hpp"
#include "nnh/result.hpp"

namespace nnh::detail {

/// Converts the samples of a parsed WAV (`file` is the whole file) to interleaved 16-bit values:
/// 8-bit samples are unsigned (0x80 = silence) and become value << 8 around zero.
void wav_to_pcm16(const std::uint8_t* file, const WavFormat& format, std::int16_t* out);

/// Decodes the whole of an open decoder into `out` (`frames × channels` values); with `out` nullptr it
/// decodes into a scratch buffer only to check the file. Errors: Corrupted if fewer frames come out.
Result<void> ogg_decode_all(OggDecoder& decoder, std::int16_t* out, std::uint32_t frames);

} // namespace nnh::detail
