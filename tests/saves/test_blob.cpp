// A blob in a save (spec 027 T5.5, FR-3): an ActionSet's bindings (spec 017's NNHB bytes from save_to) kept in a
// data::List<std::uint8_t, 256> of a settings save, stored, loaded, and applied with load_from: the same bindings.

#include <cstring>

#include "../actions/actions_fixture.hpp"
#include "saves_fixture.hpp"

using actions_test::btn;
using nnh::Button;

namespace blob {

enum class Act { Jump, Attack };

struct Settings {
    std::uint8_t volume = 5;
    nnh::data::List<std::uint8_t, 256> controls; // an opaque blob: the NNHB bytes
};
NNH_SAVE_TYPE(Settings, "settings", NNH_SAVE_FIELD(volume, 1), NNH_SAVE_FIELD(controls, 2));

nnh::ActionSet make() {
    nnh::ActionSet set{{.actions = 2}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A)}}));
    CHECK(set.add(Act::Attack, {.name = "attack", .bindings = {btn(Button::B)}}));
    return set;
}

} // namespace blob

NNH_TEST(save_keeps_an_action_set_blob_byte_for_byte) {
    actions_test::restart();
    nnh::ActionSet set = blob::make();
    set.set_source(blob::Act::Jump, 0, nnh::Source::button(Button::Y));
    std::uint8_t bytes[nnh::kMaxBindingsSize];
    const auto saved = set.save_to(bytes, sizeof bytes, 3);
    CHECK(saved.ok() && saved.value() <= 256);
    blob::Settings settings;
    for (std::size_t i = 0; i < saved.value_or(0); ++i) settings.controls.push_back(bytes[i]);
    nnh::save::Buffer<blob::Settings> buffer;
    CHECK(nnh::save::store("settings.sav", settings, buffer).ok());

    blob::Settings loaded;
    CHECK(nnh::save::load("settings.sav", loaded, buffer).ok());
    CHECK(static_cast<std::size_t>(loaded.controls.size()) == saved.value_or(0));
    CHECK(std::memcmp(loaded.controls.begin(), bytes, saved.value_or(0)) == 0);
    nnh::ActionSet fresh = blob::make();
    const auto applied = fresh.load_from(loaded.controls.begin(), static_cast<std::size_t>(loaded.controls.size()), 3);
    CHECK(applied.ok() && fresh.source(blob::Act::Jump, 0) == nnh::Source::button(Button::Y));
    nnh::shutdown();
}
