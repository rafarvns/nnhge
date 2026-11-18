#include "audio/voice_table.hpp"

namespace nnh::detail {

namespace {

std::uint16_t next_generation(std::uint16_t g) { return g == 0xFFFF ? 1 : static_cast<std::uint16_t>(g + 1); }

} // namespace

void VoiceTable::reset(VoiceState* voices, int count) {
    for (int i = 0; i < count; ++i) {
        // Keep generations moving so a handle from before the reset can't match a new voice.
        const std::uint16_t generation = voices[i].active ? next_generation(voices[i].generation) : voices[i].generation;
        voices[i] = VoiceState{};
        voices[i].generation = generation == 0 ? 1 : generation;
    }
    voices_ = voices;
    count_ = count;
}

int VoiceTable::choose(std::uint8_t priority) const {
    int candidate = -1;
    for (int i = 0; i < count_; ++i) {
        const VoiceState& v = voices_[i];
        if (!v.active) return i;
        if (candidate < 0 || v.priority < voices_[candidate].priority ||
            (v.priority == voices_[candidate].priority && v.order < voices_[candidate].order)) {
            candidate = i;
        }
    }
    if (candidate >= 0 && voices_[candidate].priority > priority) return -1;
    return candidate;
}

Voice VoiceTable::claim(int index, std::uint8_t priority) {
    VoiceState& v = voices_[index];
    const std::uint16_t generation = v.active ? next_generation(v.generation) : v.generation;
    v = VoiceState{};
    v.active = true;
    v.generation = generation;
    v.priority = priority;
    v.order = next_order_++;
    return Voice{static_cast<std::uint16_t>(index), generation};
}

VoiceState* VoiceTable::get(Voice voice) {
    if (voice.generation == 0 || voice.index >= count_) return nullptr;
    VoiceState& v = voices_[voice.index];
    return v.active && v.generation == voice.generation ? &v : nullptr;
}

void VoiceTable::release(int index) {
    VoiceState& v = voices_[index];
    if (!v.active) return;
    v.active = false;
    v.generation = next_generation(v.generation);
}

int VoiceTable::active_count() const {
    int n = 0;
    for (int i = 0; i < count_; ++i) n += voices_[i].active ? 1 : 0;
    return n;
}

} // namespace nnh::detail
