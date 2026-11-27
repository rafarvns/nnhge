// What nnh was built from (build_info.hpp, spec 033 FR-1).

#include "build_info.hpp"
#include "nnh_link/messages.hpp"
#include "remote/remote_wire.hpp"
#include "test.hpp"

#include <cctype>
#include <string>

using namespace nnh::tool;

namespace {

bool is_version(const std::string& text) { // "<n>.<n>.<n>"
    int dots = 0;
    bool digit = false;
    for (char c : text) {
        if (c == '.') {
            if (!digit) return false;
            ++dots;
            digit = false;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            digit = true;
        } else {
            return false;
        }
    }
    return dots == 2 && digit;
}

BuildInfo sample() {
    BuildInfo info;
    info.version = "0.1.0";
    info.remote_protocol = 1;
    info.link_protocol = 1;
    info.os = "windows";
    info.arch = "x64";
    return info;
}

} // namespace

NNH_TEST(the_version_line_names_version_protocols_host_and_build_kind) {
    BuildInfo info = sample();
    info.checkout_dir = "E:/src/nnhge";
    CHECK(version_line(info) == "nnh 0.1.0 (protocol 1, link 1, windows-x64, from a source checkout)");
    info.engine_url = "https://example.org/nnhge-0.1.0.tar.gz";
    info.engine_sha256 = std::string(64, 'a');
    CHECK(version_line(info) == "nnh 0.1.0 (protocol 1, link 1, windows-x64, release build)");
    info.os = "linux";
    info.arch = "arm64";
    info.remote_protocol = 2;
    info.link_protocol = 3;
    info.version = "1.2.3";
    CHECK(version_line(info) == "nnh 1.2.3 (protocol 2, link 3, linux-arm64, release build)");
}

NNH_TEST(a_release_build_is_exactly_one_with_a_pin) {
    BuildInfo info = sample();
    CHECK(!info.release());
    info.engine_url = "https://example.org/a.tar.gz";
    CHECK(!info.release()); // half a pin is no pin (the configure check refuses it anyway)
    info.engine_sha256 = std::string(64, '0');
    CHECK(info.release());
    info.engine_url.clear();
    CHECK(!info.release());
}

NNH_TEST(this_nnh_reports_its_engine_version_protocols_and_host) {
    const BuildInfo& info = build_info();
    CHECK(is_version(info.version));
    CHECK(info.remote_protocol == nnh::detail::remote::kVersion);
    CHECK(info.link_protocol == nnh::link::kProtocolVersion);
    CHECK(info.os == "windows" || info.os == "linux" || info.os == "macos");
    CHECK(info.arch == "x64" || info.arch == "x86" || info.arch == "arm64" || info.arch == "arm");
    CHECK(!info.release());            // the tests are built without NNH_RELEASE_ENGINE_URL
    CHECK(!info.checkout_dir.empty()); // a checkout build knows its checkout
    CHECK(version_line(info).rfind("nnh " + info.version + " (", 0) == 0);
}
