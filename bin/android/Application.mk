
# Uncomment this if you're using STL in your project
# You can find more information here:
# https://developer.android.com/ndk/guides/cpp-support
APP_STL := c++_shared

APP_ABI := armeabi-v7a arm64-v8a x86_64
APP_CFLAGS += -Os
NDK_TOOLCHAIN_VERSION := 4.9

# Min runtime API level
APP_PLATFORM=android-16

# For 16K page size
APP_SUPPORT_FLEXIBLE_PAGE_SIZES := TRUE

