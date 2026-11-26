// The example's tunables and watches (see tuning_handles.hpp).

#include "tuning_handles.hpp"

#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

namespace {

constexpr const char* kShapes[] = {"circle", "square", "diamond"};
int g_rebuilds = 0;

void rebuild_dust(void* context) {
    ++g_rebuilds;
    NNH_LOG_INFO(example, "live: dust rebuilt %d", static_cast<int>(*static_cast<const float*>(context)));
}

} // namespace

int dust_rebuilds() { return g_rebuilds; }

void register_live(LiveHandles& h, Tuning& t, World& w, const char* const* state_text, std::uint32_t* frame_counter,
                   void (*respawn)(void*), void* context) {
    namespace live = nnh::live;
    h.jump = live::tune("player/jump", &t.jump, {.min = 0, .max = 12, .step = 0.25f});
    h.gravity = live::tune("player/gravity", &t.gravity, {.min = 0.05f, .max = 1, .step = 0.01f});
    h.run = live::tune("player/run", &t.run, {.min = 0.25f, .max = 4, .step = 0.25f});
    h.hitboxes = live::tune("debug/hitboxes", &t.hitboxes);
    h.count = live::tune("spawn/count", &t.count, {.min = 0, .max = 32, .step = 1});
    h.shape = live::tune("spawn/shape", &t.shape, kShapes);
    h.sky = live::tune("colors/sky", &t.sky);
    h.ground = live::tune("colors/ground", &t.ground);
    h.dust = live::tune("fx/dust", &t.dust, {.min = 0, .max = 64, .step = 1}, {rebuild_dust, &t.dust});
    h.respawn = live::action("player/respawn", respawn, context);
    h.pos = live::watch("player/pos", &w.pos);
    h.vel = live::watch("player/vel", &w.vel);
    h.state = live::watch("player/state", state_text);
    h.frame = live::watch("frame/count", frame_counter);
}
