/**
* @file CSH_Log.h
* @brief Simple thread-safe logging class with printf-style and complete message logging.
*
* This header defines the CSH_Log class, which provides a singleton logger
* with configurable log levels, file output, and thread safety.
*
*/

#pragma once
// C++17, Unicode (wchar_t) interface, DLL export macro

#include <string>
#include <mutex>
#include <atomic>
#include <cstdarg>
#include <filesystem>

#if defined(_WIN32)
#if defined(CSH_LOG_EXPORT)
#define CSHLOG_API __declspec(dllexport)
#else
#define CSHLOG_API __declspec(dllimport)
#endif
#else
#define CSHLOG_API __attribute__((visibility("default")))
#endif

namespace cshlog {

    /**
     * @enum LogLevel
     * @brief Log severity levels from most severe to most verbose.
     *
     * Lower numeric values indicate higher severity. The value @ref LogLevel::Count
     * is not a real severity; it is a sentinel that indicates the number of levels.
     */
    enum class LogLevel : int {
        Fatal = 0,  /**< Fatal error that prevents the program from continuing. */
        Error,      /**< Recoverable error: operation failed but process continues. */
        Warn,       /**< Suspicious or unexpected condition that may require attention. */
        Info,       /**< High-level informational message about normal operation. */
        Debug,      /**< Detailed messages useful when debugging. */
        Trace,      /**< Very fine-grained messages for deep tracing. */
        Count       /**< Sentinel; number of log levels (not to be used as a level). */
    };

    /**
     * @class CSH_Log
     * @brief Thread-safe, Unicode-aware singleton logger.
     *
     * The logger writes UTF-8 encoded lines to a rolling log file whose name
     * is derived from the current timestamp (e.g., `YYYYMMDD_HHMMSS.log`).
     *
     * @par Thread safety
     * All public member functions are thread-safe. Internally, file writes
     * are serialized with a mutex; configuration flags are stored in atomics.
     *
     * @par Encoding
     * Public logging APIs accept wide strings (`std::wstring`) or printf-style
     * wide format strings (`const wchar_t*`). Lines are written to disk as UTF-8.
     */
    class CSHLOG_API CSH_Log final {
    public:
        /**
         * @brief Returns the singleton instance of the logger.
         *
         * @return Reference to the global logger instance.
         *
         * @note The instance is initialized on first use in a thread-safe way.
         */
        static CSH_Log& Instance();

        /**
         * @brief Initializes the global logger configuration.
         *
         * Call this once at program start (optional; the logger has defaults).
         * This sets the output directory, whether to persist logs, the minimum
         * severity level, and the alignment width for the function/line field.
         *
         * @param directory Target directory to store the log file. If empty, `"."` is used.
         * @param saveLog If `true`, log lines are appended to a file. If `false`, logging is disabled.
         * @param level Minimum severity to write. Messages below this level are ignored.
         * @param funcFieldWidth Width used to right-pad the ¡°func : line¡± field in each log line.
         *
         * @note This function is safe to call multiple times; the latest call
         *       overwrites previous settings.
         */
        static void Init(const std::wstring& directory,
            bool saveLog = true,
            LogLevel level = LogLevel::Info,
            std::size_t funcFieldWidth = 60);

        // -----------------------------
        // Configuration accessors
        // -----------------------------

        /**
         * @brief Enables or disables on-disk logging.
         * @param v Set `true` to write to the log file; `false` to disable writes.
         * @note This flag is read atomically by writers.
         */
        void setSaveLog(bool v) noexcept { bSaveLog.store(v, std::memory_order_relaxed); }

        /**
         * @brief Returns whether on-disk logging is enabled.
         * @return `true` if logging to file is enabled; otherwise `false`.
         */
        bool getSaveLog() const noexcept { return bSaveLog.load(std::memory_order_relaxed); }

        /**
         * @brief Sets the minimum log level.
         * @param lv Messages with a severity numerically greater than @p lv are ignored.
         * @note Stored atomically and read by writers.
         */
        void setLogLevel(LogLevel lv) noexcept { logLevel.store(static_cast<int>(lv), std::memory_order_relaxed); }

        /**
         * @brief Returns the current minimum log level.
         * @return The current @ref LogLevel.
         */
        LogLevel getLogLevel() const noexcept { return static_cast<LogLevel>(logLevel.load(std::memory_order_relaxed)); }

        /**
         * @brief Changes the directory where the log file will be written.
         *
         * If the directory does not exist, it is created (best effort). Future writes
         * will continue using the same timestamp-based file name.
         *
         * @param dir Directory path. If empty, `"."` is used.
         * @note Thread-safe: the directory update is protected by a mutex.
         */
        void setLogDirectory(const std::wstring& dir);

        /**
         * @brief Returns the current log directory.
         * @return The directory path currently configured for the log file.
         */
        std::wstring getLogDirectory() const { return strFilePath; }

        /**
         * @brief Returns the base log file name (without extension).
         * @return Timestamp string in the form `YYYYMMDD_HHMMSS`.
         */
        std::wstring getFileName()    const { return strFileName; }

        // -----------------------------
        // Logging APIs
        // -----------------------------

        /**
         * @brief Writes a formatted log line using a wide printf-style format.
         *
         * The final message is formatted with `std::vswprintf` and then written
         * as a UTF-8 line. If @p fmt is `nullptr`, an empty message is written.
         * Messages are dropped when logging is disabled or the level is below
         * the configured threshold.
         *
         * @param level Severity level for this message.
         * @param file Source file (typically `__FILE__`). Used for context only; not persisted by default.
         * @param line Source line number (typically `__LINE__`).
         * @param funcsig Function signature (e.g., `__FUNCSIG__` or `__PRETTY_FUNCTION__`), used to derive a concise function name.
         * @param fmt Wide-character printf-style format string (UTF-16/UTF-32 depending on platform).
         * @param ... Variadic arguments for the format string.
         *
         * @note Thread-safe. The function/line field is padded to @ref funcFieldWidth.
         * @warning Avoid passing user-controlled strings directly as format strings.
         * @see LOG_WRITE convenience macro.
         */
        void writeLog(LogLevel level,
            const char* file,
            int line,
            const char* funcsig,
            const wchar_t* fmt, ...) noexcept;

        /**
         * @brief Writes a complete message (no formatting) to the log.
         *
         * @param level Severity level for this message.
         * @param file Source file (typically `__FILE__`). Used for context only; not persisted by default.
         * @param line Source line number (typically `__LINE__`).
         * @param funcsig Function signature (e.g., `__FUNCSIG__` or `__PRETTY_FUNCTION__`), used to derive a concise function name.
         * @param msg Fully formatted wide-string message (UTF-16/UTF-32 depending on platform).
         *
         * @note Thread-safe. The message is encoded as UTF-8 and appended with a newline.
         * @see LOG_WRITE_MSG convenience macro.
         */
        void writeLog(LogLevel level,
            const char* file,
            int line,
            const char* funcsig,
            const std::wstring& msg) noexcept;

        CSH_Log(const CSH_Log&) = delete;            ///< Non-copyable (singleton).
        CSH_Log& operator=(const CSH_Log&) = delete; ///< Non-copyable (singleton).
        CSH_Log(CSH_Log&&) = delete;                 ///< Non-movable (singleton).
        CSH_Log& operator=(CSH_Log&&) = delete;      ///< Non-movable (singleton).

    private:
        /**
         * @brief Constructs the logger with default configuration.
         *
         * Use @ref Init to supply custom settings. Construction is internal-only.
         */
        CSH_Log();

        /**
         * @brief Destructor (flushes/closing are handled by ofstream RAII in write paths).
         */
        ~CSH_Log();

        /**
         * @brief Internal initializer invoked by @ref Init under lock.
         * @param directory Log directory.
         * @param saveLog Enable file logging.
         * @param level Minimum log level.
         * @param funcFieldWidth Field width for the ¡°func : line¡± block.
         */
        void initialize(const std::wstring& directory,
            bool saveLog,
            LogLevel level,
            std::size_t funcFieldWidth);

        /**
         * @brief Builds one complete log line: timestamp + function/line + level + message.
         * @param lv Log level.
         * @param funcDisplay A concise function identifier derived from @p funcsig.
         * @param line Source line number.
         * @param message Final message text.
         * @return Wide string representing a single log line.
         */
        std::wstring buildLogLine(LogLevel lv,
            const std::wstring& funcDisplay,
            int line,
            const std::wstring& message) const;

        /**
         * @brief Converts a log level to its wide-string name.
         * @param lv Log level.
         * @return Wide-string representation (e.g., L"Info").
         */
        std::wstring levelToWString(LogLevel lv) const;

        /**
         * @brief Extracts a concise function name from a compiler-specific signature.
         * @param funcsig Function signature (e.g., `__FUNCSIG__` / `__PRETTY_FUNCTION__`).
         * @return Best-effort shortened function name as wide string.
         */
        std::wstring sanitizeFunctionFromSignature(const char* funcsig) const;

        /**
         * @brief Converts UTF-8 to wide string (UTF-16 on Windows, UTF-32 on Unix-like platforms).
         * @param narrow UTF-8 encoded C-string (may be `nullptr`).
         * @return Wide string (empty on failure or `nullptr`).
         */
        static std::wstring toWString(const char* narrow);

        /**
         * @brief Converts wide string to UTF-8.
         * @param wide Wide string to convert.
         * @return UTF-8 encoded std::string (empty on failure).
         */
        static std::string  toUTF8(const std::wstring& wide);

        /**
         * @brief Composes the full path of the current log file including extension.
         * @return `<dir>/<YYYYMMDD_HHMMSS>.log`
         */
        std::filesystem::path composeFullPathWithExt() const;

        /**
         * @brief Ensures a directory exists, creating it if necessary (best effort).
         * @param dir Directory path to create.
         */
        static void ensureDirectory(const std::filesystem::path& dir);

        /**
         * @brief Converts CWatchTime ASCII timestamp to a file-stamp (seconds precision).
         * @param aTime ASCII time `"YYYY-MM-DD HH:MM:SS.mmm"`. If `nullptr`, returns `"00000000_000000"`.
         * @return Wide string `"YYYYMMDD_HHMMSS"`.
         */
        static std::wstring ToFileNameStampFromCWatchA(const char* aTime);

        /**
         * @brief Converts CWatchTime ASCII timestamp to a printable stamp with milliseconds.
         * @param aTime ASCII time `"YYYY-MM-DD HH:MM:SS.mmm"`. If `nullptr`, uses `"00000000_000000.000"`.
         * @return Wide string `"YYYYMMDD_HHMMSS.mmm"`.
         */
        static std::wstring ToMilliStampFromCWatchA(const char* aTime);

    private:
        // --- Configuration/state ---
        std::atomic<bool> bSaveLog{ true };                             ///< If false, all writes are no-ops.
        std::atomic<int>  logLevel{ static_cast<int>(LogLevel::Info) }; ///< Minimum severity to write.
        std::wstring      strFilePath;   ///< Output directory for the log file.
        std::wstring      strFileName;   ///< Base name (without extension), e.g., `YYYYMMDD_HHMMSS`.
        std::size_t       funcFieldWidth{ 60 }; ///< Padding width for the ¡°func : line¡± field.

        mutable std::mutex mtx;          ///< Serializes initialization and file writes.
    };

    /**
     * @def CSH_FUNC_SIG
     * @brief Compiler-specific macro expanding to the current function signature.
     *
     * - MSVC: `__FUNCSIG__`
     * - GCC/Clang: `__PRETTY_FUNCTION__`
     */
#if defined(_MSC_VER)
#define CSH_FUNC_SIG __FUNCSIG__
#else
#define CSH_FUNC_SIG __PRETTY_FUNCTION__
#endif

     /**
      * @def LOG_WRITE
      * @brief Convenience macro for formatted logging with source context.
      *
      * Expands to a call to @ref CSH_Log::writeLog(LogLevel,const char*,int,const char*,const wchar_t*,...)
      * with `__FILE__`, `__LINE__`, and @ref CSH_FUNC_SIG automatically supplied.
      *
      * @param level A @ref LogLevel value (e.g., @ref LogLevel::Info).
      * @param fmt   Wide printf-style format string (UTF-16/UTF-32 depending on platform).
      * @param ...   Variadic arguments for @p fmt.
      *
      * @code
      * LOG_WRITE(cshlog::LogLevel::Info, L"started: pid=%d", pid);
      * @endcode
      */
#define LOG_WRITE(level, fmt, ...) \
  ::cshlog::CSH_Log::Instance().writeLog((level), __FILE__, __LINE__, CSH_FUNC_SIG, (fmt), ##__VA_ARGS__)

      /**
       * @def LOG_WRITE_MSG
       * @brief Convenience macro for logging a preformatted wide string with source context.
       *
       * @param level A @ref LogLevel value.
       * @param wmsg  Wide string message (no formatting).
       *
       * @code
       * LOG_WRITE_MSG(cshlog::LogLevel::Error, L"failed to open configuration file");
       * @endcode
       */
#define LOG_WRITE_MSG(level, wmsg) \
  ::cshlog::CSH_Log::Instance().writeLog((level), __FILE__, __LINE__, CSH_FUNC_SIG, (wmsg))

} // namespace cshlog
