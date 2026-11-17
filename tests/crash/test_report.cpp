// Crash screen texts (crash/report.hpp) — pure formatting.

#include <cstring>

#include "crash/report.hpp"
#include "test.hpp"

using nnh::detail::CrashInfo;
using nnh::detail::CrashTexts;

namespace {

const char* const kLogs[] = {"[f1 0.0s] I app started", "[f2 0.1s] W gfx odd texture"};

CrashTexts make(bool details, bool with_registers) {
    CrashInfo info;
    info.kind = "Assertion failed";
    info.message = "index 9 out of range";
    info.expression = "i < size";
    info.file = "engine/core/src/foo/bar.cpp";
    info.line = 77;
    info.function = "lookup";
    if (with_registers) {
        info.cpu.has_registers = true;
        for (int r = 0; r < 16; ++r) info.cpu.regs[r] = 0x1000u + static_cast<unsigned>(r);
        info.cpu.cpsr = 0x60000010u;
    }
    CrashTexts out;
    nnh::detail::format_crash_texts(out, info, kLogs, 2, details, "sdmc:/3ds/game/crash.txt");
    return out;
}

} // namespace

NNH_TEST(detailed_screen_shows_message_expression_location_and_logs) {
    const CrashTexts t = make(true, false);
    CHECK(std::strstr(t.top, "Assertion failed") != nullptr);
    CHECK(std::strstr(t.top, "index 9 out of range") != nullptr);
    CHECK(std::strstr(t.top, "i < size") != nullptr);
    CHECK(std::strstr(t.top, "bar.cpp:77") != nullptr);
    CHECK(std::strstr(t.bottom, "odd texture") != nullptr);
    CHECK(std::strstr(t.bottom, "crash.txt") != nullptr);
    CHECK(std::strstr(t.bottom, "START") != nullptr);
}

NNH_TEST(registers_are_shown_when_present) {
    const CrashTexts t = make(true, true);
    CHECK(std::strstr(t.top, "pc 0000100F") != nullptr);
    CHECK(std::strstr(t.top, "r0 00001000") != nullptr);
}

NNH_TEST(friendly_screen_hides_details_but_keeps_location) {
    const CrashTexts t = make(false, true);
    CHECK(std::strstr(t.top, "The game stopped because of an error") != nullptr);
    CHECK(std::strstr(t.top, "bar.cpp:77") != nullptr);
    CHECK(std::strstr(t.top, "r0 ") == nullptr);
    CHECK(std::strstr(t.bottom, "odd texture") == nullptr);
    CHECK(std::strstr(t.bottom, "crash.txt") != nullptr);
}
