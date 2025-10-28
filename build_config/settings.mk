# ==========================================
# build/settings.mk  (공통 기본 설정)
# 프로젝트 루트(= inc/lib/dll과 같은 레벨)
# ==========================================

# ---- Toolchain / Arch ----
# 환경에 따라 local.mk에서 override 가능
CROSS_COMPILE ?=
# 허용: x86_64 | aarch64 (비우면 자동감지)
ARCH ?=
ifeq ($(strip $(ARCH)),)
  # 자동감지 (리눅스/임베디드)
  UNAME_M := $(shell uname -m 2>/dev/null | tr A-Z a-z)
  ifeq ($(UNAME_M),amd64)
    ARCH := x86_64
  else ifeq ($(UNAME_M),x86_64)
    ARCH := x86_64
  else ifeq ($(UNAME_M),arm64)
    ARCH := aarch64
  else ifeq ($(UNAME_M),aarch64)
    ARCH := aarch64
  else
    $(warning Unknown arch '$(UNAME_M)'; defaulting to x86_64)
    ARCH := x86_64
  endif
endif

# ---- Compiler paths (필요시 override) ----
CXX ?= $(CROSS_COMPILE)g++
AR  ?= $(CROSS_COMPILE)ar

# Let local.mk (or sdk_build) provide OPENCV_CFLAGS/OPENCV_LIBS via pkg-config.
# Keep these empty by default so no stale paths leak in.
OPENCV_INCLUDE_DIR ?=
OPENCV_LIB_DIR     ?=
OPENCV_LIBS        ?=

# Don’t force OpenCV globally. Each module should add the define only if it
# actually has OpenCV flags (CSH_Image already does this correctly).
GLOBAL_CPPDEFS   ?=

# If OpenCV flags are present (provided by local.mk or sdk_build), enable the header API.
ifneq ($(strip $(OPENCV_LIBS)),)
  GLOBAL_CPPDEFS += -DCSH_IMAGE_WITH_OPENCV
endif
