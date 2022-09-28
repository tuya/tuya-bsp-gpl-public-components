/*
 *  linux/drivers/mmc/host/akmci.c - Anyka MMC/SD/SDIO driver
 *
 *  Copyright (C) 2019 Anyka, Ltd, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/log2.h>
#include <linux/mmc/host.h>
#include <linux/clk.h>

#include <linux/scatterlist.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/dma-mapping.h>

#include <linux/pinctrl/consumer.h>

#include <asm/cacheflush.h>
#include <asm/div64.h>
#include <asm/io.h>
#include <asm/sizes.h>

#include <mach/ak_l2.h>
#include "ak_mci.h"

#include <asm-generic/gpio.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#define DRIVER_NAME 		"akmci"

#define ___hdbg___()		printk("akmci:----func:%s---line:%d----\n", __func__, __LINE__);
#define DDREGS(host)		dbg_dumpregs(host, __func__, __LINE__)
#define DDDATA(h, d, s)		dbg_dumpdata(h, d, s)
#define HDBG(fmt, args...) 	printk(fmt, ##args)


#define DMA_SIZE	512
#define USED_DMA(x) (((x) >= DMA_SIZE) && (!((x) % DMA_SIZE)))
#define USED_DMA_CPU(x) (((x) > DMA_SIZE) && ((x) % DMA_SIZE))
#define USED_CPU(x) (((x) > 0) && ((x) < DMA_SIZE))
#define BOTH_DIR (MMC_DATA_WRITE | MMC_DATA_READ)
	
#define CARD_UNPLUGED			0
#define CARD_PLUGED				1
#define CARD_POWER_ENABLE		1
#define CARD_POWER_DISABLE		0

#define MCI_DATA_IRQ	(1<<1)
#define MCI_CMD_IRQ		(1<<0)
static unsigned int detect_time = (HZ/2);

static unsigned int retry_count = 100;
static unsigned int request_timeout = (5 * HZ);  

unsigned int slot_index = 0;
struct mmc_host *mci_host[MCI_SLOT_NUM] = {NULL};

static void akmci_l2xfer(struct akmci_host *host);
static void akmci_init_host_cfg(struct akmci_host *host);

extern void l2_dump_registers(void);

static BLOCKING_NOTIFIER_HEAD(addetect_notifier_list);

/**
 * @brief dump mci module register, use for debug.
 * 
 * @param [in] *host information of data transmitted, including data buf pointer, data len .
 * @return void.
*/
static inline void dbg_dumpregs(struct akmci_host *host,
		const char *prefix, int eflags)
{
	u32 clkcon, cmdarg, cmd, cmdrsp, rsp1, rsp2, rsp3, rsp4;
	u32 dtimer, datlen, datcon, datcnt, stat, imask, dmamode, cpumode, pluginoff;
	struct platform_device	*pdev = host->pdev;

	clkcon = readl(host->base + MCI_CLK_REG);
	cmdarg = readl(host->base + MCI_ARGUMENT_REG);
	cmd = readl(host->base + MCI_COMMAND_REG);
	cmdrsp = readl(host->base + MCI_RESPCMD_REG);
	
	rsp1 = readl(host->base + MCI_RESPONSE0_REG);
	rsp2 = readl(host->base + MCI_RESPONSE1_REG);
	rsp3 = readl(host->base + MCI_RESPONSE2_REG);
	rsp4 = readl(host->base + MCI_RESPONSE3_REG);

	dtimer = readl(host->base + MCI_DATATIMER_REG);
	datlen = readl(host->base + MCI_DATALENGTH_REG);
	datcon = readl(host->base + MCI_DATACTRL_REG);
	datcnt = readl(host->base + MCI_DATACNT_REG);
	
	stat = readl(host->base + MCI_STATUS_REG);
	imask = readl(host->base + MCI_MASK_REG);
	dmamode = readl(host->base + MCI_DMACTRL_REG);
	cpumode = readl(host->base + MCI_FIFO_REG);
	pluginoff = readl(MCI_PLUGINOFF_REG0);

	dev_dbg(&pdev->dev,"current prefix: %s (%d)\n", prefix, eflags);
	
	dev_dbg(&pdev->dev,"SDIO_INTRCTR_REG:[%08x], clkcon:[%08x], cmdarg:[%08x], cmd:[%08x], cmdrsp:[%08x].\n",
		readl(host->base + SDIO_INTRCTR_REG), clkcon, cmdarg, cmd, cmdrsp);
	dev_dbg(&pdev->dev,"rsp1:[%08x], rsp2:[%08x], rsp3:[%08x], rsp4:[%08x]\n",
		rsp1, rsp2, rsp3, rsp4);
	dev_dbg(&pdev->dev,"dtimer:[%08x], datlen:[%08x], datcon:[%08x], datcnt:[%08x]\n",
		dtimer, datlen, datcon, datcnt);
	dev_dbg(&pdev->dev,"stat:[%08x], imask:[%08x], dmamode:[%08x], cpumode:[%08x], pluginoff:[%08x]\n",
		stat, imask, dmamode, cpumode,pluginoff);
	
#define AK_SHAREPIN_CON0		(AK_VA_SYSCTRL + 0x178)
#define AK_SHAREPIN_CON1		(AK_VA_SYSCTRL + 0x17C)
#define AK_SHAREPIN_CON2		(AK_VA_SYSCTRL + 0x180)
#define AK_SHAREPIN_CON3		(AK_VA_SYSCTRL + 0x184)
#define AK_SHAREPIN_CON4		(AK_VA_SYSCTRL + 0x188)
#define AK_SHAREPIN_CON5		(AK_VA_SYSCTRL + 0x18C)
#define AK_SHAREPIN_CON6		(AK_VA_SYSCTRL + 0x190)

#define AK_PPU_PPD_S1           (AK_VA_SYSCTRL + 0x1A4)
#define AK_PPU_PPD_S2     		(AK_VA_SYSCTRL + 0x1E8)
#define AK_PPU_PPD_S3           (AK_VA_SYSCTRL + 0x19C)
#define AK_PPU_PPD_IE           (AK_VA_SYSCTRL + 0x1F8)

#define AK_PPU_PPD1           	(AK_VA_SYSCTRL + 0x194)
#define AK_PPU_PPD2           	(AK_VA_SYSCTRL + 0x198)
#define AK_PPU_PPD3           	(AK_VA_SYSCTRL + 0x1A0)

	dev_dbg(&pdev->dev,"AK_SHAREPIN_CON0:[%08x], AK_SHAREPIN_CON1:[%08x], AK_SHAREPIN_CON2:[%08x], AK_SHAREPIN_CON3:[%08x], AK_SHAREPIN_CON4:[%08x],AK_SHAREPIN_CON5:[%08x], AK_SHAREPIN_CON6:[%08x]\n",
		readl(AK_SHAREPIN_CON0), readl(AK_SHAREPIN_CON1), 
		readl(AK_SHAREPIN_CON2), readl(AK_SHAREPIN_CON3),
		readl(AK_SHAREPIN_CON4), readl(AK_SHAREPIN_CON5),readl(AK_SHAREPIN_CON6));

	dev_dbg(&pdev->dev,"AK_PPU_PPD1:[%08x], AK_PPU_PPD2:[%08x], AK_PPU_PPD3:[%08x], AK_PPU_PPD_S1:[%08x], AK_PPU_PPD_S2:[%08x],AK_PPU_PPD_S3:[%08x], AK_PPU_PPD_IE:[%08x]\n",
		readl(AK_PPU_PPD1), readl(AK_PPU_PPD2),readl(AK_PPU_PPD3), 
		readl(AK_PPU_PPD_S1),readl(AK_PPU_PPD_S2),readl(AK_PPU_PPD_S3),
		readl(AK_PPU_PPD_IE));	
	
}

/**
 * @brief dump mci module read/write data, use for debug.
 * 
 * @param [in] *host information of data transmitted, including data buf pointer, data len .
 * @return void.
*/
static inline void dbg_dumpdata(struct akmci_host *host,
		void *data, int size)
{
	int ii;
	int dsize = (size +3)/4;
	u32 *dptr = data;
	
	printk("xfer data (size:%d):", size);
	
	for(ii = 0; ii < dsize; ii++) {
		if((ii%10) == 0)
			printk("\n");
		
		printk("%08x ", *(dptr + ii));
	}
	printk("\n");
}

/**
 * the data transfer mode description.
*/
static char* xfer_mode_desc[] = {
		"l2dma",
		//"l2pio",
	};

/**
 * the sd/mmc/sdio card detect mode description.
*/
static char* detect_mode_desc[] = {
		"plugin alway", 
		"clk detect",
		"gpio detect"
};


#define MCI_L2_ADDR(host)	(host->pdev->id + ADDR_MMC0)
		
/**
* akmci_xfer_mode - judgement the mci transfer mode.
* ret: 	AKMCI_XFER_L2DMA: use for l2 dma mode
*/
static inline int akmci_xfer_mode(struct akmci_host *host)
{
	return host->xfer_mode;
}

static void  config_plugoff_int(struct akmci_host *host, int enable)
{
	u32 val;
	unsigned long flags;

	/*lock for write mci_pluginoff reg0*/
	spin_lock_irqsave(&host->lock, flags);
	pr_debug("config_plugoff_int:%s\n",enable?"enable":"disable");
	val = readl(MCI_PLUGINOFF_REG0);
	pr_debug("config_plugoff_int, val=%x\n", val);
	if(enable)
		val |= ( 1 << (host->pdev->id + MCI_PLUGOFF_INT_OFFSET));
	else
		val &= ~(1 << (host->pdev->id + MCI_PLUGOFF_INT_OFFSET));
	
	writel(val, MCI_PLUGINOFF_REG0);
	pr_debug("config_plugoff_int, val=%x\n", readl(MCI_PLUGINOFF_REG0));
	spin_unlock_irqrestore(&host->lock, flags);
	return;
}

static void  config_plugin_int(struct akmci_host *host, int enable)
{
	u32 val;
	unsigned long flags;
	/*lock for write mci_pluginoff reg0*/
	spin_lock_irqsave(&host->lock, flags);
	pr_debug("config_plugin_int:%s\n",enable?"enable":"disable");
	val = readl(MCI_PLUGINOFF_REG0);
	pr_debug("config_plugin_int, val=%x\n", val);
	if(enable)
		val |= ( 1 << (host->pdev->id + MCI_PLUGIN_INT_OFFSET));
	else
		val &= ~(1 << (host->pdev->id + MCI_PLUGIN_INT_OFFSET));

	writel(val, MCI_PLUGINOFF_REG0);
	pr_debug("config_plugin_int, val=%x\n", readl(MCI_PLUGINOFF_REG0));
	spin_unlock_irqrestore(&host->lock, flags);
	return;

}

/* 
 * return 1 CARD_PLUGED. 
 * return 0 CARD_UNPLUGED.
 */
int _get_card_status(struct akmci_host *host)
{
	struct platform_device	*pdev = host->pdev;
	unsigned int i, status[3], detect_retry_count = 0;
	unsigned int mci_active;
	unsigned int mci_status;
	unsigned long flags;

	while (1) {
		
			for (i = 0; i < 3; i++) {
				if(host->detect_mode == AKMCI_DETECT_MODE_CLK)
				{
					spin_lock_irqsave(&host->lock, flags);
					status[i] = readl(MCI_PLUGINOFF_REG0);
					pr_debug("#status[%d](0x08000080)=%x\n", i, status[i]);
					mci_active = status[i] & (1 << (MCI_PLUG_ACTIVE_OFFSET + host->pdev->id));
					mci_status = status[i] & (1 << (MCI_PLUG_STATUS_OFFSET + host->pdev->id));

					if(mci_active)
					{
						dev_info(&pdev->dev, "Data transmission is in progress. MCI_CLK will be pulled down, and can not function as the detection pin.!");
						return CARD_PLUGED;  // "Data transmission is in progress" treat as CARD_PLUGED(1)
					}
					status[i] = mci_status;
					spin_unlock_irqrestore(&host->lock, flags);
				}
				else if(host->detect_mode == AKMCI_DETECT_MODE_GPIO)
				{
					status[i] = gpio_get_value(host->gpio_cd);
				}
				udelay(10);
				//pr_info("#status[%d]=%d\n", i, status[i]);
			}

			if ((status[0] == status[1]) && (status[0] == status[2]))
			break;

			detect_retry_count++;
			if (detect_retry_count >= 5) {
				dev_err(&pdev->dev, "this is a dithering,card detect error!");
				return CARD_UNPLUGED;  // err condition,treat as CARD_UNPLUGED(0)
			}
	}	
	return !status[0];
}

/*
 *	get_card_status 
 *  return 1 if CARD_PLUGED, return 0 if CARD_UNPLUGED.
 *
 */

int get_card_status(struct akmci_host *host)
{
	struct platform_device	*pdev = host->pdev;
	unsigned int status = CARD_PLUGED;
	unsigned int status_reg;

	if(host->detect_mode != AKMCI_PLUGIN_ALWAY)
		status &= _get_card_status(host);

	dev_dbg(&pdev->dev,"@get_card_status, card %s\n",
				(status?"PLUGED":"UNPLUGED"));
	
	return !!status;

}

/* prenset = 1 */
static int akmci_card_present(struct mmc_host *mmc)
{
	struct akmci_host *host = mmc_priv(mmc);
	unsigned int card_pwr_en = CARD_POWER_ENABLE;

	if(host->gpio_power.gpio >=0)
		card_pwr_en = (host->gpio_power.value)^(host->gpio_power.power_invert);

	pr_debug("akmci_card_present, card_pwr_en=%d, card_status=%d(1=CARD_PLUGED, 0=CARD_UNPLUGED)\n",card_pwr_en,host->card_status );
	
	return (host->card_status && card_pwr_en);
}

static inline int enable_imask(struct akmci_host *host, u32 imask)
{
	u32 newmask;

	newmask = readl(host->base + MCI_MASK_REG);
	newmask |= imask;
	writel(newmask, host->base + MCI_MASK_REG);

	return newmask;
}

static inline int disable_imask(struct akmci_host *host, u32 imask)
{
	u32 newmask;

	newmask = readl(host->base + MCI_MASK_REG);
	newmask &= ~imask;
	writel(newmask, host->base + MCI_MASK_REG);

	return newmask;
}

static inline void clear_imask(struct akmci_host *host)
{
	u32 mask = readl(host->base + MCI_MASK_REG);

	/* preserve the SDIO IRQ mask state */
	mask &= MCI_SDIOINTMASK;
	writel(mask, host->base + MCI_MASK_REG);
}
static irqreturn_t pluginoff_irq(int irq, void *dev_id)
{	
	struct akmci_host *host = dev_id;
	struct platform_device	*pdev = host->pdev;
	int curr_status = 0;	
	int ret;
	int status_reg = readl(MCI_PLUGINOFF_REG0);

	dev_dbg(&pdev->dev, "enter pluginoff_irq,status_reg=%x\n",status_reg);
	if(status_reg & (1 << (MCI_PLUGOFF_STA_OFFSET + host->pdev->id)))
	{
		dev_dbg(&pdev->dev, "plug off irq happen! status_reg=%x\n",status_reg);
		status_reg |=  1 << (MCI_PLUGIN_STA_OFFSET + host->pdev->id);
		status_reg |=  1 << (MCI_PLUGOFF_STA_OFFSET + host->pdev->id);

		writel(status_reg, MCI_PLUGINOFF_REG0);
		dev_dbg(&pdev->dev, "plug off irq happen! clear status_reg=%x\n",status_reg);
		curr_status = _get_card_status(host);
		dev_dbg(&pdev->dev,"curr_status =%d\n", curr_status );

		if(curr_status == CARD_UNPLUGED)
		{
			host->card_status = curr_status;
			config_plugoff_int(host,0);
			status_reg = readl(MCI_PLUGINOFF_REG0);
			status_reg |=  1 << (MCI_PLUGIN_STA_OFFSET + host->pdev->id);
			status_reg |=  1 << (MCI_PLUGOFF_STA_OFFSET + host->pdev->id);
			writel(status_reg, MCI_PLUGINOFF_REG0);
			config_plugin_int(host,1);

			schedule_delayed_work(&host->work, msecs_to_jiffies(host->software_debounce));
		}
	}
	else if(status_reg & (1 << (MCI_PLUGIN_STA_OFFSET + host->pdev->id)))
	{
		dev_dbg(&pdev->dev, "plug in irq happen! status_reg=%x\n", status_reg);
		status_reg |=  1 << (MCI_PLUGIN_STA_OFFSET + host->pdev->id);
		status_reg |=  1 << (MCI_PLUGOFF_STA_OFFSET + host->pdev->id);
		writel(status_reg, MCI_PLUGINOFF_REG0);
		dev_dbg(&pdev->dev, "plug in irq happen! after clear, status_reg=%x\n", readl(MCI_PLUGINOFF_REG0));
		curr_status = _get_card_status(host);
		dev_dbg(&pdev->dev,"curr_status =%d\n", curr_status );

		if(curr_status == CARD_PLUGED)
		{
			host->card_status = curr_status;
			config_plugin_int(host,0);
			status_reg = readl(MCI_PLUGINOFF_REG0);
			status_reg |=  1 << (MCI_PLUGIN_STA_OFFSET + host->pdev->id);
			status_reg |=  1 << (MCI_PLUGOFF_STA_OFFSET + host->pdev->id);
			writel(status_reg, MCI_PLUGINOFF_REG0);
			config_plugoff_int(host,1);
			akmci_init_host_cfg(host);

			schedule_delayed_work(&host->work, msecs_to_jiffies(host->software_debounce));
		}
	}
	ret = 1;
	return IRQ_RETVAL(ret);
}

/**
 * @brief  L2DMA ISR post-process
 * @return void
 */
void L2DMA_isr(unsigned long data)
{
	struct akmci_host *host = (struct akmci_host *)data;
	struct platform_device	*pdev = host->pdev;

	if(host->sg_ptr==NULL){
		dev_err(&pdev->dev,"##L2DMA_isr host->sg_ptr==null\n");		
		return;
	}
	host->sg_ptr = sg_next(host->sg_ptr); 
	host->sg_len --;
	host->sg_off = 0;
	if(!host->sg_len)
		return;
	dev_dbg(&pdev->dev, "###L2DMA_isr host->sg_len: %u\n", host->sg_len);
	akmci_l2xfer(host);			
}

/**
 * @brief  L2DMA ISR post-process
 * @return void
 */
void L2DMA_res_isr(unsigned long data)
{
	struct akmci_host *host = (struct akmci_host *)data;	
	struct platform_device	*pdev = host->pdev;
	
	if(host->data==NULL){
		dev_err(&pdev->dev, "##L2DMA_res_isr host->data==null\n");
		return;
	}
	akmci_l2xfer(host);			
}

/**
 * @brief transmitting data.
 * @param [in] *host information of data transmitted, 
 * including data buf pointer, data len .
 * @return void.
*/
static void akmci_l2xfer(struct akmci_host *host)
{
	u32 xferlen;
	u8 dir;
	u32 *tempbuf = NULL;
	dma_addr_t phyaddr = 0;
	unsigned int sg_num = 0;
	struct platform_device	*pdev = host->pdev;

	if (host->data->flags & MMC_DATA_WRITE) {		
		dir = MEM2BUF;
	} else {
		dir = BUF2MEM;
	}	

    xferlen = host->sg_ptr->length - host->sg_off;
	sg_num  = host->sg_len;

	if(xferlen==0){
		dev_dbg(&pdev->dev, "##akmci_l2xfer %s xferlen==0\n", (host->data->flags & MMC_DATA_WRITE)?"write":"read");
		return;		
	}
	if (USED_DMA(xferlen))		
	//if ((akmci_xfer_mode(host) == AKMCI_XFER_L2DMA) && USED_DMA(xferlen))	
	{
		phyaddr = sg_dma_address(host->sg_ptr) + host->sg_off;
		host->sg_off += xferlen;
		host->data_xfered += xferlen;

		if(sg_num>1){
			dev_dbg(&pdev->dev, "##sg_num>1, set dam callback\n");
			l2_set_dma_callback(host->l2buf_id, L2DMA_isr, (unsigned long)(host));
			l2_combuf_dma(phyaddr, host->l2buf_id, xferlen, dir, AK_TRUE);   
		}else {
			dev_dbg(&pdev->dev, "##sg_num==1\n");
			l2_combuf_dma(phyaddr, host->l2buf_id, xferlen, dir, AK_FALSE);	 
		}
	} 
	else if(USED_DMA_CPU(xferlen)) 
	//else if((akmci_xfer_mode(host) == AKMCI_XFER_L2DMA) && USED_DMA_CPU(xferlen)) 
	{
		unsigned int need_callback=0;
		dev_dbg(&pdev->dev, "##akmci transfer data: DMA AND CPU mode.\n");
		phyaddr = sg_dma_address(host->sg_ptr) + host->sg_off;

		//l2 limit
		//1. xferlen > 8192 
		//2.xferlen < 8192 && xferlen %64 != 0
		//3.xferlen < 8192 && xferlen %64 ==0
		
		if(xferlen > 8192){
			xferlen = (xferlen / 512) * 512;
			need_callback = 1;
		}else if((xferlen < 8192) && (xferlen % 64)){
			xferlen = (xferlen / 64) * 64;
			need_callback = 1;
		}
		
		host->sg_off += xferlen;
   	 	host->data_xfered += xferlen;

		if(need_callback){
			l2_set_dma_callback(host->l2buf_id, L2DMA_res_isr, (unsigned long)(host));				  
			l2_combuf_dma(phyaddr, host->l2buf_id, xferlen, dir, AK_TRUE);	  
		}else{
			l2_combuf_dma(phyaddr, host->l2buf_id, xferlen, dir, AK_FALSE);	  
		}
	}
	else 
	{
		/*
		 * use for cpu transfer mode.
		 * */
		#if 0
		if((xferlen >= DMA_ONE_SHOT_LEN) && (xferlen % DMA_ONE_SHOT_LEN)){
			printk(KERN_ERR "xferlen > 64 bytes and not div by 64, transfer by cpu!\n");
		}
		#endif
		
		dev_dbg(&pdev->dev,"##akmci transfer data: CPU mode.\n");
	    tempbuf = sg_virt(host->sg_ptr) + host->sg_off;
		
		host->sg_off += xferlen;
    	host->data_xfered += xferlen;

		l2_combuf_cpu((unsigned long)tempbuf, host->l2buf_id, xferlen, dir); 
	}

	/* debug info if data transfer error */
	if(host->data_err_flag > 0) {
		dev_dbg(&pdev->dev,"mci_xfer transfered: xferptr = 0x%p, xfer_offset=%d, xfer_bytes=%d\n",
			sg_virt(host->sg_ptr)+host->sg_off, host->sg_off, host->data_xfered);
	}

}


void akmci_init_sg(struct akmci_host *host, struct mmc_data *data)
{
	/*
	 * Ideally, we want the higher levels to pass us a scatter list.
	 */
	host->sg_len = data->sg_len;
	host->sg_ptr = data->sg;
	host->sg_off = 0;
}

int akmci_next_sg(struct akmci_host *host)
{
	host->sg_ptr++;
	host->sg_off = 0;
	return --host->sg_len;
}

/**
 * @brief stop data, close interrupt.
 * @param [in] *host get the base address of resgister.
 * @return void.
 */
static void akmci_stop_data(struct akmci_host *host)
{
	//struct platform_device	*pdev = host->pdev;	
	writel(0, host->base + MCI_DMACTRL_REG);
	writel(0, host->base + MCI_DATACTRL_REG);

	disable_imask(host, MCI_DATAIRQMASKS|MCI_FIFOFULLMASK|MCI_FIFOEMPTYMASK);
	
	clear_bit(MCI_DATA_IRQ, &host->pending_events);
     
	if(akmci_xfer_mode(host) ==AKMCI_XFER_L2DMA) {
		if (host->data->flags & MMC_DATA_WRITE) {
			dma_sync_sg_for_cpu(mmc_dev(host->mmc), host->data->sg, host->data->sg_len, DMA_TO_DEVICE);
			dma_unmap_sg(mmc_dev(host->mmc), host->data->sg, host->data->sg_len, DMA_TO_DEVICE);
		} else {
			dma_sync_sg_for_cpu(mmc_dev(host->mmc), host->data->sg, host->data->sg_len, DMA_FROM_DEVICE);
			dma_unmap_sg(mmc_dev(host->mmc), host->data->sg, host->data->sg_len, DMA_FROM_DEVICE);
		}
	}

	host->sg_ptr = NULL;
	host->sg_len = 0;
	host->sg_off = 0;
	
	host->data = NULL; 
}

/**
 * @brief  finish a request,release resource.
 * @param [in] *host information of sd controller.
 * @param [in] *mrq information of request.
 * @return void.
 */
static void akmci_request_end(struct akmci_host *host, struct mmc_request *mrq)
{
	struct platform_device	*pdev = host->pdev;
	
	writel(0, host->base + MCI_COMMAND_REG); 

	//BUG_ON(host->data);
	host->cmd = NULL;

	if(host->data_err_flag > 0) {
		//akmci_reset(host);
		writel(MCI_ENABLE|MCI_FALL_TRIGGER, host->base + MCI_CLK_REG);
		writel(readl(host->base + MCI_CLK_REG)|host->clkreg, host->base + MCI_CLK_REG);
		mdelay(10);
	}	
	dev_dbg(&pdev->dev, "akmci_request_end. buf_id=%d\n", host->l2buf_id);

	if(host->l2buf_id != BUF_NULL) {
		l2_free(MCI_L2_ADDR(host));
		host->l2buf_id = BUF_NULL;
	}
	
	if (mrq->data)
		mrq->data->bytes_xfered = host->data_xfered;

	
	/*
	 * Need to drop the host lock here; mmc_request_done may call
	 * back into the driver...
	 */

    mmc_request_done(host->mmc, mrq);
	dev_dbg(&pdev->dev, "finalize the mci request.\n");

}
/**
 * @brief  config sd controller, start sending command.
 * @param [in] *host information of sd controller.
 * @param [in] *cmd information of cmd sended.
 * @return void.
 */
static void akmci_start_command(struct akmci_host *host, struct mmc_command *cmd)
{
	struct platform_device	*pdev = host->pdev;
	unsigned int ccon;
	
	dev_dbg(&pdev->dev, "mci send cmd: op %i arg 0x%08x flags 0x%08x.%s data.\n", 
		cmd->opcode, cmd->arg, cmd->flags, cmd->data ? "contain":"no");

	writel(cmd->arg, host->base + MCI_ARGUMENT_REG);
	/* enable mci cmd irq */
	enable_imask(host, MCI_CMDIRQMASKS);
	
	ccon = MCI_CPSM_CMD(cmd->opcode) | MCI_CPSM_ENABLE;
	if (cmd->flags & MMC_RSP_PRESENT) {
		ccon |= MCI_CPSM_RESPONSE;
		if (cmd->flags & MMC_RSP_136)
			ccon |= MCI_CPSM_LONGRSP;
	}

	if (cmd->data)
		ccon |= MCI_CPSM_WITHDATA;
#ifdef SDIO_INTR_AUTO_DETECT
	#define GET_SDIO_FUNID(arg)  (((arg)>>28) & 0x7)
	#define GET_SDIO_REGID(arg)  (((arg)>>9) & 0x1FFFF)
	if( GET_SDIO_FUNID(cmd->arg) == 0 && GET_SDIO_REGID(cmd->arg)==0x6)
	{
		ccon |= MCI_CPSM_CMD12_IOABORT;
	}
#endif
	host->cmd = cmd;

	//ccon |= MCI_CPSM_RSPWAITTIME;		//512 clock command timeout
	ccon |= MCI_CPSM_RSPCRC_NOCHK;		//RspCrcNoChk: 1 = CPSM doesn’t check the CRC of the response.
	
	/* configurate cmd controller register */
	writel(ccon, host->base + MCI_COMMAND_REG);
}


static void print_mci_data_err(struct mmc_data *data,unsigned int status, const char *err)
{
	if (data->flags & MMC_DATA_READ) {
		pr_debug("akmci: data(read) status=0x%x data_blocks=%d data_blocksize=%d %s\n", status, data->blocks, data->blksz, err);
	} else if (data->flags & MMC_DATA_WRITE) {
		pr_debug("akmci: data(write) status=0x%x data_blocks=%d data_blocksize=%d %s\n", status, data->blocks, data->blksz, err);
	}
}

/*
 * Handle completion of command and data transfers.
 */
 static irqreturn_t akmci_irq(int irq, void *dev_id)
{
	struct akmci_host *host = dev_id;
	
	u32 stat_mask;
	u32 status;
	int ret = 1;
	struct platform_device	*pdev = host->pdev;
	
	spin_lock(&host->lock);
	status = readl(host->base + MCI_STATUS_REG);

	dev_dbg(&pdev->dev, "###akmci_irq status : %x\n", status);
	
	if (status & MCI_SDIOINT) {
	    /*
		 * must disable sdio irq ,than read status to clear the sdio status,
         * else sdio irq will come again.
	    */
		mmc_signal_sdio_irq(host->mmc);
		//status |= readl(host->base + MCI_STATUS_REG);
		host->irq_status |= status;
	}

	stat_mask = MCI_RESPCRCFAIL|MCI_RESPTIMEOUT|MCI_CMDSENT|MCI_RESPEND;
	if ((status & stat_mask) && host->cmd){
		host->irq_status = status;
		set_bit(MCI_CMD_IRQ, &host->pending_events);
	}


	stat_mask = MCI_DATACRCFAIL|MCI_DATATIMEOUT|MCI_DATAEND;

	if ((status & stat_mask) && host->data){	
		host->irq_status = status;
		set_bit(MCI_DATA_IRQ, &host->pending_events);
		wake_up(&host->intr_data_wait);
	}
	
	ret = 1;
	spin_unlock(&host->lock);
	return IRQ_RETVAL(ret);
}

static int akmci_setup_data(struct akmci_host *host, struct mmc_data *data)
{
	unsigned int datactrl, dmacon;
   	unsigned int		dma_sg_num;
	struct platform_device	*pdev = host->pdev;
	
	dev_dbg(&pdev->dev,"%s: blksz %04x blks %04x flags %08x\n",
	       __func__, data->blksz, data->blocks, data->flags);
	
	BUG_ON((data->flags & BOTH_DIR) == BOTH_DIR);
	host->l2buf_id = l2_alloc(MCI_L2_ADDR(host));
	if (BUF_NULL == host->l2buf_id)	{
		dev_err(&pdev->dev,"##############L2 buffer malloc fail!\n");
		return -1;
	}else{
		dev_dbg(&pdev->dev, "#############L2 buffer id : %d\n", host->l2buf_id);
	}
	host->data = data;
	host->size = data->blksz * data->blocks;
	host->data_xfered = 0;

	dev_dbg(&pdev->dev,"dir : %s, data->blksz : %u, data->blocks : %u, host->size : %u\n",
			(data->flags&MMC_DATA_READ)? "read" : "write", data->blksz, data->blocks, host->size);

	if (host->size > 64 * 1024)
		dev_err(&pdev->dev, "Err: %s %d akmci %s to long: %d.\n", __func__, __LINE__,
				(data->flags & MMC_DATA_WRITE) ? "write":"read", host->size);

	akmci_init_sg(host, data); 

	if(akmci_xfer_mode(host) == AKMCI_XFER_L2DMA) {
		/* set dma addr */
		if (data->flags & MMC_DATA_WRITE)
			dma_sg_num = dma_map_sg(mmc_dev(host->mmc), data->sg, data->sg_len, DMA_TO_DEVICE);
		else
			dma_sg_num = dma_map_sg(mmc_dev(host->mmc), data->sg, data->sg_len, DMA_FROM_DEVICE);
	}
	
	writel(TRANS_DATA_TIMEOUT, host->base + MCI_DATATIMER_REG);
	writel(host->size, host->base + MCI_DATALENGTH_REG);

	
	/*dma mode register*/
	dmacon = MCI_DMA_BUFEN | MCI_DMA_SIZE(MCI_L2FIFO_SIZE/4); //????????????
		
	if(akmci_xfer_mode(host) == AKMCI_XFER_L2DMA) {
		dmacon |= MCI_DMA_EN;
	}		
	writel(dmacon, host->base + MCI_DMACTRL_REG);	
	
	/* 
	 * enable mci data irq 
	 * */
	enable_imask(host, MCI_DATAIRQMASKS);

	datactrl = MCI_DPSM_ENABLE;

	switch (host->bus_width) {
	case MMC_BUS_WIDTH_8:
		datactrl |= MCI_DPSM_BUSMODE(2);
		break;
	case MMC_BUS_WIDTH_4:
		datactrl |= MCI_DPSM_BUSMODE(1);
		break;
	case MMC_BUS_WIDTH_1:
	default:
		datactrl |= MCI_DPSM_BUSMODE(0);
		break;
	}

	if (data->flags & MMC_DATA_STREAM) {
		dev_dbg(&pdev->dev, "STREAM Data\n");
		datactrl |= MCI_DPSM_STREAM;
	} else {
		dev_dbg(&pdev->dev, "BLOCK Data: %u x %u\n", data->blksz, data->blocks);
		datactrl |= MCI_DPSM_BLOCKSIZE(data->blksz);
		datactrl &= ~MCI_DPSM_STREAM;

#ifdef SDIO_INTR_AUTO_DETECT
		if(data->blocks > 1)
			datactrl |= MCI_DPSM_ABORT_EN_MULTIBLOCK;
#endif
	}

	if (data->flags & MMC_DATA_READ) 
		datactrl |= MCI_DPSM_DIRECTION;
	else if (data->flags & MMC_DATA_WRITE) 
		datactrl &= ~MCI_DPSM_DIRECTION;

	/* configurate data controller register */
	writel(datactrl, host->base + MCI_DATACTRL_REG);
	
	dev_dbg(&pdev->dev, "r DATA IRQ, datactrl: 0x%08x, timeout: 0x%08x, len: %u, dmacon:0x%08x,host->bus_width=%d\n",
	       datactrl, readl(host->base + MCI_DATATIMER_REG), host->size,dmacon,host->bus_width);
	return 0;
	
}

static int akmci_wait_cmd_complete(struct akmci_host *host)
{
	int ret = 0;
	unsigned int status;
	struct mmc_command	*cmd = host->cmd;
	unsigned int cmd_retry_count = 0;
	unsigned long flags;
	unsigned long cmd_jiffies_timeout;
	struct platform_device	*pdev = host->pdev;
	
	cmd_jiffies_timeout = jiffies + request_timeout;
	while(1){
		if (!time_before(jiffies, cmd_jiffies_timeout)) {
			host->cmd->error = -ETIMEDOUT;
			dev_err(&pdev->dev,"##wait cmd request complete is timeout!");
			return -1;
		}
		
		do {
			spin_lock_irqsave(&host->lock, flags);
			if (test_bit(MCI_CMD_IRQ, &host->pending_events))
			{
				status = host->irq_status;	
				spin_unlock_irqrestore(&host->lock, flags);

				cmd->resp[0] = readl(host->base + MCI_RESPONSE0_REG);
				cmd->resp[1] = readl(host->base + MCI_RESPONSE1_REG);
				cmd->resp[2] = readl(host->base + MCI_RESPONSE2_REG);
				cmd->resp[3] = readl(host->base + MCI_RESPONSE3_REG);
				
				dev_dbg(&pdev->dev, "resp[0]=0x%x, [1]=0x%x, resp[2]=0x%x, [3]=0x%x\n",
					cmd->resp[0],cmd->resp[1],cmd->resp[2],cmd->resp[3]);
				
				if (status & MCI_RESPTIMEOUT) {
					host->cmd->error = -ETIMEDOUT;
					dev_dbg(&pdev->dev, "CMD: send timeout\n");
					ret = -1;
				} else if ((status & MCI_RESPCRCFAIL) && (host->cmd->flags & MMC_RSP_CRC)) {
					host->cmd->error = -EILSEQ;
					dev_dbg(&pdev->dev, "CMD: illegal byte sequence\n");
					ret= -1;
				}
				/* disable mci cmd irq */
				disable_imask(host, MCI_CMDIRQMASKS);
				clear_bit(MCI_CMD_IRQ, &host->pending_events);
				
				host->cmd = NULL;
				
				dev_dbg(&pdev->dev, "end akmci_wait_cmd_complete ret=%d\n", ret);
								
				return ret;
			}
			spin_unlock_irqrestore(&host->lock, flags);
			cmd_retry_count++;
		} while (cmd_retry_count < retry_count);
		schedule();
	}

}

static int akmci_wait_data_complete(struct akmci_host *host)
{
	unsigned int status;
	struct mmc_data *data = host->data;
	long time = request_timeout;
	struct platform_device	*pdev = host->pdev;

	time = wait_event_timeout(host->intr_data_wait, test_bit(MCI_DATA_IRQ, &host->pending_events), time);
	/* disable mci data irq */
	disable_imask(host, MCI_DATAIRQMASKS|MCI_FIFOFULLMASK|MCI_FIFOEMPTYMASK);

	if(time <= 0){
		status = MCI_DATATIMEOUT;
		dev_err(&pdev->dev, "##wait data %s complete is timeout host->size: %u, host->data_xfered: %u!\n", 
				(host->data->flags & MMC_DATA_WRITE)?"write":"read", host->size, host->data_xfered);
	}else{
		status = host->irq_status;
		dev_dbg(&pdev->dev, "##wait data %s complete! host->size: %u, host->data_xfered: %u!\n", 
				(host->data->flags & MMC_DATA_WRITE)?"write":"read", host->size, host->data_xfered);
	}
	
	if (status & MCI_DATAEND)
	{
		//if((akmci_xfer_mode(host) == AKMCI_XFER_L2DMA) && 
		if((AK_TRUE == !l2_combuf_wait_dma_finish(host->l2buf_id)))
		{
			clear_bit(MCI_DATA_IRQ, &host->pending_events);
			return 0;
		}
		if (data->flags & MMC_DATA_WRITE)
			l2_clr_status(host->l2buf_id);

		if((host->size != host->data_xfered) &&
			(USED_DMA(host->size) || USED_DMA_CPU(host->size))){
				dev_err(&pdev->dev, "## %s host->size : %u , host->data_xfered : %u wait...\n",
					(host->data->flags & MMC_DATA_WRITE)?"write":"read", host->size, host->data_xfered);
			while(host->size != host->data_xfered){
				schedule();
			}
		}
		
	}else if(status &(MCI_DATACRCFAIL|MCI_DATATIMEOUT)){
		if (status & MCI_DATACRCFAIL) {
			data->error = -EILSEQ;
			print_mci_data_err(data, status, "illeage byte sequence");
			host->data_err_flag = 1;
			clear_bit(MCI_DATA_IRQ, &host->pending_events);
			return -1;
			
		} else if (status & MCI_DATATIMEOUT) {
			data->error = -ETIMEDOUT;
			print_mci_data_err(data, status, "transfer timeout");
			host->data_err_flag = 1;
			clear_bit(MCI_DATA_IRQ, &host->pending_events);
			return -2;
		}
	}else{
		print_mci_data_err(data, status, "transfer data err!");
		return -3;  // lhd add check later!!!
	}

	clear_bit(MCI_DATA_IRQ, &host->pending_events);
	return 0;
}


/*
 * static void akmci_check_status(struct akmci_host *host)
 *
 * H3D/H3B MMC/SD在读写过程中如果拔出了TF卡，拔出无中断。
 * 这属于芯片问题，需要通过软件规避。
 * 以mci0为例，出现异常时，SD Plugin-Plugoff Register 0寄存器的 
 * SD0_plugoff_status [bit8] = 0（正常触发中断，该状态位为1）
 * mck0_in [bit19] =1 （The SD card is plugged off）
 * 所以调用 _get_card_status，后者基于 mck0_in bit 来判断 card_status,并更新host->card_status变量。
 * 当host->card_status == CARD_UNPLUGED,则按照拔出中断的处理流程： 清中断状态；禁止拔出中断使能插入中断;调度delay_work进行防抖处理。
 */
static void akmci_check_status(struct akmci_host *host)
{
		int status_reg;
		struct platform_device	*pdev = host->pdev;

		 if((host->card_status == CARD_PLUGED) && (host->detect_mode == AKMCI_DETECT_MODE_CLK))
		 {
			host->card_status = _get_card_status(host);
			if(host->card_status == CARD_UNPLUGED)
			{
				dev_info(&host->pdev->dev,"akmci_check_status check card_plug_off-->");

				/*clear irq status, and set irq  enable bit*/
				config_plugoff_int(host,0);
				status_reg = readl(MCI_PLUGINOFF_REG0);
				status_reg |=  1 << (MCI_PLUGIN_STA_OFFSET + host->pdev->id);
				status_reg |=  1 << (MCI_PLUGOFF_STA_OFFSET + host->pdev->id);
				writel(status_reg, MCI_PLUGINOFF_REG0);
				config_plugin_int(host,1);
				schedule_delayed_work(&host->work, msecs_to_jiffies(host->software_debounce));
			}
		 }
}

static void akmci_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct akmci_host *host = mmc_priv(mmc);
	struct platform_device	*pdev = host->pdev;
	int ret;
	unsigned int clk;
	
	host->data_err_flag = 0;

	dev_dbg(&pdev->dev,"start the mci request.host->card_status=%d (0=plugoff,1=plugin)\n", host->card_status);

	if(!akmci_card_present(mmc)){
		dev_dbg(&pdev->dev,"%s: no medium present.\n", __func__);
		mrq->cmd->error = -ENOMEDIUM;
		goto  akmci_req_end;
	}

	if(mrq->data){
		ret = akmci_setup_data(host, mrq->data);
		if (ret) {
			mrq->data->error = ret;
			printk("data setup is error!");
			goto akmci_req_end;
		}

		if ((mrq->data->flags & MMC_DATA_READ) && 
				(USED_DMA(host->size) || USED_DMA_CPU(host->size))){
			akmci_l2xfer(host);
		}	
	}	
	akmci_start_command(host, mrq->cmd);
	ret = akmci_wait_cmd_complete(host);
	if (ret) {
		if(mrq->data)
			akmci_stop_data(host);
		akmci_check_status(host);
		goto akmci_req_end;
	}
	
	if (!(mrq->data && !mrq->cmd->error)){
		goto akmci_req_end;
	}
	
	if(mrq->data){
		if(mrq->data->flags & MMC_DATA_READ){
			dev_dbg(&pdev->dev,"MMC_DATA_READ\n");
			ret = akmci_wait_data_complete(host);
			if(ret){
				//dev_err(&pdev->dev,"MMC_DATA_READ err = %d\n", ret);
				akmci_check_status(host);
				goto akmci_data_err;
			}
			if(USED_CPU(host->size)){ //cpu mode
				dev_dbg(&pdev->dev,"want cpu mode, host->size : %d \n", host->size);
				akmci_l2xfer(host);
			}
		}
		else if(mrq->data->flags & MMC_DATA_WRITE){
			dev_dbg(&pdev->dev,"MMC_DATA_WRITE\n");
			akmci_l2xfer(host);
			ret = akmci_wait_data_complete(host);
			if(ret){
				//dev_err(&pdev->dev,"MMC_DATA_WRITE err = %d\n", ret);
				akmci_check_status(host);
				goto akmci_data_err;
			}
		}
		if (mrq->stop){
			dev_dbg(&pdev->dev,"write stop cmd\n");
			akmci_start_command(host, mrq->stop);
			ret = akmci_wait_cmd_complete(host);
			if (ret) {
				//dev_err(&pdev->dev,"stop cmd err = %d\n", ret);
				akmci_check_status(host);
				goto akmci_data_err;
			}
		}
}

akmci_data_err:
	akmci_stop_data(host);

akmci_req_end:
	akmci_request_end(host, mrq);
}

/**
 * @brief setting the mmc module working clock.
 * @param [in] *mmc information of host ,getting the sdio detect gpio.
 * @return int.
 * @retal 0:success. otherwise :err.
 */
static void akmci_set_clk(struct akmci_host *host, struct mmc_ios *ios)
{
	unsigned int clk, div;
	unsigned int clk_div_h, clk_div_l;
	struct platform_device	*pdev = host->pdev;

	if (ios->clock == 0) {
		clk = readl(host->base + MCI_CLK_REG);
		clk &= ~MCI_CLK_ENABLE;
		writel(clk, host->base + MCI_CLK_REG);	
		
		host->bus_clock = 0;
	} else {
		clk = readl(host->base + MCI_CLK_REG);
		
		// pr_err("mci_mode is %s\r\n", (host->mci_mode == MCI_MODE_SDIO)?"MCI_MODE_SDIO":"MCI_MODE_MMC_SD");
		clk |= (host->mci_mode == MCI_MODE_SDIO)?(MCI_CLK_ENABLE):(MCI_CLK_ENABLE | MCI_CLK_PWRSAVE) ;
		// pr_err("clk = 0x%08x\r\n", clk);

		clk &= ~0xffff; /* clear clk div */
		
		div = host->gclk/ios->clock;

        if (host->gclk % ios->clock)
            div += 1;
		
        div -= 2;

		/* better to 50% duty */
        clk_div_h = div/2;
        clk_div_l = div - clk_div_h;

		clk |= MMC_CLK_DIVL(clk_div_l) | MMC_CLK_DIVH(clk_div_h);
		writel(clk, host->base + MCI_CLK_REG);	
		
		host->bus_clock = host->gclk / ((clk_div_h+1)+(clk_div_l + 1));
		dev_info(&pdev->dev, "gclk = %lu Hz. div_l=%u, div_h=%u\n",host->gclk, clk_div_l, clk_div_h);
		dev_info(&pdev->dev, "ios->clock = %u Hz. host->bus_clock = %u Hz\n",ios->clock,host->bus_clock);
	}
	host->clkreg = clk;
}


static void akmci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct akmci_host *host = mmc_priv(mmc);
	struct platform_device	*pdev = host->pdev;

	switch(ios->power_mode) {
		case MMC_POWER_ON:
			dev_dbg(&pdev->dev, "mci power on.\n");
			break;
		case MMC_POWER_UP:
			dev_dbg(&pdev->dev,"mci power up.\n");
			break;
		case MMC_POWER_OFF:
			dev_dbg(&pdev->dev,"mci power off.\n");
			break;
		default:
			break;
	}
	host->bus_mode = ios->bus_mode;
	host->bus_width = ios->bus_width;

	dev_dbg(&pdev->dev, "ios->bus_mode = %d, ios->bus_width =%d, ios->clock=%u\n",ios->bus_mode,ios->bus_width,ios->clock);
	if(ios->clock != host->bus_clock) {
		akmci_set_clk(host, ios);
	}
}

/**
 * @brief   detect the sdio card writing protection.
 * @param [in] *mmc information of host ,getting the sdio detect gpio.
 * @return int.
 * @retal 1 sdio card writing protected ;0 sdio card writing is not protected
 */
static int akmci_get_ro(struct mmc_host *mmc)
{
#ifdef READ_ONLY_SD   
	struct akmci_host *host = mmc_priv(mmc);

	if (host->gpio_wp == -ENOSYS)
		return -ENOSYS;
	
	return (gpio_get_value(host->gpio_wp) == 0);
#else
	return -ENOSYS;
#endif	
}

/**
 * @brief  enable or disable sdio interrupt, mmc host not use..
 * @param [in] *mmc information of sd controller.
 * @param [in] enable  1: enable; 0: disable.
 * @return void.
 */

static void akmci_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	unsigned reg1,reg2;
	unsigned long flags;	
	struct akmci_host *host = mmc_priv(mmc);
	struct platform_device	*pdev = host->pdev;
	static int init_sdio_irq = 0 ;

	dev_dbg(&pdev->dev, "%s the sdio interrupt.\n", enable ? "enable" : "disable");
	spin_lock_irqsave(&host->lock, flags); 
	
#ifdef SDIO_INTR_AUTO_DETECT
	if (enable) {
		reg1 = readl(host->base + SDIO_INTRCTR_REG);
		reg1 &= ~SDIO_INTR_CHECK_ENABLE;
		writel(reg1, host->base + SDIO_INTRCTR_REG);

		reg1 = readl(host->base + MCI_MASK_REG);
		reg1 &= ~SDIO_INTR_MANUAL_DETECT_ENABLE;
		reg1 |= SDIO_INTR_ENABLE | SDIO_INTR_AUTO_DETECT_ENABLE;
		writel(reg1, host->base + MCI_MASK_REG);

		reg1 = readl(host->base + MCI_COMMAND_REG);
		reg1 |= MCI_RESTORE_SDIOIRQ;
		writel(reg1, host->base + MCI_COMMAND_REG);
		reg1 &= ~MCI_RESTORE_SDIOIRQ;
		writel(reg1, host->base + MCI_COMMAND_REG);

	} else {
		reg1 = readl(host->base + MCI_MASK_REG);
		reg1 &= ~SDIO_INTR_ENABLE;
		writel(reg1, host->base + MCI_MASK_REG);
	}
#else
	reg1 = readl(host->base + MCI_MASK_REG);
	reg2 = readl(host->base + SDIO_INTRCTR_REG);

	if (enable) {
		reg1 |= SDIO_INTR_ENABLE | SDIO_INTR_MANUAL_DETECT_ENABLE;
		reg2 |= SDIO_INTR_CHECK_ENABLE;
	} else {
		reg1 &= ~(SDIO_INTR_ENABLE | SDIO_INTR_MANUAL_DETECT_ENABLE);
		reg2 &= ~SDIO_INTR_CHECK_ENABLE;
	}
	writel(reg2, host->base + SDIO_INTRCTR_REG);	
	writel(reg1, host->base + MCI_MASK_REG);
#endif

	spin_unlock_irqrestore(&host->lock, flags); 
}

int ak_sdio_rescan(int slot)
{
    struct mmc_host *mmc = mci_host[slot];

    if (!mmc) {
        pr_err("invalid mmc, please check the argument\n");
        return -EINVAL;
    }

    mmc_detect_change(mmc, 0); 
    return 0;
}
EXPORT_SYMBOL_GPL(ak_sdio_rescan);
/**
 * register the function of sd/sdio driver.
 * 
 */
static struct mmc_host_ops akmci_ops = {
	.request = akmci_request,
	.set_ios = akmci_set_ios,
	.get_ro  = akmci_get_ro,
	.get_cd  = akmci_card_present,
	.enable_sdio_irq = akmci_enable_sdio_irq,
};

/**
 * @brief  initilize the mmc host.
 * @param [in] *mmc information of sd controller.
 * @return 0:success..
 */
static int akmci_init_mmc_host(struct akmci_host *host)
{
	struct mmc_host *mmc = host->mmc;
	struct platform_device	*pdev = host->pdev;
	struct device_node *np = pdev->dev.of_node;

	if (np)
		mmc_of_parse(mmc);

	mmc->ops = &akmci_ops;
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;

	mmc->f_min = host->gclk / (255+1 + 255+1);
	mmc->f_max = ((host->gclk / (0+1 + 0+1)) > mmc->f_max) ? mmc->f_max : host->gclk / (0+1 + 0+1);
	dev_dbg(&pdev->dev, "fmin=%d, fmax=%d,",mmc->f_min,mmc->f_max);

	/* if f_max < 25MHz, clear MMC_CAP_SD_HIGHSPEED flag */
	if(mmc->f_max  < 25000000)
		mmc->caps &= ~MMC_CAP_SD_HIGHSPEED;

	/* support mmc erase */
	mmc->caps |= MMC_CAP_ERASE;

#ifdef CONFIG_MMC_BLOCK_BOUNCE
	/* use block bounce buffer. */
	mmc->max_segs = 1;
#else
	/* We can do SGIO 128 */
	mmc->max_segs = MAX_MCI_REQ_SIZE/MAX_MCI_BLOCK_SIZE;
#endif

	/*
	 * Since we only have a 16-bit data length register, we must
	 * ensure that we don't exceed 2^16-1 (65535)bytes in a single request.
	 */ 
	mmc->max_req_size = MAX_MCI_REQ_SIZE;

	/*
	 * Set the maximum segment size.  Since we aren't doing DMA
	 * (yet) we are only limited by the data length register.
	 */
	mmc->max_seg_size = mmc->max_req_size;

	mmc->max_blk_size = MAX_MCI_BLOCK_SIZE; //512

	/*No limit on the number of blocks transferred.*/
	/*128*/
	mmc->max_blk_count = mmc->max_req_size / MAX_MCI_BLOCK_SIZE;
	return 0;
}

static void akmci_init_host_cfg(struct akmci_host *host)
{
	struct pinctrl_state *state;
	if(host->pdev->dev.pins)
	{
		state = pinctrl_lookup_state(host->pdev->dev.pins->p, "default");
		if(IS_ERR(state))
			pr_err("%s no default state\n", __func__);
		else
			pinctrl_select_state(host->pdev->dev.pins->p,state);
	}
	pinctrl_pm_select_default_state(&host->pdev->dev);
	/*enable the mci clock*/
	writel(MCI_ENABLE|MCI_FALL_TRIGGER, host->base + MCI_CLK_REG);
	//writel(MCI_ENABLE, host->base + MCI_CLK_REG);
	clear_imask(host);
	return;
}

static void akmci_pinctrl_idle(struct akmci_host *host)
{	
	struct pinctrl_state *state;
	if(host->pdev->dev.pins)
	{
		state = pinctrl_lookup_state(host->pdev->dev.pins->p, "idle");
		if(IS_ERR(state))
			pr_err("%s no idle state\n", __func__);
		else
			pinctrl_select_state(host->pdev->dev.pins->p,state);
	}
	return;
}

#if defined(CONFIG_OF)
static const struct of_device_id akmci_match[] = {
	{ .compatible = "anyka,ak37d-mmc0" },	
	{ .compatible = "anyka,ak37d-mmc1" },
	{ .compatible = "anyka,ak37d-mmc2" },
	{ .compatible = "anyka,ak39ev330-mmc0" },
	{ .compatible = "anyka,ak39ev330-mmc1" },
	{ .compatible = "anyka,ak37e-mmc0" },
	{ .compatible = "anyka,ak37e-mmc1" },
	{ .compatible = "anyka,ak37e-mmc2" },
	{ }
};
MODULE_DEVICE_TABLE(of, akmci_match);

static  struct akmci_host*
akmci_of_init(struct akmci_host *host)
{
	struct platform_device *pdev = host->pdev;
	struct device_node *np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return ERR_PTR(-EINVAL);
	}

	if(of_property_read_bool(np, "non-removable"))
		host->detect_mode = AKMCI_PLUGIN_ALWAY;
	else if(of_property_read_bool(np, "cd_clk"))
		host->detect_mode = AKMCI_DETECT_MODE_CLK;
	else if(of_property_read_bool(np, "detect-gpio") || of_property_read_bool(np, "detect-gpios")){
		host->detect_mode = AKMCI_DETECT_MODE_GPIO;
		
		host->gpio_cd = of_get_named_gpio(np, "detect-gpio", 0);
		if(host->gpio_cd < 0)
			host->gpio_cd = of_get_named_gpio(np, "detect-gpios", 0);
		dev_info(&pdev->dev,"host->gpio_cd=%d\n", host->gpio_cd);
	}
	else
		host->detect_mode = AKMCI_PLUGIN_ALWAY;
	
	host->xfer_mode = AKMCI_XFER_L2DMA;

	if(of_property_read_bool(np, "cap-sdio-irq"))
		host->mci_mode = MCI_MODE_SDIO;
	else
		host->mci_mode = MCI_MODE_MMC_SD;

	if (of_property_read_u32(np, "debounce-interval",
					 &host->software_debounce))
			host->software_debounce = 300;

	return host;
}
#else /* CONFIG_OF */
static inline struct akmci_host*
akmci_of_init(struct akmci_host *host)
{
	return ERR_PTR(-EINVAL);
}
#endif

static struct kobject *gpio_en_obj = NULL;
/* 
 *	return the status of actual power-gpio level to userspace.
 *	"1": high level voltage
 *	"0": lower level voltage
 */
static ssize_t mmc_card_pwr_enable_read(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct power_gpio_attribute *pga = container_of(attr, struct power_gpio_attribute, k_attr);
	struct akmci_host *host = container_of(pga, struct akmci_host, gpio_power);
	unsigned int mmc_en;
	pr_info("host->gpio_power.power_invert=%d, host->gpio_power.value=%d\n",
		host->gpio_power.power_invert,host->gpio_power.value);
	
	mmc_en = (pga->value)^(host->gpio_power.power_invert);

	return sprintf(buf, "%d\n",mmc_en);
}

/* 
 *	control the power-on or power-off of sdio device or sd card
 *	"1": enable card circuit
 *	"0": disalbe card circuit
 */
static ssize_t mmc_card_pwr_enable_write(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count)
{
	unsigned int card_pwr_en;
	struct power_gpio_attribute *pga = container_of(attr, struct power_gpio_attribute, k_attr);
	struct akmci_host *host = container_of(pga, struct akmci_host, gpio_power);
	sscanf(buf, "%d", &card_pwr_en);
	
	
	/* the voltage value to set gpio pin */
	pga->value = (host->gpio_power.power_invert ^ (!!card_pwr_en));

	pr_info("%s %s using gpio%d with gpio_val %d.",
				(card_pwr_en? "enable":"disable"),
				(host->mci_mode == MCI_MODE_MMC_SD ? "tf_card circuit": "sdio_card circuit"),
				pga->gpio,
				pga->value);

    if(host->card_status == CARD_PLUGED)
	{
		gpio_direction_output(pga->gpio, pga->value);
		if(card_pwr_en) // card_pwr_en = enable
		{
			akmci_init_host_cfg(host);
		}
		else // card_pwr_en = disable
		{
			akmci_pinctrl_idle(host);
		}
	}
	mmc_detect_change(host->mmc, msecs_to_jiffies(300));

	return count;
}

/*
 * @brief 	AKMCI_DETECT_MODE_GPIO mode using timer to detect the change of card status.
 *			This is the handler of timer.
 *
 */
static void akmci_detect_change(unsigned long data)
{
	struct akmci_host *host = (struct akmci_host *)data;
	unsigned int card_pwr_en;
	int curr_status = 0;

	pr_debug("card detect change.\n");
	
	/* the voltage value to set gpio pin */

	curr_status = _get_card_status(host);
	if(curr_status < 0)
		goto err;
	if (curr_status != host->card_status) {
		host->card_status = curr_status;
		if (curr_status == CARD_PLUGED) {
			pr_info("card connected!\n");
			
			if(host->gpio_power.gpio >= 0){
				host->gpio_power.value = (host->gpio_power.power_invert ^ (!!CARD_POWER_ENABLE));
				gpio_direction_output(host->gpio_power.gpio, host->gpio_power.value);
			}

			akmci_init_host_cfg(host);
		} else{
			pr_info("card disconnected!\n");

			if(host->gpio_power.gpio >= 0){
				host->gpio_power.value = (host->gpio_power.power_invert ^ (!!CARD_POWER_DISABLE));
				gpio_direction_output(host->gpio_power.gpio, host->gpio_power.value);
			}
			
		}
		mmc_detect_change(host->mmc, msecs_to_jiffies(300));
	}
	
err:
	mod_timer(&host->detect_timer, jiffies + detect_time);
}

/*
 * @brief  	/sys/devices/platform/soc/20108000.mmc1/card_status
 *
 */
static ssize_t akmci_show_card_status(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct akmci_host *host;
	struct mmc_host *mmc;

	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);

	if(host->card_status == CARD_PLUGED)
		return sprintf(buf, "%s\n", "card_pluged!");
	else if(host->card_status == CARD_UNPLUGED)
		return sprintf(buf, "%s\n", "card_unpluged!");
	else
		return sprintf(buf, "%s\n", "card_status_error!");
}
static DEVICE_ATTR(card_status, S_IRUGO , akmci_show_card_status, NULL);
/*
 * @brief  /sys/devices/platform/soc/20100000.mmc0/bus_clock
 *
 */
static ssize_t akmci_show_bus_clock(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct akmci_host *host;
	struct mmc_host *mmc;

	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);

	return sprintf(buf, "%uHz\n",host->bus_clock);
}
static DEVICE_ATTR(bus_clock, S_IRUGO , akmci_show_bus_clock, NULL);

/*
 * @brief  /sys/devices/platform/soc/20100000.mmc0/bus_mode
 *
 */
static ssize_t akmci_show_bus_mode(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct akmci_host *host;
	struct mmc_host *mmc;

	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);

	if(host->bus_mode == MMC_BUSMODE_OPENDRAIN)
		return sprintf(buf, "%s\n", "busmode_opendrain");
	else if(host->bus_mode == MMC_BUSMODE_PUSHPULL)
		return sprintf(buf, "%s\n", "busmode_pushpull");
	else
		return sprintf(buf, "%s\n", "busmode_error");
}
static DEVICE_ATTR(bus_mode, S_IRUGO , akmci_show_bus_mode, NULL);

/*
 * @brief  /sys/devices/platform/soc/20100000.mmc0/bus_width
 *
 */
static ssize_t akmci_show_bus_width(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct akmci_host *host;
	struct mmc_host *mmc;

	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);

	if(host->bus_width == MMC_BUS_WIDTH_1)
		return sprintf(buf, "%s\n", "bus_width_1");
	else if(host->bus_width == MMC_BUS_WIDTH_4)
		return sprintf(buf, "%s\n", "bus_width_4");
	else if(host->bus_width == MMC_BUS_WIDTH_8)
		return sprintf(buf, "%s\n", "bus_width_8");
	else
		return sprintf(buf, "%s\n", "bus_width_error");
}
static DEVICE_ATTR(bus_width, S_IRUGO , akmci_show_bus_width, NULL);

/*
 * @brief  /sys/devices/platform/soc/20100000.mmc0/mci_mode
 *
 */
static ssize_t akmci_show_mci_mode(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct akmci_host *host;
	struct mmc_host *mmc;

	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);

	if(host->mci_mode == MCI_MODE_SDIO)
		return sprintf(buf, "%s\n", "sdio_mode");
	else if(host->mci_mode == MCI_MODE_MMC_SD)
		return sprintf(buf, "%s\n", "sd_mode");
	else
		return sprintf(buf, "%s\n", "mci_mode_error");
}
static DEVICE_ATTR(mci_mode, S_IRUGO , akmci_show_mci_mode, NULL);

/*
 * @brief  /sys/devices/platform/soc/20100000.mmc0/f_min
 *
 */
static ssize_t akmci_show_f_min(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct akmci_host *host;
	struct mmc_host *mmc;

	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);

	return sprintf(buf, "%uHz\n",host->mmc->f_min);
}
static DEVICE_ATTR(f_min, S_IRUGO , akmci_show_f_min, NULL);

/*
 * @brief  /sys/devices/platform/soc/20100000.mmc0/f_max
 *
 */
static ssize_t akmci_show_f_max(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct akmci_host *host;
	struct mmc_host *mmc;

	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);

	return sprintf(buf, "%uHz\n",host->mmc->f_max);
}
/*
 * @brief  /sys/devices/platform/soc/20100000.mmc0/f_max
 *
 */
static ssize_t akmci_store_f_max(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct akmci_host *host;
	struct mmc_host *mmc;
	unsigned int f_max;
	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);

	sscanf(buf, "%d", &f_max);
	mmc->f_max = ((host->gclk / (0+1 + 0+1)) > f_max) ? f_max : host->gclk / (0+1 + 0+1);

	/* if f_max < 25MHz, clear MMC_CAP_SD_HIGHSPEED flag */
	if(mmc->f_max  < 25000000)
		mmc->caps &= ~MMC_CAP_SD_HIGHSPEED;

	return count;
}

static DEVICE_ATTR(f_max, 0600 , akmci_show_f_max, akmci_store_f_max);

/*
 * @brief  /sys/devices/platform/soc/20100000.mmc0/detect_mode
 *
 */
static ssize_t akmci_show_detect_mode(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct akmci_host *host;
	struct mmc_host *mmc;
	
	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);
	
	if(host->detect_mode == AKMCI_PLUGIN_ALWAY)
		return sprintf(buf, "%s\n", "non-removable");
	else if(host->detect_mode == AKMCI_DETECT_MODE_CLK)
		return sprintf(buf, "%s\n", "clk_detect_mode");
	else if(host->detect_mode == AKMCI_DETECT_MODE_GPIO)
		return sprintf(buf, "%s\n", "gpio_detect_mode");
	else
		return sprintf(buf, "%s\n", "detect_mode_error");

}
static DEVICE_ATTR(detect_mode, 0600 , akmci_show_detect_mode, NULL);
				
static struct attribute *akmci_attributes[] = {
	&dev_attr_card_status.attr,
	&dev_attr_bus_clock.attr,
	&dev_attr_bus_mode.attr,
	&dev_attr_bus_width.attr,
	&dev_attr_mci_mode.attr,
	&dev_attr_f_min.attr,
	&dev_attr_f_max.attr,
	&dev_attr_detect_mode.attr,
	NULL,
};

static const struct attribute_group akmci_attrs = {
	.attrs = akmci_attributes,
};


static void pluginoff_work_func(struct work_struct *work)
{
	struct akmci_host *host =
		container_of(work, struct akmci_host, work.work);
	int curr_status = 0;

	pr_debug("enter %s\n",__FUNCTION__);

	curr_status = _get_card_status(host);
	if (curr_status == host->card_status){
		if(curr_status == CARD_PLUGED)
		{
			dev_info(&host->pdev->dev,"card_plug_in-->");
			if(host->gpio_power.gpio >=0){
				host->gpio_power.value = host->gpio_power.power_invert ^CARD_POWER_ENABLE;
				pr_info("host->gpio_power.value =%d, host->gpio_power.power_invert=%d\n"
							,host->gpio_power.value,host->gpio_power.power_invert);
				gpio_direction_output(host->gpio_power.gpio, host->gpio_power.value);
			}
		}
		else{
			dev_info(&host->pdev->dev,"card_plug_off-->");

			if(host->gpio_power.gpio >=0){
				host->gpio_power.value = host->gpio_power.power_invert ^ CARD_POWER_DISABLE;
				pr_info("host->gpio_power.value =%d, host->gpio_power.power_invert=%d\n"
							,host->gpio_power.value,host->gpio_power.power_invert);
				gpio_direction_output(host->gpio_power.gpio, host->gpio_power.value);
			}
		}
		
		host-> card_status = curr_status;
		mmc_detect_change(host->mmc, msecs_to_jiffies(300));
	}else{
		pr_debug("curr_status != host->card_status\n");
		if(curr_status == CARD_PLUGED)
		{
			/* since card is pluged, enable plugoff irq and disable plugin irq*/
			config_plugoff_int(host,1);
			config_plugin_int(host,0);
		}
		else
		{
			/* since card is unpluged, enable plugin irq and disable plugoff irq*/
			config_plugoff_int(host,0);
			config_plugin_int(host,1);
		}
	}
	return;
}

/**
 * @brief   sdio driver probe and init.
 * @param [in] *pdev information of platform device ,getting the sd driver resource .
 * @return int.
 * @retval -EINVAL no platform data , fail;
 * @retval -EBUSY  requset mem  fail;
 * @retval -ENOMEM  alloc mem fail;
 */
static int akmci_probe(struct platform_device *pdev)
{
	struct akmci_host *host;
	struct mmc_host *mmc;
	struct device_node* np = pdev->dev.of_node;
	struct resource		*mem;
	int ret;

	mmc = mmc_alloc_host(sizeof(struct akmci_host), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto probe_free_host;
	}

	mmc->pm_caps |= MMC_PM_KEEP_POWER;
	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->pdev = pdev;
	spin_lock_init(&host->lock);

	host->data_err_flag = 0;
	host->l2buf_id = BUF_NULL;

	host = akmci_of_init(host);
	if(IS_ERR(host))
	{
		dev_err(&pdev->dev, "platform data not available\n");
		ret = PTR_ERR(host);
		goto probe_free_host;
	}
	pdev->id = of_alias_get_id(np, "mmc");
	
	dev_info(&pdev->dev,"akmci_probe : %s\n", host->mci_mode?"SDIO":"MCI");

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->base = devm_ioremap_resource(&pdev->dev, mem);
	
	if (IS_ERR(host->base)) {
		ret = PTR_ERR(host->base);
		goto probe_free_host;
	}
	/* We get spurious interrupts even when we have set the IMSK
	 * register to ignore everything, so use disable_irq() to make
	 * ensure we don't lock the system with un-serviceable requests. */
	//disable_irq(host->irq_mci);

	host->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(host->clk)) {
		dev_err(&pdev->dev, "failed to find clock source.\n");
		ret = PTR_ERR(host->clk);
		goto probe_free_host;
	}
	
	ret = clk_prepare_enable(host->clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable clock source.\n");
		goto probe_free_host;
	}
	host->gclk = clk_get_rate(host->clk);

	host->irq_mci = platform_get_irq(pdev, 0);
	if(host->irq_mci <= 0) {
		dev_err(&pdev->dev, "failed to get interrupt resouce.\n");
		ret = -EINVAL;
		goto clk_disable;
	}
	init_waitqueue_head(&host->intr_data_wait);

	if (devm_request_irq(&pdev->dev, host->irq_mci, akmci_irq, 0, pdev->name, host)) {
			dev_err(&pdev->dev, "failed to request mci interrupt.\n");
			ret = -ENOENT;
			goto clk_disable;
	}
	host->card_status = get_card_status(host);
	pr_debug("card_status@probe =%d.(1=CARD_PLUGED, 0=CARD_UNPLUGED)\n",host->card_status);
	/*
	 * detect_mode, define by dts, there is three kind of detect mode:
	 *		AKMCI_PLUGIN_ALWAY
	 *		AKMCI_DETECT_MODE_CLK
	 *		AKMCI_DETECT_MODE_GPIO
	 */
	if(host->detect_mode == AKMCI_DETECT_MODE_CLK)
	{
		host->irq_cd = platform_get_irq(pdev, 1);
		if(host->irq_cd <= 0) {
			dev_err(&pdev->dev, "failed to get sd plug in off resouce.\n");
			ret = -EINVAL;
			goto clk_disable;
		}
		if(host->card_status == CARD_PLUGED)
		{
			/* since card is pluged, enable plugoff irq and disable plugin irq*/
			config_plugoff_int(host,1); 
			config_plugin_int(host,0);	
		}
		else
		{
			/* since card is unpluged, enable plugin irq and disable plugoff irq*/
			config_plugoff_int(host,0); 
			config_plugin_int(host,1);
		}
		
		if (devm_request_irq(&pdev->dev, host->irq_cd, pluginoff_irq, 0, pdev->name, host)) {
			dev_err(&pdev->dev, "failed to request mci interrupt.\n");
			ret = -ENOENT;
			goto clk_disable;
		}
		/*for plugin or plugoff debouce. */
		INIT_DELAYED_WORK(&host->work, pluginoff_work_func);
		
	}
	else if(host->detect_mode == AKMCI_DETECT_MODE_GPIO) {
		if(host->gpio_cd >= 0){
			init_timer(&host->detect_timer);		
			host->detect_timer.function = akmci_detect_change;
			host->detect_timer.data = (unsigned long)host;
			host->detect_timer.expires = jiffies + detect_time;
			add_timer(&host->detect_timer);
		}
	}else  //AKMCI_PLUGIN_ALWAY
	{
	}

	host->gpio_power.gpio = of_get_named_gpio(np, "power-pins", 0);
	dev_dbg(&pdev->dev,"host->gpio_power.gpio = %d\n", host->gpio_power.gpio);
	if(host->gpio_power.gpio >= 0)
	{
			host->gpio_power.power_invert = of_property_read_bool(np, "power-inverted");
			dev_dbg(&pdev->dev,"gpio_power.power_invert=%d\n",host->gpio_power.power_invert);

			/* 
			 * set CARD_POWER_ENABLE/CARD_POWER_DISABLE according to card_status.
			 * if it need to power off tf/sdio circuit before power on to be  decided later
			 */

			if(host->card_status == CARD_UNPLUGED)
			{
				// CARD_POWER_DISABLE
				host->gpio_power.value  =  host->gpio_power.power_invert ^ (CARD_POWER_DISABLE);
				pr_info("[default setting] %s %s using gpio%d with gpio_val %d\n",
				(CARD_POWER_DISABLE? "enable":"disable"),
				(host->mci_mode == MCI_MODE_MMC_SD ? "tf_card circuit": "sdio_card circuit"),
				host->gpio_power.gpio,
				host->gpio_power.value);

				gpio_direction_output(host->gpio_power.gpio, host->gpio_power.value);
				msleep(300);
			}else
			{
				// CARD_POWER_ENABLE
				host->gpio_power.value  =  host->gpio_power.power_invert ^ (CARD_POWER_ENABLE);
				pr_info("[default setting] %s %s using gpio%d with gpio_val %d\n",
				(CARD_POWER_ENABLE? "enable":"disable"),
				(host->mci_mode == MCI_MODE_MMC_SD ? "tf_card circuit": "sdio_card circuit"),
				host->gpio_power.gpio,
				host->gpio_power.value);

				/*1. set mci_pin to idle state: gpio, pupd disable, input disable.*/
				akmci_pinctrl_idle(host);
				/*2. disable card.*/
				gpio_direction_output(host->gpio_power.gpio, !host->gpio_power.value);
				/*3. sleep.*/
				msleep(500);
				/*4. enable card.*/
				gpio_direction_output(host->gpio_power.gpio, host->gpio_power.value);

				/* if power enble sdio, reset wifi */
				if( host->mci_mode == MCI_MODE_SDIO)
				{
					host->gpio_rst =  of_get_named_gpio(np, "rst-pins", 0);
					if(host->gpio_rst >= 0){
						gpio_direction_output(host->gpio_rst, 0);
						msleep(300);
						gpio_direction_output(host->gpio_rst, 1);
					}
				}

			}

			switch (pdev->id)
			{
				case 0:
					host->gpio_power.k_attr.attr.name = __stringify(mmc0_card_pwr_en);
					break;
				case 1:
					host->gpio_power.k_attr.attr.name = __stringify(mmc1_card_pwr_en);
					break;
				case 2:
					host->gpio_power.k_attr.attr.name = __stringify(mmc2_card_pwr_en);
					break;
			}

			host->gpio_power.k_attr.attr.mode = 0666;
			host->gpio_power.k_attr.show = mmc_card_pwr_enable_read;
			host->gpio_power.k_attr.store = mmc_card_pwr_enable_write;
						
			ret = sysfs_create_file(gpio_en_obj, &host->gpio_power.k_attr.attr);
			if (ret) {
				dev_err(&pdev->dev,"Create mmc_gpio sysfs file failed\n"); 
				kobject_put(gpio_en_obj);
			}
	}

	ret = akmci_init_mmc_host(host);
	if(ret) {
		dev_err(&pdev->dev, "failed to init mmc host.\n");
		goto clk_disable;
	}

	/*init mmc/sd host.*/
	akmci_init_host_cfg(host);
	
	platform_set_drvdata(pdev, mmc);
	ret = mmc_add_host(mmc);
	if (ret) {
		goto clk_disable;
	}

	sysfs_create_group(&pdev->dev.kobj, &akmci_attrs);

	dev_info(&pdev->dev, "Mci%d Interface.using %s.%s detect mode:%s.\n",
		 pdev->id,
		 xfer_mode_desc[akmci_xfer_mode(host)],
		 mmc->caps & MMC_CAP_SDIO_IRQ ? "sdio irq" : "",
		 detect_mode_desc[host->detect_mode]);

	// for test
	//DDREGS(host);
	mci_host[slot_index++] = host->mmc;
	return 0;
	
clk_disable:
	clk_disable_unprepare(host->clk);
probe_free_host:
	mmc_free_host(host->mmc);
	return ret;
}

static int akmci_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct akmci_host *host;
	
	mmc = platform_get_drvdata(pdev);
	host = mmc_priv(mmc);

	sysfs_remove_group(&pdev->dev.kobj, &akmci_attrs);
	mmc_remove_host(mmc);
	
	clk_disable_unprepare(host->clk);
	devm_clk_put(&pdev->dev, host->clk);

	iounmap(host->base);

	if(host->detect_mode == AKMCI_DETECT_MODE_CLK)
		cancel_delayed_work_sync(&host->work);

	if(host->detect_mode == AKMCI_DETECT_MODE_GPIO)
		del_timer(&host->detect_timer);

	if (host->gpio_power.gpio >= 0) 
		sysfs_remove_file(gpio_en_obj, &host->gpio_power.k_attr.attr);

	mmc_free_host(host->mmc);
	return 0;
}

#ifdef CONFIG_PM
static int akmci_suspend(struct device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(to_platform_device(dev));
	return mmc_suspend_host(mmc);
}

static int akmci_resume(struct device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(to_platform_device(dev));
	return mmc_resume_host(mmc);
}

static struct dev_pm_ops akmci_pm = {
	.suspend = akmci_suspend,
	.resume = akmci_resume
};

#define akmci_pm_ops  &akmci_pm
#else
#define akmci_pm_ops  NULL
#endif

static struct platform_driver akmci_driver = {
	.probe = akmci_probe,
	.remove = akmci_remove,
	.driver 	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm 	= akmci_pm_ops,
		.of_match_table	= of_match_ptr(akmci_match),
	},
};

/**
 * @brief register mmc/sd module driver  to platform
 *
 * @return register status..
 */
static int __init akmci_init(void)
{
	printk("AK MCI Driver  Module_init\n");

	gpio_en_obj = kobject_create_and_add("mmc_en", NULL);
	if (!gpio_en_obj){
		pr_err("cant not creat mmc_en!\n");
		return -1;
	}
	return platform_driver_register(&akmci_driver);
}

/**
 * @brief release mmc/sd module from platform
 *
 * @return void.
 */
static void __exit akmci_exit(void)
{
	platform_driver_unregister(&akmci_driver);
	return kobject_put(gpio_en_obj);
}
module_init(akmci_init);
module_exit(akmci_exit);

MODULE_DESCRIPTION("Anyka MCI driver");
MODULE_AUTHOR("Anyka Microelectronic Ltd.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.11-hsmax");
