#pragma once
// Typed reads of the members of one JSON object with readable refusals: `layer "ground": "opacity" must be a number
// from 0 to 1`. The first problem is kept; later reads return their fallback, so a reader can read every field and
// check ok() once.

#include <cstdint>
#include <string>

#include "json.hpp"

namespace nnh::assetc {

class Fields {
public:
    /// `where` names the object in messages ("" for the file's top level).
    Fields(json::Ref object, std::string where) : object_(object), where_(std::move(where)) {}

    bool ok() const { return error_.empty(); }
    const std::string& error() const { return error_; }
    /// Records `reason` (prefixed with `where`) unless a problem is already recorded. Returns false.
    bool refuse(const std::string& reason);

    bool has(const char* key) const { return static_cast<bool>(object_.get(key)); }
    /// A whole number in [min, max]; `fallback` when absent.
    std::int64_t integer(const char* key, std::int64_t fallback, std::int64_t min, std::int64_t max);
    /// Like integer(), but the member must be there.
    std::int64_t required_integer(const char* key, std::int64_t min, std::int64_t max);
    /// A number in [min, max]; `fallback` when absent.
    double number(const char* key, double fallback, double min, double max);
    bool boolean(const char* key, bool fallback);
    std::string string(const char* key, const char* fallback = "");
    std::string required_string(const char* key);
    /// The member if it is an array; missing (an empty Ref) when absent; a refusal when of another type.
    json::Ref array(const char* key);

private:
    json::Ref object_;
    std::string where_;
    std::string error_;
};

} // namespace nnh::assetc
