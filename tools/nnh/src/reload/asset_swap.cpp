// Source files swapped by a test script (see asset_swap.hpp).

#include "reload/asset_swap.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace nnh::tool::reload {
namespace {

namespace fs = std::filesystem;

constexpr const char* kJournal = "journal.txt";

/// "assets/art/hero.png" → "assets%2Fart%2Fhero.png" (one flat folder of backups; '%' itself becomes "%25", so two
/// sources never share a name: review nit).
std::string flat(const std::string& source) {
    std::string out;
    for (const char c : source) {
        if (c == '/') out += "%2F";
        else if (c == '%') out += "%25";
        else out += c;
    }
    return out;
}

/// Copies by reading and writing the bytes, so the target's time is "now": copy_file keeps the source's time on Windows
/// (CopyFile), and a restored source older than its converted output would never be rebuilt (the next run then found
/// the swapped output already there: "nothing changed"). Setting the time afterwards isn't reliable either (MinGW).
bool copy_now(const std::string& from, const std::string& to, std::error_code& ec) {
    ec.clear();
    std::FILE* in = std::fopen(from.c_str(), "rb");
    if (in == nullptr) return false;
    std::FILE* out = std::fopen(to.c_str(), "wb");
    if (out == nullptr) {
        std::fclose(in);
        return false;
    }
    char buffer[64 * 1024];
    bool ok = true;
    std::size_t n = 0;
    while (ok && (n = std::fread(buffer, 1, sizeof buffer, in)) > 0) ok = std::fwrite(buffer, 1, n, out) == n;
    ok = ok && std::ferror(in) == 0;
    std::fclose(in);
    return std::fclose(out) == 0 && ok;
}

} // namespace

std::string swap_backup_dir(const std::string& root, const std::string& game) {
    return root + "/build/logs/reload-backup/" + game;
}

std::string journal_game_dir(const std::string& backup_dir) {
    std::FILE* f = std::fopen((backup_dir + "/" + kJournal).c_str(), "rb");
    if (f == nullptr) return "";
    char line[1024] = {};
    std::string dir;
    if (std::fgets(line, sizeof line, f) != nullptr && std::string(line).rfind("game=", 0) == 0) {
        dir = std::string(line + 5);
        while (!dir.empty() && (dir.back() == '\n' || dir.back() == '\r')) dir.pop_back();
    }
    std::fclose(f);
    return dir;
}

bool valid_swap_path(const std::string& path) {
    if (path.empty() || path.size() > 255 || path[0] == '/' || path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos) {
        return false;
    }
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        const std::string seg = path.substr(start, end - start);
        if (seg.empty() || seg == "." || seg == "..") return false;
        start = end + 1;
    }
    return true;
}

AssetSwaps::AssetSwaps(std::string game_dir, std::string backup_dir)
    : game_dir_(std::move(game_dir)), backup_dir_(std::move(backup_dir)) {}

std::string AssetSwaps::backup_of(const std::string& source) const { return backup_dir_ + "/" + flat(source); }

bool AssetSwaps::journal_left() const {
    std::error_code ec;
    return fs::exists(backup_dir_ + "/" + kJournal, ec);
}

bool AssetSwaps::write_journal() const {
    std::error_code ec;
    fs::create_directories(backup_dir_, ec);
    if (swapped_.empty()) {
        fs::remove(backup_dir_ + "/" + kJournal, ec);
        return true;
    }
    std::FILE* f = std::fopen((backup_dir_ + "/" + kJournal).c_str(), "wb");
    if (f == nullptr) return false;
    bool ok = std::fprintf(f, "game=%s\n", game_dir_.c_str()) > 0;
    for (const std::string& s : swapped_) ok = std::fprintf(f, "%s\n", s.c_str()) > 0 && ok;
    return std::fclose(f) == 0 && ok;
}

std::vector<std::string> AssetSwaps::read_journal() const {
    std::vector<std::string> out;
    std::FILE* f = std::fopen((backup_dir_ + "/" + kJournal).c_str(), "rb");
    if (f == nullptr) return out;
    char line[512];
    while (std::fgets(line, sizeof line, f) != nullptr) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (s.rfind("game=", 0) != 0 && valid_swap_path(s)) out.push_back(s);
    }
    std::fclose(f);
    return out;
}

std::string AssetSwaps::swap(const std::string& source, const std::string& fixture) {
    if (!valid_swap_path(source)) return "swap-asset: " + source + " isn't a path inside the game's folder";
    std::error_code ec;
    const std::string target = game_dir_ + "/" + source;
    if (!fs::is_regular_file(target, ec)) return "swap-asset: " + target + " doesn't exist";
    if (!fs::is_regular_file(fixture, ec)) return "swap-asset: the fixture " + fixture + " doesn't exist";
    if (std::find(swapped_.begin(), swapped_.end(), source) == swapped_.end()) {
        fs::create_directories(backup_dir_, ec);
        if (!fs::copy_file(target, backup_of(source), fs::copy_options::overwrite_existing, ec)) {
            return "swap-asset: can't back " + target + " up";
        }
        swapped_.push_back(source);
        if (!write_journal()) { // before the source changes: an interruption from here on is recoverable
            swapped_.pop_back();
            fs::remove(backup_of(source), ec);
            return "swap-asset: can't write the journal in " + backup_dir_ + " (nothing swapped)";
        }
    }
    if (!copy_now(fixture, target, ec)) {
        return "swap-asset: can't copy " + fixture + " over " + target;
    }
    return "";
}

std::string AssetSwaps::restore(const std::string& source) {
    const auto it = std::find(swapped_.begin(), swapped_.end(), source);
    if (it == swapped_.end()) return "restore-asset: " + source + " wasn't swapped";
    std::error_code ec;
    if (!copy_now(backup_of(source), game_dir_ + "/" + source, ec)) {
        return "restore-asset: can't put " + source + " back (its backup is " + backup_of(source) + ")";
    }
    fs::remove(backup_of(source), ec);
    swapped_.erase(it);
    write_journal();
    return "";
}

int AssetSwaps::restore_all() {
    for (const std::string& s : read_journal()) {
        if (std::find(swapped_.begin(), swapped_.end(), s) == swapped_.end()) swapped_.push_back(s);
    }
    int restored = 0;
    const std::vector<std::string> sources(swapped_.rbegin(), swapped_.rend());
    for (const std::string& s : sources) {
        const std::string error = restore(s);
        if (error.empty()) {
            ++restored;
            continue;
        }
        std::error_code ec;
        if (!fs::exists(backup_of(s), ec)) { // a backup that is gone: nothing to put back, forget it
            swapped_.erase(std::find(swapped_.begin(), swapped_.end(), s));
        } else { // kept in the journal: `nnh restore-assets` tries again (review S5)
            std::fprintf(stderr, "nnh: %s\n", error.c_str());
        }
    }
    (void)write_journal(); // removed when everything is back
    return restored;
}

} // namespace nnh::tool::reload
