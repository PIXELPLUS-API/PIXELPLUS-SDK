#pragma once
//
// CImageDisplayerCPP.h — C++ convenience wrapper over the pure C ABI.
// Depends on: cimage_displayer_c.h and CSH_Image.h
//

#include "CImageDisplayerC.h"
#include "CSH_Image.h"
#include <stdexcept>
#include <cstring>

/**
 * @file CImageDisplayerCPP.h
 * @brief Thin C++ wrapper around the pure-C CImageDisplayer ABI.
 *
 * Provides RAII lifetime management and strongly-typed enums while keeping ABI
 * compatibility with the C layer. Zero rendering is performed here.
 */

namespace cimage {

    // 1) Strongly-typed mirrors (scoped; safe)

    /** @brief 2D/3D mode (scoped mirror of C enums). */
    enum class Dimensionality : int { _2D = CIMG_DIM_2D, _3D = CIMG_DIM_3D };

    /** @brief Fit/Fill strategy (scoped mirror). */
    enum class FitMode : int {
        None = CIMG_FIT_None, Fit = CIMG_FIT_Fit,
        Fill = CIMG_FIT_Fill, Stretch = CIMG_FIT_Stretch
    };

    /** @brief Orbit interaction style (scoped mirror). */
    enum class OrbitStyle : int { Arcball = CIMG_ORBIT_Arcball, Turntable = CIMG_ORBIT_Turntable };

    /**
     * @brief Mouse button bitmask (scoped mirror). Combinable via bitwise ops.
     */
    enum class MouseButton : int {
        None = CIMG_BTN_None,
        Left = CIMG_BTN_Left,
        Middle = CIMG_BTN_Middle,
        Right = CIMG_BTN_Right
    };
    /// @brief Bitwise OR for mouse buttons.
    inline MouseButton operator|(MouseButton a, MouseButton b) { return static_cast<MouseButton>(static_cast<int>(a) | static_cast<int>(b)); }
    /// @brief Bitwise AND for mouse buttons.
    inline MouseButton operator&(MouseButton a, MouseButton b) { return static_cast<MouseButton>(static_cast<int>(a) & static_cast<int>(b)); }
    /// @brief In-place OR for mouse buttons.
    inline MouseButton& operator|=(MouseButton& a, MouseButton b) { a = a | b; return a; }
    /// @brief True if any button bit is set.
    inline bool any(MouseButton m) { return static_cast<int>(m) != 0; }

    /**
     * @brief Keyboard modifier bitmask (scoped mirror). Combinable via bitwise ops.
     */
    enum class KeyMod : uint32_t {
        None = CIMG_KMOD_NONE,
        Shift = CIMG_KMOD_SHIFT,
        Ctrl = CIMG_KMOD_CTRL,
        Alt = CIMG_KMOD_ALT
    };
    /// @brief Bitwise OR for key modifiers.
    inline KeyMod operator|(KeyMod a, KeyMod b) { return static_cast<KeyMod>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); }
    /// @brief Bitwise AND for key modifiers.
    inline KeyMod operator&(KeyMod a, KeyMod b) { return static_cast<KeyMod>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); }
    /// @brief In-place OR for key modifiers.
    inline KeyMod& operator|=(KeyMod& a, KeyMod b) { a = a | b; return a; }
    /// @brief True if any modifier bit is set.
    inline bool any(KeyMod m) { return static_cast<uint32_t>(m) != 0; }

    // Conversion helpers (constexpr, inline)
    /// @name Enum conversions to C ABI
    ///@{
    constexpr uint32_t           to_c(KeyMod m) { return static_cast<uint32_t>(m); }
    constexpr CImgDimensionality to_c(Dimensionality d) { return static_cast<CImgDimensionality>(static_cast<int>(d)); }
    constexpr CImgFitMode        to_c(FitMode f) { return static_cast<CImgFitMode>(static_cast<int>(f)); }
    constexpr CImgOrbitStyle     to_c(OrbitStyle o) { return static_cast<CImgOrbitStyle>(static_cast<int>(o)); }
    constexpr uint32_t           to_c(MouseButton m) { return static_cast<uint32_t>(m); }
    ///@}

    /// @name Enum conversions from C ABI
    ///@{
    constexpr KeyMod        from_c(uint32_t m) { return static_cast<KeyMod>(m); }
    constexpr Dimensionality from_c(CImgDimensionality d) { return static_cast<Dimensionality>(static_cast<int>(d)); }
    constexpr FitMode        from_c(CImgFitMode f) { return static_cast<FitMode>(static_cast<int>(f)); }
    constexpr OrbitStyle     from_c(CImgOrbitStyle o) { return static_cast<OrbitStyle>(static_cast<int>(o)); }
    constexpr MouseButton    from_c_btns(int bits) { return static_cast<MouseButton>(bits); }
    ///@}

    /**
     * @class CImageDisplayerCPP
     * @brief RAII C++ wrapper around the C ABI. No rendering; forwards to C API.
     *
     * This wrapper creates/destroys the underlying C instance and offers typed
     * helpers. ABI remains stable because the boundary stays in C.
     */
    class CImageDisplayerCPP {
    public:
        /// @brief Construct and allocate an instance. Throws @c std::bad_alloc on failure.
        CImageDisplayerCPP() {
            h_ = cimgCreate();
            if (!h_) throw std::bad_alloc();
        }

        /// @brief Destroy the owned instance.
        ~CImageDisplayerCPP() { cimgDestroy(h_); }

        CImageDisplayerCPP(const CImageDisplayerCPP&) = delete;
        CImageDisplayerCPP& operator=(const CImageDisplayerCPP&) = delete;

        /// @brief Move construct, transferring ownership.
        CImageDisplayerCPP(CImageDisplayerCPP&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }
        /// @brief Move assign, destroying any currently owned handle.
        CImageDisplayerCPP& operator=(CImageDisplayerCPP&& o) noexcept {
            if (this != &o) { cimgDestroy(h_); h_ = o.h_; o.h_ = nullptr; }
            return *this;
        }

        /**
         * @brief Set image from @ref csh_img::CSH_Image with copy semantics.
         * @param img  Source image.
         * @param mode Copy mode (MetaOnly/Shallow/Deep). Default Shallow.
         */
        void setImage(const csh_img::CSH_Image& img, csh_img::CopyMode mode = csh_img::CopyMode::Shallow) {
            const auto fmt = static_cast<CImgFormat>(static_cast<uint32_t>(img.getFormat()));
            const auto pat = static_cast<CImgPattern>(static_cast<uint32_t>(img.getPattern()));
            const auto alg = static_cast<CImgAlign>(static_cast<uint32_t>(img.getMemoryAlign()));

            const void* data = nullptr;
            size_t bytes = 0;
            if (mode != csh_img::CopyMode::MetaOnly) {
                data = img.data();
                bytes = img.getBufferSize();
            }
            cimgSetImageRaw(h_, img.getWidth(), img.getHeight(), fmt, pat, alg, data, bytes,
                static_cast<CImgCopyMode>(static_cast<uint32_t>(mode)));
        }

        /**
         * @brief Set image from a raw pointer (when not using CSH_Image).
         * @param w,h   Dimensions in pixels.
         * @param fmt   Format.
         * @param pat   Pattern / channel order.
         * @param align Memory packing/alignment.
         * @param pixels Pointer to buffer (nullable for MetaOnly).
         * @param bytes  Total buffer size.
         * @param mode   Copy semantics.
         */
        void setImageRaw(uint32_t w, uint32_t h,
            CImgFormat fmt, CImgPattern pat, CImgAlign align,
            const void* pixels, size_t bytes, CImgCopyMode mode) {
            cimgSetImageRaw(h_, w, h, fmt, pat, align, pixels, bytes, mode);
        }

        // Viewport / fit / mode

        /// @brief Set viewport size in pixels.
        void setViewport(int w, int h) { cimgSetViewport(h_, w, h); }
        /// @brief Set fit strategy (None/Fit/Fill/Stretch).
        void setFitMode(FitMode m) { cimgSetFitMode(h_, to_c(m)); }
        /// @brief Switch between 2D and 3D modes.
        void setDimensionality(Dimensionality d) { cimgSetDimensionality(h_, to_c(d)); }

        // 2D

        /// @brief Set normalized anchor inside image rect [0..1]².
        void set2DAnchor(float ax, float ay) { cimg2D_SetAnchor(h_, ax, ay); }
        /// @brief Set 2D translation in viewport pixels.
        void set2DTranslation(float tx, float ty) { cimg2D_SetTranslation(h_, tx, ty); }
        /// @brief Set 2D scale factors.
        void set2DScale(float sx, float sy) { cimg2D_SetScale(h_, sx, sy); }
        /// @brief Set rotation in degrees (CCW).
        void set2DRotationDeg(float deg) { cimg2D_SetRotationDeg(h_, deg); }
        /// @brief Reset 2D transform to identity.
        void reset2D() { cimg2D_Reset(h_); }

        // 3D

        /// @brief Set model translation.
        void set3DModelTranslate(float x, float y, float z) { cimg3D_SetModelTranslate(h_, x, y, z); }
        /// @brief Set model scale.
        void set3DModelScale(float x, float y, float z) { cimg3D_SetModelScale(h_, x, y, z); }
        /// @brief Set model rotation (quaternion).
        void set3DModelRotationQuat(float w, float x, float y, float z) { cimg3D_SetModelRotationQuat(h_, w, x, y, z); }
        /// @brief Reset model transform to identity.
        void reset3DModel() { cimg3D_ResetModel(h_); }
        /// @brief Set look-at target.
        void set3DTarget(float x, float y, float z) { cimg3D_SetTarget(h_, x, y, z); }
        /// @brief Set camera eye.
        void set3DEye(float x, float y, float z) { cimg3D_SetEye(h_, x, y, z); }
        /// @brief Set camera up vector.
        void set3DUp(float x, float y, float z) { cimg3D_SetUp(h_, x, y, z); }
        /// @brief Set orbit interaction style.
        void set3DOrbitStyle(CImgOrbitStyle s) { cimg3D_SetOrbitStyle(h_, s); }

        /// @brief Set orthographic projection.
        void setOrtho(float l, float r, float b, float t, float n, float f) { cimgProj_SetOrtho(h_, l, r, b, t, n, f); }
        /// @brief Set perspective projection (fovy in degrees).
        void setPerspective(float fovyDeg, float aspect, float zNear, float zFar) { cimgProj_SetPerspective(h_, fovyDeg, aspect, zNear, zFar); }

        // Matrices

        /// @brief Fetch current 2D model (row-major 3×3).
        void model2D_3x3(float outRowMajor3x3[9]) const { cimgGetModel2D_3x3(h_, outRowMajor3x3); }
        /// @brief Fetch current 3D model (column-major 4×4).
        void model3D_4x4(float outColMajor4x4[16]) const { cimgGetModel3D_4x4(h_, outColMajor4x4); }
        /// @brief Fetch current 3D view (column-major 4×4).
        void view3D_4x4(float outColMajor4x4[16])  const { cimgGetView3D_4x4(h_, outColMajor4x4); }
        /// @brief Fetch current projection (column-major 4×4).
        void proj_4x4(float outColMajor4x4[16])    const { cimgGetProj_4x4(h_, outColMajor4x4); }
        /// @brief Fetch current MVP = P*V*M (column-major 4×4).
        void mvp3D_4x4(float outColMajor4x4[16])   const { cimgGetMVP3D_4x4(h_, outColMajor4x4); }

        // Geometry

        /// @brief Transformed quad as 2D tri-strip with UVs ({x,y,u,v} per vertex).
        void triStrip2D_XYUV(float out4x4[16]) const { cimgTriStrip2D_XYUV(h_, out4x4); }
        /// @brief Unit quad in object space for 3D pipelines ({x,y,u,v} per vertex).
        static void triStrip3D_XYUV_ObjectSpace(float out4x4[16]) { cimgTriStrip3D_XYUV_ObjectSpace(out4x4); }

        // Upload

        /// @brief Current upload descriptor (pointer/size/stride/layout).
        CImageUploadDesc uploadDesc() const { CImageUploadDesc d{}; cimgGetUploadDesc(h_, &d); return d; }

        // Input hooks

        /// @brief Begin a pointer interaction.
        void beginPointer(float x, float y, MouseButton btn, KeyMod keyMods) { cimgBeginPointer(h_, x, y, to_c(btn), to_c(keyMods)); }
        /// @brief Update pointer position during active interaction.
        void updatePointer(float x, float y) { cimgUpdatePointer(h_, x, y); }
        /// @brief End current pointer interaction.
        void endPointer() { cimgEndPointer(h_); }
        /**
         * @brief Mouse wheel / trackpad zoom or dolly.
         * @param delta   Positive for zoom/dolly in.
         * @param cx,cy   Cursor at event time (pixels).
         */
        void wheelScroll(float delta, float cx, float cy) { cimgWheelScroll(h_, delta, cx, cy); }

        /// @brief Keyboard panning in 2D mode (pixels).
        void keyPan2D(float dx, float dy) { cimgKeyPan2D(h_, dx, dy); }
        /// @brief Keyboard dolly in 3D mode (world units along view).
        void keyDolly3D(float amount) { cimgKeyDolly3D(h_, amount); }

        /// @brief Expose raw C handle for low-level interop.
        CImageDisplayerHandle raw() const noexcept { return h_; }

    private:
        CImageDisplayerHandle h_{ nullptr }; ///< Owned C handle.
    };

} // namespace cimage
#pragma once
