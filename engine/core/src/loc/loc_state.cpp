// Localization's state, start and stop (loc_state.hpp, nnh/localization.hpp): the manifest read once, one block sized
// by the build for the largest language chain, the starting language matched, LanguageChanged published.

#include "loc/loc_state.hpp"

#include <cstdlib>
#include <cstring>

#include "core/shutdown_hooks.hpp"
#include "nnh/assert.hpp"
#include "nnh/assets.hpp"
#include "nnh/events.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "nnh/system.hpp"
#include "system/language_tag.hpp"

namespace nnh {

NNH_LOG_CHANNEL(loc);
NNH_MEM_TAG(strings);

namespace detail::loc {

LocState g_loc_state; // loc_state.hpp: read inline by tr() and format()

namespace {

void shutdown_hook() { nnh::loc::stop(); }

/// The table of `language` read into the block at `at`. @return nullptr or the reason.
const char* read_table(int language, std::size_t& at, LoadedTable& t) {
    LocState& s = g_loc_state;
    char path[40];
    const char* code = s.languages[language].code;
    std::size_t n = std::strlen(code);
    std::memcpy(path, "strings/", 8);
    std::memcpy(path + 8, code, n);
    std::memcpy(path + 8 + n, ".nnhstr", 8);
    const std::size_t room = s.block_bytes - at;
    auto got = assets::read(path, s.block + at, room);
    if (!got) return "a string table is missing";
    const std::uint8_t* data = s.block + at;
    if (const char* why = validate_table(data, got.value())) return why;
    t.base = data;
    std::memcpy(&t.h, data, sizeof t.h);
    t.buckets = reinterpret_cast<const std::uint16_t*>(data + t.h.off_buckets);
    t.keys = reinterpret_cast<const KeyRecord*>(data + t.h.off_keys);
    t.strings = reinterpret_cast<const StringRecord*>(data + t.h.off_strings);
    t.placeholders = reinterpret_cast<const PlaceholderRecord*>(data + t.h.off_placeholders);
    t.text = reinterpret_cast<const char*>(data + t.h.off_text);
    at += (got.value() + 7) & ~std::size_t{7};
    return nullptr;
}

} // namespace

const char* load_chain(int language) {
    LocState& s = g_loc_state;
    std::size_t at = 0;
    s.table_count = 0;
    for (int c = 0; c < kMaxChain && s.languages[language].chain[c] != kNoIndex; ++c) {
        if (const char* why = read_table(s.languages[language].chain[c], at, s.tables[c])) return why;
        ++s.table_count;
    }
    std::memset(s.block + at, 0, s.block_bytes - at); // the rest (and the guard) zero
    return nullptr;
}

int match_language(const char* code) {
    const LocState& s = g_loc_state;
    const int n = s.header.language_count;
    for (int i = 0; i < n; ++i)
        if (same_language_tag(code, s.languages[i].code)) return i;
    LanguageTagParts want;
    if (!split_language_tag(code, want)) return -1;
    int alone = -1, any = -1;
    for (int i = 0; i < n; ++i) {
        LanguageTagParts have;
        if (i == s.header.pseudo || !split_language_tag(s.languages[i].code, have)) continue;
        if (!same_language_tag(want.language, have.language) || !same_language_tag(want.script, have.script)) {
            const bool both_unscripted = want.script[0] == 0 && have.script[0] == 0;
            if (!both_unscripted || !same_language_tag(want.language, have.language)) continue;
        }
        if (have.region[0] == 0 && alone < 0) alone = i;
        if (any < 0) any = i;
    }
    return alone >= 0 ? alone : any;
}

void announce(int previous, int current, [[maybe_unused]] const char* why) {
    LocState& s = g_loc_state;
    s.current = current;
    ++s.serial;
    NNH_LOG_INFO(loc, "language %s (%s)", s.languages[current].code, why);
    LanguageChanged e;
    e.previous = static_cast<std::uint8_t>(previous < 0 ? nnh::loc::kNoLanguage : previous);
    e.current = static_cast<std::uint8_t>(current);
    e.serial = s.serial;
    events::publish(e);
}

} // namespace detail::loc

Result<void> loc::start(const StartOptions& options) {
    using namespace detail::loc;
    LocState& s = g_loc_state;
    if (s.started) {
        NNH_ASSERT(false, "loc::start() called twice");
        return Result<void>{};
    }
    MemTag& tag = options.tag != nullptr ? *options.tag : NNH_MEM_TAG_REF(strings);
    auto size = assets::size(".nnh_loc");
    if (!size) return Error::make(ErrorCode::NotFound, "no .nnh_loc: declare LANGUAGES in nnh_add_game");
    s.manifest = static_cast<std::uint8_t*>(mem::alloc(MemRegion::Heap, tag, size.value(), 8));
    if (s.manifest == nullptr) return Error::make(ErrorCode::OutOfMemory, "no memory for the language manifest");
    auto read = assets::read(".nnh_loc", s.manifest, size.value());
    const char* why = !read ? "unreadable" : validate_manifest(s.manifest, read.value());
    if (why != nullptr) {
        NNH_LOG_WARN(loc, "loc::start: .nnh_loc: %s", why);
        mem::free(s.manifest);
        s.manifest = nullptr;
        return Error::make(ErrorCode::Corrupted, ".nnh_loc: %s", why);
    }
    std::memcpy(&s.header, s.manifest, sizeof s.header);
    s.languages = reinterpret_cast<const LanguageRecord*>(s.manifest + s.header.off_languages);
    s.block_bytes = static_cast<std::size_t>(((std::uint64_t{s.header.block_bytes} + 7) & ~std::uint64_t{7}) + 8 * kMaxChain +
                                            kGuardBytes); // block_bytes <= kMaxBlockBytes: fits a size_t
    s.block = static_cast<std::uint8_t*>(mem::alloc(MemRegion::Heap, tag, s.block_bytes, 8));
    if (s.block == nullptr) {
        mem::free(s.manifest);
        s.manifest = nullptr;
        return Error::make(ErrorCode::OutOfMemory, "no memory for the string tables");
    }
    s.tag = &tag;
    int first = options.language != nullptr ? nnh::loc::find(options.language) : -1;
    const char* reason = "player";
    if (first < 0) {
        first = match_language(system::language());
        reason = "system";
        if (first == s.header.pseudo) first = -1; // the pseudo-language only when chosen
    }
    if (first < 0) {
        first = s.header.base;
        reason = "base";
    }
    if (const char* bad = load_chain(first)) {
        NNH_LOG_WARN(loc, "loc::start: %s", bad);
        mem::free(s.block);
        mem::free(s.manifest);
        s = LocState(); // parentheses: GCC 13 ICEs on brace value-init of this struct (gimplify.cc:774)
        return Error::make(ErrorCode::Corrupted, "string tables: %s", bad);
    }
    s.started = true;
    s.follow = options.follow_system;
    std::strncpy(s.system_code, system::language(), sizeof s.system_code - 1);
    s.hook.run = &loc_frame_hook; // switches and following happen there, after input (D32)
    detail::add_input_hook(s.hook);
    s.hook_added = true;
#if !defined(NNH_FLAVOR_RELEASE)
    const char* env = std::getenv("NNH_LOC_SHOW_KEYS");
    s.show_keys = env != nullptr && env[0] == '1';
#endif
    detail::add_shutdown_hook(&shutdown_hook);
    announce(-1, first, reason);
    if (s.fonts_switch != nullptr) s.fonts_switch(first);
    return Result<void>{};
}

void loc::stop() {
    using namespace detail::loc;
    LocState& s = g_loc_state;
    if (s.hook_added) detail::remove_input_hook(s.hook);
    if (s.fonts_switch != nullptr && s.started) s.fonts_switch(-1);
    mem::free(s.block);
    mem::free(s.manifest);
    s = LocState(); // parentheses: GCC 13 ICEs on brace value-init of this struct (gimplify.cc:774)
}

} // namespace nnh
