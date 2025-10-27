#include "GLFWImageWindow.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <vector>

// ============================ Shaders ========================================
static const char* kVS = R"(
#version 310 es
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
uniform vec2 uViewport; // (W,H)
out vec2 vUV;
void main(){
    vec2 ndc = vec2( (aPos.x/uViewport.x)*2.0 - 1.0, 1.0 - (aPos.y/uViewport.y)*2.0 );
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
}
)";

static const char* kFS = R"(
#version 310 es
precision mediump float;
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main(){
    FragColor = texture(uTex, vUV);
}
)";

// ======================== Public: ctor/dtor ==================================
GLFWImageWindow::GLFWImageWindow(
    cimage::CImageDisplayerCPP& view,
    csh_img::CSH_Image& sharedCameraImage,
    std::mutex& sharedImageMutex,
    std::atomic<bool>& hasNewFrameFlag,
    std::atomic<bool>& shuttingDownFlag)
    : view_(view)
    , cameraImage_(sharedCameraImage)
    , cameraMtx_(sharedImageMutex)
    , hasNewFrame_(hasNewFrameFlag)
    , shuttingDown_(shuttingDownFlag) {
}

GLFWImageWindow::~GLFWImageWindow() { shutdown(); }

// ======================== Public: initialize =================================
bool GLFWImageWindow::initialize(const char* title, int width, int height) {
    if (!glfwInit()) {
        std::fprintf(stderr, "[GLFW] init failed\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);

    win_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!win_) {
        std::fprintf(stderr, "[GLFW] window creation failed\n");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(win_);
    glfwSwapInterval(1); // vsync

    // If you keep GLEW, this is usually okay on Mesa; if it misbehaves on ES,
    // switch to glad/es or epoxy.
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::fprintf(stderr, "[GLEW] error: %s\n", (const char*)glewGetErrorString(err));
        return false;
    }
    glGetError(); // clear benign error from glewInit

    int w, h; glfwGetFramebufferSize(win_, &w, &h);
    fbW_ = std::max(1, w);
    fbH_ = std::max(1, h);
    glViewport(0, 0, fbW_, fbH_);
    view_.setViewport(fbW_, fbH_);

    glfwSetWindowUserPointer(win_, this);
    glfwSetFramebufferSizeCallback(win_, framebufferSizeCB_);
    glfwSetCursorPosCallback(win_, cursorPosCB_);
    glfwSetMouseButtonCallback(win_, mouseButtonCB_);
    glfwSetScrollCallback(win_, scrollCB_);
    glfwSetKeyCallback(win_, keyCB_);

    if (!initGLObjects_()) return false;

    view_.setDimensionality(cimage::Dimensionality::_2D);
    view_.setFitMode(cimage::FitMode::Fit);
    view_.reset2D();

    return true;
}

// ======================== Public: main loop ==================================
void GLFWImageWindow::mainLoop() {
    if (!win_) return;

    while (!glfwWindowShouldClose(win_)) {
        glfwPollEvents();

        if (hasNewFrame_.exchange(false, std::memory_order_acq_rel)) {
            std::lock_guard<std::mutex> lk(cameraMtx_);
            view_.setImage(cameraImage_, csh_img::CopyMode::Shallow);
            uploadTextureFromView_();
        }

        float strip[16]{ 0.0f };
        view_.triStrip2D_XYUV(strip);

        float quad[16] = {
            strip[0], strip[1], strip[2], strip[3],
            strip[4], strip[5], strip[6], strip[7],
            strip[8], strip[9], strip[10], strip[11],
            strip[12], strip[13], strip[14], strip[15],
        };

        glClearColor(0.12f, 0.12f, 0.14f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog_);
        glUniform2f(uViewport_, (float)fbW_, (float)fbH_);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_);

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        glfwSwapBuffers(win_);
    }
}

// ======================== Public: shutdown ===================================
void GLFWImageWindow::shutdown() {
    if (!win_) return;

    glfwSetWindowShouldClose(win_, GLFW_TRUE);

    if (tex_) { glDeleteTextures(1, &tex_); tex_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (prog_) { glDeleteProgram(prog_); prog_ = 0; }

    glfwDestroyWindow(win_);
    win_ = nullptr;
    glfwTerminate();
}

// ======================== Public: viewer helpers =============================
void GLFWImageWindow::setFitMode(cimage::FitMode m) { view_.setFitMode(m); }
void GLFWImageWindow::reset2D() { view_.reset2D(); }

// ======================== Private: init GL objects ===========================
bool GLFWImageWindow::initGLObjects_() {
    prog_ = makeProgram_(kVS, kFS);
    if (!prog_) return false;

    uViewport_ = glGetUniformLocation(prog_, "uViewport");
    uTex_      = glGetUniformLocation(prog_, "uTex");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0); // aPos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
    glEnableVertexAttribArray(1); // aUV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

    glBindVertexArray(0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glUseProgram(prog_);
    glUniform1i(uTex_, 0); // texture unit 0
    glUseProgram(0);

    return true;
}

// ======================== Private: shader utils ==============================
GLuint GLFWImageWindow::makeShader_(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok = GL_FALSE; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::fprintf(stderr, "[GL] shader compile error:\n%s\n", log.data());
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint GLFWImageWindow::makeProgram_(const char* vs, const char* fs) {
    GLuint v = makeShader_(GL_VERTEX_SHADER, vs);
    if (!v) return 0;
    GLuint f = makeShader_(GL_FRAGMENT_SHADER, fs);
    if (!f) { glDeleteShader(v); return 0; }

    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);

    GLint ok = GL_FALSE; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len > 1 ? len : 1);
        glGetProgramInfoLog(p, len, nullptr, log.data());
        std::fprintf(stderr, "[GL] program link error:\n%s\n", log.data());
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

// ======================== Private: texture upload ============================
void GLFWImageWindow::uploadTextureFromView_() {
    const auto d = view_.uploadDesc();
    if (!d.data || d.width <= 0 || d.height <= 0) {
        return; // nothing to upload
    }

    const auto fmt = cameraImage_.getFormat();

    // Decide GL storage/packing
    GLenum internalFormat = GL_RGB8;
    GLenum format         = GL_RGB;
    GLenum type           = GL_UNSIGNED_BYTE;

    bool swizzleGray = false;
    bool swizzleBGR  = false;

    switch (fmt) {
    case csh_img::En_ImageFormat::Gray8:
        internalFormat = GL_R8;  format = GL_RED; type = GL_UNSIGNED_BYTE; swizzleGray = true; break;
    case csh_img::En_ImageFormat::Gray16:
    case csh_img::En_ImageFormat::Gray10:
    case csh_img::En_ImageFormat::Gray12:
    case csh_img::En_ImageFormat::Gray14:
        // GLES 3.x has GL_R16. If some driver complains, downconvert to 8-bit on CPU.
        internalFormat = GL_R16; format = GL_RED; type = GL_UNSIGNED_SHORT; swizzleGray = true; break;
    case csh_img::En_ImageFormat::RGB888:
        internalFormat = GL_RGB8; format = GL_RGB; type = GL_UNSIGNED_BYTE; break;
    case csh_img::En_ImageFormat::BGR888:
        // Upload as RGB and swizzle R/B in the sampler state
        internalFormat = GL_RGB8; format = GL_RGB; type = GL_UNSIGNED_BYTE; swizzleBGR = true; break;
    case csh_img::En_ImageFormat::RGB565:
        internalFormat = GL_RGB;  format = GL_RGB; type = GL_UNSIGNED_SHORT_5_6_5; break;
    case csh_img::En_ImageFormat::YUV422:
        // Not handled here—convert before upload or use a YUV shader path.
        return;
    default:
        return;
    }

    if (tex_ == 0) glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Allocate only when needed (first time or size/format/type changed)
    const bool needAlloc =
        !texAllocated_ ||
        texW_ != d.width || texH_ != d.height ||
        texInternal_ != internalFormat || texFormat_ != format || texType_ != type;

    if (needAlloc) {
        // Allocate storage (no data yet)
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, d.width, d.height, 0, format, type, nullptr);

        // (Re)apply sampler params when reallocating
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Set swizzle once per allocation/change
        if (swizzleGray) {
            GLint swz[4] = { GL_RED, GL_RED, GL_RED, GL_ONE };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swz);
        } else if (swizzleBGR) {
            GLint swz[4] = { GL_BLUE, GL_GREEN, GL_RED, GL_ONE };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swz);
        } else {
            GLint swz[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ONE };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swz);
        }

        texAllocated_ = true;
        texW_ = d.width;  texH_ = d.height;
        texInternal_ = internalFormat; texFormat_ = format; texType_ = type;
    }

    // Fast path: just update the pixels
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, d.width, d.height, format, type, d.data);

    glBindTexture(GL_TEXTURE_2D, 0);
}


// ======================== Private: GLFW callbacks ============================
void GLFWImageWindow::framebufferSizeCB_(GLFWwindow* w, int width, int height) {
    if (auto* self = static_cast<GLFWImageWindow*>(glfwGetWindowUserPointer(w))) {
        self->onFramebufferResized_(width, height);
    }
}
void GLFWImageWindow::cursorPosCB_(GLFWwindow* w, double x, double y) {
    if (auto* self = static_cast<GLFWImageWindow*>(glfwGetWindowUserPointer(w))) {
        self->onCursorMoved_(x, y);
    }
}
void GLFWImageWindow::mouseButtonCB_(GLFWwindow* w, int button, int action, int mods) {
    if (auto* self = static_cast<GLFWImageWindow*>(glfwGetWindowUserPointer(w))) {
        self->onMouseButton_(button, action, mods);
    }
}
void GLFWImageWindow::scrollCB_(GLFWwindow* w, double xoff, double yoff) {
    if (auto* self = static_cast<GLFWImageWindow*>(glfwGetWindowUserPointer(w))) {
        self->onScrolled_(xoff, yoff);
    }
}
void GLFWImageWindow::keyCB_(GLFWwindow* w, int key, int sc, int action, int mods) {
    (void)sc; (void)mods;
    if (auto* self = static_cast<GLFWImageWindow*>(glfwGetWindowUserPointer(w))) {
        self->onKey_(key, action);
    }
}

// ---- Member handlers ---------------------------------------------------------
void GLFWImageWindow::onFramebufferResized_(int width, int height) {
    fbW_ = std::max(1, width);
    fbH_ = std::max(1, height);
    glViewport(0, 0, fbW_, fbH_);
    view_.setViewport(fbW_, fbH_);
}

void GLFWImageWindow::onCursorMoved_(double x, double y) {
    lastX_ = x; lastY_ = y;
    view_.updatePointer((float)x, (float)y);
}

void GLFWImageWindow::onMouseButton_(int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        view_.beginPointer((float)lastX_, (float)lastY_,
            btnFromGlfw_(button),
            (cimage::KeyMod)keymodsFromGlfw_(mods));
    } else if (action == GLFW_RELEASE) {
        view_.endPointer();
    }
}

void GLFWImageWindow::onScrolled_(double /*xoff*/, double yoff) {
    view_.wheelScroll((float)(yoff * 120.0), (float)lastX_, (float)lastY_);
}

void GLFWImageWindow::onKey_(int key, int action) {
    if (!(action == GLFW_PRESS || action == GLFW_REPEAT)) return;

    switch (key) {
    case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(win_, GLFW_TRUE); break;
    case GLFW_KEY_LEFT:   view_.keyPan2D(-10.f, 0.f); break;
    case GLFW_KEY_RIGHT:  view_.keyPan2D(10.f, 0.f);  break;
    case GLFW_KEY_UP:     view_.keyPan2D(0.f, -10.f); break;
    case GLFW_KEY_DOWN:   view_.keyPan2D(0.f, 10.f);  break;
    case GLFW_KEY_EQUAL:  view_.wheelScroll(+120.f, (float)lastX_, (float)lastY_); break;
    case GLFW_KEY_MINUS:  view_.wheelScroll(-120.f, (float)lastX_, (float)lastY_); break;
    default: break;
    }
}

// ---- Enum translators --------------------------------------------------------
cimage::MouseButton GLFWImageWindow::btnFromGlfw_(int b) {
    if (b == GLFW_MOUSE_BUTTON_LEFT)   return cimage::MouseButton::Left;
    if (b == GLFW_MOUSE_BUTTON_MIDDLE) return cimage::MouseButton::Middle;
    if (b == GLFW_MOUSE_BUTTON_RIGHT)  return cimage::MouseButton::Right;
    return cimage::MouseButton::None;
}

uint32_t GLFWImageWindow::keymodsFromGlfw_(int mods) {
    uint32_t m = 0;
    if (mods & GLFW_MOD_SHIFT)   m |= (uint32_t)cimage::KeyMod::Shift;
    if (mods & GLFW_MOD_CONTROL) m |= (uint32_t)cimage::KeyMod::Ctrl;
    if (mods & GLFW_MOD_ALT)     m |= (uint32_t)cimage::KeyMod::Alt;
    return m;
}
