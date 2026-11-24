#pragma once
// What the check mode must find (spec 027 AC-6), recorded from pc-debug (WinLibs GCC) at stage 7. The same on
// every host compiler and on the 3DS: a difference is a codec, conversion or migration that isn't portable.
// The sums are tests/data/saves/README.md's; regenerate them only with the golden files.

#include <cstdint>

namespace saves_example {

struct SavesGolden {
    std::uint32_t sum[3];       ///< progress_sum of golden v1, v2, v3 loaded by the v3 code
    int migrated[3];            ///< steps run for each
    std::uint32_t encode_crc;   ///< CRC-32 of the reference encoded (= v3.sav's payload)
    std::uint32_t encode_bytes; ///< its size
    std::uint32_t slot_sum;     ///< reference B stored over reference A, reloaded
    std::uint32_t backup_sum;   ///< the slot damaged: its backup, reference A
    const char* summaries;      ///< the three slots' summaries
};

inline constexpr SavesGolden kSavesGolden = {{0x0F3BA8C8u, 0x1EFDB382u, 0xD15774A7u},
                                             {2, 1, 0},
                                             0x11363B5Cu,
                                             3768,
                                             0xF6C8D12Fu,
                                             0xD15774A7u,
                                             "L300 empty empty"};

} // namespace saves_example
