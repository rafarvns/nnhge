// System capabilities and info (nnh/system.hpp).

#include <cstring>

#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/system.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::Capability;

NNH_TEST(capabilities_come_from_the_platform) {
    nnh::shutdown();
    fake::reset();
    fake::set_capabilities({Capability::SecondScreen, Capability::TouchScreen});
    CHECK(nnh::init());
    CHECK(nnh::system::has(Capability::SecondScreen));
    CHECK(nnh::system::has(Capability::TouchScreen));
    CHECK(!nnh::system::has(Capability::Stereo3D));
    nnh::shutdown();
}

NNH_TEST(info_names_the_system) {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
    const nnh::SystemInfo info = nnh::system::info();
    CHECK(info.name != nullptr && std::strlen(info.name) > 0);
    CHECK(info.model != nullptr);
    nnh::shutdown();
}

NNH_TEST(capabilities_have_names) {
    CHECK(std::strcmp(nnh::system::capability_name(Capability::Stereo3D), "Stereo3D") == 0);
}
