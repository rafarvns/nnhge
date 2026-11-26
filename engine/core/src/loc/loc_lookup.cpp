// tr, resolve and key (nnh/localization.hpp; spec 029 FR-23): the current chain searched, the base last; a key no table
// has shows a marker.

#include <cstdio>
#include <cstring>

#include "loc/loc_lookup.hpp"
#include "nnh/assert.hpp"
#include "nnh/build.hpp"
#include "nnh/log.hpp"

namespace nnh {

NNH_LOG_CHANNEL_EXTERN(loc);

namespace detail::loc {
namespace {

#if !defined(NNH_FLAVOR_RELEASE)
/// The key names of a Debug/Development table (show_keys).
const char* name_in(const LoadedTable& t, const KeyRecord& k) {
    if ((t.h.flags & kFlagNames) == 0) return nullptr;
    const std::uint32_t index = static_cast<std::uint32_t>(&k - t.keys);
    std::uint32_t offset = 0;
    std::memcpy(&offset, t.base + t.h.off_names + index * 4, 4);
    return reinterpret_cast<const char*>(t.base + t.h.off_names + offset);
}
#endif

} // namespace

const char* unknown_key(std::uint32_t hash, const char* name) {
#if defined(NNH_FLAVOR_RELEASE)
    (void)hash, (void)name;
    return "??";
#else
    {
        static std::uint32_t logged[64] = {};
        static int count = 0;
        static char hex[8][12];
        static int next = 0;
        bool seen = false;
        for (int i = 0; i < count; ++i) seen = seen || logged[i] == hash;
        if (!seen && count < 64) logged[count++] = hash;
        const char* shown = name;
        if (shown == nullptr) {
            char* h = hex[next++ & 7];
            std::snprintf(h, sizeof hex[0], "#%08X", static_cast<unsigned>(hash));
            shown = h;
        }
        if (!seen) NNH_LOG_WARN(loc, "unknown key \"%s\"", shown);
        return shown;
    }
#endif
}

} // namespace detail::loc

loc::Key loc::key(const char* name) {
    Key k(name != nullptr ? detail::fnv1a32(name) : 0u);
#if !defined(NNH_FLAVOR_RELEASE)
    k.name = name;
#endif
    return k;
}

const char* loc::tr(Key key) {
    using namespace detail::loc;
    const LocState& s = loc_state();
    if (!s.started) {
        NNH_ASSERT(false, "loc::tr before loc::start()");
        return "";
    }
    Found f;
    if (!find_key(key.hash, f)) {
#if !defined(NNH_FLAVOR_RELEASE)
        return unknown_key(key.hash, key.name);
#else
        return unknown_key(key.hash, nullptr);
#endif
    }
#if !defined(NNH_FLAVOR_RELEASE)
    if (s.show_keys) {
        if (const char* n = name_in(*f.table, *f.key)) return n;
    }
#endif
    return text_of(f, form_of(f, Other));
}

const char* loc::resolve(std::uint32_t key) { return tr(Key(key)); }

} // namespace nnh
