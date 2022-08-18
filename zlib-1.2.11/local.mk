ifdef CONFIG_TUYA_PACKAGE_ZLIB

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PKG_NAME := zlib
LOCAL_PKG_SRC := $(call qstrip,$(CONFIG_TUYA_PACKAGE_ZLIB_VERSION)).tar.xz
LOCAL_PKG_EXPORT := YES
include $(BUILD_PACKAGE)

endif
