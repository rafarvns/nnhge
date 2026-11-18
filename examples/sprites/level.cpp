#include "level.hpp"

#include <cstdlib>
#include <cstring>

bool parse_level(const char* text, std::size_t size, Level& out) {
    out = Level{};
    std::size_t start = 0;
    while (start < size) {
        std::size_t end = start;
        while (end < size && text[end] != '\n') ++end;
        char line[128];
        const std::size_t length = end - start < sizeof line - 1 ? end - start : sizeof line - 1;
        std::memcpy(line, text + start, length);
        line[length] = '\0';
        start = end + 1;
        if (std::strncmp(line, "platform", 8) != 0) continue; // comments, blank lines, future keys
        // strtol rather than sscanf: sscanf pulls the whole scanf family into a 3DS build (tens of KB).
        Platform p;
        char* cursor = line + 8;
        char* end_first = nullptr;
        char* end_last = nullptr;
        char* end_y = nullptr;
        p.first = static_cast<int>(std::strtol(cursor, &end_first, 10));
        p.last = static_cast<int>(std::strtol(end_first, &end_last, 10));
        p.y = static_cast<float>(std::strtol(end_last, &end_y, 10));
        if (end_first == cursor || end_last == end_first || end_y == end_last || p.first < 0 || p.last > 15 ||
            p.first > p.last || out.count == Level::kMaxPlatforms) {
            return false;
        }
        out.platforms[out.count++] = p;
    }
    return true;
}
