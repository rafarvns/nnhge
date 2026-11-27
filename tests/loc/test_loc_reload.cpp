// The strings kind of hot reload (spec 031 T4.2, FR-10, FR-14, FR-16): a string file of the current language reloads
// the chain (new text, the serial bumped); a fallback's file reaches the current language; a larger chain gets a larger
// block; a removed key shows its name; a manifest with another language list is refused ("rebuild and restart").

#include <cstring>
#include <string>

#include "../reload/reload_fixture.hpp"
#include "loc_fixture.hpp"
#include "nnh/localization.hpp"

using namespace loc_test;
using reload_test::Put;
namespace fake = nnh::platform_fake;
using reload_test::send_batch;

namespace {

/// The four languages installed and started in `code`, attached to the PC.
void start_in(const char* code) {
    fresh_engine();
    four_languages().install();
    CHECK(nnh::loc::start({.language = code}).ok());
    fake::remote_connect();
    reload_test::send(remote_test::hello());
    CHECK(nnh::frame());
}

Put bytes_of(const char* path, const std::vector<std::uint8_t>& b) { return Put{path, b.data(), b.size()}; }

} // namespace

NNH_TEST(loc_reload_a_table_of_the_current_language) {
    RELOAD_DEV_ONLY();
    const StopAtEnd stop;
    start_in("en");
    const std::uint32_t serial = nnh::loc::serial();
    LocBuilder b = four_languages();
    for (auto& k : b.languages[0].keys) {
        if (k.name == "menu.quit") k.text = "Leave the game";
    }
    static std::vector<std::uint8_t> table, manifest;
    table = write_table(b.languages[0], false);
    manifest = b.manifest();
    const Put batch[] = {bytes_of("strings/en.nnhstr", table), bytes_of(".nnh_loc", manifest)};
    send_batch(1, batch, 2);
    CHECK(nnh::frame());
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(std::strcmp(nnh::loc::tr("menu.quit"), "Leave the game") == 0); // larger: the block grew
    CHECK(nnh::loc::serial() == serial + 1);
    nnh::shutdown();
}

NNH_TEST(loc_reload_a_fallback_file_reaches_the_current_language) {
    RELOAD_DEV_ONLY();
    const StopAtEnd stop;
    start_in("pt-BR"); // menu.quit comes from the base
    LocBuilder b = four_languages();
    for (auto& k : b.languages[0].keys) {
        if (k.name == "menu.quit") k.text = "Exit";
    }
    static std::vector<std::uint8_t> table, manifest;
    table = write_table(b.languages[0], false);
    manifest = b.manifest();
    const Put batch[] = {bytes_of("strings/en.nnhstr", table), bytes_of(".nnh_loc", manifest)};
    send_batch(1, batch, 2);
    CHECK(nnh::frame());
    CHECK(std::strcmp(nnh::loc::tr("menu.quit"), "Exit") == 0);
    CHECK(std::strcmp(nnh::loc::tr("menu.new_game"), "Novo jogo") == 0);
    nnh::shutdown();
}

NNH_TEST(loc_reload_a_removed_key_shows_its_name) {
    RELOAD_DEV_ONLY();
    const StopAtEnd stop;
    start_in("en");
    LocBuilder b = four_languages();
    auto& keys = b.languages[0].keys;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].name == "item.sword") keys.erase(keys.begin() + static_cast<long>(i));
    }
    static std::vector<std::uint8_t> table, manifest;
    table = write_table(b.languages[0], false);
    manifest = b.manifest();
    const Put batch[] = {bytes_of("strings/en.nnhstr", table), bytes_of(".nnh_loc", manifest)};
    send_batch(1, batch, 2);
    CHECK(nnh::frame());
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(std::strstr(nnh::loc::tr("item.sword"), "item.sword") != nullptr);
    nnh::shutdown();
}

NNH_TEST(loc_reload_another_language_list_is_refused) {
    RELOAD_DEV_ONLY();
    const StopAtEnd stop;
    start_in("en");
    LocBuilder b = four_languages();
    b.add("de", "Deutsch").key("menu.new_game", "Neues Spiel");
    static std::vector<std::uint8_t> manifest;
    manifest = b.manifest();
    reload_test::Lines warn("reload refused");
    const Put batch[] = {bytes_of(".nnh_loc", manifest)};
    send_batch(1, batch, 1);
    CHECK(nnh::frame());
    CHECK(nnh::assets::reload_count() == 0);
    CHECK(warn.count == 1);
    CHECK(std::strstr(warn.last, "another language list: rebuild and restart") != nullptr);
    CHECK(std::strcmp(nnh::loc::tr("menu.quit"), "Quit") == 0);
    nnh::shutdown();
}

NNH_TEST(loc_reload_a_broken_table_keeps_the_old_strings) {
    RELOAD_DEV_ONLY();
    const StopAtEnd stop;
    start_in("en");
    static std::vector<std::uint8_t> table;
    table = write_table(four_languages().languages[0], false);
    table[table.size() / 2] ^= 0x5A;
    const Put batch[] = {bytes_of("strings/en.nnhstr", table)};
    send_batch(1, batch, 1);
    CHECK(nnh::frame());
    CHECK(nnh::assets::reload_count() == 0);
    CHECK(std::strcmp(nnh::loc::tr("menu.quit"), "Quit") == 0);
    nnh::shutdown();
}
