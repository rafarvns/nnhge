#include "audio/music_track.hpp"

namespace nnh::detail {

Result<void> MusicTrack::open(const ByteSource& source, bool ogg, void* arena, std::size_t arena_size,
                              std::uint64_t loop_start, bool loop) {
    close();
    ogg_ = ogg;
    loop_ = loop;
    auto opened = ogg ? ogg_reader_.open(source, arena, arena_size) : wav_reader_.open(source);
    if (!opened) return opened;
    if (loop_start == 0 && ogg && ogg_reader_.has_loop_start()) loop_start = ogg_reader_.loop_start();
    if (loop_start >= length()) {
        close();
        return Error::make(ErrorCode::InvalidArgument, "loop point %llu is past the end",
                           static_cast<unsigned long long>(loop_start));
    }
    loop_start_ = loop_start;
    return {};
}

void MusicTrack::close() {
    ogg_reader_.close();
    wav_reader_ = WavStream{};
    loop_start_ = 0;
}

int MusicTrack::channels() const { return ogg_ ? ogg_reader_.channels() : wav_reader_.channels(); }
std::uint32_t MusicTrack::rate() const { return ogg_ ? ogg_reader_.rate() : wav_reader_.rate(); }
std::uint64_t MusicTrack::length() const { return ogg_ ? ogg_reader_.length() : wav_reader_.length(); }

Result<std::uint32_t> MusicTrack::read_source(std::int16_t* out, std::uint32_t capacity) {
    return ogg_ ? ogg_reader_.read(out, capacity) : wav_reader_.read(out, capacity);
}

Result<void> MusicTrack::seek_source(std::uint64_t frame) {
    return ogg_ ? ogg_reader_.seek(frame) : wav_reader_.seek(frame);
}

Result<std::uint32_t> MusicTrack::read(std::int16_t* out, std::uint32_t capacity) {
    const auto ch = static_cast<std::size_t>(channels());
    std::uint32_t total = 0;
    bool just_looped = false;
    while (total < capacity) {
        auto got = read_source(out + total * ch, capacity - total);
        if (!got) return got.error();
        total += got.value();
        if (got.value() > 0) {
            just_looped = false;
            continue;
        }
        if (!loop_ || just_looped) break; // the end (or a loop that yields nothing: give up)
        auto sought = seek_source(loop_start_);
        if (!sought) return sought.error();
        just_looped = true;
    }
    return total;
}

std::uint64_t MusicTrack::position_at(std::uint64_t played) const {
    const std::uint64_t len = length();
    if (played < len) return played;
    if (!loop_) return len;
    return loop_start_ + (played - len) % (len - loop_start_);
}

} // namespace nnh::detail
