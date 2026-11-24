#pragma once
// Fuzz target of structured saves (spec 027 AC-4), in its own header (fuzz_targets.hpp is left as it is). Built as a
// libFuzzer executable (tests/fuzz/libfuzzer_saves.cpp, clang + ASan, Linux) and run as a deterministic mini-fuzz in
// nnh_tests (tests/fuzz/test_fuzz_saves.cpp).

#include <cstddef>
#include <cstdint>

namespace nnh::fuzz {

/// Structured saves (fuzz_saves.cpp), decoded into a reference type with every kind and shape: the input as a payload
/// as given; its bytes as the fields behind a valid structured header (so mutations reach the fields); and as a whole
/// file through nnh::save::load on the fake platform with the NNHS size and CRC fixed. Every accepted input is
/// re-encoded, decoded again and encoded again: the two encodings must be equal. On accepted inputs the migration
/// view answers paths taken from the input, and (Debug/Development) the dump runs. Must never crash or read out of
/// bounds; aborts on a difference.
int saves_target(const std::uint8_t* data, std::size_t size);

/// For the mini-fuzz: whether `data` decodes as a payload (as given) into the reference type.
bool saves_accepted(const std::uint8_t* data, std::size_t size);

/// A valid payload of the reference type with every field set (`full`) or mostly defaults; its size (0 if short).
std::size_t saves_seed(bool full, std::uint8_t* out, std::size_t capacity);

} // namespace nnh::fuzz
