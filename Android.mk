LOCAL_PATH := $(call my-dir)

NX_INC_TOP		:= $(LOCAL_PATH)/include
NX_LIB_TOP		:= $(LOCAL_PATH)/lib/android

include $(CLEAR_VARS)
LOCAL_MODULE    := libnxgpusurf
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_C_INCLUDE := $(NX_INC_TOP)
LOCAL_SRC_FILES := /lib/android/libnxgpusurf.so
ifeq ($(ANDROID_VERSION), 9)
LOCAL_VENDOR_MODULE := true
endif
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)

ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION := $(firstword $(ANDROID_VERSION_STR))

#
#	Compile Options
#
LOCAL_CFLAGS 		:= -DANDROID -pthread -DGL_GLEXT_PROTOTYPES -DNX_PLATFORM_DRM_USER_ALLOC_USE -DNX_DEBUG

LOCAL_C_INCLUDES := \
	$(NX_INC_TOP)

LOCAL_C_INCLUDES +=	\
	external/libdrm \
	hardware/nexell/s5pxx18/gralloc	\
	frameworks/native/opengl/include	\
	$(NX_INC_TOP) \
	$(NX_INC_TOP)/drm \
	$(LOCAL_PATH)

LOCAL_LDFLAGS := -Wl,--rpath,\$${ORIGIN}/../../../vendor/lib -Wl

LOCAL_SHARED_LIBRARIES :=	\
	liblog 		\
	libutils \
	libcutils \
	libdrm \
	libnx_renderer \
	libnx_v4l2 \
	libnxgpusurf

LOCAL_SRC_FILES := \
	nx_gl_tools.cpp

LOCAL_LDLIBS += -lGLES_mali

LOCAL_32_BIT_ONLY := true

LOCAL_MODULE := libnx_gl_tools

ifeq ($(ANDROID_VERSION), 9)
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -DPIE
else
LOCAL_MODULE_TAGS := optional
endif

include $(BUILD_SHARED_LIBRARY)
