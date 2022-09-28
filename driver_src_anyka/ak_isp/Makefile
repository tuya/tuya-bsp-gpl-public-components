########################################################################################################
#	@Copyright (C) 	Anyka 2019-6-10
#	@brief					Create Linux4.4.x linux driver module for AnyCloudv500
#	@Version				V1.0.0
#####################################GCC Compile Flags #################################################
EXTRA_CFLAGS += $(USER_EXTRA_CFLAGS)
EXTRA_CFLAGS += -O1

EXTRA_CFLAGS += -Wno-error=date-time
EXTRA_CFLAGS += -Wno-date-time

EXTRA_LDFLAGS += --strip-debug

#####################################GCC Objects #######################################################
#定义当前的工作目录
CUR_DIR=$(shell pwd)
SRC_DIR=$(CUR_DIR)

#INC_DIR=$(CUR_DIR)/include
#EXTRA_CFLAGS += -Wall -I$(INC_DIR)
EXTRA_CFLAGS += -Wall

#
#SRCS		=	${wildcard *.c}
#CFILE 	= $(wildcard *.c)

#LDFLAGS +=-L$(pwd)
#LDFLAGS +=-lakecho

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
endif

AUTOCONF := $(KSRC)/include/config/auto.conf
FEXIST := $(shell if [ -f $(AUTOCONF) ]; then echo "exist"; else echo "noexist"; fi;)
ifeq ("$(FEXIST)", "exist")
SOURCES = $(AUTOCONF)
include $(SOURCES)
ifeq ($(CONFIG_MACH_AK37D),y)
	ISP_PATH := isp_37d
	ISP_LIB := $(ISP_PATH)/isp_37d.a
endif
ifeq ($(CONFIG_MACH_AK39EV330),y)
	ISP_PATH := isp_ev33x
	ISP_LIB := $(ISP_PATH)/isp_ev33x.a
endif
endif

#将当前的所有源码文件，替换成不带目录路径的.o目标文件
#OBJS		=	${patsubst %.c, %.o, ${SRCS}}
#OBJS  	= $(CFILE:%.c=%.o)		#模式替换函数

ifeq (0,1)
OBJS  	= ak_host.o\
		ak_isp_char.o\
		sys_sensor.o\
		sys_isp.o\
		sys_host.o\
		$(ISP_LIB)
endif

ifeq (1,1)
OBJS  	= dev.o\
		  video.o\
		  camera.o\
		  isp_param.o\
		  isp_stats.o\
		  sys_host.o\
		  sys_isp.o\
		  sys_sensor.o\
		  $(ISP_LIB)
endif

MODULE_NAME := ak_isp

USER_MODULE_NAME ?= ak_isp
ifneq ($(USER_MODULE_NAME),)
MODULE_NAME := $(USER_MODULE_NAME)
endif

#####################################Make Compile Module ########################################################

ifneq ($(KERNELRELEASE),)

$(MODULE_NAME)-objs  := $(OBJS)
obj-m := $(MODULE_NAME).o $(LIB)

else

all: modules strip
	@echo "make all first step!"
modules:
	@echo "make modules!"
	@echo $(SRCS)
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KSRC) M=$(shell pwd)  modules

strip:
	@if [  -f $(MODULE_NAME).ko ]; then $(CROSS_COMPILE)strip $(MODULE_NAME).ko --strip-unneeded; fi;

.PHONY: modules clean test

clean:
	rm -rf *.o* *.ko* *.mod.c *.mod.* *.bak *.order *.symvers
	rm -rf .*.o.cmd .*.ko.cmd .*.mod.o.cmd
	rm -rf .tmp_versions

test:
	@echo $(CUR_DIR)
	@echo $(SRCS)
	@echo $(CFILE)
	@echo $(OBJS)
	@echo $(ISP_LIB)

endif
