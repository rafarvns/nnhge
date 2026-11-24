// The CPU rasteriser (spec 026 FR-22/FR-23, AC-5, T5.1–T5.5): the fill rule (a shared edge: no gap, no pixel twice),
// back faces, clipping through the near plane, depth order of intersecting triangles, nearest/bilinear sampling and
// the wrap modes, perspective-correct texture coordinates, lit pixels against FR-10's formula, cut-out and blending,
// the lighting tables.

#include <cmath>
#include <cstring>
#include <vector>

#include "raster3d.hpp"
#include "test.hpp"

namespace r3 = nnh::raster3d;

namespace {

/// A screen vertex at pixel (x, y) with depth z and no attributes (1/w = 1).
r3::ScreenVertex sv(float x, float y, float z = 0) {
    r3::ScreenVertex v;
    v.x = static_cast<std::int32_t>(std::floor(x * 16 + 0.5f));
    v.y = static_cast<std::int32_t>(std::floor(y * 16 + 0.5f));
    v.z = z;
    v.inv_w = 1;
    for (float& a : v.attr) a = 0;
    return v;
}

struct Counter {
    std::vector<int> hits = std::vector<int>(64 * 64, 0);
    static void fn(void* c, int x, int y, float, const float*) { ++static_cast<Counter*>(c)->hits[y * 64 + x]; }
};

/// Pass with an identity projection (clip = view), so vertices in view space are NDC.
nnh::platform::Pass3D identity_pass() {
    nnh::platform::Pass3D p;
    for (int i = 0; i < 16; ++i) p.projection[i] = i % 5 == 0 ? 1.0f : 0.0f;
    p.light_count = 0;
    p.ambient[0] = p.ambient[1] = p.ambient[2] = 1;
    return p;
}

} // namespace

NNH_TEST(raster_shared_edges_leave_no_gap_and_no_double_pixel) {
    Counter c;
    const r3::PixelSink sink{&Counter::fn, &c};
    // A quad split along a diagonal with sub-pixel corners, both windings.
    const r3::ScreenVertex a = sv(3.3f, 2.7f), b = sv(40.6f, 5.1f), cc = sv(37.2f, 50.9f), d = sv(1.1f, 44.4f);
    r3::rasterize(a, cc, b, false, 64, 64, sink);
    r3::rasterize(a, d, cc, false, 64, 64, sink);
    int twice = 0, once = 0;
    for (const int h : c.hits) twice += h > 1 ? 1 : 0, once += h == 1 ? 1 : 0;
    CHECK(twice == 0 && once > 1500);
    // Pixels along the shared diagonal: each covered exactly once by one of the two.
    for (int y = 6; y < 44; ++y) {
        int row = 0;
        for (int x = 0; x < 64; ++x) row += c.hits[y * 64 + x];
        int first = -1, last = -1;
        for (int x = 0; x < 64; ++x)
            if (c.hits[y * 64 + x] != 0) first = first < 0 ? x : first, last = x;
        CHECK(row == last - first + 1); // a contiguous span: no hole on the diagonal
    }
}

NNH_TEST(raster_back_faces_are_culled_by_winding) {
    Counter c;
    const r3::PixelSink sink{&Counter::fn, &c};
    // Counter-clockwise in NDC (y up) = clockwise on the y-down screen: front.
    const r3::ScreenVertex a = sv(10, 50), b = sv(50, 50), d = sv(10, 10);
    CHECK(r3::rasterize(a, b, d, true, 64, 64, sink) > 0);
    CHECK(r3::rasterize(a, d, b, true, 64, 64, sink) == 0);
    CHECK(r3::rasterize(a, d, b, false, 64, 64, sink) > 0);
}

NNH_TEST(raster_clipping_keeps_the_part_in_front_of_the_near_plane) {
    r3::ClipVertex in[3];
    const float pos[3][4] = {{0, 0, -2, 1}, {1, 0, 0.5f, 1}, {0, 1, 0.5f, 1}}; // the first behind z = −w
    for (int i = 0; i < 3; ++i) {
        std::memcpy(in[i].clip, pos[i], sizeof pos[i]);
        for (float& a : in[i].attr) a = static_cast<float>(i);
    }
    r3::ClipVertex out[9];
    const int n = r3::clip_triangle(in, out);
    CHECK(n == 4);
    for (int i = 0; i < n; ++i) CHECK(out[i].clip[2] + out[i].clip[3] >= -1e-6f);
    const float all_behind[3][4] = {{0, 0, -2, 1}, {1, 0, -3, 1}, {0, 1, -2, 1}};
    for (int i = 0; i < 3; ++i) std::memcpy(in[i].clip, all_behind[i], sizeof all_behind[i]);
    CHECK(r3::clip_triangle(in, out) == 0);
}

NNH_TEST(raster_sampling_filters_and_wraps) {
    const std::uint8_t px[2 * 2 * 4] = {0, 0, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255};
    const r3::TextureView t{px, 2, 2};
    float o[4];
    r3::sample(t, 0.75f, 0.25f, 0, 0, 0, o); // nearest: texel (1, 0) red
    CHECK(o[0] == 1 && o[1] == 0);
    r3::sample(t, 1.75f, 0.25f, 0, 0, 0, o); // repeat
    CHECK(o[0] == 1);
    r3::sample(t, 1.75f, 0.25f, 0, 1, 0, o); // clamp: still the right column
    CHECK(o[0] == 1);
    r3::sample(t, 1.25f, 0.25f, 0, 2, 0, o); // mirror: x 2.5 → texel 1
    CHECK(o[0] == 1);
    r3::sample(t, -0.25f, 0.25f, 0, 2, 0, o); // mirror below 0: texel 0
    CHECK(o[0] == 0 && o[3] == 1);
    r3::sample(t, 0.5f, 0.25f, 1, 1, 1, o); // bilinear halfway between black and red
    CHECK(std::fabs(o[0] - 0.5f) < 1e-6f);
}

NNH_TEST(raster_lighting_tables) {
    static r3::LightTables t;
    r3::make_light_tables(t);
    CHECK(t.specular[0][255] == 1 && t.specular[0][0] == 0 && t.specular[3][255] == 1);
    CHECK(std::fabs(t.specular[0][128] - (128.0f / 255) * (128.0f / 255)) < 1e-7f);
    CHECK(t.distance[0] == 1 && t.distance[255] == 0);
    CHECK(std::fabs(r3::lut_read(t.distance, 0.5f) - 0.25f) < 2e-5f);
    CHECK(r3::lut_read(t.distance, 2.0f) == 0 && r3::lut_read(t.specular[7], -1) == 0);
}

NNH_TEST(raster_lit_pixel_matches_the_formula) {
    static r3::LightTables tables;
    r3::make_light_tables(tables);
    nnh::platform::Pass3D p = identity_pass();
    p.ambient[0] = p.ambient[1] = p.ambient[2] = 0.2f;
    p.light_count = 1;
    p.lights[0].kind = 0;
    p.lights[0].vector[0] = 0, p.lights[0].vector[1] = 0.6f, p.lights[0].vector[2] = 0.8f;
    p.lights[0].color[0] = 1, p.lights[0].color[1] = 0.5f, p.lights[0].color[2] = 0.25f;
    nnh::platform::Material3DGpu m;
    m.base[0] = 0.8f, m.base[1] = 0.6f, m.base[2] = 0.4f, m.base[3] = 1;
    m.specular[0] = m.specular[1] = m.specular[2] = 0.5f;
    m.shininess = 2; // exponent 8
    float attr[r3::kAttributes] = {0, 0, -5, 0, 0, 1, 0, 0, 1, 1, 1, 1}; // facing the eye
    float out[4];
    r3::shade(p, tables, m, false, {}, attr, out);
    const double nl = 0.8, nh_x = 0.6 / std::sqrt(0.36 + 1.8 * 1.8) * 0 + (0.8 + 1) / std::sqrt(0.36 + 1.8 * 1.8);
    const double spec = std::pow(nh_x, 8);
    const double want[3] = {0.8 * (0.2 + 1.0 * nl) + 1.0 * 0.5 * spec, 0.6 * (0.2 + 0.5 * nl) + 0.5 * 0.5 * spec,
                            0.4 * (0.2 + 0.25 * nl) + 0.25 * 0.5 * spec};
    for (int c = 0; c < 3; ++c) CHECK(std::fabs(r3::to_byte(out[c]) - std::floor(std::fmin(want[c], 1) * 255 + 0.5)) <= 1);
}

NNH_TEST(raster_draws_depth_order_cutout_and_blend) {
    static r3::LightTables tables;
    r3::make_light_tables(tables);
    std::vector<std::uint8_t> rgba(64 * 64 * 4);
    std::vector<float> depth(64 * 64);
    const r3::Target t{rgba.data(), depth.data(), 64, 64};
    r3::clear(t, {0, 0, 0, 255});
    // Two quads (as 2 triangles each) in NDC with z: red at z = 0.5 (far), green at z = −0.5 (near), crossing.
    const std::int16_t v[4 * 6] = {-1000, -1000, 0, 0, 0, 127, 1000, -1000, 0, 0, 0, 127,
                                   1000,  1000,  0, 0, 0, 127, -1000, 1000, 0, 0, 0, 127};
    const std::uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
    const r3::MeshView mesh{reinterpret_cast<const std::uint8_t*>(v), 4, 12, 0, idx, 6};
    nnh::platform::Pass3D p = identity_pass();
    nnh::platform::Draw3D d;
    d.index_count = 6;
    d.normal[0] = d.normal[4] = d.normal[8] = 1;
    auto quad = [&](float zoffset, float x0, float r, float g, float b, float a, std::uint8_t mode) {
        for (float& f : d.model_view) f = 0;
        d.model_view[0] = 0.0005f, d.model_view[3] = x0, d.model_view[5] = 0.0005f, d.model_view[11] = zoffset;
        d.material.base[0] = r, d.material.base[1] = g, d.material.base[2] = b, d.material.base[3] = a;
        d.material.lit = false, d.material.alpha = mode, d.material.cutoff = 128;
        r3::draw(t, p, tables, d, mesh, {});
    };
    quad(-0.5f, 0.2f, 0, 1, 0, 1, 0); // green, nearer, drawn first, right half
    quad(0.5f, -0.2f, 1, 0, 0, 1, 0); // red, farther, left half: hidden where they overlap
    const std::uint8_t* mid = rgba.data() + (32 * 64 + 32) * 4;
    CHECK(mid[0] == 0 && mid[1] == 255);
    const std::uint8_t* left = rgba.data() + (32 * 64 + 16) * 4;
    CHECK(left[0] == 255);
    quad(-0.9f, 0, 0, 0, 1, 0.25f, 1); // cut-out below the cutoff: nothing
    CHECK(mid[2] == 0);
    quad(-0.9f, 0, 0, 0, 1, 0.5f, 2); // blended blue at alpha 128 over green: (0·127 + 255·128 + 127)/255
    CHECK(mid[2] == 128 && mid[1] == (255u * 127u + 127u) / 255u);
}
