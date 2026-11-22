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

/// The TOML-subset reader of data files (spec 014): parse, then walk every table, entry and value; a refusal must
/// name a line (>= 1) and a reason.
int toml_target(const std::uint8_t* data, std::size_t size);

/// `nnh-assetc data` from memory (spec 014, fuzz_assetc_data.cpp): the input, up to a NUL byte, is a data file of a
/// fixed schema with every field kind; an accepted one must pass the engine's validator and load into the compiled
/// struct within its rules, and its references must be usable paths. After the NUL, if any, comes a schema
/// description of its own: read with bounds, and when accepted the text is converted against its first keyed type.
int data_target(const std::uint8_t* data, std::size_t size);
/// A data file of data_target's schema setting every field (a seed).
const char* data_seed();
/// For the mini-fuzz: whether data_target's fixed schema accepts `text` as a data file.
bool data_converts(const char* text, std::size_t size);

/// `nnh-assetc sprite` from memory (spec 016, fuzz_assetc_sprite.cpp): the input is an Aseprite export
/// (`/g/assets/art/hero.ase.json`) beside a 64x16 PNG header; an accepted one must pass the engine's validator, a
/// refusal must name a file and a reason.
int sprite_target(const std::uint8_t* data, std::size_t size);
/// An export using every feature the converter reads (a seed).
const char* sprite_seed();
/// For the mini-fuzz: whether `text` converts (aborting on a broken invariant, as sprite_target does).
bool sprite_converts(const char* text, std::size_t size);

} // namespace nnh::assetc::fuzz
