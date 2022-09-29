ifdef CONFIG_TUYA_DRIVER_MODULES
LOCAL_PATH := $(call my-dir)
LOCAL_ABS_PATH := $(abspath $(LOCAL_PATH))

include $(CLEAR_VARS)
LOCAL_MODULE := ty_driver
LOCAL_CFLAGS := -I$(LOCAL_ABS_PATH)/include/kernel -I$(LOCAL_ABS_PATH)/include/usr
LOCAL_CFLAGS += -Wno-date-time
LOCAL_SRC_FILES := src/common/ty_gpio.c src/ty_driver.c
ifeq ($(CONFIG_MOTOR_MODULE), y)
LOCAL_SRC_FILES += src/motor/ty_motor.c
endif
ifeq ($(CONFIG_COLORBAND_MODULE), y)
LOCAL_SRC_FILES += src/color_band/ty_color_band.c
endif
ifeq ($(CONFIG_HDMI_MODULE), y)
LOCAL_SRC_FILES += src/hdmi/ty_hdmi_i2c.c
endif
ifeq ($(CONFIG_SUBG_MODULE),y)
LOCAL_SRC_FILES += src/subg/ty_subg.c src/subg/radio/radio.c src/subg/radio/cmt_spi3.c src/subg/radio/cmt2300a.c src/subg/radio/cmt2300a_hal.c
endif
ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC115WE4),y)
LOCAL_SRC_FILES += src/config/ty_sc115_we4.c
endif
ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC133WN03),y)
LOCAL_SRC_FILES += src/config/ty_sc133_wn03.c
endif
ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC103WS03),y)
LOCAL_SRC_FILES += src/config/ty_sc103_ws03.c
endif
ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC114WK2V3),y)
LOCAL_SRC_FILES += src/config/ty_sc114_wk2v3.c
endif
ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC081WN03),y)
LOCAL_SRC_FILES += src/config/ty_sc081_wn03.c
endif
ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC081WN03V2),y)
LOCAL_SRC_FILES += src/config/ty_sc081_wn03v2.c
endif
ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC133WN03V2),y)
LOCAL_SRC_FILES += src/config/ty_sc133_wn03v2.c
endif
ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC103WS03V2),y)
LOCAL_SRC_FILES += src/config/ty_sc103_ws03v2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015WL2),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wl2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_XM6XXV200_SPINOR),y)
LOCAL_SRC_FILES+= src/config/ty_xm6xxv200_spinor.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015WL2_SSV6155),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wl2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015WU2),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wl2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015WL3),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wl2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015WL3_SSV6155),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wl2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015WU3),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wl2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015WU4),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wl2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015WU4V2),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wl2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015W14V2),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wz2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC082WS03),y)
LOCAL_SRC_FILES+= src/config/ty_sc082_ws03.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC034WNV3),y)
LOCAL_SRC_FILES+= src/config/ty_sc034_wnv3.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC183WN03),y)
LOCAL_SRC_FILES+= src/config/ty_sc183_wn03.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC016WZ02),y)
LOCAL_SRC_FILES+= src/config/ty_sc016_wz02.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC1110W14V2),y)
LOCAL_SRC_FILES+= src/config/ty_sc1110_w14v2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC0110W14V2),y)
LOCAL_SRC_FILES+= src/config/ty_sc0110_w14v2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_TH012WND2),y)
LOCAL_SRC_FILES+= src/config/ty_th012_wnd2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015W22V2),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_w22v2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_CSSC01W02),y)
LOCAL_SRC_FILES+= src/config/ty_cssc01_w02.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015WZ2),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wz2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015W12ZC),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_w12zc.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015WZ3),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wz3.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC015W13ZC),y)
LOCAL_SRC_FILES+= src/config/ty_sc015_wz3.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_BMB101W04),y)
LOCAL_SRC_FILES+= src/config/ty_bmb101_w04.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC055WS2),y)
LOCAL_SRC_FILES+= src/config/ty_sc055_ws2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_GENERAL),y)
LOCAL_SRC_FILES+= src/config/ty_general.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC085WN2),y)
LOCAL_SRC_FILES+= src/config/ty_sc085_wn2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC085WN3),y)
LOCAL_SRC_FILES+= src/config/ty_sc085_wn3.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_SC019W23V1),y)
LOCAL_SRC_FILES+= src/config/ty_sc019_w23v1.c
endif

ifeq ($(CONFIG_TUYA_PRODUCT_RELEASE_NAME), "sc116wz2")
LOCAL_SRC_FILES+= src/config/ty_sc116_wz2.c
endif

ifeq ($(CONFIG_TUYA_PRODUCT_RELEASE_NAME), "sc116wz3")
LOCAL_SRC_FILES+= src/config/ty_sc116_wz2.c
endif

ifeq ($(CONFIG_TUYA_TARGET_BOARD_MODEL_ZIGBEE_SCREENSYNC_GW),y)
LOCAL_SRC_FILES+= src/config/ty_zigbee_screensync_gw.c
endif

TUYA_DRIVER_CONFIG := $(notdir $(call qstrip,$(CONFIG_TUYA_DRIVER_CONFIG)))
ifdef CONFIG_TUYA_DRIVER_CONFIG
LOCAL_SRC_FILES += src/config/$(TUYA_DRIVER_CONFIG)
endif

include $(LOCAL_PATH)/src/*/local_sub.mk

include $(BUILD_KMOD)

endif
