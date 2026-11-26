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

/// `nnh-assetc data-index`'s inputs (spec 014, fuzz_assetc_data.cpp): the input is a references sidecar; read
/// back, refused with a line, or indexed against data_target's schema (with the media it names present, and once
/// with its name used twice): an accepted index must pass the engine's index validator.
int refs_target(const std::uint8_t* data, std::size_t size);
/// A sidecar with every record kind (a seed).
const char* refs_seed();
/// `nnh-assetc sprite` from memory (spec 016, fuzz_assetc_sprite.cpp): the input is an Aseprite export
/// (`/g/assets/art/hero.ase.json`) beside a 64x16 PNG header; an accepted one must pass the engine's validator, a
/// refusal must name a file and a reason.
int sprite_target(const std::uint8_t* data, std::size_t size);
/// An export using every feature the converter reads (a seed).
const char* sprite_seed();
/// For the mini-fuzz: whether `text` converts (aborting on a broken invariant, as sprite_target does).
bool sprite_converts(const char* text, std::size_t size);

/// `nnh-assetc mesh` from memory (spec 026, fuzz_assetc_gltf.cpp): the input is a .gltf document (a .glb when it starts
/// with "glTF") beside a fixed buffer and PNG; accepted models pass the engine's .nnhmesh validator.
int gltf_target(const std::uint8_t* data, std::size_t size);
/// A model using every feature the converter reads (two nodes, a mirror, two materials, texture, colours).
const char* gltf_seed();
/// For the mini-fuzz: whether `text` converts (aborting on a broken invariant, as gltf_target does).
bool gltf_converts(const char* text, std::size_t size);

/// Spec 029 (fuzz_assetc_font.cpp). The BMFont reader: an accepted font's characters are in range, a refusal names a
/// line.
int bmfont_target(const std::uint8_t* data, std::size_t size);
/// The TrueType pre-validator, then — for an accepted file, faces 0 and 1 — stb_truetype rasterising a few code points
/// at two sizes (the pre-validator's promise: nothing it accepts reads out of bounds).
int ttf_target(const std::uint8_t* data, std::size_t size);
/// The BMFont page decoder (stb_image, PNG only): an accepted image has width × height × 4 bytes.
int png_target(const std::uint8_t* data, std::size_t size);
/// Font descriptions: a refusal names a line.
int font_desc_target(const std::uint8_t* data, std::size_t size);
/// A language's string file checked against a fixed base and, when clean, written (Debug and Release forms): the
/// tables must pass the engine's validator.
int strings_target(const std::uint8_t* data, std::size_t size);
const char* strings_seed();
const char* bmfont_seed();
const char* font_desc_seed();

} // namespace nnh::assetc::fuzz
