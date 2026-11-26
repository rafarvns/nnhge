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
/// `.nnhsprite` sprite libraries (spec 016, fuzz_sprite.cpp): validated as given, then with the header's size and CRC
/// fixed; an accepted file is copied into a library with extra room, every frame is queried, every clip and every
/// state is played for about 30 frames (in steps of the shortest frame, so the per-update bounds are never reached),
/// and a marker, a state and a rule are added. Inputs over 256 KiB are only validated.
int sprite_library_target(const std::uint8_t* data, std::size_t size);
/// For the mini-fuzz: whether `data`, with its size and CRC fixed as data_index_target does, is accepted.
bool data_index_fixed_accepted(const std::uint8_t* data, std::size_t size);
/// A valid index (two types, every group used) written to `out`; its size (0 if `capacity` is short).
std::size_t data_index_seed(std::uint8_t* out, std::size_t capacity);

/// Saved input bindings `NNHB` (spec 017, fuzz_bindings.cpp): validated as given, then with the header's count, CRC
/// and game version fixed (so mutations reach the entries); every accepted blob is applied to a set with every action
/// kind, saved again, and that must load back with nothing skipped and save to the same bytes.
int bindings_target(const std::uint8_t* data, std::size_t size);
/// For the mini-fuzz: whether `data`, with its header fixed as bindings_target does, is accepted.
bool bindings_fixed_accepted(const std::uint8_t* data, std::size_t size);
/// A valid blob for bindings_target's set (whole, part, invert and cleared entries); its size (0 if short).
std::size_t bindings_seed(std::uint8_t* out, std::size_t capacity);

/// Worlds (spec 020, fuzz_world_ops.cpp): the first byte picks tags per entity (0..8); every next 8 bytes are an
/// operation and its argument, run on an nnh::World and on the reference model at once; a difference aborts.
int world_ops_target(const std::uint8_t* data, std::size_t size);

/// Prefabs (spec 021, fuzz_prefab_ops.cpp): every 4 bytes are one operation of the prefabs self-check
/// (examples/prefabs/self_check.hpp), run on a prefab-spawning world and its hand-written twin; a difference aborts.
int prefab_ops_target(const std::uint8_t* data, std::size_t size);

/// Hierarchies (spec 022, fuzz_hierarchy_ops.cpp): every 4 bytes are one operation of the hierarchy self-check
/// (examples/hierarchy/self_check.hpp), run on a prefab-hierarchy world and its hand-written twin; a difference aborts.
int hierarchy_ops_target(const std::uint8_t* data, std::size_t size);

/// Transforms (spec 022, fuzz_transform.cpp): 64 bytes of float bits into every transform function; a broken promise
/// (sin_cos range, is_finite, the inverse's exponent rule, the pure-translation rule) aborts.
int transform_target(const std::uint8_t* data, std::size_t size);

/// `.nnhmesh` meshes (spec 026, fuzz_mesh.cpp): validated as given, then with the header's size and CRC fixed; an
/// accepted file is walked (every submesh's indices and vertices, every texture path) in bounds.
int mesh_reader_target(const std::uint8_t* data, std::size_t size);
/// For the mini-fuzz: whether `data`, with its size and CRC fixed as mesh_reader_target does, is accepted.
bool mesh_fixed_accepted(const std::uint8_t* data, std::size_t size);

/// `.nnhfont` fonts (spec 029, fuzz_font.cpp): validated as given, then with the header's size and CRC fixed; an accepted
/// font is walked (every glyph and outline inside its page, every kerning run inside the pairs, every page decoded).
int font_validate_target(const std::uint8_t* data, std::size_t size);
/// For the mini-fuzz: whether `data`, with its size and CRC fixed, is accepted.
bool font_fixed_accepted(const std::uint8_t* data, std::size_t size);

/// `.nnhstr` tables and the `.nnh_loc` manifest (spec 029, fuzz_loc.cpp): validated as given, then with the size and
/// CRC fixed; an accepted file is walked (every key's strings and placeholders, every language's chain, rule and name).
int loc_table_target(const std::uint8_t* data, std::size_t size);
int loc_manifest_target(const std::uint8_t* data, std::size_t size);
/// For the mini-fuzz: whether `data`, with its size and CRC fixed, is accepted.
bool loc_table_fixed_accepted(const std::uint8_t* data, std::size_t size);
bool loc_manifest_fixed_accepted(const std::uint8_t* data, std::size_t size);

} // namespace nnh::fuzz
