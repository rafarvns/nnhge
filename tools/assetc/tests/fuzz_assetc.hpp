#pragma once
// Fuzz targets of nnh-assetc (spec 012 T5.8, constitution X): arbitrary bytes must never crash, hang or read out of
// bounds. Built two ways, like tests/fuzz: libFuzzer programs (libfuzzer_assetc_*.cpp, clang + ASan, Linux) and a
// deterministic mini-fuzz in nnh_assetc_tests (test_fuzz_assetc.cpp).

#include <cstddef>
#include <cstdint>

namespace nnh::assetc::fuzz {

/// The JSON reader: parse, then walk every value (types, strings, keys, lookups).
int json_target(const std::uint8_t* data, std::size_t size);

/// A whole conversion from in-memory files: the input is a map (`levels/map.tmj`), and after the first NUL byte, if
/// any, a tileset (`tiles/set.tsj`); the image `tiles/set.png` is a valid 64x64 PNG header. An accepted map must be
/// accepted by the run-time validator too.
int map_target(const std::uint8_t* data, std::size_t size);

} // namespace nnh::assetc::fuzz
