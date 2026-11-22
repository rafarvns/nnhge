#pragma once
// The replay's bookkeeping (spec 012 T7.3, FR-10), with no engine calls so it is unit-tested
// (tests/platformer/test_replay_run.cpp). A replay is a list of segments, each a level played with a scripted
// input for a fixed number of frames. Each played frame is recorded: its sample goes into a checksum and its
// frame time into the segment's 60 Hz meter. The same build plays the same frames on every system, so the
// checksums logged every 60 frames must be equal on the PC and on the console.

#include "frame_meter.hpp"
#include "game_events.hpp"
#include "player_input.hpp"

#include <cstdint>
#include <nnh/collision.hpp>
#include <nnh/math.hpp>

/// One step of a scripted input: `input` held for `frames` frames; a jump only on the step's first frame.
struct ReplayStep {
    int frames = 1;
    PlayerInput input{};
};

/// A level played with `steps` (idle once they run out) for `frames` frames.
struct ReplaySegment {
    Demo level = Demo::Level1;
    const ReplayStep* steps = nullptr;
    int step_count = 0;
    int frames = 0;
};

/// The input of `frame` (0-based) of a scripted input; idle past its end.
PlayerInput replay_input(const ReplayStep* steps, int count, int frame);

/// What a played frame leaves behind, as the checksum sees it.
struct ReplaySample {
    nnh::Rect box{};    ///< the player's box (x, y are checked)
    nnh::Vec2 camera{}; ///< the top screen's camera
    nnh::collision::Sides touching{};
    int coins = 0; ///< coins collected in the level so far
};

inline constexpr std::uint32_t kChecksumStart = 2166136261u; ///< FNV-1a's offset basis

/// FNV-1a over the sample's words (box x, box y, camera x, camera y as float bits; touching sides as bits
/// left 1, right 2, ceiling 4, floor 8; coins), each as 4 little-endian bytes: the same on every system.
std::uint32_t checksum_add(std::uint32_t sum, const ReplaySample& sample);

/// What recording a frame finished.
struct ReplayProgress {
    bool checkpoint = false;  ///< total frames reached a multiple of kCheckpointFrames: log the checksum
    bool segment_end = false; ///< the segment's last frame: the next one starts (or the replay ends)
    bool finished = false;    ///< the last segment ended
};

class ReplayRun {
  public:
    static constexpr int kCheckpointFrames = 60;
    static constexpr int kMaxSegments = 4;

    ReplayRun() = default;
    /// Starts at the first frame of the first segment (at most kMaxSegments are used).
    ReplayRun(const ReplaySegment* segments, int count);

    bool done() const { return segment_ >= count_; }
    /// The level the current segment plays (the last one's once done).
    Demo level() const;
    int segment() const { return segment_; }
    /// Frame of the current segment, 0-based: 0 = its first frame (the level restarts on it).
    int frame() const { return frame_; }
    /// Frames recorded so far over all segments, and in all.
    int total_frame() const { return total_; }
    int total_frames() const;
    /// The scripted input of the current frame.
    PlayerInput input() const;

    /// The current frame was played: adds `sample` to the checksum and `ms` (the frame's time) to the
    /// segment's meter (except on the segment's first frame, whose time includes the switch to it), then
    /// moves to the next frame.
    ReplayProgress record(const ReplaySample& sample, double ms);

    std::uint32_t checksum() const { return checksum_; }
    /// The 60 Hz meter of segment `index`.
    const FrameMeter& meter(int index) const { return meters_[index]; }
    int segment_count() const { return count_; }
    const ReplaySegment& segment_info(int index) const { return segments_[index]; }

  private:
    ReplaySegment segments_[kMaxSegments] = {};
    FrameMeter meters_[kMaxSegments] = {};
    int count_ = 0;
    int segment_ = 0;
    int frame_ = 0;
    int total_ = 0;
    std::uint32_t checksum_ = kChecksumStart;
};
