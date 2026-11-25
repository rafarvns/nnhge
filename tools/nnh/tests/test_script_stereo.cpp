// Script runner and the 3D slider (spec 026b, T11.4): `screenshot NAME eyes` asks for the top screen's right eye too
// and names its file `-top-right.png`; `end` waits for all three screens; `slider V|off` goes out at its frame.

#include <string>

#include "fake_console.hpp"
#include "remote/script_runner.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;
using nnh::tool::test::FakeConsole;
namespace wire = nnh::detail::remote;

namespace {

ScriptRunner runner_for(const std::string& text) {
    return ScriptRunner(parse_script(text, "t.nnhs").script, RunnerOptions{.interactive = false, .quit_at_end = false});
}

} // namespace

NNH_TEST(runner_screenshot_eyes_asks_for_the_right_eye_too_and_names_it) {
    ScriptRunner r = runner_for("wait 60\nscreenshot stereo eyes\n");
    FakeConsole c(r);
    r.start();
    c.run_until_finished(200);
    CHECK(r.verdict() == Verdict::Pass);
    const auto shots = c.of_type(wire::MessageType::Screenshot);
    CHECK(shots.size() == 1 && shots[0].screens == (wire::kScreenTop | wire::kScreenBottom | wire::kScreenTopRight));
    CHECK(c.screens.size() == 3);
    if (c.screens.size() != 3) return;
    CHECK(c.screens[0].file_name == "frame-000061-stereo-top.png");
    CHECK(c.screens[1].file_name == "frame-000061-stereo-top-right.png" && c.screens[1].image.width == 400);
    CHECK(c.screens[2].file_name == "frame-000061-stereo-bottom.png");
}

NNH_TEST(runner_end_waits_for_all_three_screens_of_an_eyes_screenshot) {
    ScriptRunner r = runner_for("wait 4\nscreenshot s eyes\n");
    FakeConsole c(r);
    r.start();
    c.run(4);
    std::uint8_t out[wire::kMaxFixedRecord];
    c.to_pc(out, wire::encode_tick_record(6, out));
    CHECK(r.verdict() == Verdict::Running);
    c.run(1);
    CHECK(r.verdict() == Verdict::Pass && c.screens.size() == 3);
}

NNH_TEST(runner_sends_the_slider_at_its_frame) {
    ScriptRunner r = runner_for("wait 10\nslider 0.5\nwait 5\nslider off\n");
    FakeConsole c(r);
    r.start();
    c.run_until_finished(100);
    CHECK(r.verdict() == Verdict::Pass);
    const auto sliders = c.of_type(wire::MessageType::Slider);
    CHECK(sliders.size() == 2);
    if (sliders.size() != 2) return;
    CHECK(sliders[0].frame == 11 && sliders[0].slider == 500);
    CHECK(sliders[1].frame == 16 && sliders[1].slider == wire::kSliderRelease);
}
