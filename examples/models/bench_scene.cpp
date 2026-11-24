#include "bench_scene.hpp"

#include <cstdio>
#include <cstdlib>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/time.hpp>
#include <nnh/transform.hpp>
#include <utility>

namespace {

constexpr const char* kCases[4] = {"throughput 12000", "draws 256", "cpu 512", "fill"};
constexpr std::int64_t kFrameNs = 16'700'000;   // 60 Hz
constexpr std::int64_t kMissedNs = 17'500'000;  // a frame took longer than one vsync

double ms(std::int64_t ns) { return static_cast<double>(ns) / 1e6; }

} // namespace

void BenchScene::on_enter() {
    const char* frames = std::getenv("MODELS_BENCH_FRAMES");
    frames_per_case_ = frames != nullptr && std::atoi(frames) > 0 ? std::atoi(frames) : 600;
    case_ = frame_ = missed_ = 0;
    sum_ns_ = worst_ns_ = rec_ns_ = 0;
    failed_ = done_ = false;
    const nnh::Material3D tex{.base_color = {255, 255, 255, 255}, .texture = nnh::texture_id(world_.checker)};
    if (auto r = nnh::make_sphere(1.1f, 48, 128, tex)) dense_ = std::move(r.value());
    if (auto r = nnh::make_plane(1, 1, 1)) plane_ = std::move(r.value());
    NNH_LOG_INFO(example, "3d bench: started (%d frames per case)", frames_per_case_);
}

void BenchScene::finish_case() {
    const std::int64_t avg = sum_ns_ / frames_per_case_, rec = rec_ns_ / frames_per_case_;
    bool ok = missed_ == 0;
    if (case_ == 1) ok = ok && rec < 4'000'000;
    if (case_ == 2) ok = rec < 1'000'000; // CPU alone: no frame-rate target
    failed_ = failed_ || !ok;
    NNH_LOG_INFO(example, "3d bench: %s avg %.2f worst %.2f missed %d/%d rec %.3f%s", kCases[case_], ms(avg),
                 ms(worst_ns_), missed_, frames_per_case_, ms(rec), ok ? "" : " (over target)");
    sum_ns_ = worst_ns_ = rec_ns_ = 0;
    missed_ = frame_ = 0;
    if (++case_ == 4) {
        done_ = true;
        NNH_LOG_INFO(example, "3d bench: %s", failed_ ? "FAIL (see the cases over target)" : "PASS");
    }
}

void BenchScene::update() {
    if (nnh::input::pressed(nnh::Button::Start)) return nnh::scenes::pop();
    if (nnh::input::pressed(nnh::Button::Select) && demos_.showcase != nullptr)
        return nnh::scenes::replace(*demos_.showcase);
    if (done_) return;
    if (frame_ > 1) { // the first two frames of a case settle
        const std::int64_t d = nnh::time::delta().ns;
        sum_ns_ += d;
        worst_ns_ = d > worst_ns_ ? d : worst_ns_;
        missed_ += d > kMissedNs ? 1 : 0;
    } else {
        sum_ns_ += kFrameNs;
    }
    if (++frame_ == frames_per_case_) finish_case();
}

void BenchScene::draw() {
    using nnh::Screen;
    nnh::Camera3D cam;
    cam.position = {0, 0, 4};
    nnh::gfx3d::set_camera(Screen::Top, cam);
    nnh::Lighting l;
    l.count = case_ == 1 ? 3 : 1;
    l.lights[1] = {.kind = nnh::LightKind::Point, .vector = {-2, 1, 1}, .color = {255, 120, 60, 255}, .range = 6};
    l.lights[2] = {.kind = nnh::LightKind::Point, .vector = {2, -1, 1}, .color = {60, 120, 255, 255}, .range = 6};
    nnh::gfx3d::set_lighting(Screen::Top, l);
    const std::int64_t start = nnh::time::now().ns;
    const float spin = static_cast<float>(frame_) * 0.01f;
    if (!done_ && case_ == 0) {
        nnh::gfx3d::draw(Screen::Top, dense_, nnh::Transform3D{.rotation = nnh::quat_axis_angle({0, 1, 0}, spin)});
    } else if (!done_ && (case_ == 1 || case_ == 2)) {
        const int n = case_ == 1 ? 256 : 512;
        const nnh::Material3D a = world_.cube.material(0);
        nnh::Material3D b = a, glass = a;
        b.base_color = {80, 200, 120, 255};
        glass.alpha = nnh::AlphaMode::Blend;
        glass.base_color.a = 140;
        for (int i = 0; i < n; ++i) {
            const float x = static_cast<float>(i % 16) * 0.45f - 3.4f, y = static_cast<float>((i / 16) % 16) * 0.3f - 2.2f;
            const float z = case_ == 2 && i % 2 == 1 ? 50.0f : -static_cast<float>(i / 256); // odd: behind the camera
            const nnh::Material3D* m = case_ == 2 && i % 4 == 0 ? &glass : (i % 2 == 0 ? &a : &b);
            nnh::gfx3d::draw(Screen::Top, world_.cube,
                             nnh::Transform3D{.position = {x, y, z}, .rotation = nnh::quat_euler(spin + x, y, 0),
                                              .scale = {0.25f, 0.25f, 0.25f}},
                             {.material = m});
        }
    } else if (!done_ && case_ == 3) {
        nnh::gfx3d::draw(Screen::Top, world_.ship, nnh::Transform3D{.rotation = nnh::quat_axis_angle({0, 1, 0}, spin)});
        const nnh::Material3D glass{.base_color = {200, 220, 255, 90}, .alpha = nnh::AlphaMode::Blend, .lit = false,
                                    .cull_back = false};
        for (int k = 0; k < 3; ++k)
            nnh::gfx3d::draw(Screen::Top, plane_,
                             nnh::Transform3D{.position = {0, 0, 1.0f + 0.3f * static_cast<float>(k)},
                                              .rotation = nnh::quat_axis_angle({1, 0, 0}, nnh::kQuarterTurn),
                                              .scale = {8, 1, 6}},
                             {.material = &glass});
    }
    rec_ns_ += nnh::time::now().ns - start;
    const nnh::Stats3D s = nnh::gfx3d::stats();
    std::snprintf(line_, sizeof line_, "bench: %s  frame %d/%d\n%d draws %d culled %d tris",
                  done_ ? "done" : kCases[case_], frame_, frames_per_case_, s.draws, s.culled, s.triangles[0]);
    nnh::gfx::draw_text(Screen::Top, {4, 4}, line_, {.layer = 9});
}
