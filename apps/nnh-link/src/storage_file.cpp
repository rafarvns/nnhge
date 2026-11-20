// Loading and saving NNH Link's storage (storage.hpp) as "link.sav" in its data folder, with nnh::save
// (atomic, checksummed, with a backup). The file holds pairing secrets: NNH Link never shows them.

#include <nnh/fs.hpp>
#include <nnh/log.hpp>

#include "platform_link.hpp"
#include "storage_file.hpp"

namespace link_app {
namespace {

constexpr char kFile[] = "link.sav";
constexpr std::size_t kMaxBytes = 2048; // format 1 needs at most 871
NNH_LOG_CHANNEL(link_store);

} // namespace

void load_storage(Storage& storage) {
    std::vector<std::uint8_t> bytes(kMaxBytes);
    std::uint16_t version = 0;
    const auto read = nnh::save::read(kFile, bytes.data(), bytes.size(), &version);
    if (read) {
        bytes.resize(read.value());
        if (version != Storage::kFormat || !storage.decode(bytes)) NNH_LOG_WARN(link_store,"link.sav unreadable: starting afresh");
    }
    if (!storage.has_console_id()) {
        Id id{};
        while (id == Id{}) platform::fill_random(id.data(), id.size());
        storage.set_console_id(id);
    }
}

void save_storage(Storage& storage) {
    if (!storage.dirty()) return;
    const std::vector<std::uint8_t> bytes = storage.encode();
    if (nnh::save::write(kFile, bytes.data(), bytes.size(), Storage::kFormat)) {
        storage.clear_dirty();
    } else {
        NNH_LOG_ERROR(link_store,"can't write link.sav: pairings will be asked again");
        storage.clear_dirty(); // don't retry every frame
    }
}

} // namespace link_app
