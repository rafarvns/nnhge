// Simulator window settings (sim_settings.hpp). Lines of `key = value`; spaces and '\r' are ignored.

#include "sim_settings.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nnh::platform::pc {
namespace {

constexpr int kMinWindow = 100;
constexpr int kMaxWindow = 16384;
constexpr int kMaxPosition = 32767;

/// Copies [begin, end) without spaces and '\r' into `out` (bounded). @return false if too long.
bool trimmed(const char* begin, const char* end, char* out, std::size_t size) {
    std::size_t n = 0;
    for (const char* p = begin; p < end; ++p) {
        if (*p == ' ' || *p == '\t' || *p == '\r') continue;
        if (n + 1 >= size) return false;
        out[n++] = *p;
    }
    out[n] = '\0';
    return true;
}

/// A whole decimal integer in [lo, hi].
bool parse_int(const char* text, int lo, int hi, int& out) {
    if (*text == '\0') return false;
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (*end != '\0' || value < lo || value > hi) return false;
    out = static_cast<int>(value);
    return true;
}

bool parse_bool(const char* text, bool& out) {
    if (std::strcmp(text, "1") == 0) {
        out = true;
        return true;
    }
    if (std::strcmp(text, "0") == 0) {
        out = false;
        return true;
    }
    return false;
}

/// "x,y,w,h" with sane bounds.
bool parse_window(const char* text, SimSettings& s) {
    char part[4][16] = {};
    int count = 0;
    std::size_t n = 0;
    for (const char* p = text;; ++p) {
        if (*p == ',' || *p == '\0') {
            if (count >= 4) return false;
            part[count++][n] = '\0';
            n = 0;
            if (*p == '\0') break;
        } else {
            if (n + 1 >= sizeof part[0]) return false;
            part[count][n++] = *p;
        }
    }
    int v[4] = {};
    if (count != 4 || !parse_int(part[0], -kMaxPosition, kMaxPosition, v[0]) ||
        !parse_int(part[1], -kMaxPosition, kMaxPosition, v[1]) || !parse_int(part[2], kMinWindow, kMaxWindow, v[2]) ||
        !parse_int(part[3], kMinWindow, kMaxWindow, v[3])) {
        return false;
    }
    s.x = v[0];
    s.y = v[1];
    s.w = v[2];
    s.h = v[3];
    s.has_window = true;
    return true;
}

void apply(const char* key, const char* value, SimSettings& s) {
    int number = 0;
    if (std::strcmp(key, "layout") == 0) {
        parse_layout(value, s.layout);
    } else if (std::strcmp(key, "scale") == 0) {
        if (parse_int(value, 1, kMaxSimScale, number)) s.scale = number;
    } else if (std::strcmp(key, "fit") == 0) {
        parse_bool(value, s.fit);
    } else if (std::strcmp(key, "fullscreen") == 0) {
        parse_bool(value, s.fullscreen);
    } else if (std::strcmp(key, "window") == 0) {
        parse_window(value, s);
    } else if (std::strcmp(key, "slider") == 0) {
        parse_slider(value, s.slider);
    } else if (std::strcmp(key, "eye") == 0) {
        parse_eye(value, s.eye);
    }
}

constexpr const char* kEyeIds[3] = {"left", "right", "anaglyph"};

} // namespace

bool parse_settings(const char* text, std::size_t size, SimSettings& out) {
    if (text == nullptr || size > kMaxSettingsBytes) return false;
    SimSettings s = out;
    const char* end = text + size;
    for (const char* line = text; line < end;) {
        const char* eol = line;
        while (eol < end && *eol != '\n' && *eol != '\0') ++eol;
        const char* eq = line;
        while (eq < eol && *eq != '=') ++eq;
        char key[32];
        char value[64];
        if (eq < eol && trimmed(line, eq, key, sizeof key) && trimmed(eq + 1, eol, value, sizeof value)) {
            apply(key, value, s);
        }
        if (eol < end && *eol == '\0') break;
        line = eol + 1;
    }
    out = s;
    return true;
}

int format_settings(const SimSettings& s, char* out, std::size_t size) {
    const int slider = s.slider < 0 ? 0 : (s.slider > 100 ? 100 : s.slider);
    int n = std::snprintf(out, size, "layout=%s\nscale=%d\nfit=%d\nfullscreen=%d\nslider=%d.%02d\neye=%s\n",
                          layout_id(s.layout), s.scale, s.fit ? 1 : 0, s.fullscreen ? 1 : 0, slider / 100, slider % 100,
                          eye_id(s.eye));
    if (n < 0 || static_cast<std::size_t>(n) >= size) return -1;
    if (s.has_window) {
        const int m = std::snprintf(out + n, size - static_cast<std::size_t>(n), "window=%d,%d,%d,%d\n", s.x, s.y,
                                    s.w, s.h);
        if (m < 0 || static_cast<std::size_t>(n + m) >= size) return -1;
        n += m;
    }
    return n;
}

bool parse_slider(const char* text, int& hundredths) {
    if (text == nullptr || (text[0] != '0' && text[0] != '1')) return false;
    int value = (text[0] - '0') * 100;
    if (text[1] != '\0') {
        if (text[1] != '.' || text[2] < '0' || text[2] > '9') return false;
        value += (text[2] - '0') * 10;
        if (text[3] != '\0') {
            if (text[3] < '0' || text[3] > '9' || text[4] != '\0') return false;
            value += text[3] - '0';
        }
    }
    if (value > 100) return false;
    hundredths = value;
    return true;
}

int step_slider(int hundredths, int delta) {
    const int v = hundredths + delta;
    return v < 0 ? 0 : (v > 100 ? 100 : v);
}

SimEye next_eye(SimEye eye) {
    return eye == SimEye::Left ? SimEye::Right : (eye == SimEye::Right ? SimEye::Anaglyph : SimEye::Left);
}

const char* eye_id(SimEye eye) { return kEyeIds[static_cast<int>(eye) < 3 ? static_cast<int>(eye) : 0]; }

bool parse_eye(const char* id, SimEye& out) {
    if (id == nullptr) return false;
    for (int i = 0; i < 3; ++i) {
        if (std::strcmp(id, kEyeIds[i]) == 0) {
            out = static_cast<SimEye>(i);
            return true;
        }
    }
    return false;
}

} // namespace nnh::platform::pc
