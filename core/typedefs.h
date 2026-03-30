#pragma once

#include "core/error/error_list.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

static_assert(__cplusplus >= 201703L, "C++17 required.");

#ifndef REAL_T_IS_DOUBLE
using real_t = float;
#else
using real_t = double;
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define FORCE_INLINE __attribute__((always_inline)) inline
    #define NO_INLINE    __attribute__((noinline))
    #define LIKELY(x)    __builtin_expect(!!(x), 1)
    #define UNLIKELY(x)  __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
    #define FORCE_INLINE __forceinline
    #define NO_INLINE    __declspec(noinline)
    #define LIKELY(x)    (x)
    #define UNLIKELY(x)  (x)
#else
    #define FORCE_INLINE inline
    #define NO_INLINE
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

#undef MIN
#undef MAX
#undef CLAMP

template <typename T, typename U>
[[nodiscard]] constexpr auto
MIN(T a, U b) noexcept
{
    return a < b ? a : b;
}

template <typename T, typename U>
[[nodiscard]] constexpr auto
MAX(T a, U b) noexcept
{
    return a > b ? a : b;
}

template <typename T>
[[nodiscard]] constexpr T
CLAMP(T v, T lo, T hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}

#ifndef SWAP
    #define SWAP(a, b) std::swap((a), (b))
#endif

// Bit flag helper
#define BIT(n) (1u << (n))

// Array size without requiring <array>
template <typename T, std::size_t N>
constexpr std::size_t
std_size(const T (&)[N]) noexcept
{
    return N;
}
