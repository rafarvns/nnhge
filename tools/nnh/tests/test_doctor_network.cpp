// nnh doctor's Console and Downloads groups, and no network without the options (doctor/checks_network.cpp,
// spec 033 FR-18, FR-19, AC-8).

#include "doctor/checks.hpp"
#include "doctor_checks_util.hpp"
#include "test.hpp"

#include <fstream>
#include <iterator>
#include <string>

using namespace nnh::tool;
using namespace nnh::tool::test;

NNH_TEST(no_network_probe_runs_without_the_options) {
    const FakeMachine m = windows_machine();
    const DoctorReport r = run_doctor(m.probes(), context(Host::Windows));
    CHECK(*m.network_calls == 0);
    CHECK(has(group(r, "Console")->not_checked, "--console <ip> or --online"));
    CHECK(has(group(r, "Downloads")->not_checked, "--online"));
    const auto lines = verdict_lines(r, false);
    CHECK(lines.size() == 6); // Simulator, 3DS, Optional, Console, Downloads, This project (About has none)
}

NNH_TEST(the_console_is_found_or_named_missing) {
    FakeMachine m = windows_machine();
    DoctorContext c = context(Host::Windows);
    c.console = "192.168.10.52";
    m.console_found = "192.168.10.52";
    DoctorReport r;
    check_console(m.probes(), c, r);
    CHECK(check(r, "Console", "console").state == CheckState::Ok &&
          check(r, "Console", "console").found == "192.168.10.52 answered");
    CHECK(*m.network_calls == 1);
    m.console_found = "";
    DoctorReport none;
    check_console(m.probes(), c, none);
    const DoctorCheck missing = check(none, "Console", "console");
    CHECK(missing.state == CheckState::Fail &&
          missing.fix == "open the Homebrew Launcher on the 3DS and press Y, then try again");
    CHECK(doctor_exit_code(none, false) == 1);
}

NNH_TEST(online_fetches_exactly_the_pinned_urls) {
    FakeMachine m = windows_machine();
    DoctorContext c = context(Host::Windows);
    c.online = true;
    DoctorReport r;
    check_downloads(m.probes(), c, r);
    CHECK(m.fetched->size() == 1 && (*m.fetched)[0] == kSdl3MingwUrl);
    CHECK(check(r, "Downloads", "SDL3").state == CheckState::Ok &&
          check(r, "Downloads", "SDL3").found == kSdl3MingwUrl);
    // Inside a project that pins an engine archive: that one too
    m.file_text["E:/games/demo/CMakeLists.txt"] =
        "set(nnhge_url \"https://github.com/rafarvns/nnhge/archive/v0.1.0.tar.gz\")\n";
    c.project_root = "E:/games/demo";
    c.game_project = true;
    m.fetched->clear();
    DoctorReport p;
    check_downloads(m.probes(), c, p);
    CHECK(m.fetched->size() == 2 &&
          (*m.fetched)[1] == "https://github.com/rafarvns/nnhge/archive/v0.1.0.tar.gz");
    // Linux with a system SDL3 has nothing to download
    FakeMachine l = linux_machine();
    l.outputs["/usr/bin/pkg-config --modversion sdl3"] = RunResult{true, 0, false, "3.2.10\n"};
    DoctorContext lc = context(Host::Debian);
    lc.online = true;
    DoctorReport lr;
    check_downloads(l.probes(), lc, lr);
    CHECK(l.fetched->empty() && check(lr, "Downloads", "SDL3").found == "system SDL3: nothing to download");
    l.outputs.erase("/usr/bin/pkg-config --modversion sdl3");
    DoctorReport src;
    check_downloads(l.probes(), lc, src);
    CHECK(l.fetched->size() == 1 && (*l.fetched)[0] == kSdl3SourceUrl);
}

NNH_TEST(a_certificate_failure_gets_the_ca_fix) {
    FakeMachine m = windows_machine();
    m.fetch_error = "60;\"SSL peer certificate or SSH remote key was not OK\"";
    m.files.insert("C:/Program Files/Git/mingw64/etc/ssl/certs/ca-bundle.crt");
    m.env["ProgramFiles"] = "C:\\Program Files";
    m.env["CMAKE_TLS_CAINFO"] = "C:/old/ca.crt"; // set to another bundle: point it at Git's
    DoctorContext c = context(Host::Windows);
    c.online = true;
    DoctorReport r;
    check_downloads(m.probes(), c, r);
    const DoctorCheck s = check(r, "Downloads", "SDL3");
    CHECK(s.state == CheckState::Fail && has(s.found, "certificate"));
    CHECK(s.fix == "setx CMAKE_TLS_CAINFO \"C:/Program Files/Git/mingw64/etc/ssl/certs/ca-bundle.crt\"");
    m.env.erase("CMAKE_TLS_CAINFO"); // unset: the build already used Git's bundle, so a setx would change nothing
    DoctorReport unset;
    check_downloads(m.probes(), c, unset);
    CHECK(check(unset, "Downloads", "SDL3").fix ==
          "https://cmake.org/cmake/help/latest/variable/CMAKE_TLS_CAINFO.html");
    m.fetch_error = "6;\"Could not resolve hostname\"";
    DoctorReport other;
    check_downloads(m.probes(), c, other);
    CHECK(has(check(other, "Downloads", "SDL3").fix, "github.com"));
}

NNH_TEST(a_project_url_that_could_break_the_script_is_not_fetched) {
    FakeMachine m = windows_machine();
    m.file_text["E:/games/demo/CMakeLists.txt"] =
        "set(nnhge_url \"https://x/a\\\")\nmessage(boom)#.tar.gz\")\n";
    DoctorContext c = context(Host::Windows);
    c.online = true;
    c.project_root = "E:/games/demo";
    c.game_project = true;
    DoctorReport r;
    check_downloads(m.probes(), c, r);
    CHECK(m.fetched->size() == 1); // only the SDL3 package
}

NNH_TEST(the_sdl3_pins_match_the_engines_cmake) {
    std::ifstream in(NNH_TOOL_SOURCE_DIR "/../../cmake/nnh_sdl3_pin.cmake");
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(has(text, std::string("set(NNH_SDL3_VERSION \"") + kSdl3Version + "\")"));
    std::string mingw = kSdl3MingwUrl, source = kSdl3SourceUrl;
    const std::string base =
        "https://github.com/libsdl-org/SDL/releases/download/release-${NNH_SDL3_VERSION}";
    const std::string real_base =
        std::string("https://github.com/libsdl-org/SDL/releases/download/release-") + kSdl3Version;
    CHECK(mingw.rfind(real_base, 0) == 0 && source.rfind(real_base, 0) == 0);
    CHECK(has(text, "set(NNH_SDL3_URL_BASE \"" + base + "\")"));
    CHECK(
        has(text,
            "set(NNH_SDL3_MINGW_URL \"${NNH_SDL3_URL_BASE}/SDL3-devel-${NNH_SDL3_VERSION}-mingw.tar.gz\")"));
    CHECK(has(text, "set(NNH_SDL3_SOURCE_URL \"${NNH_SDL3_URL_BASE}/SDL3-${NNH_SDL3_VERSION}.tar.gz\")"));
    CHECK(mingw == real_base + "/SDL3-devel-" + kSdl3Version + "-mingw.tar.gz");
    CHECK(source == real_base + "/SDL3-" + kSdl3Version + ".tar.gz");
}

NNH_TEST(the_fetch_script_refuses_values_cmake_would_read) { // review 033
    const std::string ok = doctor_fetch_script("https://github.com/a/b.tar.gz", "C:\\Program Files\\Git\\ca-bundle.crt");
    CHECK(has(ok, "file(DOWNLOAD \"https://github.com/a/b.tar.gz\" RANGE_START 0 RANGE_END 0"));
    CHECK(has(ok, "TLS_CAINFO \"C:/Program Files/Git/ca-bundle.crt\""));
    CHECK(!has(doctor_fetch_script("https://x/a", ""), "TLS_CAINFO"));
    CHECK(doctor_fetch_script("https://x/a", "C:/x\")\nexecute_process(COMMAND calc)#").empty());
    CHECK(doctor_fetch_script("https://x/a", "C:/${HOME}/ca.crt").empty());
    CHECK(doctor_fetch_script("https://x/a\")\nmessage(boom)#", "").empty());
    CHECK(doctor_fetch_script("http://x/a", "").empty());
}
