#include "terminal.hpp"

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace nnh::tool {

bool enable_colour() {
    const char* no_colour = std::getenv("NO_COLOR");
    if (no_colour != nullptr && *no_colour != '\0') return false;
#ifdef _WIN32
    if (_isatty(_fileno(stdout)) == 0) return false;
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out == INVALID_HANDLE_VALUE || !GetConsoleMode(out, &mode)) return true; // e.g. MSYS terminals: ANSI works
    return SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

} // namespace nnh::tool
