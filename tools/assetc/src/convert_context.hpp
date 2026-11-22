#pragma once
// State shared by the steps of one conversion: where files come from, the assets folder, the files read (for the
// depfile) and the first problem found, as `file: reason` or, when the line is known, `file:line: reason`.

#include <string>
#include <vector>

#include "files.hpp"

namespace nnh::assetc {

struct Problem {
    std::string file;   ///< the file the problem is in
    std::string reason; ///< what is wrong and, when possible, what to do
    int line = 0;       ///< line in `file` (1-based), 0 when unknown (the Tiled converter doesn't give lines)
};

/// `file:line: reason`, or `file: reason` without a line.
inline std::string problem_text(const Problem& p) {
    return p.line > 0 ? p.file + ":" + std::to_string(p.line) + ": " + p.reason : p.file + ": " + p.reason;
}

struct Context {
    FileSource& files;
    std::string assets_root;               ///< normalized; every file read must be inside it
    std::vector<std::string> dependencies; ///< files read besides the map itself (tilesets, images)
    Problem problem;

    /// Records the problem and returns false, so readers can `return ctx.fail(...)`.
    bool fail(const std::string& file, const std::string& reason, int line = 0) {
        problem = {file, reason, line};
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
