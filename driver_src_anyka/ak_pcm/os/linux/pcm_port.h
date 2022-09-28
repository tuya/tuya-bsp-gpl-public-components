#ifndef __PCM_PORT_H__
#define __PCM_PORT_H__

#include <linux/cdev.h>
#include <linux/cpufreq.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <mach/ak_l2.h>
#include "soundcard.h"

#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/hardirq.h>
#include <asm-generic/gpio.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/kernel.h>

#define pcm_port_debug(fmt, arg...) \
	pr_debug("[%llu]:[%s:%d]: "fmt" \n\n", ktime_to_ms(ktime_get_boottime()), __func__, __LINE__, ##arg)
#define pcm_port_func(fmt, arg...) \
	pr_info("[%llu]:[%s]: "fmt" \n", ktime_to_ms(ktime_get_boottime()), __func__, ##arg)
#define pcm_port_info(fmt, arg...) \
	pr_info("[%llu]:[%s:%d]: "fmt" \n", ktime_to_ms(ktime_get_boottime()), __func__, __LINE__, ##arg)
#define pcm_port_warn(fmt, arg...) \
	pr_warn("[%llu]:[%s:%d] warn! "fmt" \n\n", ktime_to_ms(ktime_get_boottime()), __func__, __LINE__, ##arg)
#define pcm_port_err(fmt, arg...) \
	pr_err("[%llu]:[%s:%d] error! "fmt" \n", ktime_to_ms(ktime_get_boottime()), __func__, __LINE__, ##arg)

#define MHz	1000000UL
unsigned long  adc_set_samplerate(SND_PARAM *clk_param, unsigned long samplerate);
int adc_set_highspeed(SND_PARAM *clk_param);
unsigned long  dac_set_samplerate(SND_PARAM *clk_param, unsigned long samplerate);
int dac_set_highspeed(SND_PARAM *clk_param, unsigned long samplerate);

void adc_clk_enable(SND_PARAM *clk_param);
void dac_clk_enable(SND_PARAM *clk_param);
void adc_clk_disable(SND_PARAM *clk_param);
void dac_clk_disable(SND_PARAM *clk_param);

unsigned long dac_hw_setformat(SND_PARAM *clk_param, unsigned long sample_rate);
unsigned long adc_hw_setformat(SND_PARAM *clk_param, unsigned long sample_rate);

#endif  //__PCM_PORT_H__
