#include "replay_run.hpp"

#include <cstring>

namespace {

std::uint32_t add_word(std::uint32_t sum, std::uint32_t word) {
    for (int i = 0; i < 4; ++i) {
        sum ^= (word >> (8 * i)) & 0xFFu;
        sum *= 16777619u; // FNV prime
    }
    return sum;
}

std::uint32_t bits_of(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

} // namespace

PlayerInput replay_input(const ReplayStep* steps, int count, int frame) {
    for (int i = 0; i < count; ++i) {
        if (frame < steps[i].frames) {
            PlayerInput input = steps[i].input;
            input.jump = input.jump && frame == 0; // a press, not a hold
            return input;
        }
        frame -= steps[i].frames;
    }
    return {};
}

std::uint32_t checksum_add(std::uint32_t sum, const ReplaySample& sample) {
    const nnh::collision::Sides& t = sample.touching;
    const std::uint32_t sides =
        (t.left ? 1u : 0u) | (t.right ? 2u : 0u) | (t.ceiling ? 4u : 0u) | (t.floor ? 8u : 0u);
    sum = add_word(sum, bits_of(sample.box.x));
    sum = add_word(sum, bits_of(sample.box.y));
    sum = add_word(sum, bits_of(sample.camera.x));
    sum = add_word(sum, bits_of(sample.camera.y));
    sum = add_word(sum, sides);
    return add_word(sum, static_cast<std::uint32_t>(sample.coins));
}

ReplayRun::ReplayRun(const ReplaySegment* segments, int count) {
    count_ = count < kMaxSegments ? count : kMaxSegments;
    for (int i = 0; i < count_; ++i)
        segments_[i] = segments[i];
}

Demo ReplayRun::level() const {
    if (count_ == 0) return Demo::Level1;
    return segments_[done() ? count_ - 1 : segment_].level;
}

int ReplayRun::total_frames() const {
    int total = 0;
    for (int i = 0; i < count_; ++i)
        total += segments_[i].frames;
    return total;
}

PlayerInput ReplayRun::input() const {
    if (done()) return {};
    const ReplaySegment& s = segments_[segment_];
    return replay_input(s.steps, s.step_count, frame_);
}

ReplayProgress ReplayRun::record(const ReplaySample& sample, double ms) {
    ReplayProgress progress;
    if (done()) return progress;
    checksum_ = checksum_add(checksum_, sample);
    if (frame_ > 0) meters_[segment_].add(ms);
    ++frame_;
    ++total_;
    progress.checkpoint = total_ % kCheckpointFrames == 0;
    if (frame_ >= segments_[segment_].frames) {
        progress.segment_end = true;
        ++segment_;
        frame_ = 0;
        progress.finished = done();
    }
    return progress;
}
