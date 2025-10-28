#pragma once
#include <string>
#include <cstdint>
#include <CSH_Image.h>

/**
 * @file CGrabberConfig.h
 * @brief Basic runtime configuration for a frame grabber backend.
 *
 * This struct carries the desired device selection (video/subdev),
 * frame geometry, frame rate, and pixel format. Backends should interpret
 * these as *requests* and may clamp/adjust to the closest supported mode.
 *
 * @note Defaults are safe 640x480 @ 30fps RGB24.
 * @see IFrameGrabImpl::SetConfig
 */
struct CGrabberConfig {
    /**
     * @brief Logical pixel format request.
     *
     * Backends map these to their native driver/SDK codes (e.g., V4L2 fourcc).
     * Not every backend supports every value; a backend may ignore this and
     * pick the closest supported format.
     */
    enum class PixelFormat : uint32_t {
        UNKNOWN = 0,  ///< Unspecified; backend chooses default.
        GRAY8,        ///< 8-bit grayscale (Y only).
        RGB24,        ///< Interleaved RGB888 (R,G,B order).
        BGR24,        ///< Interleaved BGR888 (B,G,R order).
        YUYV422,      ///< Packed YUV 4:2:2 as Y0 U Y1 V ...
        UYVY422       ///< Packed YUV 4:2:2 as U Y0 V Y1 ...
    };

    /**
     * @brief /dev/videoX index or device ordinal where applicable.
     * @details Default -1 means "unspecified".
     * Some backends (e.g., CUVC) may ignore this and probe device 0.
     */
    int   video_id = -1;

    /**
     * @brief Sub-device index (e.g., v4l-subdevX) when relevant.
     * @details Default -1 means "unused / let backend decide".
     */
    int   subdev_id = -1;

    /**
     * @brief Explicit video device path (e.g., "/dev/video0").
     * @details Empty means "derive from @ref video\_id or probe".
     */
    std::string strVideo;

    /**
     * @brief Explicit subdevice path (e.g., "/dev/v4l-subdev0").
     * @details Empty means "derive from @ref subdev\_id or probe".
     */
    std::string strSubdev;

    /**
     * @brief Human-readable grabber name (for logs/UI).
     * @details Optional; not used for routing.
     */
    std::string strGrabberName;

    /// Requested frame width in pixels (default 640).
    uint32_t width = 640;

    /// Requested frame height in pixels (default 480).
    uint32_t height = 480;

    /// Requested frame rate in Hz (default 30).
    uint32_t fps = 30;

    /// Requested pixel format (default RGB24).
    PixelFormat pixel_format = PixelFormat::RGB24;
};
