LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libbbc-prebuilt

ifeq ($(TARGET_ARCH),x86)

LOCAL_SRC_FILES := bbclibs.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

endif

LOCAL_MODULE := main

SDL_PATH := ../SDL

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

ifeq ($(TARGET_ARCH),x86)

LOCAL_SRC_FILES := bbcvdu.c bbcvtx.c flood.c bbcsdl.c SDL2_gfxPrimitives.c SDL2_rotozoom.c

LOCAL_STATIC_LIBRARIES := libbbc-prebuilt
LOCAL_LDLIBS += -Wl,--no-warn-shared-textrel

endif

ifeq ($(TARGET_ARCH),x86_64)

LOCAL_SRC_FILES := bbdata_x86_64.asm bbmain.c bbexec.c bbeval.c bbcmos.c bbccli.c bbcvdu.c \
	bbcvtx.c flood.c bbcsdl.c bbasmb_x86_64.c SDL2_gfxPrimitives.c SDL2_rotozoom.c

endif

ifeq ($(TARGET_ARCH),arm)

LOCAL_SRC_FILES := bbdata_arm_32.s bbmain.c bbexec.c bbeval.c bbcmos.c.neon bbccli.c bbcvdu.c \
	bbcvtx.c flood.c bbcsdl.c bbasmb_arm_32.c SDL2_gfxPrimitives.c SDL2_rotozoom.c sort.c

LOCAL_CFLAGS := -munaligned-access -fsigned-char

endif

ifeq ($(TARGET_ARCH),arm64)

LOCAL_SRC_FILES := bbdata_arm_64.s bbmain.c bbexec.c bbeval.c bbcmos.c.neon bbccli.c bbcvdu.c \
	bbcvtx.c flood.c bbcsdl.c bbasmb_arm_64.c SDL2_gfxPrimitives.c SDL2_rotozoom.c sort.c

LOCAL_CFLAGS := -fsigned-char

endif

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_ttf SDL2_net Box2D

include $(BUILD_SHARED_LIBRARY)
