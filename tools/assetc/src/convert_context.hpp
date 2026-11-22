#pragma once
// State shared by the steps of one conversion: where files come from, the assets folder, the files read (for the
// depfile) and the first problem found, as `file: reason`.

#include <string>
#include <vector>

#include "files.hpp"

namespace nnh::assetc {

struct Problem {
    std::string file;   ///< the file the problem is in
    std::string reason; ///< what is wrong and, when possible, what to do
};

struct Context {
    FileSource& files;
    std::string assets_root;               ///< normalized; every file read must be inside it
    std::vector<std::string> dependencies; ///< files read besides the map itself (tilesets, images)
    Problem problem;

    /// Records the problem and returns false, so readers can `return ctx.fail(...)`.
    bool fail(const std::string& file, const std::string& reason) {
        problem = {file, reason};
        return false;
    }
    void depend_on(const std::string& path) {
        for (const std::string& known : dependencies) {
            if (known == path) return;
        }
        dependencies.push_back(path);
    }
};

} // namespace nnh::assetc
