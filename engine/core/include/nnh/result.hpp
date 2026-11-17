#pragma once
/// @file nnh/result.hpp
/// Recoverable errors without exceptions: nnh::Result<T> holds either a value or an nnh::Error.
/// See docs/api/result.md and docs/architecture/errors.md.
///
/// @code
/// nnh::Result<SaveData> load_save();
/// auto save = load_save();
/// if (!save) {
///     NNH_LOG_WARN(game, "no save: %s", save.error().message);
///     return new_game();
/// }
/// use(save.value());
/// @endcode

#include <cstdint>
#include <new>
#include <utility>

#include "nnh/assert.hpp"

namespace nnh {

enum class ErrorCode : std::uint8_t {
    None,
    InvalidArgument, ///< the caller passed something unusable (bad path, size 0...)
    NotFound,        ///< file, slot or resource doesn't exist
    IoError,         ///< read/write failed at the system level
    Corrupted,       ///< data exists but fails validation (checksum, format)
    OutOfMemory,     ///< allocation or budget exceeded
    LimitReached,    ///< a fixed capacity is full
    Unsupported,     ///< not available on this system or build
    Unknown,
};

/// Name of an error code, e.g. "Corrupted".
const char* error_code_name(ErrorCode code);

/// An error: a code plus a short message (copied, truncated to 63 characters; no heap).
struct Error {
    ErrorCode code = ErrorCode::Unknown;
    char message[64] = {};

    static Error make(ErrorCode code, const char* format, ...)
#if defined(__GNUC__)
        __attribute__((format(printf, 2, 3)))
#endif
        ;
};

/// Either a T or an Error. Check it (`if (r)`, `r.ok()`) before calling value(): reading the value of
/// a failed result is a bug and triggers NNH_VERIFY.
template <typename T>
class [[nodiscard]] Result {
public:
    Result(T value) : ok_(true) { new (&value_) T(std::move(value)); }
    Result(const Error& error) : ok_(false), error_(error) {}
    Result(const Result& other) : ok_(other.ok_) { copy_from(other); }
    Result& operator=(const Result& other) {
        if (this != &other) {
            destroy();
            ok_ = other.ok_;
            copy_from(other);
        }
        return *this;
    }
    ~Result() { destroy(); }

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }

    T& value() {
        NNH_VERIFY(ok_, "value() of a failed Result: %s", error_.message);
        return value_;
    }
    const T& value() const {
        NNH_VERIFY(ok_, "value() of a failed Result: %s", error_.message);
        return value_;
    }
    T value_or(T fallback) const { return ok_ ? value_ : fallback; }

    const Error& error() const {
        NNH_VERIFY(!ok_, "error() of a successful Result");
        return error_;
    }

private:
    void destroy() {
        if (ok_) value_.~T();
    }
    void copy_from(const Result& other) {
        if (ok_) {
            new (&value_) T(other.value_);
        } else {
            error_ = other.error_;
        }
    }

    bool ok_;
    union {
        T value_;
        Error error_;
    };
};

/// Result of an operation that returns nothing but can fail. `return {};` means success.
template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(const Error& error) : ok_(false), error_(error) {}

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }
    const Error& error() const {
        NNH_VERIFY(!ok_, "error() of a successful Result");
        return error_;
    }

private:
    bool ok_ = true;
    Error error_{ErrorCode::None, {}};
};

} // namespace nnh
