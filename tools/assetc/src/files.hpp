#pragma once
// Where nnh-assetc reads its input files from: the disk (the program) or memory (tests and fuzz targets).
// One virtual call per file read (a system, not a hot loop).

#include <cstddef>
#include <map>
#include <string>
#include <utility>

namespace nnh::assetc {

class FileSource {
public:
    virtual ~FileSource() = default;
    /// Reads at most `max_bytes` of file `path` into `out`. False when the file can't be opened or read.
    virtual bool read(const std::string& path, std::size_t max_bytes, std::string& out) = 0;
};

/// Files on disk (fopen in binary mode).
class DiskFiles final : public FileSource {
public:
    bool read(const std::string& path, std::size_t max_bytes, std::string& out) override;
};

/// Writes `size` bytes to the file `path` (replacing it). False when it can't be written whole.
bool write_file(const std::string& path, const void* data, std::size_t size);

/// Like write_file, but leaves the file (and its time) alone when it already holds exactly these bytes, so the
/// build's later steps don't rerun (Ninja `restat`).
bool write_file_if_changed(const std::string& path, const void* data, std::size_t size);

/// Files held in memory, by exact path.
class MemoryFiles final : public FileSource {
public:
    void add(const std::string& path, std::string bytes) { files_[path] = std::move(bytes); }
    bool read(const std::string& path, std::size_t max_bytes, std::string& out) override;

private:
    std::map<std::string, std::string> files_;
};

} // namespace nnh::assetc
