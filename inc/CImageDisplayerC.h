#pragma once
//
// cimage_displayer_c.h — Pure C ABI for CImageDisplayer
// Build this into your shared library alongside CImageDisplayer.cpp.
// Opaque handle + procedural functions. No C++ types leak.
//
// Note: enum values mirror csh_img enums numerically for ease of mapping.
//

#include <stddef.h>
#include <stdint.h>

/**
 * @file CImageDisplayerC.h
 * @brief C ABI wrapper for @c cimage::CImageDisplayer.
 *
 * The API exposes an opaque handle and simple functions to configure image
 * data, transforms and to query matrices/geometry/upload descriptors from C.
 */

#if defined(_WIN32) || defined(_WIN64)
#  if defined(CIMAGE_DISPLAYER_EXPORT)
#    define CIMAGE_C_API __declspec(dllexport)
#  else
#    define CIMAGE_C_API __declspec(dllimport)
#  endif
#else
#  define CIMAGE_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /** @brief Image formats (numeric values match csh_img::En_ImageFormat). */
    typedef enum {
        CIMG_FMT_Bayer8 = 100,
        CIMG_FMT_Gray8,
        CIMG_FMT_Bayer10 = 200, CIMG_FMT_Bayer12, CIMG_FMT_Bayer14, CIMG_FMT_Bayer16,
        CIMG_FMT_Gray10, CIMG_FMT_Gray12, CIMG_FMT_Gray14, CIMG_FMT_Gray16,
        CIMG_FMT_YUV422, CIMG_FMT_RGB565,
        CIMG_FMT_YUYV444 = 300, CIMG_FMT_RGB888, CIMG_FMT_BGR888
    } CImgFormat;

    /** @brief Pixel pattern / channel order. Mirrors csh_img::En_ImagePattern. */
    typedef enum {
        CIMG_PAT_RGGB = 0, CIMG_PAT_GRBG, CIMG_PAT_BGGR, CIMG_PAT_GBRG,
        CIMG_PAT_YUYV = 10, CIMG_PAT_UYVY, CIMG_PAT_YVYU, CIMG_PAT_VYUY,
        CIMG_PAT_RGB = 20, CIMG_PAT_BGR
    } CImgPattern;

    /** @brief Memory alignment / packing. Mirrors csh_img::En_ImageMemoryAlign. */
    typedef enum {
        CIMG_ALIGN_Packed = 0, CIMG_ALIGN_YYYYUUUUVVVV = 10, CIMG_ALIGN_YYYYVVVVUUUU,
        CIMG_ALIGN_UUUUVVVVYYYY, CIMG_ALIGN_VVVVUUUUYYYY, CIMG_ALIGN_RRRRGGGGBBBB = 20,
        CIMG_ALIGN_BBBBGGGGRRRR, CIMG_ALIGN_YYYYUVUV = 30, CIMG_ALIGN_YYYYVUVU
    } CImgAlign;

    /** @brief Copy semantics for image ingestion. */
    typedef enum { CIMG_COPY_MetaOnly = 0, CIMG_COPY_Shallow, CIMG_COPY_Deep } CImgCopyMode;

    /** @brief Dimensionality / fit / orbit enums mirror the C++ ones. */
    typedef enum { CIMG_DIM_2D = 0, CIMG_DIM_3D } CImgDimensionality;
    typedef enum { CIMG_FIT_None = 0, CIMG_FIT_Fit, CIMG_FIT_Fill, CIMG_FIT_Stretch } CImgFitMode;
    typedef enum { CIMG_ORBIT_Arcball = 0, CIMG_ORBIT_Turntable } CImgOrbitStyle;

    /** @name Mouse buttons (bitmask) */
    /**@{*/
    typedef uint32_t CImgMouseButton;
#define CIMG_BTN_None   0u
#define CIMG_BTN_Left   1u
#define CIMG_BTN_Middle 2u
#define CIMG_BTN_Right  4u
    /**@}*/

    /** @name Keyboard modifiers (bitmask) */
    /**@{*/
    typedef uint32_t CImgKeyMod;
#define CIMG_KMOD_NONE  0u
#define CIMG_KMOD_SHIFT 1u
#define CIMG_KMOD_CTRL  2u
#define CIMG_KMOD_ALT   4u
    /**@}*/

    /** @brief Opaque instance handle for the displayer. */
    typedef struct CImageDisplayerHandle_t* CImageDisplayerHandle;

    /**
     * @brief Upload descriptor (C layout) matching the C++ @c UploadDescriptor.
     * @note @c data may be NULL for MetaOnly copies.
     */
    typedef struct {
        const uint8_t* data;          ///< Pointer to buffer start (nullable).
        size_t         sizeBytes;     ///< Total buffer size in bytes.
        int32_t        width, height; ///< Dimensions in pixels.
        int32_t        bytesPerPixel; ///< Bytes per pixel if tightly packed.
        int32_t        strideBytes;   ///< Row pitch in bytes (0 if tightly packed).
        int32_t        layout;        ///< 0=Unknown,1=Gray8,2=RGB888,3=BGR888,4=YUV422,5=RGB565,6=Gray16,7=Bayer16
        int32_t        yuv422Pattern; ///< 0=YUYV,1=UYVY,2=YVYU,3=VYUY
        int32_t        isPacked;      ///< Boolean (0/1).
        int32_t        isLittleEndian16; ///< Boolean (0/1).
    } CImageUploadDesc;

    // ----- lifecycle -----

    /**
     * @brief Create a new displayer instance.
     * @return Opaque handle or NULL on allocation failure.
     */
    CIMAGE_C_API CImageDisplayerHandle cimgCreate(void);

    /**
     * @brief Destroy a previously created instance.
     * @param h Instance handle (nullable).
     */
    CIMAGE_C_API void cimgDestroy(CImageDisplayerHandle h);

    // ----- image set (raw) -----

    /**
     * @brief Provide/replace image data from a raw buffer.
     * @param h      Instance handle.
     * @param w,hgt  Dimensions in pixels.
     * @param fmt    Image format.
     * @param pat    Pixel pattern / order.
     * @param align  Memory alignment / packing.
     * @param pixels Pointer to buffer (nullable if @p mode is MetaOnly).
     * @param bytes  Size of @p pixels in bytes.
     * @param mode   Copy semantics (MetaOnly/Shallow/Deep).
     * @note Shallow does not take ownership; caller must keep buffer alive.
     */
    CIMAGE_C_API void cimgSetImageRaw(CImageDisplayerHandle h,
        uint32_t w, uint32_t hgt, CImgFormat fmt, CImgPattern pat, CImgAlign align,
        const void* pixels /*nullable*/, size_t bytes, CImgCopyMode mode);

    // ----- viewport / mode / fit -----

    /** @brief Set viewport size in pixels (non-negative). */
    CIMAGE_C_API void cimgSetViewport(CImageDisplayerHandle h, int32_t w, int32_t hgt);
    /** @brief Set fit strategy (None/Fit/Fill/Stretch). */
    CIMAGE_C_API void cimgSetFitMode(CImageDisplayerHandle h, CImgFitMode m);
    /** @brief Switch between 2D and 3D modes. */
    CIMAGE_C_API void cimgSetDimensionality(CImageDisplayerHandle h, CImgDimensionality d);

    // ----- 2D transform -----

    /** @brief Set normalized anchor inside image rect [0..1]². */
    CIMAGE_C_API void cimg2D_SetAnchor(CImageDisplayerHandle h, float ax, float ay);
    /** @brief Set 2D translation in viewport pixels. */
    CIMAGE_C_API void cimg2D_SetTranslation(CImageDisplayerHandle h, float tx, float ty);
    /** @brief Set 2D scale factors. */
    CIMAGE_C_API void cimg2D_SetScale(CImageDisplayerHandle h, float sx, float sy);
    /** @brief Set rotation in degrees (CCW). */
    CIMAGE_C_API void cimg2D_SetRotationDeg(CImageDisplayerHandle h, float deg);
    /** @brief Reset 2D transform to identity. */
    CIMAGE_C_API void cimg2D_Reset(CImageDisplayerHandle h);

    // ----- 3D transform / camera / projection -----

    /** @brief Set model translation. */
    CIMAGE_C_API void cimg3D_SetModelTranslate(CImageDisplayerHandle h, float x, float y, float z);
    /** @brief Set model scale. */
    CIMAGE_C_API void cimg3D_SetModelScale(CImageDisplayerHandle h, float x, float y, float z);
    /** @brief Set model rotation as quaternion (w,x,y,z). */
    CIMAGE_C_API void cimg3D_SetModelRotationQuat(CImageDisplayerHandle h, float w, float x, float y, float z);
    /** @brief Reset model transform to identity. */
    CIMAGE_C_API void cimg3D_ResetModel(CImageDisplayerHandle h);

    /** @brief Set look-at target. */
    CIMAGE_C_API void cimg3D_SetTarget(CImageDisplayerHandle h, float x, float y, float z);
    /** @brief Set camera eye. */
    CIMAGE_C_API void cimg3D_SetEye(CImageDisplayerHandle h, float x, float y, float z);
    /** @brief Set camera up vector. */
    CIMAGE_C_API void cimg3D_SetUp(CImageDisplayerHandle h, float x, float y, float z);
    /** @brief Set orbit interaction style. */
    CIMAGE_C_API void cimg3D_SetOrbitStyle(CImageDisplayerHandle h, CImgOrbitStyle s);

    /** @brief Set orthographic projection. */
    CIMAGE_C_API void cimgProj_SetOrtho(CImageDisplayerHandle h, float l, float r, float b, float t, float n, float f);
    /** @brief Set perspective projection (fovy in degrees). */
    CIMAGE_C_API void cimgProj_SetPerspective(CImageDisplayerHandle h, float fovyDeg, float aspect, float zNear, float zFar);

    // ----- matrices (outputs) -----

    /** @brief Fetch current 2D model (row-major 3×3). */
    CIMAGE_C_API void cimgGetModel2D_3x3(CImageDisplayerHandle h, float* outRowMajor3x3);
    /** @brief Fetch current 3D model (column-major 4×4). */
    CIMAGE_C_API void cimgGetModel3D_4x4(CImageDisplayerHandle h, float* outColMajor4x4);
    /** @brief Fetch current 3D view (column-major 4×4). */
    CIMAGE_C_API void cimgGetView3D_4x4(CImageDisplayerHandle h, float* outColMajor4x4);
    /** @brief Fetch current projection (column-major 4×4). */
    CIMAGE_C_API void cimgGetProj_4x4(CImageDisplayerHandle h, float* outColMajor4x4);
    /** @brief Fetch current MVP = P*V*M (column-major 4×4). */
    CIMAGE_C_API void cimgGetMVP3D_4x4(CImageDisplayerHandle h, float* outColMajor4x4);

    // ----- geometry helpers -----

    /**
     * @brief Transformed quad as 2D tri-strip with UVs.
     * @param[out] out4x4 Flattened [4][4] array in row-major {x,y,u,v} order per vertex (TL,TR,BL,BR).
     */
    CIMAGE_C_API void cimgTriStrip2D_XYUV(CImageDisplayerHandle h, float* out4x4 /*[4][4]*/);

    /**
     * @brief Unit quad in object space for 3D pipelines.
     * @param[out] out4x4 Flattened [4][4] array in row-major {x,y,u,v}.
     */
    CIMAGE_C_API void cimgTriStrip3D_XYUV_ObjectSpace(float* out4x4 /*[4][4]*/);

    // ----- upload descriptor -----

    /**
     * @brief Query the current upload descriptor (pointer/size/stride/layout).
     * @param h       Instance handle.
     * @param outDesc Output struct (must not be NULL).
     */
    CIMAGE_C_API void cimgGetUploadDesc(CImageDisplayerHandle h, CImageUploadDesc* outDesc);

    // ----- input hooks (for UI callbacks) -----

    /**
     * @brief Begin a pointer interaction.
     * @param h       Instance handle.
     * @param x,y     Cursor in viewport pixels.
     * @param btn     Mouse button bitmask.
     * @param keyMods Keyboard modifier bitmask.
     */
    CIMAGE_C_API void cimgBeginPointer(CImageDisplayerHandle h, float x, float y, uint32_t btn, uint32_t keyMods /*bitmask*/);

    /** @brief Update pointer position during active interaction. */
    CIMAGE_C_API void cimgUpdatePointer(CImageDisplayerHandle h, float x, float y);
    /** @brief End current pointer interaction. */
    CIMAGE_C_API void cimgEndPointer(CImageDisplayerHandle h);

    /**
     * @brief Mouse wheel / trackpad zoom or dolly.
     * @param delta   Positive for zoom in (2D) / dolly in (3D).
     * @param cursorX,cursorY Cursor at event time (pixels).
     */
    CIMAGE_C_API void cimgWheelScroll(CImageDisplayerHandle h, float delta, float cursorX, float cursorY);

    // ----- convenience keyboard -----

    /** @brief Keyboard panning in 2D mode (pixels). */
    CIMAGE_C_API void cimgKeyPan2D(CImageDisplayerHandle h, float dx, float dy);
    /** @brief Keyboard dolly in 3D mode (world units along view). */
    CIMAGE_C_API void cimgKeyDolly3D(CImageDisplayerHandle h, float amount);

#ifdef __cplusplus
} // extern "C"
#endif
