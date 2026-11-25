// The 3DS 3D pass's record of the GPU state it last sent (engine/platforms/3ds/src/pass_cache3d.hpp, spec 026
// optimisation round 026a): each piece of per-draw state is sent the first time in a pass and again only when it
// changes, so a run of draws of one mesh and material sends only its matrices and the draw.

#include "pass_cache3d.hpp"
#include "test.hpp"

using nnh::platform::ctr::PassCache3D;

namespace {

constexpr std::uintptr_t kCube = 0x1000, kShip = 0x2000;
constexpr std::uintptr_t kChecker = 0x3000, kFence = 0x4000;

} // namespace

NNH_TEST(pass_cache_binds_the_first_vertex_buffer_then_skips_the_same_one) {
    PassCache3D c;
    CHECK(c.vertex_buffer(kCube, 1));
    CHECK(!c.vertex_buffer(kCube, 1));
    CHECK(!c.vertex_buffer(kCube, 1));
}

NNH_TEST(pass_cache_rebinds_the_vertex_buffer_when_the_mesh_changes) {
    PassCache3D c;
    CHECK(c.vertex_buffer(kCube, 1));
    CHECK(c.vertex_buffer(kShip, 1));
    CHECK(c.vertex_buffer(kCube, 1));
}

NNH_TEST(pass_cache_rebinds_the_vertex_buffer_when_the_layout_changes) {
    PassCache3D c;
    CHECK(c.vertex_buffer(kCube, 3));
    CHECK(c.vertex_buffer(kCube, 1)); // the same mesh drawn without its vertex colours reads fewer attributes
    CHECK(!c.vertex_buffer(kCube, 1));
}

NNH_TEST(pass_cache_binds_a_mesh_at_address_zero_with_layout_zero_the_first_time) {
    PassCache3D c;
    CHECK(c.vertex_buffer(0, 0));
    CHECK(!c.vertex_buffer(0, 0));
}

NNH_TEST(pass_cache_sets_each_fixed_attribute_once_per_pass) {
    PassCache3D c;
    CHECK(c.fixed_attribute(3));
    CHECK(!c.fixed_attribute(3));
    CHECK(c.fixed_attribute(2)); // independent of attribute 3
    CHECK(!c.fixed_attribute(2));
    CHECK(!c.fixed_attribute(3));
}

NNH_TEST(pass_cache_uploads_the_first_constants_then_skips_identical_ones) {
    PassCache3D c;
    const float v[8] = {1, 1, 0, 0, 0.9f, 0.5f, 0.1f, 1};
    CHECK(c.constants(v));
    CHECK(!c.constants(v));
    const float same[8] = {1, 1, 0, 0, 0.9f, 0.5f, 0.1f, 1};
    CHECK(!c.constants(same));
}

NNH_TEST(pass_cache_uploads_the_constants_when_any_value_changes) {
    PassCache3D c;
    const float base[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(c.constants(base));
    for (int i = 0; i < 8; ++i) {
        float v[8];
        for (int k = 0; k < 8; ++k) v[k] = base[k];
        v[i] += 0.5f;
        CHECK(c.constants(v));    // changed at position i
        CHECK(c.constants(base)); // and back
    }
}

NNH_TEST(pass_cache_compares_constants_by_bits) {
    PassCache3D c;
    const float plus[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const float minus[8] = {0, 0, 0, 0, 0, 0, 0, -0.0f}; // equal as numbers, not as bits: the GPU gets what was sent
    CHECK(c.constants(plus));
    CHECK(c.constants(minus));
}

NNH_TEST(pass_cache_binds_the_first_texture_then_skips_the_same_texture_and_parameters) {
    PassCache3D c;
    CHECK(c.texture(kChecker, 0x000100));
    CHECK(!c.texture(kChecker, 0x000100));
}

NNH_TEST(pass_cache_rebinds_when_the_texture_or_its_parameters_change) {
    PassCache3D c;
    CHECK(c.texture(kChecker, 0));
    CHECK(c.texture(kFence, 0));
    CHECK(c.texture(kFence, 1)); // linear filtering now
    CHECK(c.texture(kFence, 0x10000)); // another wrap mode
    CHECK(!c.texture(kFence, 0x10000));
}

NNH_TEST(pass_cache_binds_texture_zero_with_parameters_zero_the_first_time) {
    PassCache3D c;
    CHECK(c.texture(0, 0));
}

NNH_TEST(pass_cache_reset_forgets_every_piece_of_state) {
    PassCache3D c;
    const float v[8] = {1, 1, 0, 0, 1, 1, 1, 1};
    (void)c.vertex_buffer(kCube, 1);
    (void)c.fixed_attribute(3);
    (void)c.constants(v);
    (void)c.texture(kChecker, 0);
    c.reset();
    CHECK(c.vertex_buffer(kCube, 1));
    CHECK(c.fixed_attribute(3));
    CHECK(c.constants(v));
    CHECK(c.texture(kChecker, 0));
}
