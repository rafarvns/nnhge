#pragma once
// Shared implementation of the fs contract (nnh/platform/fs.hpp) on top of C stdio, used by platform
// libraries whose file systems speak stdio (PC, 3DS via libctru's sdmc devoptab). Each platform sets
// the data root at init().

namespace nnh::platform::shared {

/// Sets the absolute data folder (must end with '/') and creates it, including missing parents.
/// @return false if the folder can't be created.
bool stdio_fs_set_root(const char* root);

} // namespace nnh::platform::shared
