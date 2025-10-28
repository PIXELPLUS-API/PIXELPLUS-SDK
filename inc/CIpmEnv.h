#pragma once
/**
 * @file CIpmEnv.h
 * @brief Orchestrator singleton wrapping CPU and GPU environment probes and selection.
 *
 * Responsibilities:
 * - One-time initialization of @ref CIpmCpuEnv and @ref CIpmGpuEnv.
 * - GPU device enumeration and selection helpers (by name/CUDA/OpenCL).
 * - Logging of the detected environment (see implementation for formatted summary).
 *
 * Threading:
 * - @ref Instance returns a process-wide singleton and ensures @ref Initialize is called once.
 * - All getters are thread-safe after initialization completes.
 */

#include <string>
#include <vector>
#include <mutex>
#include <atomic>

#if defined(_WIN32) || defined(_WIN64)
#ifdef CSH_IPM_EXPORT
#define IPM_API __declspec(dllexport)
#else
#define IPM_API __declspec(dllimport)
#endif
#else
#define IPM_API __attribute__((visibility("default")))
#endif

#include "CIpmCpuEnv.h"
#include "CIpmGpuEnv.h"

namespace ipm {

    /**
     * @brief Process-wide IPM environment (CPU + GPU).
     */
    class IPM_API CIpmEnv {
    public:
        /**
         * @brief Get the singleton instance (calls @ref Initialize once on first use).
         */
        static CIpmEnv& Instance();

        /** @brief Initialize CPU/GPU probes and write diagnostic summary (idempotent). */
        void        Initialize();

        /** @brief Refresh GPU list and runtime states (CPU rarely changes; not re-probed by default). */
        void        Refresh();

        // --- CPU facade ---
        En_CpuType  cpuType() const noexcept { return cpu_.cpu(); }

        // --- GPU facade (public members for convenience) ---
        CIpmCpuEnv  cpu_;
        CIpmGpuEnv  gpu_;

        size_t      getGpuCount()       const { return gpu_.getGpuCount(); }
        GpuInfo     getGpu(size_t idx)  const { return gpu_.getGpu(idx); }
        int         getSelectedIndex()  const { return gpu_.getSelectedIndex(); }
        GpuInfo     getSelected()       const { return gpu_.getSelected(); }

        bool        selectByNameSubstring(const std::string& s, bool preferCUDA = true) { return gpu_.selectByNameSubstring(s, preferCUDA); }
        bool        selectByCudaIndex(int i) { return gpu_.selectByCudaIndex(i); }
        bool        selectByOpenCL(int p, int d) { return gpu_.selectByOpenCL(p, d); }
        void        clearSelection() { gpu_.clearSelection(); }

        std::string getSelectedOpenGLVersion() const { return gpu_.getSelectedOpenGLVersion(); }

        SupportState selectedCudaState()   const { return gpu_.selectedCudaState(); }
        SupportState selectedOpenCLState() const { return gpu_.selectedOpenCLState(); }
        SupportState selectedOpenGLState() const { return gpu_.selectedOpenGLState(); }

    private:
        CIpmEnv() = default;

        /// @brief (Internal) Set OpenGL version string for the selected GPU (when probing externally).
        void        setSelectedOpenGLVersion(const std::string& v) { gpu_.setSelectedOpenGLVersion(v); }

        /// @brief Log a one-shot environment summary (see .cpp; uses IpmStringUtils for UTF).
        void        writeCpuGpuStatus();

    private:
        std::atomic<bool> initialized_{ false };
    };

} // namespace ipm
