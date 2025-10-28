#pragma once
/**
 * @file IpmTypes.h
 * @brief Core IPM type aliases, enums, and small PODs used across the image processing modules.
 *
 * This header centralizes:
 * - The canonical function signature for all processing algorithms (#IpmFn).
 * - Frontend/UI enumerations for backends and modules (#ipmcommon::EnProcessBackend, #ipmcommon::EnIpmModule).
 * - Canonical status codes (#IpmStatus) returned by algorithms and dispatchers.
 * - Lightweight structures that describe registered functions and catalog entries (#FuncInfo, #AlgEntry).
 *
 * @see CIpmFuncTable.h  For the registry that uses these types.
 * @see CImageProcessMng.h For the pipeline manager that consumes registered functions.
 */

#include <functional>
#include <string>
#include <CSH_Image.h>

using namespace csh_img;

/**
 * @brief Canonical function signature for all processing algorithms.
 *
 * The function must return an #IpmStatus cast to int (see @ref IpmStatus).
 * Ownership:
 * - `in` is a borrowed pointer (may be null for source-less stages; most algorithms require it).
 * - `out` must be a valid, writable image (caller typically allocates; algorithms may reallocate if needed).
 * - `p1`, `p2` are opaque user parameters (algorithm-specific).
 *
 * @param in   Input image (may be nullptr only for specific source operators).
 * @param out  Output image (must not be nullptr).
 * @param p1   Optional parameter block (algorithm-specific).
 * @param p2   Optional parameter block (algorithm-specific).
 * @return int Status code compatible with #IpmStatus.
 */
using IpmFn = std::function<int(const csh_img::CSH_Image*, csh_img::CSH_Image*, void*, void*)>;

namespace ipmcommon {

    /**
     * @brief Compute backend options (UI first list).
     *
     * These enumerators segment the same algorithm catalog by execution target.
     * The function table stores separate maps per backend.
     *
     * @see CIpmFuncTable
     */
    enum class EnProcessBackend : int {
        CPU_Serial = 0,   ///< Single-threaded CPU path.
        CPU_Parallel,     ///< Multi-threaded CPU path (TBB/OpenMP/tasking; TBD).
        GPU_GL_Compute,   ///< GPU via OpenGL Compute or GLES compute (if enabled).
        GPU_OpenCL,       ///< GPU via OpenCL (if available).
        GPU_CUDA,         ///< GPU via CUDA (if driver/runtime available).
        Count
    };

    /**
     * @brief High-level module groups (UI second list).
     *
     * Modules group related algorithms. Registration and lookup use (backend,module,algIndex).
     * @note `User_Custom` must remain the last regular module so plug-ins append cleanly.
     */
    enum class EnIpmModule : int {
        Converter = 0,    ///< Color space / pixel format converters.
        Scaler,           ///< Resamplers / scalers.
        Splitter,         ///< Stream/image split utilities.

        User_Custom,      ///< User plug-in module bucket (always last before Count).
        Count
    };

} // namespace ipmcommon

/**
 * @brief IPM status / error codes.
 *
 * Returned by algorithm functions and by #CIpmFuncTable::process.
 */
enum class IpmStatus : int {
    NotAvailable = 0,      ///< Feature/backend not available on this system.
    OK,                    ///< Success.
    Err_InvalidBackend,    ///< Backend out of range / not registered.
    Err_InvalidModule,     ///< Module out of range / not registered.
    Err_AlgNotFound,       ///< Algorithm index not found for (backend,module).
    Err_InvalidSize,       ///< Unsupported or mismatched size.
    Err_InvalidFormat,     ///< Unsupported or mismatched pixel format.
    Err_NullFunction,      ///< Function pointer not set.
    Err_NullImage,         ///< Null `in` or `out` where required.
    Err_Internal,          ///< Exception or internal fault.
    IsDevelopping          ///< Placeholder status for WIP paths.
};

/**
 * @brief Metadata for a registered algorithm.
 *
 * @see CIpmFuncTable
 */
struct FuncInfo {
    IpmFn       fn;       ///< Callable entry point (may be empty prior to registration).
    std::wstring uiName;  ///< Display name for UI lists (UTF-16).
};

/**
 * @brief Entry stored in a module catalog.
 *
 * Combines an integer algorithm ID (stable within module/backend) and #FuncInfo.
 */
struct AlgEntry {
    int       alg;    ///< Algorithm index/key within the module.
    FuncInfo  func;   ///< Function + localized UI name.
};
