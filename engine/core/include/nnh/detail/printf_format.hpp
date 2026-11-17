#pragma once
/// @file nnh/detail/printf_format.hpp
/// NNH_PRINTF_FORMAT(fmt, args): compile-time checking of printf-style arguments (GCC/Clang).
/// MinGW uses the gnu_printf archetype so C99 formats like %zu and %lld are understood.

#if defined(__MINGW32__)
#define NNH_PRINTF_FORMAT(fmt_index, args_index) __attribute__((format(gnu_printf, fmt_index, args_index)))
#elif defined(__GNUC__)
#define NNH_PRINTF_FORMAT(fmt_index, args_index) __attribute__((format(printf, fmt_index, args_index)))
#else
#define NNH_PRINTF_FORMAT(fmt_index, args_index)
#endif
