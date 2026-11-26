// The dump (see live_dump.hpp).

#include "live/live_dump.hpp"

#include <cstdio>
#include <cstring>

#include "live/live_apply.hpp"
#include "live/live_format.hpp"
#include "live/live_names.hpp"

namespace nnh::detail::live {
namespace {

bool differs(const Entry& e) { return e.kind != Kind::Action && read_bits(e) != e.def; }

int group_length_of(const Entry& e) {
    const int depth = group_depth(e.name, e.name_length);
    return depth == 0 ? 0 : static_cast<int>(group_length(e.name, e.name_length, depth - 1));
}

int compare(const char* a, int an, const char* b, int bn) {
    const int n = an < bn ? an : bn;
    const int c = std::memcmp(a, b, static_cast<std::size_t>(n));
    return c != 0 ? c : an - bn;
}

void line(void (*emit)(void*, const char*), void* context, const char* text) { emit(context, text); }

void entry_line(const Entry& e, void (*emit)(void*, const char*), void* context) {
    char value[48], def[48], text[160];
    dump_value(e, read_bits(e), value);
    dump_value(e, e.def, def);
    const char* leaf_text = leaf(e.name, e.name_length);
    const int leaf_length = static_cast<int>(e.name + e.name_length - leaf_text);
    std::snprintf(text, sizeof text, "dump: %.*s = %s # default %s", leaf_length, leaf_text, value, def);
    line(emit, context, text);
}

} // namespace

int dump_value(const Entry& e, std::uint32_t bits, char out[48]) {
    if (e.kind == Kind::Color) {
        char c[kValueTextBytes];
        format_color(bits, false, c);
        return std::snprintf(out, 48, "\"%s\"", c);
    }
    if (e.kind == Kind::Enum) {
        const char* n = bits < e.enum_count && e.enum_names != nullptr ? e.enum_names[bits] : "?";
        return std::snprintf(out, 48, "\"%s\"", n);
    }
    return format_bits(e.kind, bits, false, out);
}

int write_dump(const Registry& r, std::uint32_t frame, void (*emit)(void*, const char*), void* context) {
    int changed = 0, tunables = 0;
    for (int i = 0; i < r.tunable_count; ++i) {
        const Entry& e = r.entries[r.tunable_index[i]];
        if (e.kind == Kind::Action) continue;
        ++tunables;
        changed += differs(e) ? 1 : 0;
    }
    char text[96];
    std::snprintf(text, sizeof text, "dump: begin frame %lu (%d changed of %d)", static_cast<unsigned long>(frame),
                  changed, tunables);
    line(emit, context, text);
    for (int i = 0; i < r.tunable_count; ++i) { // names without a group first
        const Entry& e = r.entries[r.tunable_index[i]];
        if (differs(e) && group_length_of(e) == 0) entry_line(e, emit, context);
    }
    const char* last = nullptr;
    int last_length = 0;
    for (;;) { // then each group path in byte order, once
        const Entry* next = nullptr;
        int next_length = 0;
        for (int i = 0; i < r.tunable_count; ++i) {
            const Entry& e = r.entries[r.tunable_index[i]];
            const int g = group_length_of(e);
            if (g == 0 || !differs(e)) continue;
            if (last != nullptr && compare(e.name, g, last, last_length) <= 0) continue;
            if (next == nullptr || compare(e.name, g, next->name, next_length) < 0) {
                next = &e;
                next_length = g;
            }
        }
        if (next == nullptr) break;
        char table[40];
        for (int c = 0; c < next_length; ++c) table[c] = next->name[c] == '/' ? '.' : next->name[c];
        std::snprintf(text, sizeof text, "dump: [%.*s]", next_length, table);
        line(emit, context, text);
        for (int i = 0; i < r.tunable_count; ++i) {
            const Entry& e = r.entries[r.tunable_index[i]];
            if (differs(e) && group_length_of(e) == next_length && compare(e.name, next_length, next->name, next_length) == 0) {
                entry_line(e, emit, context);
            }
        }
        last = next->name;
        last_length = next_length;
    }
    line(emit, context, "dump: end");
    return changed;
}

} // namespace nnh::detail::live
