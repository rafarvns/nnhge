#include "preset_names.hpp"

#include <cstdio>

namespace nnh::tool {

namespace {

/// A bounded JSON reader that only records the "name" of each object directly inside the top-level
/// "configurePresets" and "buildPresets" arrays. Any syntax error makes the whole text define nothing.
class PresetScan {
public:
    PresetScan(std::string_view text, const std::string& wanted) : s_(text), wanted_(wanted) {}

    /// Reads the whole text; false when it isn't one valid JSON value.
    bool run() {
        if (!value(0, Where::Top)) return false;
        space();
        return at_ == s_.size();
    }
    bool configure = false;
    bool build = false;

private:
    enum class Where { Top, TopObject, PresetArray, PresetObject, Other };
    static constexpr int kMaxDepth = 64;

    std::string_view s_;
    const std::string& wanted_;
    std::size_t at_ = 0;
    bool in_configure_ = false; ///< the array being read is "configurePresets" (else "buildPresets")

    void space() {
        while (at_ < s_.size() && (s_[at_] == ' ' || s_[at_] == '\t' || s_[at_] == '\n' || s_[at_] == '\r')) ++at_;
    }
    bool eat(char c) {
        space();
        if (at_ >= s_.size() || s_[at_] != c) return false;
        ++at_;
        return true;
    }
    bool string(std::string& out) {
        space();
        if (at_ >= s_.size() || s_[at_] != '"') return false;
        out.clear();
        for (++at_; at_ < s_.size(); ++at_) {
            const char c = s_[at_];
            if (c == '"') {
                ++at_;
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c == '\\') {
                if (++at_ >= s_.size()) return false;
                const char e = s_[at_];
                if (e == 'u') {
                    if (at_ + 4 >= s_.size()) return false;
                    at_ += 4; // the code point itself never matters for a preset name nnh accepts
                    out += '?';
                    continue;
                }
                const char* from = "\"\\/bfnrt";
                const char* to = "\"\\/\b\f\n\r\t";
                bool known = false;
                for (int i = 0; from[i] != 0; ++i) {
                    if (e == from[i]) {
                        out += to[i];
                        known = true;
                    }
                }
                if (!known) return false;
                continue;
            }
            out += c;
        }
        return false;
    }
    bool literal() { // numbers, true, false, null: their exact form never matters here
        space();
        const std::size_t from = at_;
        while (at_ < s_.size()) {
            const char c = s_[at_];
            const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || c == '-' || c == '+' || c == '.' ||
                            c == 'E';
            if (!ok) break;
            ++at_;
        }
        return at_ > from;
    }
    bool object(int depth, Where where) {
        if (!eat('{')) return false;
        if (eat('}')) return true;
        do {
            std::string key;
            if (!string(key) || !eat(':')) return false;
            Where inner = Where::Other;
            if (where == Where::TopObject && (key == "configurePresets" || key == "buildPresets")) {
                inner = Where::PresetArray;
                in_configure_ = key == "configurePresets";
            }
            if (where == Where::PresetObject && key == "name") {
                space();
                std::string name;
                if (at_ < s_.size() && s_[at_] == '"') {
                    if (!string(name)) return false;
                    if (name == wanted_) (in_configure_ ? configure : build) = true;
                    continue;
                }
            }
            if (!value(depth + 1, inner)) return false;
        } while (eat(','));
        return eat('}');
    }
    bool array(int depth, Where where) {
        if (!eat('[')) return false;
        if (eat(']')) return true;
        const bool configure_array = in_configure_;
        do {
            in_configure_ = configure_array;
            if (!value(depth + 1, where == Where::PresetArray ? Where::PresetObject : Where::Other)) return false;
        } while (eat(','));
        return eat(']');
    }
    bool value(int depth, Where where) {
        if (depth > kMaxDepth) return false;
        space();
        if (at_ >= s_.size()) return false;
        const char c = s_[at_];
        if (c == '{') return object(depth, where == Where::Top ? Where::TopObject : where);
        if (c == '[') return array(depth, where);
        if (c == '"') {
            std::string ignored;
            return string(ignored);
        }
        return literal();
    }
};

void scan(std::string_view text, const std::string& preset, bool& configure, bool& build) {
    if (text.empty() || text.size() > kMaxPresetsBytes) return;
    PresetScan p(text, preset);
    if (!p.run()) return;
    configure = configure || p.configure;
    build = build || p.build;
}

std::string read_small_file(const std::string& path) {
    std::string text;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return text;
    char buffer[4096];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0 && text.size() <= kMaxPresetsBytes) text.append(buffer, n);
    std::fclose(f);
    return text;
}

} // namespace

bool presets_define(std::string_view presets_json, std::string_view user_presets_json, const std::string& preset) {
    bool configure = false;
    bool build = false;
    scan(presets_json, preset, configure, build);
    scan(user_presets_json, preset, configure, build);
    return configure && build;
}

bool presets_file_defines(const std::string& presets_path, const std::string& user_presets_path,
                          const std::string& preset) {
    const std::string user = user_presets_path.empty() ? std::string() : read_small_file(user_presets_path);
    return presets_define(read_small_file(presets_path), user, preset);
}

} // namespace nnh::tool
