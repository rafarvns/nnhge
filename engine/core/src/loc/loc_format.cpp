// loc::format (nnh/localization.hpp; spec 029 FR-24): the key's string (its plural form chosen by the interpreter),
// literal spans copied, placeholders replaced by their arguments, truncated at a UTF-8 boundary. No allocation, no
// printf.

#include <cstring>

#include "loc/loc_lookup.hpp"
#include "loc/loc_number.hpp"
#include "loc/plural.hpp"
#include "nnh/assert.hpp"
#include "nnh/log.hpp"

namespace nnh {

NNH_LOG_CHANNEL_EXTERN(loc);

namespace {

using namespace detail::loc;

struct Out {
    char* p;
    int capacity;
    int at = 0;
    bool cut = false;
    void put(const char* s, int n) {
        if (n <= 0) return;
        const int room = capacity - 1 - at;
        if (n > room) {
            n = room > 0 ? room : 0;
            cut = true;
        }
        std::memcpy(p + at, s, static_cast<std::size_t>(n));
        at += n;
    }
    /// Drops a code point the cut left incomplete.
    void finish() {
        if (cut) {
            int lead = at;
            while (lead > 0 && (static_cast<unsigned char>(p[lead - 1]) & 0xC0) == 0x80) --lead;
            if (lead > 0) {
                const auto b = static_cast<unsigned char>(p[lead - 1]);
                const int length = b >= 0xF0 ? 4 : (b >= 0xE0 ? 3 : (b >= 0xC0 ? 2 : 1));
                if (lead - 1 + length > at) at = lead - 1;
            }
        }
        p[at] = 0;
    }
};

/// The language index of the chain's table `t` (whose rule chooses its plural forms).
int language_of(const LoadedTable* t) {
    const LocState& s = loc_state();
    for (int i = 0; i < s.table_count; ++i)
        if (&s.tables[i] == t) return s.languages[s.current].chain[i];
    return s.current;
}

const nnh::loc::Arg* find_arg(const nnh::loc::Arg* args, int count, std::uint32_t name) {
    for (int i = 0; i < count; ++i)
        if (args[i].name == name) return &args[i];
    return nullptr;
}

} // namespace

int loc::format(char* out, int capacity, Key key, const Arg* args, int count) {
    if (out == nullptr || capacity <= 0) {
        NNH_ASSERT(false, "loc::format: a buffer of capacity %d", capacity);
        return 0;
    }
    Out o{out, capacity};
    const LocState& s = loc_state();
    Found f;
    if (!s.started || !find_key(key.hash, f)) {
        const char* marker = tr(key);
        o.put(marker, static_cast<int>(std::strlen(marker)));
        o.finish();
        return o.at;
    }
    int category = Other;
    if (f.key->flags == 1) {
        const Arg* a = find_arg(args, count, f.key->plural_arg);
        if (a != nullptr && a->kind == 1) {
            const LanguageRecord& lang = s.languages[language_of(f.table)];
            const std::uint64_t n = a->number < 0 ? 0ull - static_cast<std::uint64_t>(a->number) : static_cast<std::uint64_t>(a->number);
            category = plural_category(s.manifest + s.header.off_rules + lang.rule, lang.rule_bytes, n);
        }
    }
    const StringRecord& r = form_of(f, category);
    const StringRecord& next = (&r)[1];
    const char* text = text_of(f, r);
    const LanguageRecord& here = s.languages[s.current];
    int cursor = 0;
    for (std::uint32_t i = r.first_placeholder; i < next.first_placeholder; ++i) {
        const PlaceholderRecord& p = f.table->placeholders[i];
        o.put(text + cursor, p.at - cursor);
        cursor = p.at + p.span;
        const Arg* a = find_arg(args, count, p.name);
        if (a == nullptr) {
#if !defined(NNH_FLAVOR_RELEASE)
            o.put(text + p.at, p.span); // "{name}"
            NNH_LOG_WARN(loc, "format: no argument for %.*s", static_cast<int>(p.span), text + p.at);
#endif
            continue;
        }
        if (a->kind == 1) {
            char digits[kMaxNumberBytes];
            o.put(digits, format_integer(a->number, here.grouping, here.grouping_min, digits));
        } else if (a->kind == 2 && a->text != nullptr) {
            o.put(a->text, static_cast<int>(std::strlen(a->text)));
        } else if (a->kind == 3) {
            const char* t = tr(Key(a->key));
            o.put(t, static_cast<int>(std::strlen(t)));
        }
    }
    o.put(text + cursor, r.length - cursor);
    o.finish();
    return o.at;
}

} // namespace nnh
