#ifndef __TY_COLOR_BAND_H__
#define __TY_COLOR_BAND_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>

#include <ty_gpio.h>

#define COLORBAND_DEV_NAME		"tycolorband"

#define COLORBAND_NUM			2
#define BYTE_MAX_NUM			8196

typedef struct ty_colorband_dev {
    struct miscdevice 	misc_dev;

	BYTE				pins_num;
	BYTE 				*rgb_data;
	BYTE				channel;
	DWORD				byte_num;
	UINT				tx_nbits;

	struct mutex 		colorband_mutex;
	
	void 				*private_data; 		/* 私有数据，用于存放spi_device */	
}ty_colorband_dev_t;

#endif