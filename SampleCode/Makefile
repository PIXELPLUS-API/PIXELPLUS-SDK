# ===== shimcheong/Makefile =====
# Usage:
#   make                 # Release (default)
#   make debug           # Debug
#   make clean           # Clean current config

.RECIPEPREFIX := >

-include ../build/settings.mk
-include ../build/local.mk

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

CXX ?= g++

ifeq ($(OS),Windows_NT)
  TARGET_OS := windows
else
  TARGET_OS := linux
endif

INC_DIR      := ../inc
DELIVER_LIB  := ../lib
BUILD_ROOT   := build
BUILD_DIR    := $(BUILD_ROOT)/$(CONFIG_DIR)

EXE := shimcheong$(DBG_SUFFIX)

PKGCONF ?= pkg-config

# ---------- OpenCV detection ----------
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

# ---------- GLFW detection ----------
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
  LDLIBS   += -lglfw
endif

# ---------- GLES2 + EGL (Wayland/EGL on RPi OS) ----------
GLESV2_CFLAGS := $(shell $(PKGCONF) --cflags glesv2 2>/dev/null)
GLESV2_LIBS   := $(shell $(PKGCONF) --libs   glesv2 2>/dev/null)
EGL_CFLAGS    := $(shell $(PKGCONF) --cflags egl    2>/dev/null)
EGL_LIBS      := $(shell $(PKGCONF) --libs   egl    2>/dev/null)

CXXFLAGS += $(GLESV2_CFLAGS) $(EGL_CFLAGS)
LDLIBS   += $(GLESV2_LIBS)   $(EGL_LIBS)

ifeq ($(strip $(GLESV2_LIBS)),)
  LDLIBS += -lGLESv2
endif
ifeq ($(strip $(EGL_LIBS)),)
  LDLIBS += -lEGL
endif

# -------- Preprocessor defines --------
# IMPORTANT: prevent GLFW from including GL/glext.h to avoid macro clashes with GLAD
APP_CPPDEFS := -DCSH_IMAGE_WITH_OPENCV -DGLFW_INCLUDE_NONE
CPPDEFS := $(APP_CPPDEFS) $(CPPDEFS)

# -------- Flags --------
CPPFLAGS ?= -MMD -MP \
            -I"$(INC_DIR)" -I"$(INC_DIR)/include" \
            -Ithird_party \
            -Ithird_party/include \
            $(CPPDEFS)

CXXFLAGS ?= -std=c++17 -Wall -Wextra $(OPTFLAGS)


# ----- Link setup (Linux default) -----
LDFLAGS  ?= -L"$(DELIVER_LIB)" -Wl,--no-as-needed -Wl,-rpath,'$$ORIGIN/../lib'
LDLIBS   += \
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

ifeq ($(TARGET_OS),windows)
  EXE := shimcheong$(DBG_SUFFIX).exe
  LDFLAGS  :=
  LDLIBS   := \
    "$(DELIVER_LIB)/ImageProcessorManager$(DBG_SUFFIX).lib" \
    "$(DELIVER_LIB)/CIpmUserCustom$(DBG_SUFFIX).lib" \
    "$(DELIVER_LIB)/CSH_Image$(DBG_SUFFIX).lib" \
    "$(DELIVER_LIB)/CWatchTime$(DBG_SUFFIX).lib" \
    "$(DELIVER_LIB)/SH_Log$(DBG_SUFFIX).lib"
endif

# -------- Sources / Objects (config-separated) --------
# App sources
CPPSRCS := shimcheong.cpp \
           third_party/GLFWImageWindow.cpp

# GLAD source (your layout is third_party/src/glad.c)
GLAD_SRC := third_party/src/glad.c

CPPOBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CPPSRCS))
COBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(GLAD_SRC))
OBJS    := $(CPPOBJS) $(COBJS)

DEPS    := $(OBJS:.o=.d)

.PHONY: all release debug clean help

ifeq ($(BUILD),Debug)
all: debug
else
all: release
endif

help:
> echo "Usage:"
> echo "  make [BUILD=Release|Debug]"
> echo "  make debug"
> echo "  make clean"
> echo "Notes:"
> echo "  - Builds with GLFW + GLAD (GLES) + EGL (no GLEW)."
> echo "  - Using GLAD at third_party/src/glad.c and headers in third_party/include."

release:
> $(MAKE) BUILD=Release $(EXE)

debug:
> $(MAKE) BUILD=Debug   $(EXE)

# Compile C++ (auto-creates subdirs)
$(BUILD_DIR)/%.o: %.cpp
> mkdir -p "$(dir $@)"
> $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile C (GLAD)
$(BUILD_DIR)/%.o: %.c
> mkdir -p "$(dir $@)"
> $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Link
$(EXE): $(OBJS)
> $(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)
> echo "Built: $(EXE)"

clean:
> rm -rf "$(BUILD_DIR)" "$(EXE)" 2>/dev/null || true
> echo "Cleaned: $(CONFIG_DIR)"

-include $(DEPS)
