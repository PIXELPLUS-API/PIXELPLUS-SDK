# ==========================================
# build/local.mk  (개인/머신별 override)
# 예시: aarch64 크로스컴파일 + OpenCV 경로
# ==========================================


# ============== ubuntu x86_64 ==============
#CROSS_COMPILE := aarch64-linux-gnu-
#ARCH := aarch64

# Tell pkg-config where to find opencv4.pc if it’s under /usr/local
PKG_CONFIG_PATH := /usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:$(PKG_CONFIG_PATH)

# Use pkg-config to provide the exact flags the top Makefile expects
OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4)
OPENCV_LIBS   := $(shell pkg-config --libs   opencv4)

# ===========================================
