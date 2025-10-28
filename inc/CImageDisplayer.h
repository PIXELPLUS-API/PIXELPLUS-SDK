#pragma once
//
// CImageDisplayer.h  —  framework-agnostic image "displayer"
// - DOES NOT render; only exposes math and upload descriptors
// - 2D and 3D transforms (model/view/proj)
// - Input hooks for UI callbacks (mouse/keys)
// - Cross-platform export macro (Windows DLL / Linux .so)
//
// C++17
//

#include <array>
#include <cstdint>
#include <cstddef>

#include "CSH_Image.h" // canonical image container (namespace csh_img)

// ===== export macro (Windows/Linux) =========================================
#if defined(_WIN32) || defined(_WIN64)
#  if defined(CIMAGE_DISPLAYER_EXPORT)
#    define CIMAGE_API __declspec(dllexport)
#  else
#    define CIMAGE_API __declspec(dllimport)
#  endif
#else
#  define CIMAGE_API __attribute__((visibility("default")))
#endif

/**
 * @file CImageDisplayer.h
 * @brief Math-only image “displayer” that owns an image buffer and exposes
 *        2D/3D transform state and upload descriptors. No rendering is performed.
 *
 * Typical use:
 *   1) Provide image data via @ref cimage::CImageDisplayer::setImage or @ref setImageRaw.
 *   2) Configure viewport/fit and 2D/3D transforms.
 *   3) Fetch matrices, geometry and @ref uploadDesc for your renderer.
 *
 * Threading: this type is not thread-safe. Call from a single UI/render thread.
 */
namespace cimage {

    // ---------- Upload / pixel descriptors ----------

    /**
     * @brief High-level pixel layout of the buffer to upload.
     */
    enum class PixelLayout : uint32_t {
        Unknown = 0,   ///< Unspecified layout.
        Gray8,         ///< 8-bit grayscale.
        RGB888,        ///< Interleaved 8-bit RGB.
        BGR888,        ///< Interleaved 8-bit BGR.
        YUV422Packed,  ///< Packed YUV 4:2:2 (pattern indicated separately).
        RGB565,        ///< 16-bit packed RGB 5:6:5.
        Gray16,        ///< 16-bit grayscale.
        Bayer16        ///< 16-bit Bayer (10/12/14/16 packed in 16).
    };

    /**
     * @brief YUV422 packing order for packed layouts.
     */
    enum class Yuv422Pattern : uint32_t { YUYV = 0, UYVY, YVYU, VYUY };

    /**
     * @brief Description of the memory block to upload to a GPU/renderer.
     * @note Values are derived from @ref csh_img::CSH_Image via @ref CImageDisplayer::uploadDesc.
     */
    struct UploadDescriptor {
        const std::uint8_t* data = nullptr; ///< Pointer to first byte (may be null for MetaOnly).
        std::size_t sizeBytes = 0;          ///< Total buffer size in bytes.
        int width = 0, height = 0;          ///< Image dimensions in pixels.
        int bytesPerPixel = 0;              ///< Bytes per pixel for tightly packed data.
        int strideBytes = 0;                ///< Row stride in bytes (0 => tightly packed width*bpp).
        PixelLayout layout = PixelLayout::Unknown;  ///< Pixel layout.
        Yuv422Pattern yuv422Pattern = Yuv422Pattern::YUYV; ///< YUV422 order if applicable.
        bool isPacked = true;               ///< True if contiguous packed memory.
        bool isLittleEndian16 = true;       ///< True if 16-bit samples are little-endian.
    };

    // ---------- simple math types ----------

    /// 2D vector.
    struct Vec2 { float x = 0.f, y = 0.f; };
    /// 3D vector.
    struct Vec3 { float x = 0.f, y = 0.f, z = 0.f; };
    /// Quaternion (w + xyz). Identity by default.
    struct Quat { float w = 1.f, x = 0.f, y = 0.f, z = 0.f; static Quat identity() { return {}; } };

    /**
     * @brief Column-major 4x4 matrix (OpenGL style).
     */
    struct Mat4 {
        float m[16]{ 1,0,0,0,
                     0,1,0,0,
                     0,0,1,0,
                     0,0,0,1 };
        /// @brief Identity matrix.
        static Mat4 identity() { return {}; }
    };

    /// Row-major 3x3 (for 2D APIs).
    using Mat3 = std::array<float, 9>;
    /// Identity 3x3.
    inline constexpr Mat3 kMat3Identity{ 1,0,0, 0,1,0, 0,0,1 };

    // ---------- modes & input ----------

    /// Viewing dimensionality.
    enum class Dimensionality : uint32_t { Mode2D = 0, Mode3D };
    /// Fit/Fill strategy into viewport.
    enum class FitMode : uint32_t { None = 0, Fit, Fill, Stretch };
    /// Orbit interaction style for 3D rotation.
    enum class OrbitStyle : uint32_t { Arcball = 0, Turntable };

    /// Mouse button bitmask (can be combined).
    enum class MouseButton : uint32_t { None = 0, Left = 1, Middle = 2, Right = 4 };
    /// Keyboard modifier bitmask (can be combined).
    enum class KeyMod : uint32_t { None = 0, Shift = 1, Ctrl = 2, Alt = 4 };

    /// @brief Bitwise OR for key modifiers.
    inline KeyMod operator|(KeyMod a, KeyMod b) { return KeyMod(uint32_t(a) | uint32_t(b)); }

    // ====== CImageDisplayer class ===============================================

    /**
     * @class CImageDisplayer
     * @brief Owns/aliases an image and exposes 2D/3D transforms and upload metadata.
     *
     * The class is rendering-framework agnostic. Use @ref uploadDesc to obtain
     * the pixel/stride/format info and the various matrix getters for your shader
     * pipeline. Input hooks are provided to drive interaction from a host UI.
     */
    class CIMAGE_API CImageDisplayer {
    public:
        /// @brief Construct with empty image and identity transforms.
        CImageDisplayer();

        // ---- Image ownership / allocation (delegates to CSH_Image) ----

        /**
         * @brief Set image from an existing @ref csh_img::CSH_Image.
         * @param[in] img  Source image.
         * @param[in] mode Deep, Shallow, or MetaOnly copy semantics.
         * @note Shallow retains caller’s buffer; caller must keep it alive.
         */
        void setImage(const csh_img::CSH_Image& img, csh_img::CopyMode mode = csh_img::CopyMode::Shallow);

        /**
         * @brief Set image from a foreign raw buffer with explicit metadata.
         * @param[in] w,h     Dimensions in pixels.
         * @param[in] fmt     Image format.
         * @param[in] pat     Pixel pattern (RGB/BGR/YUV422 order, etc.).
         * @param[in] align   Memory alignment/packing.
         * @param[in] pixels  Pointer to source bytes (nullable for MetaOnly).
         * @param[in] bytes   Size of @p pixels in bytes.
         * @param[in] mode    Deep/Shallow/MetaOnly semantics.
         * @note Shallow uses a no-op deleter; caller retains ownership.
         */
        void setImageRaw(uint32_t w, uint32_t h,
            csh_img::En_ImageFormat fmt,
            csh_img::En_ImagePattern pat,
            csh_img::En_ImageMemoryAlign align,
            const void* pixels, std::size_t bytes,
            csh_img::CopyMode mode);

        /**
         * @brief Allocate (or reallocate) an internal image buffer.
         * @param w,h   Dimensions in pixels.
         * @param fmt   Desired format.
         * @param count Image count (defaults to 1).
         * @post Internal buffer is owned by this object.
         */
        void allocateImageBuffer(std::uint32_t w, std::uint32_t h, csh_img::En_ImageFormat fmt, std::uint32_t count = 1);

        /// @brief Immutable access to the current image.
        const csh_img::CSH_Image& image() const noexcept;
        /// @brief Mutable access to the current image.
        csh_img::CSH_Image& image()       noexcept;

        // ---- Viewport & fit ----

        /// @brief Set viewport size in pixels (non-negative).
        void setViewport(int w, int h);
        /// @return Current viewport width in pixels.
        int  viewportWidth()  const noexcept;
        /// @return Current viewport height in pixels.
        int  viewportHeight() const noexcept;
        /// @brief Set fit strategy (None/Fit/Fill/Stretch).
        void setFitMode(FitMode m);
        /// @return Current fit strategy.
        FitMode fitMode() const noexcept;

        // ---- Dimensionality ----

        /// @brief Switch between 2D and 3D modes.
        void setDimensionality(Dimensionality d);
        /// @return Current dimensionality.
        Dimensionality dimensionality() const noexcept;

        // ---- 2D transform state ----

        /// @brief Set normalized anchor inside image rect [0..1]².
        void set2DAnchor(float ax, float ay);
        /// @brief Set 2D translation in viewport pixels.
        void set2DTranslation(float tx, float ty);
        /// @brief Set 2D scale factors (multiplicative).
        void set2DScale(float sx, float sy);
        /// @brief Set rotation in degrees (counter-clockwise).
        void set2DRotationDeg(float deg);
        /// @brief Reset 2D transform to identity (anchor center).
        void reset2D();

        // ---- 3D transform state ----

        /// @brief Set model translation.
        void set3DModelTranslate(const Vec3& t);
        /// @brief Set model scale.
        void set3DModelScale(const Vec3& s);
        /// @brief Set model rotation (quaternion).
        void set3DModelRotation(const Quat& q);
        /// @brief Reset model transform to identity.
        void reset3DModel();

        /// @brief Set look-at target point.
        void set3DTarget(const Vec3& t);
        /// @brief Set camera eye position.
        void set3DEye(const Vec3& e);
        /// @brief Set camera up vector (normalized internally).
        void set3DUp(const Vec3& u);
        /// @brief Set orbit interaction style (Arcball/Turntable).
        void set3DOrbitStyle(OrbitStyle s);

        // ---- projection ----

        /// @brief Set orthographic projection (OpenGL style).
        void setOrtho(float l, float r, float b, float t, float n, float f);
        /// @brief Set perspective projection (fovy in degrees).
        void setPerspective(float fovyDeg, float aspect, float zNear, float zFar);
        /// @return True if orthographic projection is active.
        bool isOrthographic() const noexcept;

        // ---- matrices ----

        /// @brief Current 2D model matrix (row-major 3×3).
        Mat3 modelMatrix2D() const;
        /// @brief Current 3D model matrix (column-major 4×4).
        Mat4 modelMatrix3D() const;
        /// @brief Current 3D view matrix (column-major 4×4).
        Mat4 viewMatrix3D()  const;
        /// @brief Current projection matrix (column-major 4×4).
        Mat4 projectionMatrix() const;
        /// @brief Convenience @c P*V*M for 3D (column-major 4×4).
        Mat4 mvp3D() const;

        // ---- geometry helpers ----

        /**
         * @brief Transformed quad as 2D tri-strip with UVs.
         * @return TL,TR,BL,BR each as {x,y,u,v}.
         */
        std::array<std::array<float, 4>, 4> triStrip2D_XYUV() const;

        /**
         * @brief Unit quad in object space for 3D pipelines.
         * @return TL,TR,BL,BR each as {x,y,u,v} with x/y in [-0.5,+0.5].
         */
        static std::array<std::array<float, 4>, 4> triStrip3D_XYUV_ObjectSpace();

        // ---- upload descriptor ----

        /**
         * @brief Produce an upload descriptor reflecting the current image and format.
         * @return @ref UploadDescriptor with pointer/size/stride/layout info.
         */
        UploadDescriptor uploadDesc() const;

        // ==================== Input hooks (call from UI callbacks) ====================

        /**
         * @brief Begin a pointer interaction (mouse/touch).
         * @param x,y  Cursor position in viewport pixels.
         * @param btn  Button mask.
         * @param mods Key modifiers.
         */
        void beginPointer(float x, float y, MouseButton btn, KeyMod mods = KeyMod::None);

        /**
         * @brief Update pointer position during active interaction.
         */
        void updatePointer(float x, float y);

        /// @brief End current pointer interaction.
        void endPointer();

        /**
         * @brief Mouse wheel / trackpad zoom or dolly.
         * @param delta   Positive for zoom in (2D) / dolly in (3D).
         * @param cursorX,cursorY Cursor at event time (pixels).
         */
        void wheelScroll(float delta, float cursorX, float cursorY);

        /// @brief Keyboard panning in 2D mode (pixels).
        void keyPan2D(float dx, float dy);

        /// @brief Keyboard dolly in 3D mode (world units along view).
        void keyDolly3D(float amount);

    private:
        // ---------- state ----------
        csh_img::CSH_Image image_{};  ///< Owned/aliased image.

        int viewportW_ = 0, viewportH_ = 0;
        FitMode        fitMode_ = FitMode::None;
        Dimensionality mode_ = Dimensionality::Mode2D;

        // 2D
        Vec2  anchor2D_{ 0.5f,0.5f };
        Vec2  translate2D_{ 0,0 };
        Vec2  scale2D_{ 1,1 };
        Vec2  currDelta_{ 0,0 };
        Vec2  cumDelta_{ 0,0 };
        float rotation2D_ = 0.f;
        mutable Vec3  alpha_{ 0, 0, 1 }; ///< Internal cursor anchor (implementation detail).
        mutable Mat3  modelMatrix2D_ = kMat3Identity;

        // 3D model/camera
        Vec3  modelT_{ 0,0,0 };
        Vec3  modelS_{ 1,1,1 };
        Quat  modelR_{ Quat::identity() };
        Vec3  target_{ 0,0,0 };
        Vec3  eye_{ 0,0,1000 };
        Vec3  up_{ 0,1,0 };
        OrbitStyle orbitStyle_{ OrbitStyle::Arcball };
        Mat4  proj_{ Mat4::identity() };
        bool  isOrtho_{ true };

        // Pointer interaction (cached)
        bool        pActive_ = false;
        MouseButton pBtn_ = MouseButton::None;
        KeyMod      pMods_ = KeyMod::None;
        Vec2        pPrev_{}, pStart_{};

        // 3D anchors
        Vec3 arcballStart_{};
        Vec3 eyeStart_{};
        Quat modelRStart_{};
        // 2D anchors
        Vec2 translate2DStart_{};
        Vec2 scale2DStart_{};
    };

} // namespace cimage
