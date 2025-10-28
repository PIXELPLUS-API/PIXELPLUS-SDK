#pragma once
/**
 * @file CIpmGpuEnv.h
 * @brief GPU runtime/environment probe (OS adapters, CUDA/OpenCL presence, optional OpenGL probing) and selection.
 *
 * Platforms:
 * - Windows: DXGI enumeration, CUDA via nvcuda.dll, OpenCL via OpenCL.dll, optional WGL probe.
 * - Linux: DRM (/sys/class/drm) enumeration, CUDA via libcuda.so, OpenCL via libOpenCL.so,
 *          optional X11/GLX or EGL/GLES probe (controlled by macros).
 *
 * All public strings are UTF-8.
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

namespace ipm {

    /// @brief Coarse GPU type bucket.
    enum class En_GpuType : int { None = 0, Internal, nVidia };

    /// @brief Feature/runtime support state.
    enum class SupportState : int { Unknown = 0, Available, NotAvailable };

    /**
     * @brief One GPU record with runtime capability flags.
     *
     * Notes:
     * - `cudaVersion` is a driver version string (e.g., "Driver 12070") if CUDA Driver API is present.
     * - `openclVersion` is the platform version string for the enumerated ICD.
     * - `openglVersion` is set when the optional GL probe succeeds.
     */
    struct GpuInfo {
        int         id = -1;                  ///< Internal index within the enumerated list.
        std::string name;                     ///< Adapter/device name.
        std::string vendor;                   ///< "NVIDIA", "Intel", "AMD", or "Unknown".
        En_GpuType  type = En_GpuType::None;

        SupportState cudaState = SupportState::Unknown;
        SupportState openclState = SupportState::Unknown;
        SupportState openglState = SupportState::Unknown;

        std::string  cudaVersion;
        std::string  openclVersion;
        std::string  openglVersion;

        int          cudaDeviceIndex = -1;
        int          openclPlatformIndex = -1;
        int          openclDeviceIndex = -1;
    };

    /**
     * @brief GPU environment probe and selection (non-singleton).
     *
     * Call @ref Refresh to fully rescan the system and update states.
     * Selection APIs set a best-effort `selected_` index for convenience.
     */
    class IPM_API CIpmGpuEnv {
    public:
        CIpmGpuEnv() = default;

        /** @brief Full refresh: enumerate OS adapters, guess active display GPU, probe GL/CUDA/OpenCL. */
        void        Refresh();

        // List / selection
        size_t      getGpuCount() const;
        GpuInfo     getGpu(size_t idx) const;
        int         getSelectedIndex() const;
        GpuInfo     getSelected() const;

        /**
         * @brief Select the first GPU whose name or vendor contains the substring (case-insensitive).
         * @param substr      Key to search in name or vendor.
         * @param preferCUDA  If true, prefer a CUDA-capable candidate when multiple match.
         * @return true if a selection was made.
         */
        bool        selectByNameSubstring(const std::string& substr, bool preferCUDA = true);

        /// @brief Select by CUDA device ordinal (as discovered during probing).
        bool        selectByCudaIndex(int cudaIndex);

        /// @brief Select by OpenCL (platformIndex, deviceIndex) pair.
        bool        selectByOpenCL(int platformIndex, int deviceIndex);

        /// @brief Clear selection to "none".
        void        clearSelection();

        /// @brief Override OpenGL version string for the selected GPU (when you probe in your own GL context).
        void        setSelectedOpenGLVersion(const std::string& glVersion);
        /// @brief Get OpenGL version string for the selected GPU (empty if unknown).
        std::string getSelectedOpenGLVersion() const;

        /// @brief Summaries for the selected GPU (Unknown if none).
        SupportState selectedCudaState() const;
        SupportState selectedOpenCLState() const;
        SupportState selectedOpenGLState() const;

    private:
        // Internal steps
        void enumerateGpusOS();
        void selectOsActiveDisplayGpu();  // best-effort on Windows (DXGI + QueryDisplayConfig)
        void probeOpenGLRuntime();        // optional by macro (CSH_IPM_ENABLE_GL_PROBE)
        void probeCudaRuntime();          // CUDA Driver API (no CUDA SDK dependency)
        void probeOpenCLRuntime();        // OpenCL ICD

        // List helpers
        void clearGpuListUnsafe();
        int  addOrMergeByKey(const std::string& nameU8,
            const std::string& vendorU8,
            En_GpuType type);

    private:
        mutable std::mutex  mtx_;
        std::vector<GpuInfo> gpus_;
        int                  selected_{ -1 };
    };

} // namespace ipm
