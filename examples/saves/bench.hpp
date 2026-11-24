#pragma once
// The saves example's bench workloads (spec 027 AC-8), on the reference save (~3.8 KB): encode it, validate
// and decode it, decode a v1 file with its two steps (in memory); store it to the card, load it, load it from
// the backup after the main file is damaged, read three slot summaries, and spec 002's raw save::write of the
// same byte count (on the card). bench_scene.cpp times them.

#include "progress_history.hpp"
#include "summary.hpp"

#include <cstddef>
#include <cstdint>
#include <nnh/saves.hpp>

namespace saves_example {

class SavesBench {
  public:
    /// Encodes the payloads the memory workloads read and makes the files the card workloads need. Load time.
    bool start();
    /// Removes the bench's files.
    void finish();

    // Each runs one workload once; false when it failed. The memory ones add to a sink so nothing is
    // optimised away.
    bool encode();
    bool decode();
    bool decode_v1();
    bool store();
    bool load();
    bool load_backup(); ///< needs prepare_backup() first (untimed)
    bool summaries();   ///< needs prepare_summaries() first (untimed)
    bool raw_write();

    bool prepare_backup();
    bool prepare_summaries();

    std::uint32_t sink() const { return sink_; }

  private:
    Progress value_;
    Progress loaded_;
    ProgressV1 v1_;
    nnh::save::Buffer<Progress, 256> buffer_;
    std::uint8_t v3_payload_[nnh::save::max_size<Progress>()] = {};
    std::size_t v3_size_ = 0;
    std::uint8_t v1_payload_[nnh::save::max_size<ProgressV1>()] = {};
    std::size_t v1_size_ = 0;
    SlotSummary summary_;
    std::uint32_t sink_ = 0;
};

} // namespace saves_example
