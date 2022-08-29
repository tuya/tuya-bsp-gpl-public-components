
LOCAL_PATH:= $(call my-dir)
################## build iwlib ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := iwlib.c
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libiw
LOCAL_STATIC_LIBRARIES := libcutils libc libm
include $(BUILD_STATIC_LIBRARY)
################## build iwconfig ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := iwconfig.c
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= iwconfig
LOCAL_STATIC_LIBRARIES := libcutils libc libm libiw
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES) # install to system/xbin
include $(BUILD_EXECUTABLE)
################## build iwlist ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := iwlist.c
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= iwlist
LOCAL_STATIC_LIBRARIES := libcutils libc libm libiw
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES) # install to system/xbin
include $(BUILD_EXECUTABLE)
################## build iwpriv ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := iwpriv.c
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= iwpriv
LOCAL_STATIC_LIBRARIES := libcutils libc libm libiw
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES) # install to system/xbin
include $(BUILD_EXECUTABLE)
################## build iwspy ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := iwspy.c
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= iwspy
LOCAL_STATIC_LIBRARIES := libcutils libc libm libiw
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES) # install to system/xbin
include $(BUILD_EXECUTABLE)
################## build iwgetid ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := iwgetid.c
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= iwgetid
LOCAL_STATIC_LIBRARIES := libcutils libc libm libiw
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES) # install to system/xbin
include $(BUILD_EXECUTABLE)
################## build iwevent ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := iwevent.c
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= iwevent
LOCAL_STATIC_LIBRARIES := libcutils libc libm libiw
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES) # install to system/xbin
include $(BUILD_EXECUTABLE)
################## build ifrename ###################
#include $(CLEAR_VARS)
#LOCAL_SRC_FILES := ifrename.c
#LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
#LOCAL_MODULE_TAGS := optional
#LOCAL_MODULE:= ifrename
#LOCAL_STATIC_LIBRARIES := libcutils libc libm libiw
#LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES) # install to system/xbin
#include $(BUILD_EXECUTABLE)
################## build macaddr ###################
include $(CLEAR_VARS)
LOCAL_SRC_FILES := macaddr.c
LOCAL_CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wpointer-arith -Wcast-qual -Winline -MMD -fPIC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= macaddr
LOCAL_STATIC_LIBRARIES := libcutils libc libm libiw
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES) # install to system/xbin
include $(BUILD_EXECUTABLE)
