#include <atomic>
#include <mutex>
#include <iostream>

#include <CWatchTime.h>
#include <CSH_Log.h>
#include <CSH_Image.h>
#include <CFrameGrabber/CFrameGrabber.h>
#include <CImageProcessMng.h>
#include <CIpmEnv.h>
#include <plugins/CIpmUserCustom/CIpmUserCustom.h>

#include "GLFWImageWindow.h"

// ============================================================================
// Shared globals
// ============================================================================
csh_img::CSH_Image           gCameraImage;
std::mutex                   gCameraMtx;
std::atomic<bool>            gHasNewFrame{ false };
std::atomic<bool>            gShuttingDown{ false };
cimage::CImageDisplayerCPP   gView;
CFrameGrabber                grab;
CImageProcessMng             ipm0, ipm1;

using namespace cshlog;

// ============================================================================
// Example_WatchTime
// ============================================================================
void Example_WatchTime() {
    CWatchTime wt;
    wt.start();
    int a = 10, b = 20, tmp;
    for (int i = 0; i < 100000000; i++) {
        tmp = a; a = b; b = tmp;
    }
    wt.stop();
    std::cout << "Elapsed: " << wt.GetMilliSecond() << " ms\n";
}

// ============================================================================
// Example_Log
// ============================================================================
void Example_Log() {
    CSH_Log::Init(L"logs", true, LogLevel::Trace, 64);
    LOG_WRITE(LogLevel::Info, L"Hello, %d devices connected", 5);
    LOG_WRITE_MSG(LogLevel::Error, L"Error: Failed to connect the device");
    CSH_Log::Instance().setLogLevel(LogLevel::Info);
    LOG_WRITE_MSG(LogLevel::Warn, L"Warning!");
}

// ============================================================================
// Example_Image
// ============================================================================
void Example_Image() {
#ifdef CSH_IMAGE_WITH_OPENCV
    cv::Mat mat = cv::imread("test.png", cv::IMREAD_COLOR);
    cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);

    // Shallow
    CSH_Image img_shallow(mat.rows, mat.cols, En_ImageFormat::RGB888, true);
    img_shallow.fromCvMat(mat, CopyMode::Shallow, En_ImageFormat::RGB888, En_ImagePattern::RGB);

    // Deep
    CSH_Image img_deep(mat.rows, mat.cols, En_ImageFormat::RGB888, true);
    img_deep.fromCvMat(mat, CopyMode::Deep, En_ImageFormat::RGB888, En_ImagePattern::RGB);

    cv::Mat v = img_shallow.toCvMat(); // shallow view
    cv::Mat deep = img_deep.toCvMat(true); // deep copy

    img_shallow.saveImage("test.ish");
#endif
}

// ============================================================================
// Helper: ensureAllocatedOrResize
// ============================================================================
void ensureAllocatedOrResize(csh_img::CSH_Image& dst, const csh_img::CSH_Image& src) {
    bool needRealloc =
        dst.getWidth() != src.getWidth() ||
        dst.getHeight() != src.getHeight() ||
        dst.getFormat() != src.getFormat() ||
        dst.getImageCount() != src.getImageCount();

    if (needRealloc || !dst.buffer) {
        csh_img::CSH_Image fresh(src.getWidth(), src.getHeight(), src.getFormat(), src.getImageCount());
        fresh.recomputeBufferSize();
        fresh.allocateBuffer();
        dst = std::move(fresh);
    }
}

// ============================================================================
// Example_Grabber
// ============================================================================
void Example_Grabber() {
    grab.SetBackend(CFrameGrabber::En_GrabberBackend::UVC);
    int count = 0; std::vector<std::string> names;
    if (!grab.GetConnected(count, names)) {
        std::cerr << "GetConnected failed\n"; return;
    }
    for (auto& n : names) std::cout << "Device: " << n << "\n";

    CGrabberConfig cfg;
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.fps = 30;
    cfg.strGrabberName = "TestUVC";
    grab.SetConfig(&cfg);
    
    if (!grab.Connect()) {
        std::cerr << "Config/Connect failed\n"; return;
    }

    auto cb = std::bind(
        &CImageProcessMng::onNewFrame,
        &ipm0,
        std::placeholders::_1
    );
    grab.RegisterCallbackProcessor(cb);

    grab.RegisterCallbackDisplayer([&](const csh_img::CSH_Image& img) {
        std::lock_guard<std::mutex> lk(gCameraMtx);
        ensureAllocatedOrResize(gCameraImage, img);
        if (gShuttingDown.load()) return;
        gCameraImage.copy(img, csh_img::CopyMode::Deep);
        gHasNewFrame.store(true);
        });

    if (!grab.GrabFrames()) {
        std::cerr << "GrabFrames failed\n"; return;
    }
}

// ============================================================================
// Example_VideoSaver
// (stubbed)
// ============================================================================
void Example_VideoSaver() {
    std::cout << "[VideoSaver] Not implemented yet.\n";
}

csh_img::CSH_Image in0(1920, 1080, En_ImageFormat::YUV422, true);
csh_img::CSH_Image out0(1920, 1080, En_ImageFormat::RGB888, true);
// ============================================================================
// Example_ImageProcessor
// ============================================================================
void Example_ImageProcessor() {
    using namespace ipm;
    using namespace csh_img;

    in0.camera_id = 0; in0.pattern = En_ImagePattern::UYVY;

    ipm0.addProcList(ipmcommon::EnProcessBackend::CPU_Serial, ipmcommon::EnIpmModule::Converter,
        static_cast<int>(CConverter::Ipm_Converter_Func::YUV422_8bit_To_RGB888), &in0, &out0, nullptr, nullptr);

    ipm0.registerDisplayerCallback([&](int camId, int step, const CSH_Image& img) {
        LOG_WRITE(cshlog::LogLevel::Info, L"Called");
        std::lock_guard<std::mutex> lk(gCameraMtx);
        ensureAllocatedOrResize(gCameraImage, img);
        if (gShuttingDown.load()) return;
        gCameraImage.copy(img, csh_img::CopyMode::Deep);
        gHasNewFrame.store(true);
    });


    ipm0.initialize();
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char** argv) {

    Example_WatchTime();

    Example_Log();

    Example_Image();

    Example_Grabber();

    Example_ImageProcessor();
    GLFWImageWindow window(gView, gCameraImage, gCameraMtx, gHasNewFrame, gShuttingDown);
    if (!window.initialize("CImageDisplayer - GLFW + GLEW", 1280, 720)) {
        std::cerr << "Window init failed\n"; return 1;
    }
    window.mainLoop();

    gShuttingDown.store(true);
    grab.RegisterCallbackDisplayer(nullptr);
    grab.StopGrabbing();
    grab.Disconnect();

    window.shutdown();

	//Example_VideoSaver(); // Not implemented yet.

    return 0;
}
