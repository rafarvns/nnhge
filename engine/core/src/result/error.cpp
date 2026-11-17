// nnh::Error helpers (nnh/result.hpp).

#include <cstdarg>
#include <cstdio>

#include "nnh/result.hpp"

namespace nnh {

const char* error_code_name(ErrorCode code) {
    switch (code) {
    case ErrorCode::None: return "None";
    case ErrorCode::InvalidArgument: return "InvalidArgument";
    case ErrorCode::NotFound: return "NotFound";
    case ErrorCode::IoError: return "IoError";
    case ErrorCode::Corrupted: return "Corrupted";
    case ErrorCode::OutOfMemory: return "OutOfMemory";
    case ErrorCode::LimitReached: return "LimitReached";
    case ErrorCode::Unsupported: return "Unsupported";
    case ErrorCode::Unknown: break;
    }
    return "Unknown";
}

Error Error::make(ErrorCode code, const char* format, ...) {
    Error error;
    error.code = code;
    std::va_list args;
    va_start(args, format);
    std::vsnprintf(error.message, sizeof error.message, format, args);
    va_end(args);
    return error;
}

} // namespace nnh
