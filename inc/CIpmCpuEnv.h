#pragma once
/**
 * @file CIpmCpuEnv.h
 * @brief CPU feature probing (x86/x86_64 AVX2/AVX-512/AMX, ARM NEON/SVE/SVE2) and best-SIMD selection.
 *
 * Implementation notes (see .cpp):
 * - x86_64 uses CPUID and XGETBV to ensure OS state enables YMM/ZMM before reporting AVX/AVX-512.
 * - ARM64 on Linux reads HWCAP/HWCAP2 and PR_SVE_GET_VL to discover SVE vector length (bits).
 * - `Detect()` is cheap and intended to run once; call via @ref ipm::CIpmEnv::Initialize.
 */

#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
#ifdef CSH_IPM_EXPORT
#define IPM_API __declspec(dllexport)
#else
#define IPM_API __declspec(dllimport)
#endif
#else
#define IPM_API __attribute__((visibility("default")))
#endif

namespace ipm {

    /// @brief Coarse CPU family.
    enum class En_CpuType : int { x86 = 0, x86_64, ARM8, ARM9, Count };

    /// @brief SIMD kinds recognized by the library (generic categorization).
    enum class En_SimdKind : int {
        None = 0,
        // x86
        AVX2,        ///< 256-bit vectors
        AVX512F,     ///< 512-bit (foundation)
        AVX512BW,    ///< 512-bit byte/word extensions
        AMX_Tile,    ///< Matrix tile ISA (not a generic vector ISA)
        // ARM
        NEON,        ///< 128-bit SIMD
        SVE,         ///< Scalable Vector Extension (128–2048 bits)
        SVE2         ///< SVE2 (integer/bit ops enhanced)
    };

    /// @brief Coarse operation profiles used to pick a "best" SIMD.
    enum class En_OpProfile : int {
        Integer8_16,   ///< Pixel processing heavy in 8/16-bit integer math.
        Float32_64,    ///< Floating-point oriented workloads.
        Matrix2D       ///< Convolution/correlation/GEMM; AMX if available.
    };

    /**
     * @brief CPU capability probe (non-owning, POD-like; call @ref Detect once).
     *
     * The object caches the detected state; accessors are cheap and thread-safe
     * after @ref Detect completes.
     */
    class IPM_API CIpmCpuEnv {
    public:
        CIpmCpuEnv() = default;

        /** @brief Probe CPU family and SIMD features (idempotent per instance). */
        void Detect();

        // --- Basic identity/state ---
        En_CpuType  cpu() const { return cpu_; }

        // --- x86 feature flags ---
        bool hasAVX2()     const { return has_avx2_; }
        bool hasAVX512F()  const { return has_avx512f_; }
        bool hasAVX512BW() const { return has_avx512bw_; }
        bool hasAMX()      const { return has_amx_tile_; }

        // --- ARM feature flags ---
        bool hasNEON()     const { return has_neon_; }
        bool hasSVE()      const { return has_sve_; }
        bool hasSVE2()     const { return has_sve2_; }

        /** @brief Maximum generic SIMD vector width in bits (AMX excluded). */
        int  simdMaxBits() const { return simd_max_bits_; }

        /** @brief SVE vector length in bits (0 if unknown/not applicable). */
        int  sveVectorBits() const { return sve_vl_bits_; }

        /** @brief Best generic SIMD candidate independent of workload. */
        En_SimdKind bestSimdGeneric() const { return best_simd_generic_; }

        /**
         * @brief Choose best SIMD for a given workload profile.
         * @param prof Operation profile.
         * @return Preferred SIMD kind (or #En_SimdKind::None).
         */
        En_SimdKind bestSimdFor(En_OpProfile prof) const;

    private:
        void DetectCpuType_();
        void DetectSimd_x86_();
        void DetectSimd_arm_();

        // x86 helpers
        static void     cpuid_(int leaf, int subleaf, int regs[4]) noexcept;
        static uint64_t xgetbv_(uint32_t xcr) noexcept;

    private:
        // State
        En_CpuType  cpu_{ En_CpuType::x86 };
        int         simd_max_bits_{ 0 };
        En_SimdKind best_simd_generic_{ En_SimdKind::None };

        // x86 feature flags
        bool has_avx2_{ false };
        bool has_avx512f_{ false };
        bool has_avx512bw_{ false };
        bool has_amx_tile_{ false };

        // ARM feature flags
        bool has_neon_{ false };
        bool has_sve_{ false };
        bool has_sve2_{ false };
        int  sve_vl_bits_{ 0 };
    };

} // namespace ipm
