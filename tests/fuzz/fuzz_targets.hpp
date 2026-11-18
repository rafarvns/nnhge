#pragma once
// Fuzz targets: functions that take arbitrary bytes and must never crash, hang or read out of bounds.
// Built two ways: as libFuzzer executables (tests/fuzz/libfuzzer_*.cpp, clang + ASan, Linux) and as a
// deterministic mini-fuzz inside nnh_tests on every compiler (tests/fuzz/test_fuzz_smoke.cpp).

#include <cstddef>
#include <cstdint>

namespace nnh::fuzz {

int path_target(const std::uint8_t* data, std::size_t size);
int save_target(const std::uint8_t* data, std::size_t size);
int asset_index_target(const std::uint8_t* data, std::size_t size);

} // namespace nnh::fuzz
