#pragma once
/**
 * @file CSH_Image.h
 * @brief Cross-platform C++17 image container class for camera/vision pipelines.
 *
 * - Dynamic library ready (dllexport/dllimport on Windows, default visibility on Linux).
 * - Smart-pointer owned buffer with view pointer managed internally (no raw address moves).
 * - Shallow / deep copy semantics.
 * - Robust, forward/backward-compatible binary persistence via tagged fields (TLV).
 * - Optional zero-copy OpenCV cv::Mat view (CSH_IMAGE_WITH_OPENCV).
 */

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <type_traits>
#include <filesystem>

#if defined(_WIN32) || defined(_WIN64)
#  if defined(CSH_IMAGE_EXPORT)
#    define CSH_IMAGE_API __declspec(dllexport)
#  else
#    define CSH_IMAGE_API __declspec(dllimport)
#  endif
#else
#  define CSH_IMAGE_API __attribute__((visibility("default")))
#endif

#ifdef CSH_IMAGE_WITH_OPENCV
#include <opencv2/opencv.hpp>
#endif

namespace csh_img {

    /**
     * @enum En_ImageFormat
     * @brief Logical pixel/container formats (not colorspace conversions).
     *
     * Values are grouped by typical container bit depth:
     *  - 100s: 8-bit family (e.g., Bayer8, Gray8)
     *  - 200s: 16-bit/packed 10/12/14-bit family, YUV422, RGB565
     *  - 300s: 24-bit family (RGB888/BGR888/YUYV444)
     */
    enum class En_ImageFormat : uint32_t {
        // 8-bit container group (100+)
        Bayer8 = 100,  /**< 8-bit Bayer mosaic (pattern in En_ImagePattern). */
        Gray8,         /**< 8-bit grayscale. */

        // 16-bit container group (200+)
        Bayer10 = 200, /**< 10-bit Bayer stored in 16-bit container or packed as policy. */
        Bayer12,       /**< 12-bit Bayer stored in 16-bit container or packed as policy. */
        Bayer14,       /**< 14-bit Bayer stored in 16-bit container or packed as policy. */
        Bayer16,       /**< 16-bit Bayer, full 16-bit container. */
        Gray10,        /**< 10-bit gray stored in 16-bit container or packed. */
        Gray12,        /**< 12-bit gray stored in 16-bit container or packed. */
        Gray14,        /**< 14-bit gray stored in 16-bit container or packed. */
        Gray16,        /**< 16-bit gray, full 16-bit container. */
        YUV422,        /**< Packed 4:2:2 (2 bytes per pixel pair). */
        RGB565,        /**< 16-bit RGB (5-6-5). */

        // 24-bit container group (300+)
        YUYV444 = 300, /**< 3¡¿8-bit container variant (API compatibility). */
        RGB888,        /**< 8-bit per channel RGB. */
        BGR888,        /**< 8-bit per channel BGR. */
    };

    /**
     * @enum En_ImagePattern
     * @brief Pixel order / component layout associated with a format.
     *
     * For Bayer formats, set the CFA order; for YUV422, the packed ordering;
     * for 24-bit RGB/BGR, choose channel order.
     */
    enum class En_ImagePattern : uint32_t {
        // Bayer
        RGGB = 0, GRBG, BGGR, GBRG,
        // YUV422 (packed)
        YUYV = 10, UYVY, YVYU, VYUY,
        // RGB/BGR (24-bit)
        RGB = 20, BGR,
    };

    /**
     * @enum En_ImageMemoryAlign
     * @brief Memory layout / plane arrangement.
     *
     * @note Current implementation mainly operates on @ref Packed. Other values
     *       exist for forward compatibility and future planar/semi-planar support.
     */
    enum class En_ImageMemoryAlign : uint32_t {
        Packed = 0,                  /**< Interleaved/packed bytes in a single plane. */
        // planar examples
        YYYYUUUUVVVV = 10, YYYYVVVVUUUU, UUUUVVVVYYYY, VVVVUUUUYYYY,
        // planar RGB
        RRRRGGGGBBBB = 20, BBBBGGGGRRRR,
        // semi-planar examples
        YYYYUVUV = 30, YYYYVUVU,
    };

    /**
     * @enum CopyMode
     * @brief Copy semantics for @ref CSH_Image::copy and related APIs.
     *
     * - @ref CopyMode::MetaOnly : metadata only (no buffer).
     * - @ref CopyMode::Shallow  : share the same buffer (shared_ptr) and view.
     * - @ref CopyMode::Deep     : copy bytes into an already allocated destination buffer.
     */
    enum class CopyMode : uint32_t { MetaOnly = 0, Shallow, Deep };

    /**
     * @class CSH_Image
     * @brief Image container with explicit format metadata and flexible buffer ownership.
     *
     * ### Key properties
     * - **Ownership**: buffer is stored as `std::shared_ptr<byte[]>`. Shallow copies share
     *   the buffer; deep copies require a pre-allocated destination.
     * - **View**: `buffer_offset` + `sel_image` define the current view inside a multi-image
     *   allocation (e.g., frame arrays).
     * - **Persistence**: TLV-based binary I/O (`saveImage`/`loadImage`) with a stable header.
     * - **Interoperability**: Optional zero-copy interop with OpenCV (when `CSH_IMAGE_WITH_OPENCV`).
     *
     * @warning Deep copies require the destination to have an allocated buffer with enough
     *          writable bytes from the current view; otherwise an exception is thrown.
     */
    class CSH_IMAGE_API CSH_Image {
    public:
        using byte = uint8_t; ///< Unsigned 8-bit byte alias for buffer access.

        /**
         * @brief Constructs an empty, disabled image (no buffer).
         */
        CSH_Image();

        /**
         * @brief Constructs an image with metadata and optional allocation.
         * @param width      Image width in pixels.
         * @param height     Image height in pixels.
         * @param format     Pixel/container format.
         * @param alloc_mem  If true, allocate a buffer sized to `width*height*bpp*image_count`.
         * @param image_count Number of images (frames) in the allocation.
         *
         * @note The per-frame byte size is computed by @ref recomputeBufferSize().
         */
        CSH_Image(uint32_t width, uint32_t height, En_ImageFormat format,
            bool alloc_mem = true, uint32_t image_count = 1);

        CSH_Image(const CSH_Image&) = default;              ///< Shallow copy (shared buffer).
        CSH_Image(CSH_Image&&) noexcept = default;          ///< Move constructor.
        CSH_Image& operator=(const CSH_Image&) = default;   ///< Shallow assignment (shared buffer).
        CSH_Image& operator=(CSH_Image&&) noexcept = default; ///< Move assignment.
        ~CSH_Image() = default;

        // -------------------------
        // Copy operations
        // -------------------------

        /**
         * @brief Copies metadata and optionally buffer depending on @p mode.
         *
         * - @ref CopyMode::MetaOnly : copies metadata; resets buffer.
         * - @ref CopyMode::Shallow  : shares @p src buffer; adopts capacity info if known.
         * - @ref CopyMode::Deep     : copies bytes into *existing* destination buffer.
         *
         * @param src  Source image.
         * @param mode Copy behavior.
         * @throw std::runtime_error If mode is Deep and the destination has no buffer
         *                            or insufficient writable bytes from current view.
         * @throw std::invalid_argument If @p mode is unknown.
         */
        void copy(const CSH_Image& src, CopyMode mode);

        /**
         * @brief Shares the underlying buffer pointer/view with @p src (shallow ownership).
         * @param src Source image (its buffer/shared_ptr is adopted).
         */
        void copyBufferPointer(const CSH_Image& src);

        /**
         * @brief Adopts an external raw pointer as the buffer (shallow, no delete).
         * @param pFrame Raw pointer to externally-managed memory.
         * @throw std::invalid_argument If @p pFrame is `nullptr`.
         *
         * @warning The memory is **not** freed by this class. Ensure the lifetime
         *          of @p pFrame outlives this image or any shared copies.
         */
        void copyBufferPointer(byte* pFrame);

        // -------------------------
        // Persistence (TLV)
        // -------------------------

        /**
         * @brief Saves the image (header + fields + optional bytes) to a file.
         * @param filepath Destination path.
         * @throw std::runtime_error On I/O errors or when the buffer is missing but required.
         *
         * @details Format: Magic (CHSI) + Version + field count + {TLV...}. The buffer is
         *          written as a single field when present. All integers are little-endian.
         */
        void saveImage(const std::filesystem::path& filepath) const;

        /**
         * @brief Loads an image from a TLV file written by @ref saveImage().
         * @param filepath Source path.
         * @throw std::runtime_error On I/O errors, bad magic/version, or allocation failures.
         */
        void loadImage(const std::filesystem::path& filepath);

        /// @name Backward-compatible overloads
        ///@{
        inline void saveImage(const std::string& filepath) const { saveImage(std::filesystem::path(filepath)); }
        inline void loadImage(const std::string& filepath) { loadImage(std::filesystem::path(filepath)); }
        inline void saveImage(const char* filepath) const { saveImage(std::filesystem::path(filepath)); }
        inline void loadImage(const char* filepath) { loadImage(std::filesystem::path(filepath)); }
#if defined(_WIN32) || defined(_WIN64)
        inline void saveImage(const std::wstring& filepath) const { saveImage(std::filesystem::path(filepath)); }
        inline void loadImage(const std::wstring& filepath) { loadImage(std::filesystem::path(filepath)); }
        inline void saveImage(const wchar_t* filepath) const { saveImage(std::filesystem::path(filepath)); }
        inline void loadImage(const wchar_t* filepath) { loadImage(std::filesystem::path(filepath)); }
#endif
        ///@}

        // -------------------------
        // Accessors / view
        // -------------------------

        /// @return Image width in pixels.
        inline uint32_t getWidth()  const { return width; }
        /// @return Image height in pixels.
        inline uint32_t getHeight() const { return height; }
        /// @return Whether the image is logically enabled/valid.
        inline bool     isEnabled() const { return bEnable; }
        /// @return Associated camera identifier (user-defined).
        inline uint32_t getCameraId() const { return camera_id; }
        /// @return Current image format.
        inline En_ImageFormat getFormat() const { return format; }
        /// @return Container bit depth (memory).
        inline uint32_t getMemoryBit() const { return memory_bit; }
        /// @return Original sensor bit depth (semantic).
        inline uint32_t getOriginalBit() const { return original_bit; }
        /// @return Pixel pattern (CFA/YUV order/channel order).
        inline En_ImagePattern getPattern() const { return pattern; }
        /// @return Memory alignment/plane arrangement.
        inline En_ImageMemoryAlign getMemoryAlign() const { return memory_align; }
        /// @return Per-frame byte size.
        inline std::size_t getBufferSize() const { return buffer_size; }
        /// @return Number of images in the allocation.
        inline uint32_t getImageCount() const { return image_count; }
        /// @return Currently selected image index (0-based).
        inline uint32_t getSelectedImage() const { return sel_image; }

        /**
         * @brief Returns a pointer to the current view (selected image).
         * @return Writable pointer or `nullptr` if no buffer.
         */
        inline byte* data() { return buffer ? (buffer.get() + buffer_offset) : nullptr; }

        /**
         * @brief Returns a const pointer to the current view (selected image).
         * @return Read-only pointer or `nullptr` if no buffer.
         */
        inline const byte* data() const { return buffer ? (buffer.get() + buffer_offset) : nullptr; }

        /**
         * @brief Returns the base pointer to the n-th image (0-based) without changing state.
         * @param n Image index.
         * @return Pointer to the start of image @p n or `nullptr` if no buffer.
         * @throw std::out_of_range If @p n >= @ref image_count.
         */
        byte* getImagePtr(uint32_t n);

        /**
         * @copydoc getImagePtr(uint32_t)
         */
        const byte* getImagePtr(uint32_t n) const;

        /**
         * @brief Selects the active image index for the view.
         * @param idx New index (0-based).
         * @throw std::out_of_range If @p idx >= @ref image_count
         *                           or if the computed offset exceeds capacity.
         */
        void setSelectedImage(uint32_t idx);

        /**
         * @brief Logical total bytes = per-frame bytes ¡¿ image count.
         * @return Total logical size of the allocation.
         */
        inline std::size_t totalBytes() const { return buffer_size * static_cast<std::size_t>(image_count); }

        /**
         * @brief Recomputes @ref buffer_size based on format, width, height, and bit depth.
         *
         * Uses @ref bytesPerPixelForFormat() when known; otherwise falls back
         * to `ceil(memory_bit/8)` bytes per pixel.
         */
        void recomputeBufferSize();

        /**
         * @brief Allocates exactly @ref totalBytes() and updates the current view.
         * @throw std::runtime_error If @ref buffer_size or @ref image_count is zero.
         *
         * @details On success, sets @ref buffer_capacity_bytes, ensures @ref sel_image
         *          is in range, and updates @ref buffer_offset accordingly.
         */
        void allocateBuffer();

#ifdef CSH_IMAGE_WITH_OPENCV
        // ==== OpenCV zero-copy view (CSH -> cv::Mat) ====

        /**
         * @brief Returns an OpenCV view or deep copy of the current image.
         * @param deep_copy If false, returns a zero-copy view; if true, copies data.
         * @return `cv::Mat` representing the image.
         * @throw std::runtime_error On unsupported format, invalid size, or missing buffer.
         * @note Only @ref En_ImageMemoryAlign::Packed is currently supported.
         */
        cv::Mat toCvMat(bool deep_copy = false) const;

        /**
         * @brief Checks whether the image can be shared as a zero-copy `cv::Mat`.
         * @return `true` if a zero-copy view is possible; otherwise `false`.
         */
        bool    isShareableToCv() const;

        // ==== OpenCV -> CSH  (instance method) ====

        /**
         * @brief Imports an OpenCV image into this instance.
         *
         * - If @p mode is @ref CopyMode::Shallow and the Mat is continuous with the expected step,
         *   the buffer is shared (lifetime is tied to an internal keeper).
         * - Otherwise performs a deep copy, allocating if necessary; when a buffer already exists,
         *   only reuse-within-capacity is allowed (no reallocation).
         *
         * @param mat  Source `cv::Mat`.
         * @param mode Copy mode (default Deep).
         * @param fmt  Optional override for format (auto if 0).
         * @param pat  Optional override for pattern (auto or derived from fmt if 0).
         * @param align Memory alignment (default Packed).
         * @return Status code: 1=success, 0=invalid/empty Mat, 2=unsupported Mat type,
         *         4=not enough writable space (reuse-only policy), 5=other internal error.
         */
        int fromCvMat(const cv::Mat& mat,
            CopyMode mode = CopyMode::Deep,
            En_ImageFormat fmt = static_cast<En_ImageFormat>(0),   // auto if 0
            En_ImagePattern pat = static_cast<En_ImagePattern>(0), // auto if 0 or fmt-provided
            En_ImageMemoryAlign align = En_ImageMemoryAlign::Packed);
#endif

        // -------------------------
        // Public metadata (intentionally plain for POD-like access)
        // -------------------------

        uint32_t width = 0;                     ///< Image width in pixels.
        uint32_t height = 0;                    ///< Image height in pixels.
        bool     bEnable = false;               ///< Logical enabled flag.
        uint32_t camera_id = 0;                 ///< User-defined camera identifier.

        En_ImageFormat      format = En_ImageFormat::Gray8;          ///< Current image format.
        uint32_t            memory_bit = 8;                          ///< Memory container bit depth.
        uint32_t            original_bit = 8;                        ///< Original sensor bit depth.
        En_ImagePattern     pattern = En_ImagePattern::RGGB;         ///< Pixel/component layout.
        En_ImageMemoryAlign memory_align = En_ImageMemoryAlign::Packed; ///< Memory layout.
        std::size_t         buffer_size = 0;                         ///< Per-frame byte size.
        uint32_t            image_count = 1;                         ///< Number of images in allocation.
        uint32_t            sel_image = 0;                           ///< Currently selected image index.

        std::shared_ptr<byte[]> buffer;       ///< Base shared buffer pointer (may be null).

    private:
        /**
         * @brief Byte offset of the current view from the base buffer pointer.
         *
         * Equals `sel_image * buffer_size` for linear multi-image allocations.
         */
        std::size_t buffer_offset = 0;

        /**
         * @brief Actual allocated capacity in bytes for @ref buffer.
         *
         * When zero, bounds checks fall back to @ref totalBytes().
         */
        std::size_t buffer_capacity_bytes = 0;

        // ---- Static helpers (format defaults / math) ----

        /**
         * @brief Returns default container bit depth for @p fmt.
         */
        static uint32_t            defaultMemoryBitForFormat(En_ImageFormat fmt);

        /**
         * @brief Returns default pixel pattern for @p fmt.
         */
        static En_ImagePattern     defaultPatternForFormat(En_ImageFormat fmt);

        /**
         * @brief Returns default memory alignment for @p fmt.
         */
        static En_ImageMemoryAlign defaultAlignForFormat(En_ImageFormat fmt);

        /**
         * @brief Exact bytes-per-pixel for known formats; 0 if unknown.
         */
        static std::size_t         bytesPerPixelForFormat(En_ImageFormat fmt);

        /**
         * @brief Safe addition/subtraction of signed delta to an offset with bounds checks.
         * @param total     Total allowable size.
         * @param delta     Signed delta to apply.
         * @param outOffset Offset to update (throws on overflow/underflow).
         * @throw std::out_of_range On overflow, negative underflow, or beyond-total.
         */
        static void checkedAddOffset(std::size_t total, std::ptrdiff_t delta, std::size_t& outOffset);

        /**
         * @brief Bytes writable from current view to the end of allocation.
         * @return `buffer_capacity_bytes - buffer_offset` (or `totalBytes()` fallback).
         */
        std::size_t writableBytesFromView() const {
            const std::size_t cap = buffer_capacity_bytes ? buffer_capacity_bytes : totalBytes();
            return (cap > buffer_offset) ? (cap - buffer_offset) : 0;
        }

        // ---- TLV constants & helpers ----
        static constexpr uint32_t kMagic = 0x43485349; ///< File magic `'CHSI'`.
        static constexpr uint32_t kVersion = 1;        ///< TLV format version.

        enum : uint32_t {
            F_WIDTH = 1, F_HEIGHT = 2, F_BENABLE = 3, F_CAMERA_ID = 4, F_FORMAT = 5, F_MEMORY_BIT = 6,
            F_ORIGINAL_BIT = 7, F_PATTERN = 8, F_MEM_ALIGN = 9, F_BUFFER_SIZE = 10,
            F_IMAGE_COUNT = 11, F_SEL_IMAGE = 12, F_BUFFER_OFF = 13, F_BUFFER_BYTES = 100
        };

        static void write_u32(std::ostream& os, uint32_t v);
        static void write_u64(std::ostream& os, uint64_t v);
        static uint32_t read_u32(std::istream& is);
        static uint64_t read_u64(std::istream& is);
        static void     write_bytes(std::ostream& os, const void* data, std::size_t sz);
        static void     read_bytes(std::istream& is, void* data, std::size_t sz);

#ifdef CSH_IMAGE_WITH_OPENCV
        /**
         * @brief Attempts to infer @ref En_ImageFormat and @ref En_ImagePattern from a `cv::Mat`.
         * @param mat     Source Mat.
         * @param outFmt  Output inferred format.
         * @param outPat  Output inferred pattern.
         * @return True if a supported mapping exists; otherwise false.
         */
        static bool inferFormatFromMat(const cv::Mat& mat, En_ImageFormat& outFmt, En_ImagePattern& outPat);
#endif
    };

} // namespace csh_img
