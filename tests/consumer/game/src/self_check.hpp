#pragma once
// The consumer game's self-check (spec 032 FR-16): loads one asset of each kind the game ships and sums what it read
// into a CRC-32, the same on every system and every way the engine came in. Line: "consumer: PASS <crc32>" or
// "consumer: FAIL <what>".

#include <cstddef>
#include <cstdint>

struct ConsumerCheck {
    bool pass = false;
    std::uint32_t checksum = 0;
    char failure[64] = {};
};

/// Call after nnh::init(). Loads and releases what it loads.
ConsumerCheck run_consumer_check();

void format_consumer_check(const ConsumerCheck& r, char* out, std::size_t size);
