// Scene attachments (D30, spec 015 FR-15; T5.1): nodes notified in attach order, attach during a pass waits
// for the next pass, detach during a pass is safe, a scene can't be destroyed with attachments left.

#include "crash/crash_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "recording_scene.hpp"
#include "scene/scene_attachments.hpp"
#include "test.hpp"

#include <cstring>
#include <new>

using nnh::detail::SceneAttachment;
using nnh::detail::SceneHook;
using scene_test::Log;

namespace {

/// Records "<name>b" (BeforeUpdate) or "<name>x" (AfterExit); can attach or detach other probes when
/// notified.
struct Probe : SceneAttachment {
    Probe(char n, Log* l) : name(n), log(l) { notify = &Probe::on_notify; }
    char name;
    Log* log;
    nnh::Scene* scene = nullptr;
    Probe* attach_on_notify = nullptr;
    Probe* detach_on_notify = nullptr;

    static void on_notify(SceneAttachment& self, SceneHook hook) {
        Probe& p = static_cast<Probe&>(self);
        p.log->add(p.name, hook == SceneHook::BeforeUpdate ? 'b' : 'x');
        if (p.attach_on_notify != nullptr) nnh::detail::scene_attach(*p.scene, *p.attach_on_notify);
        if (p.detach_on_notify != nullptr) nnh::detail::scene_detach(*p.scene, *p.detach_on_notify);
        p.attach_on_notify = p.detach_on_notify = nullptr;
    }
};

class Plain : public nnh::Scene {};

void destroy_scene_with_attachment() {
    static Log log;
    static Probe probe('A', &log);
    alignas(Plain) unsigned char storage[sizeof(Plain)];
    auto* scene = new (storage) Plain();
    nnh::detail::scene_attach(*scene, probe);
    scene->~Plain();
}

} // namespace

NNH_TEST(attachments_are_notified_in_attach_order) {
    Log log;
    Plain scene;
    Probe a('A', &log), b('B', &log), c('C', &log);
    nnh::detail::scene_attach(scene, a);
    nnh::detail::scene_attach(scene, b);
    nnh::detail::scene_attach(scene, c);
    nnh::detail::scene_notify(scene, SceneHook::BeforeUpdate);
    nnh::detail::scene_notify(scene, SceneHook::AfterExit);
    CHECK(log.is("Ab Bb Cb Ax Bx Cx"));
    nnh::detail::scene_detach(scene, b);
    log.clear();
    nnh::detail::scene_notify(scene, SceneHook::BeforeUpdate);
    CHECK(log.is("Ab Cb"));
    nnh::detail::scene_detach(scene, a);
    nnh::detail::scene_detach(scene, c);
}

NNH_TEST(a_scene_without_attachments_notifies_nothing) {
    Plain scene;
    nnh::detail::scene_notify(scene, SceneHook::BeforeUpdate);
    nnh::detail::scene_notify(scene, SceneHook::AfterExit);
    CHECK(true);
}

NNH_TEST(a_node_attached_during_a_pass_waits_for_the_next_pass) {
    Log log;
    Plain scene;
    Probe a('A', &log), b('B', &log);
    a.scene = &scene;
    a.attach_on_notify = &b;
    nnh::detail::scene_attach(scene, a);
    nnh::detail::scene_notify(scene, SceneHook::BeforeUpdate);
    CHECK(log.is("Ab"));
    nnh::detail::scene_notify(scene, SceneHook::BeforeUpdate);
    CHECK(log.is("Ab Ab Bb"));
    nnh::detail::scene_detach(scene, a);
    nnh::detail::scene_detach(scene, b);
}

NNH_TEST(a_node_attached_during_a_pass_waits_even_when_the_pass_reaches_it) {
    Log log;
    Plain scene;
    Probe a('A', &log), b('B', &log), c('C', &log);
    a.scene = &scene;
    a.attach_on_notify = &b; // appended after C, which the pass still visits
    nnh::detail::scene_attach(scene, a);
    nnh::detail::scene_attach(scene, c);
    nnh::detail::scene_notify(scene, SceneHook::BeforeUpdate);
    CHECK(log.is("Ab Cb"));
    nnh::detail::scene_notify(scene, SceneHook::BeforeUpdate);
    CHECK(log.is("Ab Cb Ab Cb Bb"));
    nnh::detail::scene_detach(scene, a);
    nnh::detail::scene_detach(scene, b);
    nnh::detail::scene_detach(scene, c);
}

NNH_TEST(detaching_during_a_pass_is_safe_and_the_detached_node_is_skipped) {
    Log log;
    Plain scene;
    Probe a('A', &log), b('B', &log), c('C', &log);
    a.scene = &scene;
    a.detach_on_notify = &b; // the node the pass would visit next
    nnh::detail::scene_attach(scene, a);
    nnh::detail::scene_attach(scene, b);
    nnh::detail::scene_attach(scene, c);
    nnh::detail::scene_notify(scene, SceneHook::AfterExit);
    CHECK(log.is("Ax Cx"));
    c.scene = &scene;
    c.detach_on_notify = &c; // itself
    log.clear();
    nnh::detail::scene_notify(scene, SceneHook::BeforeUpdate);
    nnh::detail::scene_notify(scene, SceneHook::BeforeUpdate);
    CHECK(log.is("Ab Cb Ab"));
    nnh::detail::scene_detach(scene, a);
}

NNH_TEST(a_scene_destroyed_with_attachments_stops_the_game) {
    nnh::platform_fake::reset();
    nnh::detail::crash_install();
    CHECK(nnh::platform_fake::run_expecting_crash(destroy_scene_with_attachment));
    CHECK(!nnh::build::kDevToolsEnabled ||
          std::strstr(nnh::platform_fake::last_crash().top, "still attached") != nullptr);
}
