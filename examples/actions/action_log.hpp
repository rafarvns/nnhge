#pragma once
// The actions example's fixed-format log lines (spec 017 AC-8): what the console script expects. Pure.

#include <cstddef>
#include <cstdint>
#include <nnh/actions.hpp>

/// "actions: jump pressed" / "actions: jump released".
void format_edge(char* out, std::size_t size, const char* action, bool pressed);

/// A listen or set_source result: "actions: rebind jump -> B", "... -> B (swapped with attack)",
/// "... -> B (replaced attack)", "actions: rebind jump rejected (B is used by pause)", "actions: rebind jump
/// cancelled", "actions: rebind jump timed out". `name_of(id)` gives action names.
void format_remap(char* out, std::size_t size, const nnh::RemapResult& r, const nnh::ActionSet& set);

/// "actions: aim uses C-Stick" or "actions: aim uses L/R (no C-Stick)".
const char* aim_line(bool has_c_stick);

/// FNV-1a 32 of saved bindings bytes (the checksum the self-check compares after a reload).
std::uint32_t bindings_sum(const void* data, std::size_t size);
