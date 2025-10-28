#pragma once
#include <functional>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include "CSH_Image.h"
#include "CGrabberConfig.h"

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
 * @brief Per-frame processing callback signature.
 * @details Called from the backend's grabbing thread whenever a frame is ready.
 *          The image is read-only within the callback.
 */
using FrameGrabCallbackProc = std::function<void(const csh_img::CSH_Image&)>;

/**
 * @brief Per-frame display callback signature.
 * @details Typically used by a renderer/viewer. Called from the same grabbing
 *          thread as @ref FrameGrabCallbackProc and after it, if both are set.
 */
using FrameGrabCallbackDisp = std::function<void(const csh_img::CSH_Image&)>;

/**
 * @class IFrameGrabImpl
 * @brief Abstract interface implemented by concrete grabber backends (e.g., UVC, V4L2).
 *
 * Lifecycle:
 * 1) Optionally call GetConnected() to enumerate devices.
 * 2) Optionally call SetConfig() to request width/height/fps/pixel format.
 * 3) Call Connect().
 * 4) Register callbacks (processor/display).
 * 5) Call GrabFrames() to start the grabbing worker thread.
 * 6) Call StopGrabbing() and Disconnect() to tear down.
 *
 * @thread_safety Public methods are not re-entrant; CFrameGrabber wraps calls with a mutex.
 * Callbacks are invoked from the backend's worker thread and must be fast and exception-safe.
 */
class FRAMEGRAB_API IFrameGrabImpl {
public:
    virtual ~IFrameGrabImpl() = default;

    /**
     * @brief Probe for available capture devices.
     * @param[out] outDeviceCount Number of devices found.
     * @param[out] outModelNames  Human-readable names; may include device paths.
     * @return true on successful probe (even if 0 devices), false on failure.
     * @note CUVC probes indices [0..15] with OpenCV; CV4L2 scans /dev/video*.
     */
    virtual bool GetConnected(int& outDeviceCount, std::vector<std::string>& outModelNames) = 0;

    /**
     * @brief Establish connection to the currently selected device.
     * @return true on success, false otherwise.
     * @note If no config was set, implementations use safe defaults
     *       (e.g., CUVC defaults to device 0 and current camera mode).
     */
    virtual bool Connect() = 0;

    /**
     * @brief Close the active connection and release OS handles.
     * @post After return, the implementation is disconnected and not grabbing.
     */
    virtual void Disconnect() = 0;

    /**
     * @brief Apply a configuration request (resolution, fps, pixel format, paths).
     * @param[in] cfg Pointer to configuration; must not be null.
     * @return true if accepted (not necessarily fully honored), false if rejected.
     * @note CUVC applies width/height/fps via cv::VideoCapture properties.
     *       CV4L2 maps @ref CGrabberConfig::PixelFormat to V4L2 fourcc and
     *       may also adjust the media graph (rp1-cfe pipeline).
     */
    virtual bool SetConfig(const CGrabberConfig* cfg) = 0;

    /**
     * @brief Start the background grabbing thread and begin delivering frames.
     * @return true if the worker started (idempotent), false on failure.
     * @warning Callbacks—if registered—are invoked on this worker thread.
     */
    virtual bool GrabFrames() = 0;

    /**
     * @brief Stop the background grabbing thread.
     * @note Safe to call if not grabbing (no-op).
     */
    virtual void StopGrabbing() = 0;

    /**
     * @brief Register the processing callback.
     * @param[in] cb Callback or empty function to clear.
     * @note Delivery order per frame: processor first, then displayer (if both set).
     */
    virtual void RegisterCallbackProcessor(FrameGrabCallbackProc cb) = 0;

    /**
     * @brief Register the display callback.
     * @param[in] cb Callback or empty function to clear.
     * @note Delivery order per frame: processor first, then displayer (if both set).
     */
    virtual void RegisterCallbackDisplayer(FrameGrabCallbackDisp cb) = 0;

    /**
     * @brief Write a sensor register (if supported by backend).
     * @param[in] address Register address.
     * @param[in] value   Value to write.
     * @return true if the backend supports and successfully writes the register; false otherwise.
     * @note CUVC and current CV4L2 implementations return false (not supported).
     */
    virtual bool SetSensorRegister(uint32_t address, uint32_t value) = 0;

    /**
     * @brief Read a sensor register (if supported by backend).
     * @param[in]  address  Register address.
     * @param[out] outValue Read-back value on success (unchanged on failure).
     * @return true if the backend supports and successfully reads the register; false otherwise.
     * @note CUVC and current CV4L2 implementations return false (not supported).
     */
    virtual bool GetSensorRegister(uint32_t address, uint32_t& outValue) = 0;

    /**
     * @brief Access the last configuration passed via @ref SetConfig.
     * @return Immutable reference to the stored configuration.
     */
    const CGrabberConfig& Config() const { return grabberConfig; }

protected:
    /// Last configuration provided by the application (may not reflect negotiated/actual mode).
    CGrabberConfig grabberConfig{};
};
