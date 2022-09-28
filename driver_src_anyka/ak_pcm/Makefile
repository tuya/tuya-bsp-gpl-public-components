########################################################################################################
#	@Copyright (C) 	Anyka 2019-6-10
#	@brief			Create Linux4.4.x linux driver module for AnyCloudv500
#	@Version		V1.0.0
#####################################GCC Compile Flags #################################################
EXTRA_CFLAGS += $(USER_EXTRA_CFLAGS)
EXTRA_CFLAGS += -O1

EXTRA_CFLAGS += -Wno-unused-variable
EXTRA_CFLAGS += -Wno-unused-value
EXTRA_CFLAGS += -Wno-unused-label
EXTRA_CFLAGS += -Wno-unused-parameter
EXTRA_CFLAGS += -Wno-unused-function
EXTRA_CFLAGS += -Wno-unused
EXTRA_CFLAGS += -fno-common

EXTRA_LDFLAGS += --strip-debug

#####################################GCC Objects #######################################################
# 定义当前的工作目录
$(info CURRENT DIR: $(shell pwd))
ifeq ($(KERNELRELEASE),)
	AK_PCM_DRV_DIR = $(shell pwd)
	export AK_PCM_DRV_DIR
endif

SRC_DIR = $(AK_PCM_DRV_DIR)
COMMON_DIR = common
OS_DIR = os
DEVICES_DIR = devices
CHIP_DIR = chip
CHIP_SET = ak39ev330
OS_TYPE = linux

EXTRA_CFLAGS += -Wall

ccflags-y += -I$(SRC_DIR)/include

SRCS :=
# common layer
SRCS += ${wildcard $(SRC_DIR)/$(COMMON_DIR)/*.c}
# SRCS += $(COMMON_DIR)/soundcard.c $(COMMON_DIR)/codec_adc_dac.c
ccflags-y += -I$(SRC_DIR)/$(COMMON_DIR)

# devices layer
SRCS += ${wildcard $(SRC_DIR)/$(DEVICES_DIR)/*.c}
# SRCS += $(DEVICES_DIR)/playback.c $(DEVICES_DIR)/capture.c $(DEVICES_DIR)/loopback.c
ccflags-y += -I$(SRC_DIR)/$(DEVICES_DIR)

# os layer
SRCS += ${wildcard $(SRC_DIR)/$(OS_DIR)/$(OS_TYPE)/*.c}
# SRCS += $(OS_DIR)/$(OS_TYPE)/ak_pcm_dev.c
# SRCS += $(OS_DIR)/$(OS_TYPE)/ak_pcm_sys.c
# SRCS += $(OS_DIR)/$(OS_TYPE)/os_common.c
# SRCS += $(OS_DIR)/$(OS_TYPE)/pcm_port.c
ccflags-y += -I$(SRC_DIR)/$(OS_DIR)/$(OS_TYPE)

# chip layer
ifeq ($(CONFIG_MACH_AK39EV330), y)
	CHIP_SET = ak39ev330
endif
ifeq ($(CONFIG_MACH_AK37D), y)
	CHIP_SET = ak37d
endif
ifeq ($(CONFIG_MACH_AK37E), y)
	CHIP_SET = ak37e
endif

SRCS += ${wildcard $(SRC_DIR)/$(CHIP_DIR)/$(CHIP_SET)/*.c}
# SRCS += $(CHIP_DIR)/$(CHIP_SET)/sub_codec_adc_dac.c

#添加前缀
#SRCS := $(addprefix "$(SRC_DIR)/", $(SRCS))
FULL_OBJS = ${patsubst %.c, %.o, ${SRCS}}
NONEPREFIX =
OBJS = ${subst $(SRC_DIR)/, $(NONEPREFIX), $(FULL_OBJS)}
# $(info GCC init OBJS: $(OBJS))

MODULE_NAME := akpcm_drv

#####################################Platform Related ###################################################

CONFIG_PLATFORM_ARM_ANYKA = y

ifeq ($(CONFIG_PLATFORM_ARM_ANYKA), y)
EXTRA_CFLAGS += -DCONFIG_LITTLE_ENDIAN
ARCH := arm
CROSS_COMPILE := arm-anykav500-linux-uclibcgnueabi-
KVER := $(shell uname -r)
ifndef KSRC
	KSRC := $(shell pwd)/../../bd
endif

AUTOCONF := $(KSRC)/include/config/auto.conf
FEXIST := $(shell if [ -f $(AUTOCONF) ]; then echo "exist"; else echo "noexist"; fi;)
ifeq ("$(FEXIST)", "exist")
	SOURCES = $(AUTOCONF)
	include $(SOURCES)
endif

endif

USER_MODULE_NAME ?= ak_pcm
ifneq ($(USER_MODULE_NAME),)
MODULE_NAME := $(USER_MODULE_NAME)
endif

#####################################Make Compile Module ########################################################

ifneq ($(KERNELRELEASE),)

# $(info GCC MODULE OBJS: $(OBJS))
# $(info GCC MODULE KERNELRELEASE: $(KERNELRELEASE))
# $(info GCC MODULE ccflags-y: $(ccflags-y))

$(MODULE_NAME)-objs := $(OBJS)
ifeq ($(CONFIG_ARCH_AK),y)
	obj-m := $(MODULE_NAME).o
endif

else

all: modules strip
	@echo "make all first step!"

modules:
	@echo "make modules!"
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(shell pwd) modules

strip:
	@echo "make strip!"
	@if [ -f $(MODULE_NAME).ko ]; then $(CROSS_COMPILE)strip $(MODULE_NAME).ko --strip-unneeded; fi;

.PHONY: modules clean test

clean:
	rm -rf *.o* *.ko* *.mod.c *.mod.* *.bak *.order *.symvers
	rm -rf .*.o.cmd .*.ko.cmd .*.mod.o.cmd
	rm -rf .tmp_versions
	rm -rf ./**/*.o
	rm -rf ./**/.*.o.cmd
	rm -rf ./**/**/*.o
	rm -rf ./**/**/.*.o.cmd

test:
	@echo $(SRC_DIR)
	@echo $(SRCS)
	@echo $(OBJS)

endif
