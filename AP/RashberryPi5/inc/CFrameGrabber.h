#pragma once
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include "IFrameGrabImpl.h"

// ---------------- Export macro ----------------
#pragma once
#if defined(_WIN32) || defined(_WIN64)
#ifdef FRAMEGRAB_BUILD
#define FRAMEGRAB_API __declspec(dllexport)
#else
#define FRAMEGRAB_API __declspec(dllimport)
#endif
#else
#define FRAMEGRAB_API __attribute__((visibility("default")))
#endif

/**
 * @file CFrameGrabber.h
 * @brief Thread-safe façade over pluggable frame grabber backends.
 *
 * This class owns an @ref IFrameGrabImpl and serializes public calls with a mutex,
 * so callers don’t need to worry about re-entrancy while managing connection and
 * streaming state. It provides a simple, backend-agnostic API for:
 *  - Backend selection (UVC / V4L2 / gStreamer placeholder).
 *  - Device probing, connect/disconnect.
 *  - Configuration forwarding.
 *  - Start/stop grabbing with callback fan-out.
 *
 * @note Callbacks registered through this façade are forwarded directly to the backend.
 *       They are invoked from the backend’s grabbing thread.
 */
class FRAMEGRAB_API CFrameGrabber {
public:
    /**
     * @brief Available backend implementations.
     * @note gStreamer is currently a placeholder that maps to UVC in the factory.
     */
    enum class En_GrabberBackend { UVC = 0, V4L2, gStreamer, Count };

    /// Construct an empty façade (no backend selected).
    CFrameGrabber();

    /// RAII teardown; does not implicitly stop/close—callers should manage lifecycle.
    ~CFrameGrabber();

    /**
     * @brief Select and create the backend implementation.
     * @param[in] be Backend enum to instantiate.
     * @return true if the backend instance was created, false otherwise.
     * @post Any previous backend instance is destroyed.
     */
    bool SetBackend(En_GrabberBackend be);

    /**
     * @brief Probe for devices through the selected backend.
     * @param[out] outDeviceCount Number of devices found.
     * @param[out] outModelNames  Human-readable device names or paths.
     * @return true on successful probe (even if none found), false on error or no backend.
     */
    bool GetConnected(int& outDeviceCount, std::vector<std::string>& outModelNames);

    /**
     * @brief Open the device using current backend and configuration.
     * @return true on success, false otherwise (including no backend).
     * @post On success, @ref IsConnecting returns true.
     */
    bool Connect();

    /**
     * @brief Close the device and release resources.
     * @post @ref IsConnecting becomes false; grabbing is also stopped if active.
     */
    void Disconnect();

    /**
     * @brief Forward configuration to the backend.
     * @param[in] cfg Non-null pointer to configuration.
     * @return true if accepted by the backend, false otherwise or no backend.
     * @note Must be called before @ref Connect for best results; some backends
     *       allow changing mode while connected.
     */
    bool SetConfig(const CGrabberConfig* cfg);

    /**
     * @brief Start the backend’s grabbing worker and begin frame delivery.
     * @return true if started (idempotent), false on failure or no backend.
     * @post On success, @ref IsGrabbing returns true.
     */
    bool GrabFrames();

    /**
     * @brief Stop the backend’s grabbing worker.
     * @post @ref IsGrabbing becomes false.
     */
    void StopGrabbing();

    /**
     * @brief Register processing callback (processor).
     * @param[in] cb Callback or empty function to clear.
     * @warning The callback runs on the backend’s grabbing thread.
     */
    void RegisterCallbackProcessor(FrameGrabCallbackProc cb);

    /**
     * @brief Register display callback (renderer/viewer).
     * @param[in] cb Callback or empty function to clear.
     * @warning The callback runs on the backend’s grabbing thread.
     */
    void RegisterCallbackDisplayer(FrameGrabCallbackDisp cb);

    /**
     * @brief Write a sensor register via backend (if supported).
     * @return true on success, false if unsupported or failure.
     * @note Current CUVC and CV4L2 return false (not implemented).
     */
    bool SetSensorRegister(uint32_t address, uint32_t value);

    /**
     * @brief Read a sensor register via backend (if supported).
     * @return true on success, false if unsupported or failure.
     * @note Current CUVC and CV4L2 return false (not implemented).
     */
    bool GetSensorRegister(uint32_t address, uint32_t& outValue);

    /// Last probed device count (cached from GetConnected()).
    int  deviceCount() const { return deviceCount_; }

    /// Last probed model names (cached from GetConnected()).
    const std::vector<std::string>& modelNames() const { return modelNames_; }

    /// True after successful Connect(), false after Disconnect().
    bool IsConnecting() const { return isConnecting_; }

    /// True after successful GrabFrames(), false after StopGrabbing().
    bool IsGrabbing() const { return isGrabbing_; }

private:
    /// Owned backend implementation (UVC/V4L2).
    std::unique_ptr<IFrameGrabImpl> impl_;

    // Cached probe results
    int  deviceCount_ = 0;
    std::vector<std::string> modelNames_;

    // Simple state flags (best-effort)
    bool isConnecting_ = false;
    bool isGrabbing_ = false;

    // Serialize public API; backends have their own internal sync.
    std::mutex mtx_;
};
