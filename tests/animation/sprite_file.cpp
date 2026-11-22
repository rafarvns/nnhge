// The test-only `.nnhsprite` writer (see sprite_file.hpp).

#include "sprite_file.hpp"

#include <utility>

#include "animation/sprite_library_internal.hpp"
#include "animation/sprite_validate.hpp"

namespace anim_test {

using namespace nnh::detail::spritefmt;

std::vector<std::uint8_t> sprite_file(const nnh::SpriteLibrary& library) {
    const nnh::detail::SpriteBlock* block = nnh::SpriteLibraryAccess::block(library);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(block);
    std::vector<std::uint8_t> file(bytes, bytes + block->header.file_size);
    Header h = header_of(file);
    h.serial = 0;
    set_header(file, h);
    reseal(file);
    return file;
}

void reseal(std::vector<std::uint8_t>& file) {
    Header h = header_of(file);
    h.crc = nnh::detail::sprite_file_crc(file.data(), file.size());
    set_header(file, h);
}

nnh::SpriteLibrary rich_library() {
    auto made = nnh::SpriteLibrary::create({.frames = 6, .clips = 6, .clip_frames = 16, .markers = 6, .states = 3,
                                            .transitions = 3, .pivot = nnh::SpritePivot::BottomCenter});
    nnh::SpriteLibrary lib = std::move(made.value());
    (void)lib.add_grid({.frame_width = 16, .frame_height = 16, .count = 5, .columns = 5});
    (void)lib.add_frame({.region = {80, 0, 10, 12}, .offset_x = 3, .offset_y = 4, .source_width = 16,
                         .source_height = 16});
    using nnh::SpriteMode;
    const nnh::SpriteClipId idle = lib.add_clip({.name = "idle", .count = 2, .frame_time = nnh::Frames{8}}).value();
    const nnh::SpriteClipId run = lib.add_clip({.name = "run", .first = 1, .count = 4, .mode = SpriteMode::PingPong,
                                                .repeat = 2, .direction = nnh::SpriteDirection::Reverse})
                                      .value();
    const int list[] = {5, 0, 5};
    const nnh::FrameTime each[] = {nnh::Frames{1}, nnh::Duration::from_ms(70), nnh::Frames{3}};
    const nnh::SpriteClipId hit = lib.add_clip({.name = "hit", .frames = list, .count = 3, .durations = each,
                                                .mode = SpriteMode::Once})
                                      .value();
    (void)lib.add_clip({.name = "spin", .first = 2, .count = 3, .mode = SpriteMode::Repeat, .repeat = 3});
    (void)lib.add_clip({.name = "pp", .count = 4, .mode = SpriteMode::PingPong});
    (void)lib.add_marker(run, 1, "step");
    (void)lib.add_marker(run, 3, "step", 2);
    (void)lib.add_marker(hit, 2, "hit", -7);
    (void)lib.add_marker(idle, 0, "blink");
    const nnh::SpriteStateId s_idle = lib.add_state("idle", idle).value();
    const nnh::SpriteStateId s_run = lib.add_state("run", run).value();
    const nnh::SpriteStateId s_hit = lib.add_state("hit", hit).value();
    (void)lib.set_next_on_finish(s_hit, s_idle);
    (void)lib.add_transition({.from = s_hit, .when = nnh::SpriteSwitch::Never});
    (void)lib.add_transition({.to = s_run, .when = nnh::SpriteSwitch::AtEnd});
    (void)lib.add_transition({.from = s_idle, .to = s_run, .keep_time = true});
    return lib;
}

} // namespace anim_test
