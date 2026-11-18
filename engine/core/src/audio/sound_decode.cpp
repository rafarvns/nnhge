#include "audio/sound_decode.hpp"

namespace nnh::detail {

void wav_to_pcm16(const std::uint8_t* file, const WavFormat& format, std::int16_t* out) {
    const std::uint8_t* data = file + format.data_offset;
    const std::size_t values = format.data_size / static_cast<std::size_t>(format.bits / 8);
    if (format.bits == 8) {
        for (std::size_t i = 0; i < values; ++i) out[i] = static_cast<std::int16_t>((data[i] - 128) * 256);
        return;
    }
    for (std::size_t i = 0; i < values; ++i) {
        const std::uint16_t raw = static_cast<std::uint16_t>(data[2 * i] | data[2 * i + 1] << 8);
        out[i] = static_cast<std::int16_t>(raw);
    }
}

Result<void> ogg_decode_all(OggDecoder& decoder, std::int16_t* out, std::uint32_t frames) {
    static std::int16_t scratch[2 * 1024];
    const auto channels = static_cast<std::uint32_t>(decoder.channels());
    std::uint32_t done = 0;
    while (done < frames) {
        std::uint32_t want = frames - done;
        std::int16_t* into = out != nullptr ? out + static_cast<std::size_t>(done) * channels : scratch;
        if (out == nullptr && want > 1024) want = 1024;
        auto got = decoder.read(into, want);
        if (!got) return got.error();
        if (got.value() == 0) {
            return Error::make(ErrorCode::Corrupted, "Ogg data ends at frame %u of %u", unsigned(done), unsigned(frames));
        }
        done += got.value();
    }
    return {};
}

} // namespace nnh::detail
