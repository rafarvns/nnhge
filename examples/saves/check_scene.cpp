// The saves example's check (check_scene.hpp).

#include "check_scene.hpp"

#include "check_golden.hpp"
#include "progress_sum.hpp"
#include "reference.hpp"
#include "slot_files.hpp"
#include "slots_panel.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <nnh/fs.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

using namespace saves_example;

namespace {

constexpr int kSteps = 9; // then the verdict
unsigned long hex(std::uint32_t v) {
    return static_cast<unsigned long>(v);
}

} // namespace

void CheckScene::on_enter() {
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    step_ = 0;
    verdict_ = false;
    failed_[0] = '\0';
    for (auto& l : lines_)
        l[0] = '\0';
}

void CheckScene::line(bool ok, const char* format, ...) {
    char text[72];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);
    if (!ok && failed_[0] == '\0')
        std::snprintf(failed_, sizeof failed_, "%s", text + 7); // without "saves: "
    std::snprintf(lines_[step_ < 10 ? step_ : 9], sizeof lines_[0], "%s", text);
    NNH_LOG_INFO(example, "%s", text);
}

void CheckScene::run_step() {
    const SavesGolden& g = kSavesGolden;
    if (step_ == 0) {
        for (int i = 0; i < kSlots; ++i)
            (void)nnh::save::remove(slot_path(i));
        for (const char* f : {"golden_v1.sav", "golden_v2.sav", "golden_v3.sav", "settings.sav"}) {
            (void)nnh::save::remove(
                f); // the script's runs repeat: the settings start from their defaults too
        }
        line(true, "saves: check started");
    } else if (step_ <= 3) {
        const int v = step_;
        const char* path = install_golden(v, buffer_.bytes, sizeof buffer_.bytes);
        const auto r = path != nullptr
                           ? nnh::save::load(path, progress_, buffer_)
                           : nnh::Result<nnh::save::Report>(nnh::Error::make(nnh::ErrorCode::NotFound, "-"));
        const std::uint32_t sum = r ? progress_sum(progress_) : 0;
        const int migrated = r ? r.value().migrated : -1;
        line(sum == g.sum[v - 1] && migrated == g.migrated[v - 1], "saves: v%d sum %08lX migrated %d", v,
             hex(sum), migrated);
    } else if (step_ == 4) {
        fill_reference_v3(stage_);
        const auto n = nnh::save::encode(stage_, buffer_.bytes, sizeof buffer_.bytes);
        const std::uint32_t crc = n ? crc32_of(buffer_.bytes, n.value()) : 0;
        const unsigned bytes = n ? static_cast<unsigned>(n.value()) : 0;
        line(crc == g.encode_crc && bytes == g.encode_bytes, "saves: encode crc %08lX bytes %u", hex(crc),
             bytes);
    } else if (step_ == 5) {
        fill_reference_v3(stage_);
        bool stored = nnh::save::store(slot_path(0), stage_, buffer_).ok();
        fill_reference_v3_b(stage_);
        stored = stored && nnh::save::store(slot_path(0), stage_, buffer_);
        const auto r = nnh::save::load(slot_path(0), progress_, buffer_);
        const std::uint32_t sum = stored && r ? progress_sum(progress_) : 0;
        line(sum == g.slot_sum && !r.value_or({}).used_backup, "saves: slot sum %08lX", hex(sum));
    } else if (step_ == 6) {
        const bool damaged = damage_save(slot_path(0), buffer_.bytes, sizeof buffer_.bytes);
        const auto r = nnh::save::load(slot_path(0), progress_, buffer_);
        const bool backup = damaged && r && r.value().used_backup;
        const std::uint32_t sum = backup ? progress_sum(progress_) : 0;
        line(backup && sum == g.backup_sum, "saves: backup used sum %08lX", hex(sum));
    } else if (step_ == 7) {
        char text[3][8];
        for (int i = 0; i < kSlots; ++i) {
            SlotView s;
            s.used = nnh::save::load(slot_path(i), s.summary, buffer_.bytes, sizeof buffer_.bytes).ok();
            format_slot_short(text[i], sizeof text[i], s);
        }
        char all[32];
        std::snprintf(all, sizeof all, "%s %s %s", text[0], text[1], text[2]);
        line(std::strcmp(all, g.summaries) == 0, "saves: summaries %s", all);
    } else if (step_ == 8) {
        const bool removed = nnh::save::remove(slot_path(0)).ok();
        const int exists = nnh::save::exists(slot_path(0)) ? 1 : 0;
        line(removed && exists == 0, "saves: removed slot1 exists %d", exists);
    }
}

void CheckScene::update() {
    if (step_ < kSteps) {
        run_step();
        ++step_;
    } else if (!verdict_) { // the frame after the last line: each line can be expected on its own
        verdict_ = true;
        if (failed_[0] == '\0') {
            NNH_LOG_INFO(example, "saves: PASS");
        } else {
            NNH_LOG_ERROR(example, "saves: FAIL %s", failed_);
        }
    }
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.showcase);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void CheckScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 20}, "SAVES CHECK", {.scale = 2, .align = nnh::TextAlign::Center});
    for (int i = 0; i < 10; ++i) {
        gfx::draw_text(nnh::Screen::Top, {8, static_cast<float>(50 + 14 * i)}, lines_[i],
                       {.color = nnh::rgb(220, 228, 240)});
    }
    if (verdict_) {
        const bool pass = failed_[0] == '\0';
        gfx::draw_text(nnh::Screen::Top, {200, 200}, pass ? "PASS" : failed_,
                       {.color = pass ? nnh::colors::green : nnh::colors::red,
                        .scale = pass ? 2 : 1,
                        .align = nnh::TextAlign::Center});
    }
    gfx::draw_text(
        nnh::Screen::Bottom, {8, 8},
        "Golden v1, v2 and v3 saves copied to the\ndata folder and loaded by the v3 code;\nthe reference "
        "encoded; a slot stored\ntwice, damaged, loaded from its backup;\nsummaries; a removal. Every value "
        "must\nbe the simulator's on the 3DS.\n\nSELECT showcase  START quit",
        {.color = nnh::rgb(200, 210, 230)});
}
