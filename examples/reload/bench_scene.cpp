// The hot reload example's bench (bench_scene.hpp).

#include "bench_scene.hpp"

#include <nnh/app.hpp>
#include <nnh/assets.hpp>
#include <nnh/log.hpp>
#include <nnh/scene.hpp>



namespace {

constexpr int kFrames = 300;
constexpr int kReads = 1000;
const char* const kPaths[] = {"art/hero.png", "levels/room.nnhmap", "art/backdrop.png", "sfx/blip.wav"};

void bench_reads() {
    const nnh::Duration start = nnh::time::now();
    std::uint64_t bytes = 0;
    for (int i = 0; i < kReads; ++i) {
        const char* path = kPaths[i % 4];
        if (!nnh::assets::exists(path)) continue;
        auto size = nnh::assets::size(path);
        if (size) bytes += size.value();
    }
    const double us = (nnh::time::now() - start).seconds() * 1e6 / kReads;
    NNH_LOG_INFO(example, "reload bench: reads %.2f us per read (%u bytes each, after %u batches)", us,
                 static_cast<unsigned>(bytes / kReads), static_cast<unsigned>(nnh::assets::reload_count()));
}

} // namespace

void BenchScene::on_enter() {
    showcase_.on_enter();
    NNH_LOG_INFO(example, "reload bench: started");
}

void BenchScene::update() {
    start_ = nnh::time::now();
    showcase_.update();
}

void BenchScene::draw() {
    showcase_.draw();
    const double ms = (nnh::time::now() - start_).seconds() * 1000.0;
    total_ms_ += ms;
    worst_ms_ = ms > worst_ms_ ? ms : worst_ms_;
    if (++frames_ != kFrames) return;
    NNH_LOG_INFO(example, "reload bench: idle avg %.3f ms worst %.3f ms over %d frames", total_ms_ / kFrames, worst_ms_,
                 kFrames);
    bench_reads();
    if (pushed_) nnh::scenes::pop();
    frames_ = 0;
    total_ms_ = worst_ms_ = 0;
}
