#pragma once
// One music track (spec 008 FR-3): a WAV or Ogg Vorbis source read in pieces, looping back to its loop
// point inside a read, and the map from "frames played since it started" to its position. Unit tested in
// tests/audio/test_music_track.cpp.

#include <cstdint>

#include "audio/ogg_reader.hpp"
#include "audio/wav_stream.hpp"
#include "nnh/result.hpp"

namespace nnh::detail {

class MusicTrack {
public:
    /// Opens `source` (Ogg needs `arena`). `loop_start` 0 = the Ogg LOOPSTART tag if any, else the start.
    /// Errors: those of the readers; InvalidArgument if the loop point isn't inside the track.
    Result<void> open(const ByteSource& source, bool ogg, void* arena, std::size_t arena_size, std::uint64_t loop_start,
                      bool loop);
    void close();

    int channels() const;
    std::uint32_t rate() const;
    std::uint64_t length() const;
    std::uint64_t loop_start() const { return loop_start_; }
    bool loops() const { return loop_; }

    /// Up to `capacity` frames; at the end it continues from the loop point when looping.
    /// @return frames; fewer than `capacity` only at the end of a track that doesn't loop.
    Result<std::uint32_t> read(std::int16_t* out, std::uint32_t capacity);

    /// The track position after `played` frames from its start (loops counted).
    std::uint64_t position_at(std::uint64_t played) const;

private:
    Result<std::uint32_t> read_source(std::int16_t* out, std::uint32_t capacity);
    Result<void> seek_source(std::uint64_t frame);

    bool ogg_ = false;
    bool loop_ = false;
    std::uint64_t loop_start_ = 0;
    OggDecoder ogg_reader_;
    WavStream wav_reader_;
};

} // namespace nnh::detail
