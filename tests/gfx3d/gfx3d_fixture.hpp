#pragma once
// Shared helpers for the 3D drawing tests (spec 026 stage 4): an engine with 3D on the fake platform, a cube, the
// last frame's 3D calls, the misuse helper.

#include <cmath>
#include <utility>

#include "crash/crash_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/gfx.hpp"
#include "nnh/gfx3d.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace gfx3d_test {

namespace fake = nnh::platform_fake;

inline void restart(int max_draws_3d = 64) {
    nnh::shutdown();
    fake::reset();
    nnh::AppConfig config;
    config.gfx.max_draws_3d = max_draws_3d;
    CHECK(nnh::init(config));
}

inline nnh::Mesh cube(float size = 1, const nnh::Material3D& m = {}) {
    auto r = nnh::make_cube(size, m);
    CHECK(r.ok());
    return r ? std::move(r.value()) : nnh::Mesh{};
}

/// A model matrix moving to `p`.
inline nnh::Mat4 at(nnh::Vec3 p) { return nnh::to_mat4(nnh::Transform3D{.position = p}); }

/// The draw_3d calls of the last frame, in order.
inline int last_draws(const nnh::platform::Draw3D** out, int capacity) {
    const fake::Fake3D& l = fake::pass3d_log();
    int n = 0;
    for (int i = 0; i < l.last_frame_call_count && i < fake::kMaxRecorded3D; ++i)
        if (l.last_frame_calls[i].kind == 'd' && n < capacity) out[n++] = &l.last_frame_calls[i].draw;
    return n;
}

/// Ends the open frame (submitting it) and opens the next.
inline void next_frame() { CHECK(nnh::frame()); }

inline bool asserts_or_is_ignored(void (*fn)()) {
    nnh::detail::crash_install();
    return fake::run_expecting_crash(fn) == nnh::build::kAssertsEnabled;
}

} // namespace gfx3d_test
