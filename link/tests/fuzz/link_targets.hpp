#pragma once
// Fuzz targets for everything NNH Link reads from the network: message decoders, the frame reader with
// the sealed channel, and discovery answers. They must never crash, hang or read out of bounds, and a
// decoded message must encode back to something that decodes again (std::abort otherwise). Run as a
// mini-fuzz in nnh_link_tests (test_fuzz_link.cpp) and as libFuzzer executables with NNH_BUILD_FUZZERS.

#include <cstddef>
#include <cstdint>

namespace nnh::link::fuzz {

int messages_target(const std::uint8_t* data, std::size_t size);
int frames_target(const std::uint8_t* data, std::size_t size);
int discovery_target(const std::uint8_t* data, std::size_t size);

} // namespace nnh::link::fuzz
