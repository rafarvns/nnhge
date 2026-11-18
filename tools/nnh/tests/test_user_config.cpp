// nnh's per-user settings (user_config.hpp, spec 010 FR-4).

#include "test.hpp"
#include "user_config.hpp"

using namespace nnh::tool;

NNH_TEST(settings_round_trip) {
    const Settings s = parse_settings("# nnh\nconsole = 192.168.10.52\r\n\nbroken line\n=x\nother=a=b\n");
    CHECK(s.size() == 2 && s.at("console") == "192.168.10.52" && s.at("other") == "a=b");
    CHECK(parse_settings(format_settings(s)) == s);
}

NNH_TEST(settings_path_follows_the_platform_conventions) {
    CHECK(settings_path("C:/Users/me/AppData/Roaming", "", "") == "C:/Users/me/AppData/Roaming/nnhge/nnh.ini");
    CHECK(settings_path("", "/home/me/.cfg", "/home/me") == "/home/me/.cfg/nnhge/nnh.ini");
    CHECK(settings_path("", "", "/home/me") == "/home/me/.config/nnhge/nnh.ini");
    CHECK(settings_path("", "", "").empty());
}
