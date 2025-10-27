# ===== shimcheong/Makefile =====
# Usage:
#   make                                 # Release (default)
#   make debug                           # Debug
#   make clean                           # clean current config
# Notes:
#   - Config-separated objects: build/Release, build/Debug
#   - Links utility/libs from ../lib with proper _d suffix in Debug
#   - rpath(Linux): $ORIGIN/../lib
#   - You can pass preprocessor defines via CPPDEFS
#   - Recipe prefix set to '>' to avoid hard tab issues

.RECIPEPREFIX := >

# -------- Load shared settings (optional) --------
-include ../build/settings.mk
-include ../build/local.mk

# -------- Build config --------
BUILD ?= Release
ifeq ($(BUILD),Debug)
  OPTFLAGS   := -O0 -g
  DBG_SUFFIX := _d
  CONFIG_DIR := Debug
else
  OPTFLAGS   := -O2 -DNDEBUG
  DBG_SUFFIX :=
  CONFIG_DIR := Release
endif

# -------- Toolchain --------
CXX ?= g++

# -------- OS detection --------
ifeq ($(OS),Windows_NT)
  TARGET_OS := windows
else
  TARGET_OS := linux
endif

# -------- Paths --------
INC_DIR      := ../inc
DELIVER_LIB  := ../lib
BUILD_ROOT   := build
BUILD_DIR    := $(BUILD_ROOT)/$(CONFIG_DIR)

# -------- Outputs --------
EXE := shimcheong$(DBG_SUFFIX)

# ---------- OpenCV detection ----------
PKGCONF ?= pkg-config
ifeq ($(strip $(OPENCV_LIBS)$(OPENCV_CFLAGS)),)
  ifeq ($(shell $(PKGCONF) --exists opencv4 && echo yes || $(PKGCONF) --exists opencv && echo yes),yes)
    OPENCV_CFLAGS := $(shell $(PKGCONF) --cflags opencv4 2>/dev/null || $(PKGCONF) --cflags opencv 2>/dev/null)
    OPENCV_LIBS   := $(shell $(PKGCONF) --libs   opencv4 2>/dev/null || $(PKGCONF) --libs   opencv   2>/dev/null)
    HAVE_OPENCV := yes
  else
    HAVE_OPENCV := no
  endif
else
  HAVE_OPENCV := yes
endif

ifeq ($(HAVE_OPENCV),yes)
  CXXFLAGS += -DCSH_IMAGE_WITH_OPENCV $(OPENCV_CFLAGS)
  LDLIBS   += $(OPENCV_LIBS)
endif
# ---------- GLEW detection ----------
ifeq ($(strip $(GLEW_CFLAGS)$(GLEW_LIBS)),)
  ifeq ($(shell $(PKGCONF) --exists glew && echo yes),yes)
    GLEW_CFLAGS := $(shell $(PKGCONF) --cflags glew)
    GLEW_LIBS   := $(shell $(PKGCONF) --libs   glew)   # typically -lGLEW
    HAVE_GLEW := yes
  else
    HAVE_GLEW := no
  endif
else
  HAVE_GLEW := yes
endif

ifeq ($(HAVE_GLEW),yes)
  CXXFLAGS += $(GLEW_CFLAGS)
  LDLIBS   += $(GLEW_LIBS)
else
  # Fallback if no pkg-config for GLEW:
  LDLIBS   += -lGLEW
endif

# Some distros' glfw3.pc doesn't add -lGL; add a safe fallback:
LDLIBS += -lGL

# ---------- GLFW detection ----------
PKGCONF ?= pkg-config
ifeq ($(strip $(GLFW_CFLAGS)$(GLFW_LIBS)),)
  ifeq ($(shell $(PKGCONF) --exists glfw3 && echo yes),yes)
    GLFW_CFLAGS := $(shell $(PKGCONF) --cflags glfw3)
    GLFW_LIBS   := $(shell $(PKGCONF) --libs   glfw3)
    HAVE_GLFW := yes
  else
    HAVE_GLFW := no
  endif
else
  HAVE_GLFW := yes
endif

ifeq ($(HAVE_GLFW),yes)
  CXXFLAGS += $(GLFW_CFLAGS)
  LDLIBS   += $(GLFW_LIBS)
else
  # Fallback if pkg-config for GLFW isn't available
  LDLIBS   += -lglfw
endif

# -------- Preprocessor defines --------
# NOTE: Do NOT export from an application. CSH_IMAGE_EXPORT is for building the DLL/SO only.
# If the app needs OpenCV integration paths in headers, keep only CSH_IMAGE_WITH_OPENCV here.
APP_CPPDEFS := -DCSH_IMAGE_WITH_OPENCV
# user-provided CPPDEFS can append
CPPDEFS := $(APP_CPPDEFS) $(CPPDEFS)

# -------- Flags --------
CPPFLAGS ?= -MMD -MP -I"$(INC_DIR)" -I"$(INC_DIR)/include" $(OPENCV_INC) $(CPPDEFS)
CXXFLAGS ?= -std=c++17 -Wall -Wextra $(OPTFLAGS)

# ----- Link setup (Linux default) -----
# Library order matters on Linux: place higher-level first, dependencies later.
# App -> IPM -> (plugin & utils) -> OpenCV/pthread
LDFLAGS  ?= -L"$(DELIVER_LIB)" $(OPENCV_LIBD) -Wl,--no-as-needed -Wl,-rpath,'$$ORIGIN/../lib'
LDLIBS += \
  -Wl,--start-group \
    -lImageProcessorManager$(DBG_SUFFIX) \
    -lCIpmUserCustom$(DBG_SUFFIX) \
    -lCSH_Image$(DBG_SUFFIX) \
    -lSH_Log$(DBG_SUFFIX) \
    -lCWatchTime$(DBG_SUFFIX) \
    -lCFrameGrabber$(DBG_SUFFIX) \
    -lCImageDisplayer$(DBG_SUFFIX) \
  -Wl,--end-group \
  -pthread -ldl


# ----- Windows overrides -----
ifeq ($(TARGET_OS),windows)
  EXE := shimcheong$(DBG_SUFFIX).exe
  # Link against import libs in ../lib (MinGW g++ can consume .lib)
  LDFLAGS  := $(OPENCV_LIBD)
  LDLIBS   := \
    "$(DELIVER_LIB)/ImageProcessorManager$(DBG_SUFFIX).lib" \
    "$(DELIVER_LIB)/CIpmUserCustom$(DBG_SUFFIX).lib" \
    "$(DELIVER_LIB)/CSH_Image$(DBG_SUFFIX).lib" \
    "$(DELIVER_LIB)/CWatchTime$(DBG_SUFFIX).lib" \
    "$(DELIVER_LIB)/SH_Log$(DBG_SUFFIX).lib" \
    $(OPENCV_LINK)
endif

# -------- Sources / Objects (config-separated) --------
SRCS := shimcheong.cpp GLFWImageWindow.cpp
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# -------- Phony --------
.PHONY: all release debug clean help dirs

# Default target respects BUILD
ifeq ($(BUILD),Debug)
all: debug
else
all: release
endif

help:
> echo "Usage:"
> echo "  make [BUILD=Release|Debug]                       # defaults to Release"
> echo "  make debug                                       # Debug build"
> echo "  make clean                                       # clean current config (build/$(CONFIG_DIR))"
> echo "Notes:"
> echo "  - Links ../lib/lib{ImageProcessorManager,CIpmUserCustom,CSH_Image,CWatchTime,SH_Log}$(DBG_SUFFIX).so on Linux."
> echo "  - On Windows, links ../lib/*.lib import libraries with the same suffix."

release:
> $(MAKE) BUILD=Release $(EXE)

debug:
> $(MAKE) BUILD=Debug   $(EXE)

# Ensure build dir
dirs:
> mkdir -p "$(BUILD_DIR)"

# Compile (config-separated)
$(BUILD_DIR)/%.o: %.cpp | dirs
> $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Link
$(EXE): $(OBJS)
> $(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)
> echo "Built: $(EXE)"

# Clean current config only
clean:
> rm -rf "$(BUILD_DIR)" "$(EXE)" 2>/dev/null || true
> echo "Cleaned: $(CONFIG_DIR)"

# Auto deps
-include $(DEPS)
