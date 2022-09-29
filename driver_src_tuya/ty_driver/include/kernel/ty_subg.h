#ifndef __TY_SUBG_H__
#define __TY_SUBG_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/irq.h>

#include <ty_gpio.h>

#define SUBG_DEV_NAME		"tysubg"

#define SUBG_PIN_NUM		5
#define SUBG_IRQ_PIN_NUM	1

#define RCV_DATA_LEN		4

#define LOW_CLK_NUM			22		//逻辑分析仪抓取下开始处低电平的时钟个数，不同设备个数不一样，取个最小值
#define HIGH_LOW_NUM		3		//逻辑分析仪抓取，一个数据是多少个时钟

#define __IRQT_FALEDGE 		IRQ_TYPE_EDGE_FALLING
#define __IRQT_RISEDGE 		IRQ_TYPE_EDGE_RISING
#define __IRQT_LOWLVL 		IRQ_TYPE_LEVEL_LOW
#define __IRQT_HIGHLVL 		IRQ_TYPE_LEVEL_HIGH
#define IRQT_NOEDGE 		(0)
#define IRQT_RISING 		(__IRQT_RISEDGE)
#define IRQT_FALLING 		(__IRQT_FALEDGE)
#define IRQT_BOTHEDGE 		(__IRQT_RISEDGE|__IRQT_FALEDGE)
#define IRQT_LOW 			(__IRQT_LOWLVL)
#define IRQT_HIGH 			(__IRQT_HIGHLVL)
#define IRQT_PROBE 			IRQ_TYPE_PROBE

typedef enum {
	SUBG_SPI0_CS0N_PIN,
	SUBG_SPI0_FCS_PIN,
	SUBG_SPI0_CLK_PIN,
	SUBG_SPI0_TRXD_PIN,
	SUBG_GPIO1_IN_PIN,
}ty_subg_pins_e;

typedef struct ty_irq_gpio {
	unsigned 	gpio;	
	const char 	*pin_name;
}ty_irq_gpio_t;

typedef struct ty_subg_dev {
    struct miscdevice 			misc_dev;	
	ty_used_gpio_t    			used_pins[SUBG_PIN_NUM];
	ty_irq_gpio_t				irq_pins[SUBG_IRQ_PIN_NUM];
	unsigned char				sh4_data[RCV_DATA_LEN];
	int							wait_event;
	int							irq[SUBG_IRQ_PIN_NUM];
	int							recycle_en;		//subg初始化结束后释放管脚
}ty_subg_dev_t;

#endif
