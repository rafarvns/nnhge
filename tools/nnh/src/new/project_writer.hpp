#pragma once
// Writing a new project (spec 033 FR-5, FR-6, D56 item 4): the target is a new folder or an existing empty one (never
// a link), every file an exclusive create, sub-folders made as needed, and on any failure everything created is
// removed in reverse order (the folder too if nnh made it). The file system is a seam (NewFs) so tests can fail any
// step; the real one is new_fs_system.cpp. Tested in tools/nnh/tests/test_project_writer.cpp.

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "new/template_render.hpp"

namespace nnh::tool {

enum class PathKind { Missing, EmptyFolder, Folder, File, Link, Other };

struct NewFs {
    std::function<PathKind(const std::string&)> kind;            ///< what is at a path (a link is never followed)
    std::function<bool(const std::string&)> create_folder;       ///< false if anything is there already
    std::function<bool(const std::string&, const std::string&)> create_file; ///< exclusive: false if it exists
    std::function<bool(const std::string&)> remove_file;
    std::function<bool(const std::string&)> remove_folder;       ///< an empty folder
};

/// The real file system: UTF-8 paths (wide APIs on Windows), O_CREAT|O_EXCL creates.
NewFs system_new_fs();

struct WriteResult {
    bool ok = false;
    std::string error; ///< without "nnh: "; says nothing was changed, or that what was created was removed
};

/// Writes `files` under `folder` (the name the user typed, relative to the current folder).
WriteResult write_project(const std::string& folder, const std::vector<OutputFile>& files, const NewFs& fs);

} // namespace nnh::tool
