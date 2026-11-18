# result — recoverable errors

Header: `<nnh/result.hpp>` · Design: [architecture/errors.md](../architecture/errors.md)

Functions that can fail for reasons that aren't bugs (missing file, corrupted save, full slot) return
`nnh::Result<T>`: either a value or an `nnh::Error`. No exceptions.

```cpp
nnh::Result<int> parse_level(const char* text) {
    if (text == nullptr) return nnh::Error::make(nnh::ErrorCode::InvalidArgument, "no text");
    return std::atoi(text);
}

auto level = parse_level(arg);
if (!level) {
    NNH_LOG_WARN(game, "bad level (%s): %s", nnh::error_code_name(level.error().code), level.error().message);
    return;
}
start(level.value());
```

| Member | |
|--------|-|
| `ok()`, `explicit operator bool` | success? |
| `value()` | the value; calling it on a failed result is a bug (`NNH_VERIFY`) |
| `value_or(fallback)` | the value, or `fallback` on failure |
| `error()` | the `Error`; calling it on success is a bug |
| `Result<void>` | for operations without a value: `return {};` is success |

`Error` = `ErrorCode code` + `char message[64]` (formatted with `Error::make(code, fmt, ...)`, truncated,
no heap). Codes: `InvalidArgument`, `NotFound`, `IoError`, `Corrupted`, `OutOfMemory`, `LimitReached`,
`Unsupported`, `Unknown`; `error_code_name(code)` gives the name.

`Result` is `[[nodiscard]]`: ignoring a returned result is a compiler warning.

`T` may be move-only (e.g. [`nnh::Texture`](texture.md)): take the value out with
`std::move(result.value())`; `value_or` needs a copyable `T`.
