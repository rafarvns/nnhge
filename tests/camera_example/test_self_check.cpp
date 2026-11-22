// The camera example's self-check (examples/camera/self_check.hpp, spec 018 T6.2, AC-2): the script reaches
// the golden checksum on this host, twice the same, with a checkpoint every 60 frames, and the lines
// examples/camera/tests/check.nnhs expects.

#include "camera/self_check.hpp"
#include "test.hpp"

#include <cstdio>
#include <cstring>

namespace {

std::uint32_t run_to_end(int* checkpoints = nullptr) {
    CameraSelfCheck check;
    int count = 0;
    while (!check.done()) {
        if (check.update()) ++count;
    }
    if (checkpoints != nullptr) *checkpoints = count;
    return check.checksum();
}

} // namespace

NNH_TEST(the_camera_self_check_reaches_the_golden_checksum) {
    const std::uint32_t sum = run_to_end();
    if (sum != kCameraCheckGolden) std::printf("  camera self-check sum %08X\n", static_cast<unsigned>(sum));
    CHECK(sum == kCameraCheckGolden);
}

NNH_TEST(the_camera_self_check_gives_the_same_bits_twice) {
    CHECK(run_to_end() == run_to_end());
}

NNH_TEST(the_camera_self_check_has_a_checkpoint_every_60_frames_and_stops) {
    int checkpoints = 0;
    run_to_end(&checkpoints);
    CHECK(checkpoints == CameraSelfCheck::kFrames / CameraSelfCheck::kCheckpointEvery);
    CameraSelfCheck check;
    while (!check.done())
        check.update();
    const std::uint32_t sum = check.checksum();
    CHECK(!check.update() && check.checksum() == sum && check.frame() == CameraSelfCheck::kFrames);
}

NNH_TEST(the_camera_self_check_exercises_the_shake_and_both_layouts) {
    CameraSelfCheck check;
    bool shook = false, tall = false;
    while (!check.done()) {
        check.update();
        if (!(check.rig().position() == check.rig().followed())) shook = true;
        if (check.rig().settings().layout == nnh::CameraLayout::TallWorld) tall = true;
    }
    CHECK(shook && tall && check.rig().drives(nnh::Screen::Bottom) && !check.rig().drives(nnh::Screen::Top));
}

NNH_TEST(camera_checkpoint_and_verdict_lines_have_the_scripts_format) {
    char line[96];
    format_camera_checkpoint(line, sizeof line, 120, 0x0CF7E4CEu);
    CHECK(std::strcmp(line, "camera: 120 sum 0CF7E4CE") == 0);
    format_camera_verdict(line, sizeof line, kCameraCheckGolden);
    char expected[96];
    std::snprintf(expected, sizeof expected, "camera: PASS %08X", static_cast<unsigned>(kCameraCheckGolden));
    CHECK(std::strcmp(line, expected) == 0);
    format_camera_verdict(line, sizeof line, kCameraCheckGolden ^ 1u);
    CHECK(std::strncmp(line, "camera: FAIL sum ", 17) == 0);
}
