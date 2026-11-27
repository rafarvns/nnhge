// The hot reload lines of a script (see script_reload.hpp).

#include "remote/script_reload.hpp"

#include "reload/asset_swap.hpp"

namespace nnh::tool::remote {
namespace {

const char* const kSwapUsage = "usage: swap-asset SOURCE FIXTURE [unheld]";
const char* const kExpectUsage = "usage: expect-reload [PATH] [refused] [within N]";

std::string path_argument(const Token& t, const char* what, std::string& out) {
    if (!reload::valid_swap_path(t.text)) {
        return std::string(what) + " must be a relative path with '/' and no '.' or '..' part";
    }
    out = t.text;
    return "";
}

std::string expect_reload(const std::vector<Token>& a, Step& step) {
    for (std::size_t i = 1; i < a.size();) {
        const std::string w = lower(a[i].text);
        if (w == "refused" && !a[i].quoted) {
            step.refused = true;
            ++i;
        } else if (w == "within" && !a[i].quoted && i + 1 < a.size()) {
            if (!parse_frames(a[i + 1].text, step.frames)) return "frames must be a whole number 1 to 216000";
            i += 2;
        } else if (i == 1) {
            step.text = a[i].text; // an asset path or load name ("art/hero")
            if (step.text.empty() || step.text.size() > kMaxTextChars) return kExpectUsage;
            ++i;
        } else {
            return kExpectUsage;
        }
    }
    return "";
}

} // namespace

bool is_reload_command(Command c) {
    return c == Command::SwapAsset || c == Command::RestoreAsset || c == Command::ExpectReload;
}

std::string read_reload_arguments(const std::vector<Token>& a, Step& step) {
    switch (step.command) {
    case Command::SwapAsset: {
        std::string e = path_argument(a[1], "SOURCE", step.text);
        if (e.empty()) e = path_argument(a[2], "FIXTURE", step.value);
        if (!e.empty()) return e;
        if (a.size() == 4) {
            if (lower(a[3].text) != "unheld") return kSwapUsage;
            step.unheld = true;
        }
        return "";
    }
    case Command::RestoreAsset: return path_argument(a[1], "SOURCE", step.text);
    case Command::ExpectReload: return expect_reload(a, step);
    default: return "";
    }
}

} // namespace nnh::tool::remote
