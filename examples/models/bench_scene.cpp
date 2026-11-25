#include "bench_scene.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>
#include <nnh/system.hpp>
#include <nnh/time.hpp>
#include <nnh/transform.hpp>
#include <utility>

namespace {

constexpr const char* kCases[6] = {"throughput 12000", "draws 256",           "cpu 512",
                                   "fill",             "throughput 6000 stereo", "draws 256 stereo"};
constexpr int kMonoCases = 4;
constexpr int kSliderWaitFrames = 300;
constexpr std::int64_t kFrameNs = 16'700'000;   // 60 Hz
constexpr std::int64_t kMissedNs = 17'500'000;  // a frame took longer than one vsync

double ms(std::int64_t ns) { return static_cast<double>(ns) / 1e6; }

/// The previous frame's time in a profile scope (0 without it: the engine's scopes exist in Development builds only).
std::int64_t scope_ns(const char* name) {
    nnh::profile::ScopeStats scopes[nnh::profile::kMaxScopes];
    const int n = nnh::profile::slowest(scopes, nnh::profile::kMaxScopes);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(scopes[i].name, name) == 0) return scopes[i].total.ns;
    }
    return 0;
}

} // namespace

void BenchScene::on_enter() {
    const char* frames = std::getenv("MODELS_BENCH_FRAMES");
    frames_per_case_ = frames != nullptr && std::atoi(frames) > 0 ? std::atoi(frames) : 600;
    case_ = frame_ = missed_ = 0;
    sum_ns_ = worst_ns_ = rec_ns_ = sub_ns_ = prep_ns_ = 0;
    failed_ = done_ = verdict_logged_ = false;
    slider_wait_ = -1;
    const nnh::Material3D tex{.base_color = {255, 255, 255, 255}, .texture = nnh::texture_id(world_.checker)};
    if (auto r = nnh::make_sphere(1.1f, 48, 128, tex)) dense_ = std::move(r.value());
    if (auto r = nnh::make_sphere(1.1f, 48, 64, tex)) half_ = std::move(r.value()); // 6 016 triangles (stereo)
    if (auto r = nnh::make_plane(1, 1, 1)) plane_ = std::move(r.value());
    NNH_LOG_INFO(example, "3d bench: started (%d frames per case)", frames_per_case_);
}

// AC-8's budgets (set 2026-11-25 from console run 2 + ~25 %, plan 026 "CPU/GPU overlap tried (D47) and budgets set"):
// throughput and fill at 60 Hz; draws: frame avg < 35.5 ms and CPU share < 14.8 ms; cpu: CPU share < 8.8 ms.
// AC-8's CPU shares: draws = recording + the whole submission (culling, ordering, the platform's state and draw calls;
// "gfx submit" holds the HUD's 2D too, so it errs high); cpu = recording + culling and ordering ("gfx3d prepare").
void BenchScene::finish_case() {
    const std::int64_t n = frames_per_case_, timed = n > 2 ? n - 2 : 1; // the scopes are read from the third frame
    const std::int64_t avg = sum_ns_ / n, rec = rec_ns_ / n, sub = sub_ns_ / timed, prep = prep_ns_ / timed;
    const std::int64_t cpu = case_ == 2 ? rec + prep : rec + sub;
    bool ok = missed_ == 0;
    if (case_ == 1) ok = avg < 35'500'000 && cpu < 14'800'000; // 256 small draws don't fit 60 Hz on an Old 3DS
    if (case_ == 2) ok = cpu < 8'800'000;                       // CPU alone: no frame-rate target
    const bool judged = case_ < kMonoCases; // stereo budgets come from the console (spec 026 AC-8, 026b)
    failed_ = failed_ || (judged && !ok);
    NNH_LOG_INFO(example, "3d bench: %s avg %.2f worst %.2f missed %d/%d rec %.3f sub %.3f prep %.3f cpu %.3f%s",
                 kCases[case_], ms(avg), ms(worst_ns_), missed_, frames_per_case_, ms(rec), ms(sub), ms(prep), ms(cpu),
                 !judged ? " (not judged)" : (ok ? "" : " (over target)"));
    sum_ns_ = worst_ns_ = rec_ns_ = sub_ns_ = prep_ns_ = 0;
    missed_ = frame_ = 0;
    if (++case_ == 6) done_ = true; // the verdict follows next frame: a script expects one line per frame
}

bool BenchScene::wait_for_slider() {
    if (nnh::system::stereo_slider() > 0.0f) return true;
    if (slider_wait_ < 0) {
        NNH_LOG_INFO(example, "3d bench: stereo waiting for the slider");
        slider_wait_ = 0;
    } else if (++slider_wait_ > kSliderWaitFrames) {
        NNH_LOG_INFO(example, "3d bench: stereo skipped (slider at 0)");
        done_ = true;
    }
    return false;
}

void BenchScene::update() {
    if (nnh::input::pressed(nnh::Button::Start)) return nnh::scenes::pop();
    if (nnh::input::pressed(nnh::Button::Select) && demos_.showcase != nullptr)
        return nnh::scenes::replace(*demos_.showcase);
    if (done_) {
        if (!verdict_logged_)
            NNH_LOG_INFO(example, "3d bench: %s", failed_ ? "FAIL (see the cases over target)" : "PASS");
        verdict_logged_ = true;
        return;
    }
    if (case_ == kMonoCases && frame_ == 0 && !wait_for_slider()) return; // the stereo cases need the slider up
    if (frame_ > 1) { // the first two frames of a case settle
        const std::int64_t d = nnh::time::delta().ns;
        sum_ns_ += d;
        worst_ns_ = d > worst_ns_ ? d : worst_ns_;
        missed_ += d > kMissedNs ? 1 : 0;
        sub_ns_ += scope_ns("gfx submit"); // the previous frame's, a frame of this case from its third frame on
        prep_ns_ += scope_ns("gfx3d prepare");
    } else {
        sum_ns_ += kFrameNs;
    }
    if (++frame_ == frames_per_case_) finish_case();
}

void BenchScene::draw() {
    using nnh::Screen;
    nnh::Camera3D cam;
    cam.position = {0, 0, 4};
    cam.stereo.enabled = case_ >= kMonoCases; // mono cases stay mono whatever the slider
    nnh::gfx3d::set_camera(Screen::Top, cam);
    const int kind = case_ >= kMonoCases ? case_ - kMonoCases : case_; // stereo: throughput, draws
    nnh::Lighting l;
    l.count = kind == 1 ? 3 : 1;
    l.lights[1] = {.kind = nnh::LightKind::Point, .vector = {-2, 1, 1}, .color = {255, 120, 60, 255}, .range = 6};
    l.lights[2] = {.kind = nnh::LightKind::Point, .vector = {2, -1, 1}, .color = {60, 120, 255, 255}, .range = 6};
    nnh::gfx3d::set_lighting(Screen::Top, l);
    const std::int64_t start = nnh::time::now().ns;
    const float spin = static_cast<float>(frame_) * 0.01f;
    if (!done_ && kind == 0) {
        nnh::gfx3d::draw(Screen::Top, case_ == 0 ? dense_ : half_, nnh::Transform3D{.rotation = nnh::quat_axis_angle({0, 1, 0}, spin)});
    } else if (!done_ && (kind == 1 || kind == 2)) {
        const int n = kind == 1 ? 256 : 512;
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
    } else if (!done_ && kind == 3) {
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
