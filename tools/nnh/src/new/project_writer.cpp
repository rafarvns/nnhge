// Writing a new project (project_writer.hpp).

#include "new/project_writer.hpp"

namespace nnh::tool {
namespace {

/// What was created, removed in reverse order on failure.
struct Created {
    std::vector<std::pair<std::string, bool>> items; ///< path, is a folder

    /// Removes everything, newest first. @return what could not be removed, ", "-separated ("" = all gone).
    std::string undo(const NewFs& fs) {
        std::string stuck;
        for (auto it = items.rbegin(); it != items.rend(); ++it) {
            const bool removed = it->second ? fs.remove_folder(it->first) : fs.remove_file(it->first);
            if (!removed) stuck += (stuck.empty() ? "" : ", ") + it->first;
        }
        items.clear();
        return stuck;
    }
};

std::string refusal(const std::string& folder, PathKind kind) {
    switch (kind) {
    case PathKind::File:
        return folder + " exists and is a file: nothing was changed (pick another name)";
    case PathKind::Folder:
        return folder + " exists and is not empty: nothing was changed (pick another name, or empty it)";
    case PathKind::Link:
        return folder + " is a link: nothing was changed (pick another name)";
    default:
        return folder + " exists and is not a folder: nothing was changed (pick another name)";
    }
}

/// The parent folders of `path` ("a/b/c.txt" -> "a", "a/b"), outermost first.
std::vector<std::string> parents(const std::string& path) {
    std::vector<std::string> out;
    for (std::size_t slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1)) {
        out.push_back(path.substr(0, slash));
    }
    return out;
}

} // namespace

WriteResult write_project(const std::string& folder, const std::vector<OutputFile>& files, const NewFs& fs) {
    WriteResult r;
    const PathKind kind = fs.kind(folder);
    if (kind != PathKind::Missing && kind != PathKind::EmptyFolder) {
        r.error = refusal(folder, kind);
        return r;
    }
    Created created;
    const auto fail = [&](const std::string& what) {
        const std::string stuck = created.undo(fs);
        r.error = "could not " + what + (stuck.empty() ? ": everything nnh new created was removed"
                                                       : ": could not remove " + stuck + " (remove them by hand)");
        return r;
    };
    if (kind == PathKind::Missing) {
        if (!fs.create_folder(folder)) return fail("create the folder " + folder);
        created.items.emplace_back(folder, true);
    }
    std::vector<std::string> made;
    for (const OutputFile& f : files) {
        for (const std::string& sub : parents(f.path)) {
            bool known = false;
            for (const std::string& m : made)
                known = known || m == sub;
            if (known) continue;
            const std::string path = folder + "/" + sub;
            if (!fs.create_folder(path)) return fail("create the folder " + path);
            created.items.emplace_back(path, true);
            made.push_back(sub);
        }
        const std::string path = folder + "/" + f.path;
        if (!fs.create_file(path, f.bytes)) return fail("write " + path);
        created.items.emplace_back(path, false);
    }
    r.ok = true;
    return r;
}

} // namespace nnh::tool
