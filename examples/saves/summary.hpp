#pragma once
// A "Load game" summary of a slot (spec 027 FR-16): NNH_SAVE_SUMMARY reads only the fields a slot row shows
// from a Progress file, with every check of a full load (the whole file validated, the backup when it is
// damaged), and runs no migration step: a v1 slot shows its v1 values (`report.version` says which version
// wrote it).

#include "progress.hpp"

namespace saves_example {

struct SlotSummary {
    nnh::data::String<24> name;
    std::uint16_t level = 0;
    std::uint64_t play_frames = 0;
    std::uint32_t gems = 0;
};
NNH_SAVE_SUMMARY(SlotSummary, "progress", 3, NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(level, 2),
                 NNH_SAVE_FIELD(play_frames, 5), NNH_SAVE_FIELD(gems, 12));

} // namespace saves_example
