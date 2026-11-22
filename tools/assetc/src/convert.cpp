// One whole map conversion (see convert.hpp).

#include "convert.hpp"

#include "map_writer.hpp"
#include "paths.hpp"
#include "tiled_map.hpp"

namespace nnh::assetc {

bool convert_map(FileSource& files, const std::string& map_path, const std::string& assets_root, Converted& out,
                 Problem& problem) {
    out = {};
    const std::string map_file = normalize_path(map_path);
    Context ctx{files, normalize_path(assets_root), {}, {}};
    Map map;
    if (!read_tiled_map(map_file, ctx, map)) {
        problem = ctx.problem;
        return false;
    }
    std::string error;
    if (!write_map(map, out.bytes, error)) {
        problem = {map_file, error};
        return false;
    }
    out.dependencies.push_back(map_file);
    for (const std::string& path : ctx.dependencies) out.dependencies.push_back(path);
    return true;
}

} // namespace nnh::assetc
