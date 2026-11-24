#pragma once
// One node's world transform (spec 022 FR-8, FR-9; plan 022 "The pass"), inline for the pass and world_now: a
// non-finite local counts as the identity (an integer test on its bits), a pure-translation local skips the sine, any
// other local reads its sine and cosine from the node's cache (the same bits as computing them). The rules are
// transform/compose.hpp's, so compose(), the pass and world_now agree bit for bit.

#include <cstring>

#include "hierarchy/hierarchy_internal.hpp"

namespace nnh::detail {

/// The local's five words read as integers (plan 022 T10.3: one load multiple on the ARM11, no VFP-to-core transfer
/// per float) and tested at once: an all-ones exponent plus 2²³ carries into the sign bit, any other doesn't. The same
/// answer as is_finite_bits().
NNH_DET_INLINE std::uint32_t word_at(const float& f) {
    std::uint32_t b;
    std::memcpy(&b, &f, sizeof b); // an integer load, inlined (a bit_cast of the value goes through a VFP register)
    return b;
}
NNH_DET_INLINE std::uint32_t exponent_carry(std::uint32_t b) { return (b & kExponentBits) + 0x00800000u; }
NNH_DET_INLINE bool local_finite(const Node& node) {
    const Transform2D& l = node.local;
    const std::uint32_t any = exponent_carry(word_at(l.position.x)) | exponent_carry(word_at(l.position.y)) |
                              exponent_carry(word_at(l.rotation)) | exponent_carry(word_at(l.scale.x)) |
                              exponent_carry(word_at(l.scale.y));
    return (any & kSignBit) == 0;
}

/// The local the pass composes: the node's, or the identity when it isn't finite (`bad` set).
NNH_DET_INLINE const Transform2D& usable_local(const Node& node, bool& bad) {
    static constexpr Transform2D kIdentity{};
    if (local_finite(node)) return node.local;
    bad = true;
    return kIdentity;
}

/// The sine and cosine of `local`'s rotation: the node's cache when its bits match (`update`: refreshed otherwise),
/// else computed (world_now, which stores nothing).
NNH_DET_INLINE SinCos node_sin_cos(Node& node, const Transform2D& local, bool update) {
    const std::uint32_t bits = float_bits(local.rotation);
    if (bits == NA::cached_rotation(node)) return {NA::cached_sin(node), NA::cached_cos(node)};
    const SinCos sc = sin_cos_inline(local.rotation);
    if (update) {
        NA::cached_rotation(node) = bits;
        NA::cached_sin(node) = sc.sin;
        NA::cached_cos(node) = sc.cos;
    }
    return sc;
}

/// `node`'s world from `parent`'s (null: a root) into `out`.
NNH_DET_INLINE void compute_world(Node& node, const WorldTransform2D* parent, bool update, WorldTransform2D& out,
                                  bool& bad) {
    const Transform2D& local = usable_local(node, bad);
    if (parent == nullptr) {
        root_world(local, node_sin_cos(node, local, update), out);
    } else if (pure_translation(local)) {
        translate_world(*parent, local.position, out);
    } else {
        full_world(*parent, local, node_sin_cos(node, local, update), out);
    }
}

} // namespace nnh::detail
