########################################################################################################
#	@Copyright (C) 	Anyka 2019-6-10
#	@brief					Create Linux4.4.x linux driver module for AnyCloudv500
#	@Version				V1.0.0
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

#将当前的所有源码文件，替换成不带目录路径的.o目标文件
#OBJS		=	${patsubst %.c, %.o, ${SRCS}}
#OBJS  	= $(CFILE:%.c=%.o)		#模式替换函数
OBJS  	= sc2336.o\
			../common/sensor_sys.o\
			../common/sensor_i2c.o

MODULE_NAME := sensor_sc2335

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

USER_MODULE_NAME ?= sensor_sc2335
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
	rm -rf ../common/*.o* *.o* *.ko* *.mod.c *.mod.* *.bak *.order *.symvers
	rm -rf ../common/.*.o.cmd .*.o.cmd .*.ko.cmd .*.mod.o.cmd
	rm -rf .tmp_versions

test:
	@echo $(CUR_DIR)
	@echo $(SRCS)
	@echo $(CFILE)
	@echo $(OBJS)

endif








