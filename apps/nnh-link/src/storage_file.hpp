#pragma once
// NNH Link's storage on disk (storage_file.cpp): needs nnh::init().

#include "storage.hpp"

namespace link_app {

/// Reads link.sav (if any) and makes sure the console has an id (a new random one the first time).
void load_storage(Storage& storage);
/// Writes link.sav when something changed.
void save_storage(Storage& storage);

} // namespace link_app
