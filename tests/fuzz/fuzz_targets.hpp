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
/// Converted data assets `NNHD` (spec 014, fuzz_data.cpp), against a fixed schema with every field kind: validated
/// as given, then with the header's size, CRC and fingerprint fixed (so mutations reach the body); every fixed input
/// is applied into a guarded struct (apply must stay in bounds even on files the validator refuses), and an accepted
/// one is applied and its values checked against the schema's rules.
int data_target(const std::uint8_t* data, std::size_t size);
/// For the mini-fuzz: whether `data`, with its header fixed as data_target does, is accepted.
bool data_fixed_accepted(const std::uint8_t* data, std::size_t size);
/// A valid file of data_target's schema with every field set, written to `out`; its size (0 if `capacity` is short).
std::size_t data_seed(std::uint8_t* out, std::size_t capacity);
/// The data index `.nnh_data` (spec 014, fuzz_data_index.cpp): validated as given, then with the header's size and CRC
/// fixed; on an accepted index every lookup runs and must agree with the others (types by key and entry, entries
/// by group and hash, paths and their hashes).
int data_index_target(const std::uint8_t* data, std::size_t size);
/// For the mini-fuzz: whether `data`, with its size and CRC fixed as data_index_target does, is accepted.
bool data_index_fixed_accepted(const std::uint8_t* data, std::size_t size);
/// A valid index (two types, every group used) written to `out`; its size (0 if `capacity` is short).
std::size_t data_index_seed(std::uint8_t* out, std::size_t capacity);

} // namespace nnh::fuzz
