// The models example's self-check (self_check.hpp): each operation's inputs come from the engine's integer hash
// (FNV-1a of the seed, the step and a salt), scaled exactly (k / 256), so the only float work is the engine's.

#include "self_check.hpp"

#include <bit>

#include "nnh/detail/name_hash.hpp"
#include "nnh/gfx3d.hpp"

namespace models {
namespace {

constexpr std::uint32_t kSeed = 0x3D3D2026u;

} // namespace

std::uint32_t SelfCheck::hash(std::uint32_t salt) const {
    const std::uint32_t words[3] = {kSeed, static_cast<std::uint32_t>(step_), salt};
    char bytes[12];
    for (int i = 0; i < 12; ++i) bytes[i] = static_cast<char>((words[i / 4] >> (8 * (i % 4))) & 0xFFu);
    return nnh::detail::fnv1a32(bytes, sizeof bytes);
}

/// A value in [−range, range) on a 1/256 grid of `range` (exact: an integer times a power of two when range is one).
float SelfCheck::value(std::uint32_t salt, float range) const {
    const auto k = static_cast<std::int32_t>(hash(salt) & 0xFFFFu) - 32768;
    return static_cast<float>(k) * (range / 32768.0f);
}

void SelfCheck::mix(std::uint32_t word) {
    for (int i = 0; i < 4; ++i) sum_ = (sum_ ^ ((word >> (8 * i)) & 0xFFu)) * 16777619u;
}

void SelfCheck::mix(float f) { mix(std::bit_cast<std::uint32_t>(f)); }

void SelfCheck::one() {
    using namespace nnh;
    const std::uint32_t kind = hash(0) % 8u;
    const Vec3 a{value(1, 64), value(2, 64), value(3, 64)};
    const Vec3 b{value(4, 2), value(5, 2), value(6, 2)};
    const Quat q = quat_axis_angle(Vec3{b.x, b.y, b.z + 4.0f}, value(7, 8));
    const Transform3D t{.position = a, .rotation = q, .scale = {1.0f + value(8, 0.5f), 1.0f, 1.0f - value(9, 0.5f)}};
    Mat4 m = to_mat4(t);
    switch (kind) {
    case 0: { // a chain of transforms and a point through it
        const Mat4 chain = m * to_mat4(Transform3D{.position = b, .rotation = quat_euler(b.x, b.y, b.z)});
        const Vec3 p = transform_point(chain, Vec3{value(10, 512), value(11, 512), value(12, 512)});
        mix(p.x), mix(p.y), mix(p.z);
        for (const float f : chain.m) mix(f);
        break;
    }
    case 1: { // inverses
        Mat4 inv, aff;
        mix(static_cast<std::uint32_t>(inverse(m, inv)) | (static_cast<std::uint32_t>(inverse_affine(m, aff)) << 1));
        for (int i = 0; i < 16; ++i) mix(inv.m[i]), mix(aff.m[i]);
        break;
    }
    case 2: { // quaternions: products, conjugates, rotation of a vector
        const Quat r = normalize(q * quat_euler(value(13, 3), value(14, 3), value(15, 3)) * conjugate(q));
        const Vec3 v = rotate(r, a);
        mix(r.x), mix(r.y), mix(r.z), mix(r.w), mix(v.x), mix(v.y), mix(v.z);
        break;
    }
    case 3: { // interpolation
        const Quat r = quat_axis_angle(Vec3{a.x, a.y + 1.0f, a.z}, value(16, 3));
        const float s = value(17, 0.5f) + 0.5f;
        const Quat i = slerp(q, r, s), n = nlerp(q, r, s);
        mix(i.x), mix(i.y), mix(i.z), mix(i.w), mix(n.x), mix(n.y), mix(n.z), mix(n.w);
        break;
    }
    case 4: { // look-at, look_rotation and projections
        const Mat4 view = look_at(a, a + b + Vec3{0, 0, -1}, Vec3{0, 1, 0});
        const Mat4 proj = (hash(18) & 1u) != 0 ? perspective(0.5f + value(19, 0.25f) + 0.5f, 400.0f / 240.0f, 0.1f, 100)
                                               : orthographic(10.0f + value(19, 4), 400.0f / 240.0f, -50, 50);
        const Mat4 vp = proj * view;
        const Quat l = look_rotation(b + Vec3{0.01f, 0, 0});
        for (const float f : vp.m) mix(f);
        mix(l.x), mix(l.y), mix(l.z), mix(l.w);
        break;
    }
    case 6: { // to_screen with a camera placed by look-at (perspective or orthographic)
        Camera3D cam;
        cam.position = Vec3{b.x * 4.0f, b.y * 4.0f + 1.0f, 6.0f + b.z};
        gfx3d::point_at(cam, Vec3{});
        if ((hash(20) & 1u) != 0) cam.projection = Projection::Orthographic;
        const ScreenPoint p = gfx3d::to_screen(cam, Vec3{a.x / 16.0f, a.y / 16.0f, a.z / 16.0f});
        mix(p.pixel.x), mix(p.pixel.y), mix(p.depth);
        mix(static_cast<std::uint32_t>(p.in_front) | (static_cast<std::uint32_t>(p.in_view) << 1));
        break;
    }
    case 7: { // the ray under a pixel
        Camera3D cam;
        cam.position = a;
        cam.rotation = q;
        cam.fov_y = 0.6f + value(21, 0.4f) + 0.4f;
        const Ray3 r = gfx3d::screen_ray(cam, Vec2{value(22, 200) + 200, value(23, 120) + 120});
        mix(r.origin.x), mix(r.origin.y), mix(r.origin.z), mix(r.direction.x), mix(r.direction.y), mix(r.direction.z);
        break;
    }
    default: { // points projected to clip space, homogeneous and divided
        const Mat4 vp = perspective(1.04719758f, 400.0f / 240.0f, 0.1f, 100) * look_at(Vec3{0, 2, 8}, Vec3{}) * m;
        const Vec4 c = vp * Vec4{b.x, b.y, b.z, 1};
        const Vec3 p = transform_point(vp, b);
        mix(c.x), mix(c.y), mix(c.z), mix(c.w), mix(p.x), mix(p.y), mix(p.z);
        mix(length(a)), mix(dot(a, b)), mix(cross(a, b).y);
        break;
    }
    }
}

bool SelfCheck::run(int count) {
    for (int i = 0; i < count && step_ < kOps; ++i) {
        one();
        ++step_;
        if (step_ % kCheckpointEvery == 0) checkpoints_[step_ / kCheckpointEvery - 1] = sum_;
    }
    return step_ >= kOps;
}

} // namespace models
