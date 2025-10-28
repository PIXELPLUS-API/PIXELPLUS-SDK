#pragma once
#include <chrono>
#include <cwchar>

/**
 * @file CWatchTime.h
 * @brief Lightweight wall-clock stopwatch and timestamp formatting utilities.
 *
 * Provides simple start/stop timing with queries in seconds, milliseconds,
 * and microseconds, plus convenience functions to get the current local time
 * as a formatted wide/narrow C string.
 *
 * @note This class is **not thread-safe**. The narrow/wide time-string functions
 *       return pointers to internal static/stack buffers; treat the returned
 *       pointers as transient and copy the string if you need to retain it.
 */

#if defined(_WIN32)
#if defined(WATCHTIME_EXPORTS)
#define WATCHTIME_API __declspec(dllexport)
#else
#define WATCHTIME_API __declspec(dllimport)
#endif
#else
#define WATCHTIME_API __attribute__((visibility("default")))
#endif

 /**
  * @class CWatchTime
  * @brief RAII-less stopwatch using high_resolution_clock with simple elapsed queries.
  *
  * Typical usage:
  * @code
  * CWatchTime wt;
  * wt.start();
  * // ... work ...
  * wt.stop();
  * double ms = wt.GetMilliSecond();              // elapsed milliseconds
  * const wchar_t* label = wt.getString();        // L"123.456ms"
  * const char* now = wt.GetCurrentTimeStringA(); // "YYYY-MM-DD HH:MM:SS.mmm"
  * @endcode
  *
  * @warning Not thread-safe. Do not share a single instance across threads without external synchronization.
  * @warning String-returning methods return pointers to internal buffers whose contents are overwritten on subsequent calls.
  */
class WATCHTIME_API CWatchTime {
public:
    /// @brief High-resolution clock type used for measurements.
    using Clock = std::chrono::high_resolution_clock;
    /// @brief Time point type from #Clock.
    using TimePoint = Clock::time_point;

    /**
     * @brief Construct an idle stopwatch.
     *
     * The stopwatch starts in a non-running state. Call start() to begin measuring.
     */
    CWatchTime();

    /**
     * @brief Begin (or restart) timing.
     *
     * Sets the start timestamp to now and marks the stopwatch as running.
     * Calling start() again restarts the measurement.
     */
    void start();

    /**
     * @brief Stop timing.
     *
     * Captures the end timestamp and marks the stopwatch as not running.
     * Elapsed queries will use the captured end time until start() is called again.
     */
    void stop();

    /**
     * @brief Get elapsed time in seconds.
     * @return Elapsed seconds as a floating-point value.
     *
     * If the stopwatch is running, computes duration from start to "now";
     * otherwise, uses the captured end timestamp.
     */
    double GetSecond();

    /**
     * @brief Get elapsed time in milliseconds.
     * @return Elapsed milliseconds as a floating-point value.
     *
     * If the stopwatch is running, computes duration from start to "now";
     * otherwise, uses the captured end timestamp.
     */
    double GetMilliSecond();

    /**
     * @brief Get elapsed time in microseconds.
     * @return Elapsed microseconds as a floating-point value.
     *
     * If the stopwatch is running, computes duration from start to "now";
     * otherwise, uses the captured end timestamp.
     */
    double GetMicroSecond();

    /**
     * @brief Format the current elapsed time as a wide string like `L"123.456ms"`.
     * @return Pointer to an internal wide buffer containing the formatted string.
     *
     * @note The returned pointer remains valid until the next call to getString()
     *       on this instance. Copy the contents if you need to keep it.
     */
    const wchar_t* getString() const;

    /**
     * @brief Get the current local time as a wide string in the form `YYYY-MM-DD HH:MM:SS`.
     * @return Pointer to an internal static wide buffer.
     *
     * @note Uses a process-wide static buffer. Subsequent calls overwrite the buffer.
     * @note Thread-safety: The buffer is shared across all instances; concurrent calls
     *       from multiple threads will race.
     */
    const wchar_t* GetCurrentTimeString();

    /**
     * @brief Get the current local time as a narrow string in the form `YYYY-MM-DD HH:MM:SS.mmm`.
     * @return Pointer to an internal static char buffer.
     *
     * @note Uses a process-wide static buffer. Subsequent calls overwrite the buffer.
     * @note Thread-safety: The buffer is shared across all instances; concurrent calls
     *       from multiple threads will race.
     */
    const char* GetCurrentTimeStringA();

private:
    /// @brief Start timestamp for the current/last measurement.
    TimePoint startTime_{};
    /// @brief End timestamp captured on stop().
    TimePoint endTime_{};
    /// @brief Whether the stopwatch is currently running.
    bool      running_{ false };

    /// @brief Instance-local scratch buffer used by getString().
    mutable wchar_t buffer_[32];
};
