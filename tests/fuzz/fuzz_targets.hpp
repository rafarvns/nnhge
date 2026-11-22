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
int wav_target(const std::uint8_t* data, std::size_t size);
int ogg_target(const std::uint8_t* data, std::size_t size);
/// Console remote (spec 013): every 16-byte window that decodes encodes back to the same bytes; then the
/// input runs as a stream over simulated frames (reader → schedule → remote input). Each chunk starts with a
/// control byte: bits 0-5 = bytes that arrive (capped by the queue's room), bits 6-7 = frames that pass after.
int remote_target(const std::uint8_t* data, std::size_t size);
/// `.nnhmap` maps (spec 012, fuzz_tilemap.cpp): validated as given, then again with the header's size and CRC
/// fixed (so mutations reach the content checks); an accepted file is copied into a map with extra room and
/// every query, the draw-time tile expansion and a few edits run on it. Inputs over 256 KiB are only validated.
int tilemap_target(const std::uint8_t* data, std::size_t size);

} // namespace nnh::fuzz
