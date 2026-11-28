// The real file system of nnh new (project_writer.hpp NewFs): UTF-8 paths, exclusive creates, links never
// followed. Windows: wide APIs (_wopen, CreateDirectoryW, ...), so a folder name outside the ANSI code page
// works; elsewhere POSIX.

#include "new/project_writer.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nnh::tool {
namespace {

std::filesystem::path u8path(const std::string& s) {
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

PathKind kind_of(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path p = u8path(path);
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(p.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? PathKind::Missing
                                                                              : PathKind::Other;
    }
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) return PathKind::Link; // symbolic links and junctions
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) return PathKind::File;
#else
    const fs::file_status s = fs::symlink_status(p, ec);
    if (ec || s.type() == fs::file_type::not_found)
        return ec && s.type() != fs::file_type::not_found ? PathKind::Other : PathKind::Missing;
    if (s.type() == fs::file_type::symlink) return PathKind::Link;
    if (s.type() == fs::file_type::regular) return PathKind::File;
    if (s.type() != fs::file_type::directory) return PathKind::Other;
#endif
    const bool empty = fs::is_empty(p, ec);
    if (ec) return PathKind::Other;
    return empty ? PathKind::EmptyFolder : PathKind::Folder;
}

bool create_folder(const std::string& path) {
#ifdef _WIN32
    return CreateDirectoryW(u8path(path).c_str(), nullptr) != 0;
#else
    return mkdir(path.c_str(), 0777) == 0;
#endif
}

} // namespace

bool create_file_exclusive(const std::string& path, const std::string& bytes, std::size_t fail_after) {
#ifdef _WIN32
    const int fd =
        _wopen(u8path(path).c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
    const int fd = open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0666);
#endif
    if (fd < 0) return false;
    std::size_t done = 0;
    bool ok = true;
    while (ok && done < bytes.size()) {
        if (done >= fail_after) {
            ok = false;
            break;
        }
        const unsigned chunk = static_cast<unsigned>(
            std::min<std::size_t>({bytes.size() - done, std::size_t{1} << 20, fail_after - done}));
#ifdef _WIN32
        const int n = _write(fd, bytes.data() + done, chunk);
#else
        const auto n = write(fd, bytes.data() + done, chunk);
#endif
        ok = n > 0;
        if (ok) done += static_cast<std::size_t>(n);
    }
#ifdef _WIN32
    ok = _close(fd) == 0 && ok;
    if (!ok) DeleteFileW(u8path(path).c_str()); // a half-written file goes too: nothing is left behind (FR-6)
#else
    ok = close(fd) == 0 && ok;
    if (!ok) unlink(path.c_str());
#endif
    return ok;
}

namespace {

bool remove_file(const std::string& path) {
#ifdef _WIN32
    return DeleteFileW(u8path(path).c_str()) != 0;
#else
    return unlink(path.c_str()) == 0;
#endif
}

bool remove_folder(const std::string& path) {
#ifdef _WIN32
    return RemoveDirectoryW(u8path(path).c_str()) != 0;
#else
    return rmdir(path.c_str()) == 0;
#endif
}

} // namespace

NewFs system_new_fs() {
    NewFs f;
    f.kind = kind_of;
    f.create_folder = create_folder;
    f.create_file = [](const std::string& path, const std::string& bytes) {
        return create_file_exclusive(path, bytes, static_cast<std::size_t>(-1));
    };
    f.remove_file = remove_file;
    f.remove_folder = remove_folder;
    return f;
}

} // namespace nnh::tool
