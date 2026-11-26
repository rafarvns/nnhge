#include "bench_scene.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/build.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>
#include <nnh/time.hpp>

namespace {

const char* const kNames[BenchScene::kPhases] = {"base",          "builtin400", "font400", "frame",  "measure_latin",
                                                 "measure_ja",    "tr1000",     "format100", "switch", "page1024"};
// AC-6's targets in ms (0: reported with its pair or not judged on its own); the glyph ratio and the frame are below.
constexpr double kTargets[BenchScene::kPhases] = {0, 0, 0, 0, 0.1, 0.15, 0.1, 0.1, 100, 0};
constexpr int kFrames = 300, kFrameTest = 600, kSwitchEvery = 30, kSwitches = 20;

// 400 characters of Latin text and of Japanese (kana and kyoiku kanji), the measured paragraphs and the drawn glyphs
const char kLatin[] =
    "The lanterns along the road went dark one by one, and the keeper of the last tower climbed the stairs with a "
    "small flame cupped in both hands. Carry it to the valley before dawn, he said, and do not let the wind put it "
    "out, for the travellers on the north road still look for the light. The merchant at the gate sold maps, keys "
    "and potions, and the guard counted every coin twice before the bridge was opened again at first light.";
const char kJapanese[] =
    "道ぞいのランタンが、ひとつ、またひとつと消えていった。夜明けまでに、この火を最後の塔へ運んでほしい。谷の風に消されない"
    "ように、気をつけて。北の道を行く旅人たちは、まだその光をさがしている。門の商人は地図とかぎと薬を売り、番兵は橋を開"
    "ける前に、すべての金貨を二度数えた。山の上の村では、子どもたちが歌いながら火をまっている。川の水は冷たく、森の中は"
    "暗いが、星の光が道を教えてくれる。朝が来るまで歩きつづけよう。道ぞいのランタンが、ひとつ、またひとつと消えていった。"
    "夜明けまでに、この火を最後の塔へ運んでほしい。谷の風に消されないように、気をつけて。北の道を行く旅人たちは、まだ。";

double scope_ms(const char* name) {
    nnh::profile::ScopeStats scopes[nnh::profile::kMaxScopes];
    const int n = nnh::profile::slowest(scopes, nnh::profile::kMaxScopes);
    for (int i = 0; i < n; ++i)
        if (std::strcmp(scopes[i].name, name) == 0) return static_cast<double>(scopes[i].total.ns) / 1e6;
    return 0;
}

bool judged() { return nnh::build::flavor() == nnh::build::Flavor::Development; }

/// 400 glyphs: ten lines of 40 characters of kLatin.
int draw400(const nnh::FontRef& font) {
    char line[41];
    for (int i = 0; i < 10; ++i) {
        std::memcpy(line, kLatin + i * 40, 40);
        line[40] = '\0';
        nnh::gfx::draw_text(nnh::Screen::Top, {2, 16.0f + 20.0f * static_cast<float>(i)}, line, {.font = font});
    }
    return 400;
}

} // namespace

void BenchScene::on_enter() {
    std::snprintf(previous_, sizeof previous_, "%s", nnh::loc::code());
    (void)nnh::loc::set_language("en");
    auto ja = nnh::load_font("fonts/body_ja_13");
    if (ja) ja_ = static_cast<nnh::Font&&>(ja.value());
    auto sprite = nnh::load_texture("art/spark");
    if (sprite) sprite_ = static_cast<nnh::Texture&&>(sprite.value());
    for (int i = 0; i < 1000; ++i) { // the bench table's keys (strings/en/bench.strings), hashed once
        char name[24];
        std::snprintf(name, sizeof name, "bench.k%04d", i);
        keys_[i] = nnh::loc::key(name).hash;
    }
    for (Sum& s : sums_) s = {};
    phase_ = frame_ = missed_ = 0;
    worst_frame_ = 0;
    failed_[0] = '\0';
    done_ = memory_logged_ = false;
    NNH_LOG_INFO(example, "loc bench: started (10 phases, about 1 min)");
}

void BenchScene::on_exit() {
    ja_ = nnh::Font{};
    sprite_ = nnh::Texture{};
    (void)nnh::loc::set_language(previous_);
}

double BenchScene::per_frame(Phase p) const {
    const Sum& s = sums_[p];
    const Sum& b = sums_[kBase];
    const double n = s.n > 0 ? s.n : 1, bn = b.n > 0 ? b.n : 1;
    return (s.own / n - b.own / bn) + (s.submit / n - b.submit / bn);
}

void BenchScene::run_phase(double& ms) {
    const nnh::FontRef body = nnh::loc::font("body");
    const nnh::Duration start = nnh::time::now();
    switch (phase_) {
    case kBuiltin: glyphs_ = draw400(nnh::FontRef{}); break;
    case kFont:
    case kFrame: glyphs_ = draw400(body); break;
    case kMeasureLatin: (void)nnh::gfx::measure_text(kLatin, {.font = body, .wrap_width = 300}); break;
    case kMeasureJa: (void)nnh::gfx::measure_text(kJapanese, {.font = ja_, .wrap_width = 300}); break;
    case kTr: {
        int bytes = 0;
        for (int i = 0; i < 1000; ++i) bytes += nnh::loc::tr(nnh::loc::Key(keys_[i]))[0];
        (void)bytes;
        break;
    }
    case kFormat: {
        char out[96];
        for (int i = 0; i < 100; ++i)
            nnh::loc::format(out, sizeof out, "shop.items", nnh::loc::arg("count", i * 7), nnh::loc::arg("name", "Ana"));
        break;
    }
    case kPage: {
        auto big = nnh::load_font("fonts/bench_1024");
        if (!big) NNH_LOG_ERROR(example, "loc bench: fonts/bench_1024: %s", big.error().message);
        break;
    }
    default: break;
    }
    ms = (nnh::time::now() - start).seconds() * 1000.0;
    if (phase_ == kFrame && sprite_.valid()) { // 300 game sprites, drawn untimed like a game's
        for (int i = 0; i < 300; ++i) {
            nnh::gfx::draw(nnh::Screen::Top, sprite_,
                           {static_cast<float>((i * 37 + frame_) % 384), static_cast<float>((i * 53) % 224)},
                           {.layer = 2});
        }
    }
}

void BenchScene::finish() {
    const Phase p = static_cast<Phase>(phase_);
    const char* judge = judged() ? "" : " not judged (Debug)";
    char line[200] = {};
    bool pass = true;
    if (p == kBase) {
        const double n = sums_[p].n > 0 ? sums_[p].n : 1;
        std::snprintf(line, sizeof line, "loc bench: base gfx submit avg %.4f ms", sums_[p].submit / n);
    } else if (p == kFont) {
        const double builtin = per_frame(kBuiltin) * 1000.0 / 400, font = per_frame(kFont) * 1000.0 / 400;
        const double ratio = builtin > 0 ? font / builtin : 0;
        pass = ratio <= 1.15;
        const auto part = [&](Phase q, bool submit) {
            const double n = sums_[q].n > 0 ? sums_[q].n : 1, bn = sums_[kBase].n > 0 ? sums_[kBase].n : 1;
            return (submit ? sums_[q].submit / n - sums_[kBase].submit / bn : sums_[q].own / n - sums_[kBase].own / bn) *
                   1000.0 / 400;
        };
        NNH_LOG_INFO(example, "loc bench: glyphs cpu builtin %.3f font %.3f us, submit builtin %.3f font %.3f us",
                     part(kBuiltin, false), part(kFont, false), part(kBuiltin, true), part(kFont, true));
        std::snprintf(line, sizeof line, "loc bench: glyphs builtin %.2f us font %.2f us per glyph ratio %.2f (target <= 1.15)%s %s",
                      builtin, font, ratio, judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    } else if (p == kFrame) {
        pass = missed_ == 0;
        std::snprintf(line, sizeof line, "loc bench: frame worst %.1f ms missed %d of %d%s %s", worst_frame_, missed_,
                      kFrameTest, judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    } else if (p == kSwitch) {
        const double n = sums_[p].n > 0 ? sums_[p].n : 1;
        pass = sums_[p].own_worst < kTargets[p];
        std::snprintf(line, sizeof line, "loc bench: switch avg %.1f ms worst %.1f ms over %d (target < 100 ms)%s %s",
                      sums_[p].own / n, sums_[p].own_worst, sums_[p].n, judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    } else if (p == kPage) {
        const double n = sums_[p].n > 0 ? sums_[p].n : 1;
        std::snprintf(line, sizeof line, "loc bench: page1024 load and release avg %.1f ms worst %.1f ms", sums_[p].own / n,
                      sums_[p].own_worst);
    } else if (p != kBuiltin) {
        const double n = sums_[p].n > 0 ? sums_[p].n : 1;
        const double avg = sums_[p].own / n;
        pass = avg < kTargets[p];
        std::snprintf(line, sizeof line, "loc bench: %s avg %.3f ms worst %.3f ms (target < %g ms)%s %s", kNames[p], avg,
                      sums_[p].own_worst, kTargets[p], judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    }
    if (judged() && !pass) {
        const std::size_t at = std::strlen(failed_);
        std::snprintf(failed_ + at, sizeof failed_ - at, "%s%s", at == 0 ? "" : ", ", kNames[p]);
    }
    if (line[0] != '\0') NNH_LOG_INFO(example, "%s", line);
    ++phase_;
    frame_ = 0;
}

void BenchScene::verdict() {
    if (!memory_logged_) { // the memory line, then the verdict in the next frame (each line its own expect)
        memory_logged_ = true;
        const nnh::FontMetrics body = nnh::gfx::font_metrics(nnh::loc::font("body"));
        const nnh::FontMetrics ja = nnh::gfx::font_metrics(ja_);
        NNH_LOG_INFO(example, "loc bench: memory body %d glyphs %u B ja %d glyphs %u B", body.glyphs,
                     static_cast<unsigned>(body.bytes), ja.glyphs, static_cast<unsigned>(ja.bytes));
        return;
    }
    done_ = true;
    if (failed_[0] == '\0') NNH_LOG_INFO(example, "loc bench: PASS");
    else NNH_LOG_INFO(example, "loc bench: FAIL (%s)", failed_);
}

void BenchScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) {
        nnh::scenes::pop();
        return;
    }
    if (done_) return;
    if (phase_ >= kPhases) {
        verdict();
        return;
    }
    Sum& s = sums_[phase_];
    if (phase_ == kSwitch) { // en → ja → en …, one switch every 30 frames, timed by the hook's profile scope
        const double ms = scope_ms("loc switch");
        if (frame_ % kSwitchEvery == 2 && frame_ > kSwitchEvery) { // the hook ran at the start of the frame after the request; its scope reads a frame later
            s.own += ms;
            if (ms > s.own_worst) s.own_worst = ms;
            ++s.n;
        }
        if (frame_ % kSwitchEvery == 0) (void)nnh::loc::set_language(std::strcmp(nnh::loc::code(), "ja") == 0 ? "en" : "ja");
        if (++frame_ > kSwitchEvery * (kSwitches + 1)) {
            (void)nnh::loc::set_language("en");
            finish();
        }
        return;
    }
    if (frame_ >= 2) {
        s.submit += scope_ms("gfx submit");
        ++s.n;
    }
    double ms = 0;
    run_phase(ms);
    if (frame_ >= 2) {
        s.own += ms;
        if (ms > s.own_worst) s.own_worst = ms;
    }
    if (phase_ == kFrame && frame_ >= 2) {
        const double delta = nnh::time::delta().seconds() * 1000.0;
        if (delta > worst_frame_) worst_frame_ = delta;
        if (delta > 20) ++missed_;
    }
    if (++frame_ >= (phase_ == kFrame ? kFrameTest : (phase_ == kPage ? 12 : kFrames))) finish();
}

void BenchScene::draw() {
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 8}, done_ ? "Bench done. SELECT back" : "Bench running...",
                        {.shadow = true, .layer = 15});
    (void)demo_;
}
