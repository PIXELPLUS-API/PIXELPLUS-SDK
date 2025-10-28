#pragma once
// ============================================================================
// GLFWImageWindow.h  (GLFW + GLAD/GLES)
// ----------------------------------------------------------------------------
// A self-contained GLFW + GLAD(GLES) window/renderer that draws an image
// managed by cimage::CImageDisplayerCPP. The class owns all GL/GLFW objects
// (window, VAO/VBO, shader program, texture) and exposes a simple lifecycle:
//   - initialize()
//   - mainLoop()
//   - shutdown()
//
// External threads (e.g., camera capture) can signal new frames via an atomic
// flag; when the window loop sees the flag, it pulls the latest CSH_Image
// (protected by a mutex) into the viewer and uploads to a GL texture.
// ----------------------------------------------------------------------------
// NOTE:
//  - This version uses OpenGL ES 3.1 and GLAD as the function loader.
//  - GLEW has been removed because it targets desktop GL (GLX/WGL) and
//    misbehaves on Wayland/EGL paths typical on Raspberry Pi OS Bookworm.
// ============================================================================

#include <atomic>
#include <mutex>
#include <vector>
#include <cstdint>

// 3rd-party
#include <GLFW/glfw3.h>     // window + context (creates EGL/GLES on Wayland)
// GLAD for OpenGL ES (generate with: glad --api="gles2=3.1" --generator=c)
#include <include/glad/glad.h>

// Your SDK
#include <../CImageDisplayer/CImageDisplayerCPP.h>
#include <CSH_Image.h>

class GLFWImageWindow {
public:
    GLFWImageWindow(
        cimage::CImageDisplayerCPP& view,
        csh_img::CSH_Image& sharedCameraImage,
        std::mutex& sharedImageMutex,
        std::atomic<bool>& hasNewFrameFlag,
        std::atomic<bool>& shuttingDownFlag);

    ~GLFWImageWindow();

    bool initialize(const char* title = "CImageDisplayer - GLFW + GLES",
                    int width = 1280, int height = 720);

    void mainLoop();
    void shutdown();

    void setFitMode(cimage::FitMode m);
    void reset2D();
    GLFWwindow* window() const { return win_; }

private:
    // ==== Data references provided by the app (not owned here) ====
    cimage::CImageDisplayerCPP& view_;
    csh_img::CSH_Image&  cameraImage_;   // latest frame buffer (producer-owned)
    std::mutex&          cameraMtx_;     // protects cameraImage_
    std::atomic<bool>&   hasNewFrame_;   // producer signals "new frame available"
    std::atomic<bool>&   shuttingDown_;  // global shutdown fence

    // ==== Window / framebuffer state ====
    GLFWwindow* win_ = nullptr;
    int fbW_ = 1280;
    int fbH_ = 720;

    // ==== GL objects ====
    GLuint prog_ = 0;
    GLint  uViewport_ = -1; // vec2
    GLint  uTex_ = -1;
    GLuint vao_ = 0, vbo_ = 0;
    GLuint tex_ = 0;
    // Persisted texture state so we don’t reallocate every frame
    int    texW_ = 0, texH_ = 0;
    GLenum texInternal_ = 0, texFormat_ = 0, texType_ = 0;
    bool   texAllocated_ = false;

    // mouse state (for anchored zoom)
    double lastX_ = 0.0, lastY_ = 0.0;

    // ---- Internal helpers ----
    bool   initGLObjects_();
    static GLuint makeShader_(GLenum type, const char* src);
    static GLuint makeProgram_(const char* vs, const char* fs);
    void   uploadTextureFromView_();

    // ---- GLFW callbacks (static trampolines -> member functions) ----
    static void framebufferSizeCB_(GLFWwindow* w, int width, int height);
    static void cursorPosCB_(GLFWwindow* w, double x, double y);
    static void mouseButtonCB_(GLFWwindow* w, int button, int action, int mods);
    static void scrollCB_(GLFWwindow* w, double xoff, double yoff);
    static void keyCB_(GLFWwindow* w, int key, int sc, int action, int mods);

    void onFramebufferResized_(int width, int height);
    void onCursorMoved_(double x, double y);
    void onMouseButton_(int button, int action, int mods);
    void onScrolled_(double xoff, double yoff);
    void onKey_(int key, int action);

    // Utilities to translate GLFW enums to your viewer enums.
    static cimage::MouseButton btnFromGlfw_(int b);
    static uint32_t            keymodsFromGlfw_(int mods);
};
