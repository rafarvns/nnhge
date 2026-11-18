#include "audio/wav_reader.hpp"

#include <cstring>

namespace nnh::detail {

namespace {

std::uint32_t u16(const std::uint8_t* p) { return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8; }
std::uint32_t u32(const std::uint8_t* p) { return u16(p) | u16(p + 2) << 16; }

constexpr std::uint32_t kPcm = 1;
constexpr std::uint32_t kExtensible = 0xFFFE;

/// Checks the `fmt ` payload (`size` bytes at `p`) and fills the format fields of `out`.
Result<void> read_fmt(const std::uint8_t* p, std::size_t size, WavFormat& out) {
    if (size < 16) return Error::make(ErrorCode::Corrupted, "WAV fmt chunk too short");
    std::uint32_t format = u16(p);
    if (format == kExtensible) {
        // cbSize(2) valid bits(2) channel mask(4) sub-format GUID(16): the GUID's first 2 bytes are the format.
        if (size < 40) return Error::make(ErrorCode::Corrupted, "WAV extensible fmt chunk too short");
        format = u16(p + 24);
    }
    const std::uint32_t channels = u16(p + 2);
    const std::uint32_t rate = u32(p + 4);
    const std::uint32_t block_align = u16(p + 12);
    const std::uint32_t bits = u16(p + 14);
    if (format != kPcm) return Error::make(ErrorCode::Unsupported, "WAV encoding %u (only PCM)", unsigned(format));
    if (bits != 8 && bits != 16) return Error::make(ErrorCode::Unsupported, "WAV %u-bit (8 or 16)", unsigned(bits));
    if (channels < 1 || channels > 2) {
        return Error::make(ErrorCode::Unsupported, "WAV with %u channels (1 or 2)", unsigned(channels));
    }
    if (rate < 8000 || rate > 48000) {
        return Error::make(ErrorCode::Unsupported, "WAV at %u Hz (8000-48000)", unsigned(rate));
    }
    if (block_align != channels * bits / 8) return Error::make(ErrorCode::Corrupted, "WAV block size mismatch");
    out.channels = static_cast<int>(channels);
    out.bits = static_cast<int>(bits);
    out.rate = rate;
    return {};
}

} // namespace

Result<WavFormat> parse_wav(const void* bytes, std::size_t size, std::size_t file_size) {
    if (size > file_size) return Error::make(ErrorCode::InvalidArgument, "WAV header larger than the file");
    const auto* p = static_cast<const std::uint8_t*>(bytes);
    if (size < 12 || std::memcmp(p, "RIFF", 4) != 0 || std::memcmp(p + 8, "WAVE", 4) != 0) {
        return Error::make(ErrorCode::Corrupted, "not a WAV file");
    }
    WavFormat out;
    bool have_fmt = false;
    std::size_t at = 12;
    // Each chunk: id(4) size(4) payload, padded to an even size. `at` never passes `size`.
    while (size - at >= 8) {
        const std::uint8_t* chunk = p + at;
        const std::size_t payload = u32(chunk + 4);
        const std::size_t start = at + 8;
        if (std::memcmp(chunk, "data", 4) == 0) {
            if (!have_fmt) return Error::make(ErrorCode::Corrupted, "WAV data before fmt");
            if (payload > file_size - start) return Error::make(ErrorCode::Corrupted, "WAV data past the end");
            const std::size_t frame = static_cast<std::size_t>(out.channels * out.bits / 8);
            out.data_offset = start;
            out.data_size = payload - payload % frame;
            if (out.data_size == 0) return Error::make(ErrorCode::Corrupted, "WAV without samples");
            return out;
        }
        if (payload > size - start) return Error::make(ErrorCode::Corrupted, "WAV chunk past the end");
        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            auto fmt = read_fmt(chunk + 8, payload, out);
            if (!fmt) return fmt.error();
            have_fmt = true;
        }
        const std::size_t padded = payload + (payload & 1);
        if (padded > size - start) break; // pad byte missing: nothing can follow
        at = start + padded;
    }
    return Error::make(ErrorCode::Corrupted, "WAV without a data chunk");
}

} // namespace nnh::detail
