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

/// Files held in memory, by exact path.
class MemoryFiles final : public FileSource {
public:
    void add(const std::string& path, std::string bytes) { files_[path] = std::move(bytes); }
    bool read(const std::string& path, std::size_t max_bytes, std::string& out) override;

private:
    std::map<std::string, std::string> files_;
};

} // namespace nnh::assetc
