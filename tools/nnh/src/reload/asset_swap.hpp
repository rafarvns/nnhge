#pragma once
// Source files swapped by a test script (spec 031 FR-22, Q11): `swap-asset SOURCE FIXTURE` backs SOURCE up (next to the
// build logs, in `build/logs/reload-backup/<game>/`, with a journal) and copies the fixture over it; the end of the
// script, a failure or Ctrl+C put every original back byte for byte and remove the journal. A journal left by an
// interrupted run makes `nnh run` refuse to start until `nnh restore-assets <game>` restores it.
// Tests: tools/nnh/tests/test_script_reload.cpp.

#include <string>
#include <vector>

namespace nnh::tool::reload {

class AssetSwaps {
public:
    /// `game_dir`: the folder SOURCE paths are relative to; `backup_dir`: build/logs/reload-backup/<game>.
    AssetSwaps(std::string game_dir, std::string backup_dir);

    /// True when a journal of an earlier, interrupted run exists.
    bool journal_left() const;
    /// Copies `fixture` (absolute or relative to the working folder) over `source` (relative to the game's folder),
    /// backing the original up the first time. @return "" or the error.
    std::string swap(const std::string& source, const std::string& fixture);
    /// Puts `source`'s original back (it stays swapped otherwise). @return "" or the error.
    std::string restore(const std::string& source);
    /// Puts every original back (this run's and a journal's) and removes the journal. @return how many were restored.
    int restore_all();
    /// The sources swapped now.
    const std::vector<std::string>& swapped() const { return swapped_; }

private:
    std::string backup_of(const std::string& source) const;
    bool write_journal() const; ///< false: it couldn't be written
    std::vector<std::string> read_journal() const;

    std::string game_dir_, backup_dir_;
    std::vector<std::string> swapped_;
};

/// The backup folder of `game` under the repository `root`: <root>/build/logs/reload-backup/<game>.
std::string swap_backup_dir(const std::string& root, const std::string& game);
/// The game folder a journal in `backup_dir` names ("" when there is no journal).
std::string journal_game_dir(const std::string& backup_dir);

/// A SOURCE path a script may swap: relative, '/'-separated, no "." or ".." segment, no empty segment.
bool valid_swap_path(const std::string& path);

} // namespace nnh::tool::reload
