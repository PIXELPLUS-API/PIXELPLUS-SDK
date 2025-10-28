#pragma once
/**
 * @file CIpmFuncTable.h
 * @brief Lazy-initialized registry mapping (backend,module,algIndex) -> callable #IpmFn + UI name.
 *
 * Design:
 * - Singleton accessed via @ref CIpmFuncTable::Instance() (thread-safe lazy init using std::once_flag).
 * - 3-level container: `funcTable_[backend][module]` is an `unordered_map<algIndex, FuncInfo>`.
 * - Built-ins:
 *    - Converter algorithms from @ref CConverter are registered for CPU_Serial in InitConverterFuncTable().
 *    - User plug-ins discovered/loaded via @ref ipm_internal::UserCustomLoader into `User_Custom`.
 *
 * @see CIpmUserCustomLoader.h  for the plug-in ABI and search logic.
 * @see IpmTypes.h              for #EnProcessBackend, #EnIpmModule, #IpmStatus, #FuncInfo.
 */

#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <mutex>
#include <cstdint>
#include <atomic>

#include "IpmTypes.h"

namespace ipmcommon {

    /**
     * @brief Function Table Registry (singleton).
     *
     * Usage:
     * - Call @ref Instance() anywhere; the first call triggers InitFuncTable() once.
     * - @ref process dispatches to the registered function and returns an #IpmStatus.
     * - @ref getAlgorithmList exposes (algIndex, uiName) for UI population.
     *
     * Threading:
     * - Registration is protected by a mutex.
     * - Lazy initialization is guarded by `std::once_flag`.
     * - Read access is lock-free after initialization.
     */
    class CIpmFuncTable final {
    public:
        /** @brief Get the singleton (initializes on first use). */
        static CIpmFuncTable& Instance();

        /**
         * @brief Dispatch a processing call to the registered function.
         *
         * @param backend   Execution backend.
         * @param module    Module (Converter/Scaler/Splitter/User_Custom).
         * @param algIndex  Algorithm index/key within the module.
         * @param in        Input image (nullable for algorithms that don't need it).
         * @param out       Output image (must not be null).
         * @param param1    Opaque parameter 1.
         * @param param2    Opaque parameter 2.
         * @return          #IpmStatus result of the function call (or an error mapped by the table).
         */
        IpmStatus process(ipmcommon::EnProcessBackend backend,
            ipmcommon::EnIpmModule module,
            int algIndex,
            const csh_img::CSH_Image* in,
            csh_img::CSH_Image* out,
            void* param1,
            void* param2) const;

        /**
         * @brief Enumerate algorithms for (backend,module) for UI population.
         * @return Vector of (algIndex, uiName).
         */
        std::vector<std::pair<int, std::wstring>>
            getAlgorithmList(ipmcommon::EnProcessBackend backend,
                ipmcommon::EnIpmModule module) const;

        /// @brief Localized backend names in enum order (for UI).
        static const std::vector<std::wstring>& getBackendNames();
        /// @brief Localized module names in enum order (for UI).
        static const std::vector<std::wstring>& getModuleNames();

        /// @brief Parse backend name into enum (exact match).
        static bool TryParseBackend(const std::wstring& name, ipmcommon::EnProcessBackend& out);
        /// @brief Parse module name into enum (exact match).
        static bool TryParseModule(const std::wstring& name, ipmcommon::EnIpmModule& out);

    private:
        CIpmFuncTable();  // internal construction only

        // Helpers
        static bool isValidBackendIndex_(int b);
        static bool isValidModuleIndex_(int m);

        /// @brief Ensure table is initialized (idempotent; safe from any const method).
        void ensureInitialized_() const;

        /// @brief Register a function into the catalog (thread-safe).
        IpmStatus registerFunc(ipmcommon::EnProcessBackend backend,
            ipmcommon::EnIpmModule module,
            int algIndex,
            IpmFn fn,
            std::wstring uiName);

        // Initialization steps (called once)
        void InitFuncTable();
        void InitConverterFuncTable();
        void InitScalerFuncTable();
        void InitUserCustomFuncTable();   ///< Load and merge User_Custom plug-ins.

    private:
        /// @brief 3D table: [backend][module] -> map(algIndex -> FuncInfo).
        std::vector<std::vector<std::unordered_map<int, FuncInfo>>> funcTable_;

        mutable std::mutex     m_regMtx_;
        mutable std::once_flag init_once_;
        mutable std::atomic<bool> initialized_{ false };
    };
} // namespace ipmcommon

/**
 * @brief Optional developer hook to add temporary registrations during development.
 * @note No-op by default.
 */
void CIpmFuncTable_RegisterDummyForDev();
