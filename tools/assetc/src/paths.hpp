#pragma once
// '/'-separated path helpers for nnh-assetc: Tiled stores paths relative to the file that names them, and every
// file the converter reads must stay inside the game's assets folder.

#include <string>

namespace nnh::assetc {

/// '\' → '/', "." segments and repeated '/' removed, "a/.." folded. Keeps a leading '/' or "X:/". A ".." that
/// would climb above an absolute path's root is dropped; leading ".." of a relative path stay.
std::string normalize_path(const std::string& path);

/// True for "/x", "X:/x" and "X:\x".
bool is_absolute_path(const std::string& path);

/// The folder of a file ("a/b/c.tmj" → "a/b", "c.tmj" → "").
std::string parent_path(const std::string& path);

/// `relative` seen from folder `dir`, normalized (an absolute `relative` is only normalized).
std::string join_path(const std::string& dir, const std::string& relative);

/// `path` seen from folder `base` (both absolute; normalized here): "../../x/y.tsj". A drive letter matches in any
/// case; on another drive `path` is returned normalized.
std::string relative_path(const std::string& base, const std::string& path);

/// When normalized `path` lies strictly inside normalized folder `root`, sets `inside` to the part after
/// "root/" and returns true.
bool path_inside(const std::string& root, const std::string& path, std::string& inside);

} // namespace nnh::assetc
