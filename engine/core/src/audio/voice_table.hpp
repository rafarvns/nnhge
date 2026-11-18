#pragma once
// The voices (spec 008): which sound each platform voice plays and with which settings. A Voice handle
// is {index, generation}; releasing a voice bumps its generation, so old handles go stale. When every
// voice is busy, a new sound takes the lowest-priority voice (the oldest among equals) unless that
// priority is higher than its own. Pure logic; unit tested in tests/audio/test_voice_table.cpp.

#include <cstdint>

#include "nnh/audio.hpp"

namespace nnh::detail {

struct VoiceState {
    bool active = false;
    std::uint16_t generation = 1;
    std::uint8_t priority = 0;
    std::uint32_t order = 0;      ///< when it started (larger = newer)
    int sound_slot = -1;          ///< the sound it plays
    std::uint32_t sound_generation = 0;
    AudioGroup group = AudioGroup::Effects;
    float volume = 1.0f;
    float pan = 0.0f;
    float pitch = 1.0f;
    bool paused = false;
    bool loop = false;
    double silent_left = 0;       ///< without audio output: seconds of play left (ends the voice)
};

class VoiceTable {
public:
    /// Uses `voices` for storage and forgets every voice (generations continue, so handles go stale).
    void reset(VoiceState* voices, int count);

    /// The voice a new sound of `priority` should use: a free one, else the one to replace; -1 = drop.
    int choose(std::uint8_t priority) const;

    /// Starts a new voice at `index` (free or replaced): fresh state, a new generation.
    Voice claim(int index, std::uint8_t priority);

    /// The state behind a handle, or nullptr if it is stale.
    VoiceState* get(Voice voice);
    VoiceState& at(int index) { return voices_[index]; }

    /// Frees a voice; its handles go stale.
    void release(int index);

    int active_count() const;
    int count() const { return count_; }

private:
    VoiceState* voices_ = nullptr;
    int count_ = 0;
    std::uint32_t next_order_ = 1;
};

} // namespace nnh::detail
