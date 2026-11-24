// Field paths as text (FieldPath::format) and the load report's warning line (spec 027 FR-15).

#include "saves/save_internal.hpp"

#include <cstdarg>
#include <cstdio>

namespace nnh {
NNH_LOG_CHANNEL(saves);
} // namespace nnh

namespace nnh::save {

int FieldPath::format(char* out, std::size_t capacity) const {
    if (capacity == 0) return 0;
    std::size_t n = 0;
    out[0] = '\0';
    for (int i = 0; i < count_; ++i) {
        char step[12];
        const bool item = is_item(i);
        const int length = std::snprintf(step, sizeof step, item ? "[%u]" : (i > 0 ? ".%u" : "%u"),
                                         static_cast<unsigned>(steps_[i]));
        for (int k = 0; k < length && n + 1 < capacity; ++k)
            out[n++] = step[k];
        out[n] = '\0';
    }
    return static_cast<int>(n);
}

} // namespace nnh::save

namespace nnh::detail {
namespace {

/// Appends to a fixed line, cutting at its end.
struct Line {
    char text[log::kMaxLineLength] = {};
    std::size_t length = 0;

    void add(const char* format, ...) NNH_PRINTF_FORMAT(2, 3) {
        if (length + 1 >= sizeof text) return;
        va_list args;
        va_start(args, format);
        const int n = std::vsnprintf(text + length, sizeof text - length, format, args);
        va_end(args);
        if (n > 0) {
            const std::size_t room = sizeof text - 1 - length;
            length += static_cast<std::size_t>(n) < room ? static_cast<std::size_t>(n) : room;
        }
    }
    /// "; dropped 2 (8.2, 9[3].10)": the count, then the paths kept (Debug/Development).
    void count(const char* what, int n, const save::FieldPath* paths) {
        if (n == 0) return;
        add("; %s %d", what, n);
        const int kept = n < save::kReportPaths ? n : save::kReportPaths;
        for (int i = 0; i < kept && paths[i].size() > 0; ++i) {
            char where[48];
            paths[i].format(where, sizeof where);
            add("%s%s", i == 0 ? " (" : ", ", where);
            if (i + 1 == kept || paths[i + 1].size() == 0) add(")");
        }
    }
};

} // namespace

void save_log_report(const char* path, const SaveTypeInfo& type, const save::Report& report,
                     const char* backup_reason) {
    if (!report.used_backup && report.dropped == 0 && report.adjusted == 0) return;
    Line line;
    line.add("save %s:", path);
    if (report.used_backup)
        line.add(" backup used (%s)", backup_reason != nullptr ? backup_reason : "");
    else
        line.add(" loaded");
    if (report.migrated > 0) {
        line.add("; v%lu -> v%lu (%d steps)", static_cast<unsigned long>(report.version),
                 static_cast<unsigned long>(type.version), report.migrated);
    }
    line.count("unknown", report.unknown, report.unknown_at);
    line.count("dropped", report.dropped, report.dropped_at);
    line.count("adjusted", report.adjusted, report.adjusted_at);
    NNH_LOG_WARN(saves, "%s", line.text);
}

} // namespace nnh::detail
