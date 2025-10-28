#pragma once
/**
 * @file CIpmUserCustomLoader.h
 * @brief Loader for User_Custom plug-ins and exposure of their registered algorithms.
 *
 * ABI (expected exported symbols in the plug-in shared library):
 * - `int ipm_user_custom_register(const AlgEntry** out, int* cnt);`
 *     - On success, returns 0 and sets (*out, *cnt) to a contiguous array of AlgEntry.
 * - `void ipm_user_custom_unregister();`
 *
 * Search locations:
 * - Windows: executable directory and /plugins subdir (see .cpp).
 * - Linux:   `<exe>/../lib`, `<exe>`, `<exe>/plugins`, and dynamic loader fallback.
 *
 * Threading:
 * - `loadOnce()` is guarded by std::once_flag and is idempotent per process.
 * - `unload()` is safe to call during shutdown; it frees the library and clears entries.
 */

#include <vector>
#include <string>
#include <mutex>
#include "IpmTypes.h"

 // Internal implementation namespace
namespace ipm_internal {

    /**
     * @brief Singleton user plug-in loader for the User_Custom module.
     *
     * After a successful load, call @ref entries to retrieve the list of plug-in algorithms.
     * The function table merges these entries into its own registry.
     *
     * @see ipmcommon::CIpmFuncTable
     */
    class UserCustomLoader {
    public:
        /// @brief Get the singleton instance.
        static UserCustomLoader& instance();

        /**
         * @brief Attempt to load and register plug-ins exactly once.
         * @return The number of entries registered (>= 0).
         *
         * On success, the internal @ref entries_ vector becomes populated and remains valid until @ref unload.
         */
        int loadOnce();

        /// @brief Access registered entries (valid after successful @ref loadOnce).
        const std::vector<AlgEntry>& entries() const { return entries_; }

        /// @brief Unregister and unload the plug-in module (safe during process teardown).
        void unload();

    private:
        UserCustomLoader() = default;
        ~UserCustomLoader();

        // Platform library handle
        void* hmod_ = nullptr;

        // Plug-in ABI types
        using RegisterFn = int  (*)(const AlgEntry** out, int* cnt);
        using UnregisterFn = void (*)();

        RegisterFn   reg_ = nullptr;
        UnregisterFn unreg_ = nullptr;

        std::vector<AlgEntry> entries_;
        std::once_flag once_;
        std::mutex mtx_;

        /// @brief Try a list of candidate library paths; return true on the first successful open+symbol resolution.
        bool tryOpen(const std::vector<std::string>& names);
    };

} // namespace ipm_internal
