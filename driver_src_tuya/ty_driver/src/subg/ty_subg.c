/*
* ty_subg.c - Tuya
*
* Copyright (C) 2020 Tuya Technology Corp.
*
* Author: dyh
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/
#include <ty_subg.h>
#include "radio/radio.h"

static DECLARE_WAIT_QUEUE_HEAD(subg_waitqueue);

extern struct ty_subg_dev g_subg_init __attribute__((weak));
struct ty_subg_dev g_subg_init;

struct ty_subg_dev *g_subg_dev = NULL;

UINT g_spi0_cs0n	= 0;
UINT g_spi0_fcs		= 0;
UINT g_spi0_clk		= 0;
UINT g_spi0_trxd	= 0;
UINT g_gpio1_in		= 0;
UINT g_gpio2_in		= 0;

void ty_subg_exit(void);

void ty_set_gpio1_in(void)
{
	gpio_direction_input(g_gpio1_in);
	return;
}

void ty_set_gpio2_in(void)
{
	gpio_direction_input(g_gpio2_in);
	return;	
}

void ty_set_gpio3_in(void)
{
	return;
}

int ty_read_gpio1_pin(void)
{
	return __gpio_get_value(g_gpio1_in);
}

int ty_read_gpio2_pin(void)
{
	return __gpio_get_value(g_gpio2_in);
}

int ty_read_gpio3_pin(void)
{
	return 0;
}

void ty_set_csb_out(void)
{
	gpio_direction_output(g_spi0_cs0n, 1);
	return;
}

void ty_set_fcsb_out(void)
{
	gpio_direction_output(g_spi0_fcs,1);
	return;
}

void ty_set_sclk_out(void)
{
	gpio_direction_output(g_spi0_clk,1);
	return;
}

void ty_set_sdio_out(void)
{
	gpio_direction_output(g_spi0_trxd,1);
	return;
}

void ty_set_sdio_in(void)
{
	gpio_direction_input(g_spi0_trxd);
	return;
}

void ty_set_csb_h(void)
{
	__gpio_set_value(g_spi0_cs0n,1);
	return;
}

void ty_set_csb_l(void)
{
	__gpio_set_value(g_spi0_cs0n, 0);
	return;
}

void ty_set_fcsb_h(void)
{
	__gpio_set_value(g_spi0_fcs,1);
	return;
}

void ty_set_fcsb_l(void)
{
	__gpio_set_value(g_spi0_fcs,0);
	return;
}

void ty_set_sclk_h(void)
{
	__gpio_set_value(g_spi0_clk,1);
	return;
}

void ty_set_sclk_l(void)
{
	__gpio_set_value(g_spi0_clk,0);
	return;
}

void ty_set_sdio_h(void)
{
	__gpio_set_value(g_spi0_trxd,1);
	return;
}

void ty_set_sdio_l(void)
{
	__gpio_set_value(g_spi0_trxd,0);
	return;
}

int ty_read_sdio_pin(void)
{
	return __gpio_get_value(g_spi0_trxd);
}

static void subg_func(struct ty_subg_dev *data_t)
{
	struct ty_subg_dev *tydata	= data_t;
	int input_val				= 0;
	BYTE sh4_data[4] 			= {0x00};
	static BYTE sh4_high_level_num	 	= 0;
	static BYTE sh4_low_level_num 		= 0;
	static BYTE sh4_level_start 		= 0;
	static BYTE sh4_data_analyze_start 	= 0;
	static BYTE	sh4_begin_flag			= 0;
	static BYTE sh4_data_num 			= 31;
	static DWORD sh4_data_buf 			= 0x00000000;

	input_val = gpio_get_value(g_gpio2_in);
	
	if(sh4_level_start == 0)
	{
		if(input_val == 0)
		{
			sh4_low_level_num++;
			if(sh4_low_level_num > LOW_CLK_NUM)			//è¿‡æ»¤ä¸€ä¸‹ä¿¡å·ï¼Œ160æ˜¯æµ‹è¯•å€¼
			{
				sh4_level_start = 1;
				sh4_low_level_num = 0;
				sh4_high_level_num = 0;
			}
		}
		else
		{
			sh4_low_level_num = 0;
			sh4_high_level_num = 0;
		}
	}
	else if(sh4_level_start == 1)
	{
		if(input_val == 1)
		{
			sh4_high_level_num++;
			sh4_low_level_num = 0;
			sh4_level_start = 2;
		}
	}
	else if(sh4_level_start == 2)
	{
		if(input_val == 0)
		{
			sh4_low_level_num++;
			sh4_begin_flag = 1;
			if(sh4_low_level_num > 4)
			{
				if(sh4_data_num == 7)		//Ö»ÓĞÈı¸ö×Ö½ÚµÄÇé¿ö
				{
					sh4_data_analyze_start = 1;
					sh4_data_buf		  &= 0xFFFFFF00;
				}
				else
				{
					goto reset;
				}
			}
		}

		if(input_val == 1)
		{
			if(sh4_begin_flag)				//Óöµ½ÏÂÒ»¸ö¸ßµçÆ½µÄÊ±ºòÅĞ¶ÏÉÏÒ»Ö¡Êı¾İ
			{
				sh4_begin_flag = 0;
				if((sh4_high_level_num <= 4) && (sh4_low_level_num <= 4))		//ÓĞĞ§¸ßµÍµçÆ½¸öÊı£¬ÊıÖµÓĞÂß¼­·ÖÎöÒÇ×¥È¡Îª×¼
				{
					if(sh4_high_level_num > sh4_low_level_num)
					{
						sh4_data_buf |=(0x01<<sh4_data_num);
					}
					else if(sh4_high_level_num < sh4_low_level_num)
					{
						sh4_data_buf &=~(0x01<<sh4_data_num);
					}
					else
					{
						goto reset;
					}
					
					if(sh4_data_num == 0)
					{
						sh4_data_analyze_start = 1;
					}
					else
					{
						sh4_data_num--;
					}					
				}
				else
				{
					goto reset;
				}
				sh4_high_level_num 	= 0;
				sh4_low_level_num	= 0;
			}
			sh4_high_level_num++;
		}
	}
	
	if(sh4_data_analyze_start == 1)
	{
		sh4_data[0] = (sh4_data_buf >> 24)&0xff;
		sh4_data[1] = (sh4_data_buf >> 16)&0xff;
		sh4_data[2] = (sh4_data_buf >> 8)&0xff;
		sh4_data[3] = (sh4_data_buf)&0xff;
		
		memcpy(tydata->sh4_data, sh4_data, RCV_DATA_LEN);

		tydata->wait_event = 1;
		wake_up_interruptible(&subg_waitqueue);                        //å”¤é†’é˜Ÿåˆ—	
	}else{
		return;
	}
reset:
	sh4_begin_flag			= 0;
	sh4_high_level_num 		= 0;
	sh4_low_level_num 		= 0;
	sh4_level_start 		= 0;
	sh4_data_analyze_start 	= 0;
	sh4_data_num 			= 31;
	sh4_data_buf 			= 0x00000000;

	return;
}

irqreturn_t subg_irq(int irq, void *dev_instance)
{
	struct ty_subg_dev *tydata = (struct ty_subg_dev *)dev_instance;     //è·å–å¼•è„šæè¿°ç»“æ„ä½“

	subg_func(tydata);

    return IRQ_HANDLED;
}

static INT ty_irq_gpio_init(ty_irq_gpio_t *irq_pins, BYTE pin_num)
{
	BYTE 	i 	= 0;
    INT 	ret	= 0;
	struct ty_subg_dev *ty_subg_dev = container_of(irq_pins, struct ty_subg_dev, irq_pins[0]);
	
	for (i = 0; i < pin_num; i++) {
		gpio_direction_input(irq_pins[i].gpio);
		ty_subg_dev->irq[i] = gpio_to_irq(irq_pins[i].gpio);
		ret |= request_irq(ty_subg_dev->irq[i], subg_irq, IRQT_RISING, irq_pins[i].pin_name, (void *)ty_subg_dev);
	}

    return ret;	
}

static INT ty_irq_gpio_deinit(ty_irq_gpio_t *irq_pins, BYTE pin_num)
{
	BYTE 	i 	= 0;
	struct ty_subg_dev *ty_subg_dev = container_of(irq_pins, struct ty_subg_dev, irq_pins[0]);
	
	for (i = 0; i < pin_num; i++) {
		free_irq(ty_subg_dev->irq[i], (void *)ty_subg_dev);
	}

    return 0;	
}

int ty_subg_open(struct inode *node, struct file *filp)
{
	struct ty_subg_dev *ty_subg_dev = container_of(filp->private_data, struct ty_subg_dev, misc_dev);

	if (!(ty_subg_dev->recycle_en)) {		//æœ‰äº›è®¾å¤‡ç®¡è„šæ¯”è¾ƒç´§å¼ ï¼Œåˆå§‹åŒ–rfåé‡Šæ”¾gpioå£ï¼Œåªå‰©ä¸‹æ¥æ”¶çš„
		if (ty_used_gpio_init(ty_subg_dev->used_pins, SUBG_PIN_NUM)){
			MS_ERROR("init gpio error.\n");
			return -1;
		}

		if (ty_irq_gpio_init(ty_subg_dev->irq_pins, SUBG_IRQ_PIN_NUM)){
			MS_ERROR("init irq error.\n");
			return -1;
		}

		if (RF_Init()) {
			MS_ERROR("subg is not existed.\n");
			return -1;
		}
	}
		
    return 0;
}

int ty_subg_release(struct inode *node, struct file *filp)
{
	struct ty_subg_dev *ty_subg_dev = container_of(filp->private_data, struct ty_subg_dev, misc_dev);

	if (!(ty_subg_dev->recycle_en)) {
		if (ty_used_gpio_deinit(ty_subg_dev->used_pins, SUBG_PIN_NUM)){
			MS_ERROR("deinit gpio error.\n");
			return -1;
		}

		if (ty_irq_gpio_deinit(ty_subg_dev->irq_pins, SUBG_IRQ_PIN_NUM)){
			MS_ERROR("deinit irq error.\n");
			return -1;
		}
	}
	
    return 0;
}

static ssize_t ty_subg_read(struct file *filp, char __user *buf, size_t count, loff_t *ptr)
{
	DWORD ret 			= -1;
	struct ty_subg_dev *ty_subg_dev = container_of(filp->private_data, struct ty_subg_dev, misc_dev);

	wait_event_interruptible(subg_waitqueue, ty_subg_dev->wait_event);            //è¿›å…¥ç­‰å¾…é˜Ÿåˆ—ä¼‘çœ ,å¦‚æœä¸­æ–­æ¥æ•°æ®,åˆ™è·³å‡º

	ret = copy_to_user(buf, ty_subg_dev->sh4_data, RCV_DATA_LEN);
	if (ret) {
		MS_ERROR("copy to user error.\n");
		return -1;
	}
	
	ty_subg_dev->wait_event =0;
		 
	return RCV_DATA_LEN;
}

static ssize_t ty_subg_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
#if 0
	DWORD ret = -1;
	BYTE ker_buf[2];
	struct ty_subg_dev *ty_subg_dev = container_of(filp->private_data, struct ty_subg_dev, misc_dev);
	
	ret = copy_from_user(ker_buf, buf, 2);
	if (ret)
		printk("copy from user error.\n");
	
	if (DEBUG_ENABLE)
		printk("write reg 0x%02x : 0x%02x\n",ker_buf[0],ker_buf[1]);
	
	cmt_spi3_write(ker_buf[0], ker_buf[1]);
#endif

	return 0;
}

long  ty_subg_ioctl(struct file *filp, UINT cmd, DWORD arg)
{
//    struct ty_subg_t* tydata = (struct ty_subg_t*)filp->private_data;
	BYTE udata[2] 	= {0x00};
	BYTE addr 		= 0;
	BYTE value 		= 0;

    if(copy_from_user(udata, (void *)arg, sizeof(udata)/sizeof(BYTE))) {
        return -1;
    }
	
	addr 	= udata[0];
	value 	= udata[1];		
    switch(cmd) {
        case 0:
			value = 0x00;
//			cmt_spi3_read(addr, &value);
			value = CMT2300A_ReadReg(addr);
			MS_INFO("read value 0x%x from addr 0x%x\n", value, addr);

			udata[0] = addr;
			udata[1] = value;
			if (copy_to_user((void *)arg, &udata, sizeof(udata)/sizeof(BYTE))) {
				MS_ERROR("copy to user error.\n");
				return -1;
			}
			break;
        case 1:
			MS_INFO("write value 0x%x to addr 0x%x\n", value, addr);
//			cmt_spi3_write(addr, value);
			CMT2300A_WriteReg(addr, value);
            break;
        default:
            break;
    }

    return 0;
}

static struct file_operations ty_subg_fops = {
	.owner   =  THIS_MODULE,
	.open	 =  ty_subg_open,
	.release =  ty_subg_release,
	.write	 =  ty_subg_write,
	.read	 =  ty_subg_read,
    .unlocked_ioctl = ty_subg_ioctl,	
};

int ty_subg_init(void)
{
	int ret = 0;

	g_subg_dev = kzalloc(sizeof(struct ty_subg_dev), GFP_KERNEL);
	if (!g_subg_dev) {
        MS_ERROR("kmalloc stepmotor dev fail.\n");
        return -1;
    }

	memcpy(g_subg_dev, &g_subg_init, sizeof(struct ty_subg_dev));
	g_spi0_cs0n	= g_subg_init.used_pins[SUBG_SPI0_CS0N_PIN].gpio;
	g_spi0_fcs	= g_subg_init.used_pins[SUBG_SPI0_FCS_PIN].gpio;
	g_spi0_clk	= g_subg_init.used_pins[SUBG_SPI0_CLK_PIN].gpio;
	g_spi0_trxd	= g_subg_init.used_pins[SUBG_SPI0_TRXD_PIN].gpio;
	g_gpio2_in	= g_subg_init.used_pins[SUBG_GPIO1_IN_PIN].gpio;
	g_gpio1_in	= g_subg_init.irq_pins[0].gpio;
	
	g_subg_dev->misc_dev.minor  = MISC_DYNAMIC_MINOR,
    g_subg_dev->misc_dev.name   = SUBG_DEV_NAME,
    g_subg_dev->misc_dev.fops   = &ty_subg_fops,
    ret = misc_register(&g_subg_dev->misc_dev);
    if(unlikely(ret < 0)) {
        MS_ERROR("register misc fail\n");
    }

	if (g_subg_dev->recycle_en) {
		if (ty_used_gpio_init(g_subg_dev->used_pins, SUBG_PIN_NUM)){
			MS_ERROR("init gpio error.\n");
			return -1;
		}
		
		if (ty_irq_gpio_init(g_subg_dev->irq_pins, SUBG_IRQ_PIN_NUM)){
			MS_ERROR("init irq error.\n");
			return -1;
		}
		
		if (RF_Init()) {
			MS_ERROR("subg is not existed.\n");
			ret = -1;
		}

		if (ty_used_gpio_deinit(g_subg_dev->used_pins, SUBG_PIN_NUM - 1)){
			MS_ERROR("deinit gpio error.\n");
			return -1;
		}
		
		if (ret < 0)
			goto error;
	}
    MS_INFO("%s module is installed.\n", __func__);
	
	return ret;

error:
	ty_subg_exit();
	
	return 0;
}

void ty_subg_exit(void)
{	
	misc_deregister(&g_subg_dev->misc_dev);
	if (g_subg_dev->recycle_en) {
		if (ty_used_gpio_deinit(g_subg_dev->used_pins + 4, SUBG_PIN_NUM - 4)){		//å‰å››ä¸ªç®¡è„šåœ¨opené˜¶æ®µå·²ç»é‡Šæ”¾
			MS_ERROR("deinit gpio error.\n");
			goto end;
		}
		
		if (ty_irq_gpio_deinit(g_subg_dev->irq_pins, SUBG_IRQ_PIN_NUM)){
			MS_ERROR("deinit irq error.\n");
			goto end;
		}
	}
end:	
	kfree(g_subg_dev);
	g_subg_dev = NULL;

    MS_INFO("%s module is removed.\n", __func__);
}

