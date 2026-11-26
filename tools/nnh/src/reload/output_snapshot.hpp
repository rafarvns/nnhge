#pragma once
// What the game has (spec 031 FR-2, FR-4, plan "`nnh` side"): a mirror of the game's converted assets folder — size,
// modification time and FNV-1a 64 content hash per file. After a reconversion, files whose size or time moved are hashed
// again; the files whose bytes differ from the mirror, and those added or removed, are the batch, in the order the game
// takes them (paths sorted, the indexes last: `.nnh_data`, `.nnh_loc`, `.nnh_assets`). The mirror moves only when the
// game applied the batch (accept()), so a refused batch is sent again with the next save.
// Tests: tools/nnh/tests/test_output_snapshot.cpp.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nnh::tool::reload {

struct OutputFile {
    std::uint64_t size = 0;
    std::int64_t time = 0;   ///< last write time (file clock ticks)
    std::uint64_t hash = 0;  ///< FNV-1a 64 of the bytes
};

struct OutputChange {
    std::string path;        ///< asset path ('/'-separated, relative to the folder)
    bool removed = false;
    OutputFile file;         ///< the new file (puts)
};

/// FNV-1a 64 of a file's bytes; 0 (with ok false) when it can't be read.
std::uint64_t file_hash(const std::string& path, bool& ok);

class OutputSnapshot {
public:
    /// Reads the folder and hashes every file (the session's start).
    void scan(const std::string& folder);
    /// The batch: what differs between the folder now and the mirror (the mirror doesn't move).
    std::vector<OutputChange> diff(const std::string& folder) const;
    /// The game applied `changes`: the mirror takes them.
    void accept(const std::vector<OutputChange>& changes);
    /// Forgets `path` so the next diff sends it even when unchanged (`reload PATH`).
    void forget(const std::string& path);

    const std::map<std::string, OutputFile>& files() const { return files_; }
    /// Paths sharing a 32-bit FNV-1a hash (the game's path hash, risk 10): they would reload together.
    std::vector<std::string> hash_collisions() const;

private:
    std::map<std::string, OutputFile> files_;
};

/// Index files come last in a batch (`.nnh_assets` the very last); the rest by path.
bool batch_order(const OutputChange& a, const OutputChange& b);

} // namespace nnh::tool::reload
