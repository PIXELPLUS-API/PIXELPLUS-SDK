#pragma once
/**
 * @file IpmClamp.h
 * @brief Tiny header-only clamping and saturating-cast utilities optimized for image pipelines.
 *
 * Key points:
 * - `constexpr` + always-inline to encourage full inlining in hot loops.
 * - Branch-minimized fixed-range helpers for 8/10/12/16b common pixel depths.
 * - Type-safe `saturated_cast<D,S>()` that gracefully clamps across signed/unsigned and float/int.
 */

#include <cstdint>
#include <type_traits>
#include <limits>

#if defined(_MSC_VER)
#define IPM_FORCE_INLINE __forceinline
#else
#define IPM_FORCE_INLINE inline __attribute__((always_inline))
#endif

namespace ipm {
    namespace util {

        /**
         * @brief Generic clamp to [lo, hi].
         * @tparam T Arithmetic type (integral or floating).
         * @param v  Value to clamp.
         * @param lo Lower bound (inclusive).
         * @param hi Upper bound (inclusive).
         * @return Clamped value.
         * @warning Assumes `lo <= hi`.
         */
        template <typename T>
        IPM_FORCE_INLINE constexpr T clamp(const T& v, const T& lo, const T& hi) noexcept {
            return (v < lo) ? lo : ((v > hi) ? hi : v);
        }

        /// @name Fixed-range fast clamp helpers
        ///@{

        /// @brief Clamp to [0,255], returning uint8_t (branch-minimized).
        IPM_FORCE_INLINE constexpr std::uint8_t clamp_u8(std::int32_t v) noexcept {
            return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
        }

        /// @brief Clamp to [0,65535], returning uint16_t (branch-minimized).
        IPM_FORCE_INLINE constexpr std::uint16_t clamp_u16(std::int32_t v) noexcept {
            return static_cast<std::uint16_t>(v < 0 ? 0 : (v > 65535 ? 65535 : v));
        }

        /// @brief Clamp to [0,1023] (10-bit).
        IPM_FORCE_INLINE constexpr std::uint16_t clamp_u10(std::int32_t v) noexcept {
            return static_cast<std::uint16_t>(v < 0 ? 0 : (v > 1023 ? 1023 : v));
        }

        /// @brief Clamp to [0,4095] (12-bit).
        IPM_FORCE_INLINE constexpr std::uint16_t clamp_u12(std::int32_t v) noexcept {
            return static_cast<std::uint16_t>(v < 0 ? 0 : (v > 4095 ? 4095 : v));
        }
        ///@}

        /**
         * @brief Saturating cast from `Src` to `Dst` with bounds checking against `std::numeric_limits<Dst>`.
         *
         * Useful when converting intermediate accumulators to pixel types
         * (e.g., `int -> uint8_t` or `float -> uint16_t`) without UB.
         */
        template <typename Dst, typename Src>
        IPM_FORCE_INLINE constexpr Dst saturated_cast(Src v) noexcept {
            static_assert(std::is_arithmetic<Src>::value, "Src must be arithmetic");
            static_assert(std::is_arithmetic<Dst>::value, "Dst must be arithmetic");

            constexpr long double Dmin = static_cast<long double>(std::numeric_limits<Dst>::lowest());
            constexpr long double Dmax = static_cast<long double>(std::numeric_limits<Dst>::max());
            const long double lv = static_cast<long double>(v);

            if (lv < Dmin) return static_cast<Dst>(Dmin);
            if (lv > Dmax) return static_cast<Dst>(Dmax);
            return static_cast<Dst>(lv);
        }

        /// @name Convenience wrappers
        ///@{
        IPM_FORCE_INLINE constexpr std::uint8_t  sat_u8(int v)  noexcept { return saturated_cast<std::uint8_t>(v); }
        IPM_FORCE_INLINE constexpr std::uint16_t sat_u16(int v) noexcept { return saturated_cast<std::uint16_t>(v); }
        IPM_FORCE_INLINE constexpr std::int16_t  sat_i16(int v) noexcept { return saturated_cast<std::int16_t>(v); }
        ///@}

    } // namespace util
} // namespace ipm
