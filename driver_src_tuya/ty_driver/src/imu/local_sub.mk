ifeq ($(CONFIG_IMU_MODULE),y)

LOCAL_SRC_FILES += \
		src/imu/ty_imu.c \
		src/imu/imu_dev.c \

include $(LOCAL_PATH)/src/imu/*/local_sub.mk

endif
