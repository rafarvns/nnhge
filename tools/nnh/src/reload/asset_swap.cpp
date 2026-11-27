// Source files swapped by a test script (see asset_swap.hpp).

#include "reload/asset_swap.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace nnh::tool::reload {
namespace {

namespace fs = std::filesystem;

constexpr const char* kJournal = "journal.txt";

/// "assets/art/hero.png" → "assets__art__hero.png" (one flat folder of backups).
std::string flat(const std::string& source) {
    std::string out;
    for (const char c : source) {
        if (c == '/') out += "__";
        else out += c;
    }
    return out;
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

void AssetSwaps::write_journal() const {
    std::error_code ec;
    fs::create_directories(backup_dir_, ec);
    if (swapped_.empty()) {
        fs::remove(backup_dir_ + "/" + kJournal, ec);
        return;
    }
    std::FILE* f = std::fopen((backup_dir_ + "/" + kJournal).c_str(), "wb");
    if (f == nullptr) return;
    std::fprintf(f, "game=%s\n", game_dir_.c_str());
    for (const std::string& s : swapped_) std::fprintf(f, "%s\n", s.c_str());
    std::fclose(f);
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
        write_journal(); // before the source changes: an interruption from here on is recoverable
    }
    if (!fs::copy_file(fixture, target, fs::copy_options::overwrite_existing, ec)) {
        return "swap-asset: can't copy " + fixture + " over " + target;
    }
    return "";
}

std::string AssetSwaps::restore(const std::string& source) {
    const auto it = std::find(swapped_.begin(), swapped_.end(), source);
    if (it == swapped_.end()) return "restore-asset: " + source + " wasn't swapped";
    std::error_code ec;
    if (!fs::copy_file(backup_of(source), game_dir_ + "/" + source, fs::copy_options::overwrite_existing, ec)) {
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
    while (!swapped_.empty()) {
        const std::string s = swapped_.back();
        if (restore(s).empty()) {
            ++restored;
        } else {
            swapped_.pop_back(); // a backup that is gone: nothing to put back
            write_journal();
        }
    }
    std::error_code ec;
    fs::remove(backup_dir_ + "/" + kJournal, ec);
    return restored;
}

} // namespace nnh::tool::reload
