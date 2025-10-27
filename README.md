# Overview

The **PIXELPLUS API** is designed to help developers quickly integrate PIXELPLUS image sensors with their application processors (APs).  
It is a cross-platform (Windows and Linux), multi-paradigm (CPU serial, CPU parallel, and GPU parallel processing) API capable of handling multiple video device interfaces — currently supporting **UVC** and **V4L2**, with more to be added in the future.

The API provides the following key features:

* Capture image data from connected camera sensors or devices  
* Save still images or video streams  
* Manipulate image data (e.g., filtering, cropping, etc.)  
* Compute transformation matrices based on user input  
* Print and save device log information with configurable log levels  
* Measure elapsed time for performance benchmarking

---

# Package Contents

The API consists of **seven interdependent dynamic libraries**:

* **SH_Log**  
* **CWatchTime**  
* **CSH_Image**  
* **CFrameGrabber**  
* **CImageSaver** *(to be developed)*  
* **CImageDisplayer**  
* **ImageProcessorManager**

This repository also includes a **test project** demonstrating example use cases for each module.  
The test application uses [GLEW](https://glew.sourceforge.net/) and [GLFW](https://www.glfw.org/) for rendering and graphical user interface support.



# Installation

## Windows

The API relies on **OpenCV** across most modules.  
Make sure OpenCV is correctly installed and configured in your development environment.  
For more details, see the official [OpenCV installation guide](https://docs.opencv.org/4.x/d3/d52/tutorial_windows_install.html).

Download the latest release package (`.zip`) from the **GitHub Releases** page.  
Unpack the archive — you will find folders containing the header files and precompiled library binaries required to use the API.

If you are working exclusively on Windows, you can safely remove the Linux folders, and vice versa.

---

## Linux

Ensure that **OpenCV**, **GLEW**, and **GLFW** are installed on your system before building or running any examples.

For Ubuntu-based systems:
```bash
sudo apt update
sudo apt install libopencv-dev libglew-dev libglfw3-dev
```
Download and extract the same .zip release package from GitHub.
Copy the header and library folders to your preferred location, such as /usr/local/include and /usr/local/lib, or adjust your project’s include and library paths accordingly


# Usage Example
To demonstrate a simple GUI application, the repository includes a helper class named **GLFWImageWindow**, which creates an interactive rendering window capable of handling typical user inputs such as mouse and keyboard events.

This class depends on **GLFW** and **GLEW**, so ensure that both are properly installed following the aforementioned installation links.

## CWatchTime
The **CWatchTime** class provides a simple way to benchmark execution time.  
Below is an example of its usage:
```c++
CWatchTime wt;
wt.start();
int a = 10, b = 20, tmp;
for (int i = 0; i < 100000000; i++) {
    tmp = a; a = b; b = tmp;
}
wt.stop();
std::cout << "Elapsed: " << wt.GetMilliSecond() << " ms\n";
```

## CSH_Log
The CSH_Log class handles both console logging and persistent log files.
You can configure the log directory, enable or disable file output, and specify the minimum log level that should be written.
```c++
CSH_Log::Init(L"logs", true, LogLevel::Trace, 64);
LOG_WRITE(LogLevel::Info, L"Hello, %d devices connected", 5);
LOG_WRITE_MSG(LogLevel::Error, L"Error: Failed to connect the device");
CSH_Log::Instance().setLogLevel(LogLevel::Info);
LOG_WRITE_MSG(LogLevel::Warn, L"Warning!");
```

## CSH_Image
The CSH_Image class serves as an image container with metadata and flexible buffer ownership semantics.
It supports deep and shallow copies, TLV-based serialization, and OpenCV interoperability.

An image created with cv::Mat can be converted to or from a CSH_Image instance:
```c++
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
```

## CFrameGrabber
For a typical Windows PC with a webcam, set the backend to **UVC** — the webcam will then act as the video capture device.  
On Linux, assuming your application processor (AP) board has a camera connected via a MIPI interface, set the backend to **V4L2** instead.  
Then, scan for connected devices:
```c++
CFrameGrabber grab;
grab.SetBackend(CFrameGrabber::En_GrabberBackend::UVC);
int count = 0; std::vector<std::string> names;
if (!grab.GetConnected(count, names)) {
    std::cerr << "GetConnected failed\n"; return;
}
for (auto& n : names) std::cout << "Device: " << n << "\n";
```
Set up the configuration according to your device’s specifications:
```c++
CGrabberConfig cfg;
cfg.width = 1920;
cfg.height = 1080;
cfg.fps = 30;
cfg.strGrabberName = "TestUVC";
grab.SetConfig(&cfg);

if (!grab.Connect()) {
    std::cerr << "Config/Connect failed\n"; return;
}
```
If you have a callback function that should be triggered whenever a new frame is acquired, register it as follows:

```c++
auto cb = std::bind(
    &CImageProcessMng::onNewFrame,
    &ipm0,
    std::placeholders::_1
);
grab.RegisterCallbackProcessor(cb);
```
The function CFrameGrabber::RegisterCallbackProcessor is also capable of taking lambda expression as an input. Make sure your lambda follows the function's requirement.

```c++
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
```

## Image Processor Manager
For demonstration purposes, let's assume the backend is set to **V4L2**, and the connected camera outputs image data in **YUV422** format.  
To render the image in RGB, each pixel must be converted from YUV to RGB.  
The **ImageProcessorManager** library is designed to handle this task — and many others — efficiently.

Below is an example of how to perform such a conversion.

First, instantiate the necessary objects for the conversion:
```c++
CImageProcessMng ipm0;
csh_img::CSH_Image in0(1920, 1080, En_ImageFormat::YUV422, true);
csh_img::CSH_Image out0(1920, 1080, En_ImageFormat::RGB888, true);
in0.camera_id = 0; in0.pattern = En_ImagePattern::UYVY;
```

Next, configure the computation parameters (processor type, module, and function) and start the processing thread:
```c++
ipm0.addProcList(ipmcommon::EnProcessBackend::CPU_Serial, ipmcommon::EnIpmModule::Converter,
    static_cast<int>(CConverter::Ipm_Converter_Func::YUV422_8bit_To_RGB888), &in0, &out0, nullptr, nullptr);

ipm0.initialize();
```

## CImageDisplayerCPP (with GLFWImageWindow)
The CImageDisplayer class computes transformation matrices for displaying images based on user input.
Despite the name, it does not perform rendering directly and is therefore independent of any graphics library.

The actual rendering loop resides in the auxiliary class GLFWImageWindow, which takes:

* The image data from the CFrameGrabber callback

* The transformation matrix from the CImageDisplayer class

Together, they form the complete display pipeline.

Example render loop:

```c++
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

        // OpenGL Render stuff...
    }
}

```