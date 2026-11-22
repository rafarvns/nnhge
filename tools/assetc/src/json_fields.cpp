// Typed reads of JSON object members with readable refusals (see json_fields.hpp).

#include "json_fields.hpp"

#include <cmath>
#include <cstdio>

namespace nnh::assetc {

namespace {

std::string quoted(const char* key) { return std::string("\"") + key + "\""; }

/// A double printed without a useless ".000000" (bounds in messages are whole or simple).
std::string show(double v) {
    char text[32];
    std::snprintf(text, sizeof text, "%g", v);
    return text;
}

} // namespace

bool Fields::refuse(const std::string& reason) {
    if (error_.empty()) error_ = where_.empty() ? reason : where_ + ": " + reason;
    return false;
}

std::int64_t Fields::integer(const char* key, std::int64_t fallback, std::int64_t min, std::int64_t max) {
    const json::Ref v = object_.get(key);
    if (!v) return fallback;
    const double n = v.number();
    if (!v.is_number() || std::floor(n) != n || n < static_cast<double>(min) || n > static_cast<double>(max)) {
        refuse(quoted(key) + " must be a whole number from " + std::to_string(min) + " to " + std::to_string(max));
        return fallback;
    }
    return static_cast<std::int64_t>(n);
}

std::int64_t Fields::required_integer(const char* key, std::int64_t min, std::int64_t max) {
    if (!has(key)) {
        refuse(quoted(key) + " is missing");
        return 0;
    }
    return integer(key, 0, min, max);
}

double Fields::number(const char* key, double fallback, double min, double max) {
    const json::Ref v = object_.get(key);
    if (!v) return fallback;
    if (!v.is_number() || v.number() < min || v.number() > max) {
        refuse(quoted(key) + " must be a number from " + show(min) + " to " + show(max));
        return fallback;
    }
    return v.number();
}

bool Fields::boolean(const char* key, bool fallback) {
    const json::Ref v = object_.get(key);
    if (!v) return fallback;
    if (!v.is_bool()) {
        refuse(quoted(key) + " must be true or false");
        return fallback;
    }
    return v.boolean();
}

std::string Fields::string(const char* key, const char* fallback) {
    const json::Ref v = object_.get(key);
    if (!v) return fallback;
    if (!v.is_string()) {
        refuse(quoted(key) + " must be a string");
        return fallback;
    }
    return std::string(v.string());
}

std::string Fields::required_string(const char* key) {
    if (!has(key)) {
        refuse(quoted(key) + " is missing");
        return "";
    }
    return string(key);
}

json::Ref Fields::array(const char* key) {
    const json::Ref v = object_.get(key);
    if (v && !v.is_array()) {
        refuse(quoted(key) + " must be an array");
        return {};
    }
    return v;
}

} // namespace nnh::assetc
