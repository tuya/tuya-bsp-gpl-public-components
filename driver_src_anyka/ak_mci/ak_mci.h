/*
 *  include/plat-anyka/akmci.h - Anyka MMC/SD driver
 *
 *  Copyright (C) 2010 Anyka, Ltd, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __AK_MCI_H__
#define __AK_MCI_H__

#define SDIO_INTRCTR_REG		0x000
#define SDIO_INTR_CHECK_ENABLE	(1 << 8)
/* H3B/H3D/Sundance4 using auto detect sdio irq*/
#define SDIO_INTR_AUTO_DETECT

#define SDIO_INTR_ENABLE				(0x1 << 19)
#define SDIO_INTR_AUTO_DETECT_ENABLE	(0x1 << 18)
#define SDIO_INTR_MANUAL_DETECT_ENABLE	(0x1 << 17)

#define MCI_CLK_REG			0x004
#define MMC_CLK_DIVL(x)		((x) & 0xff)
#define MMC_CLK_DIVH(x)		(((x) & 0xff) << 8)
#define MCI_CLK_ENABLE		(1 << 16)
#define MCI_CLK_PWRSAVE		(1 << 17)
#define MCI_FALL_TRIGGER	(1 << 19)
#define MCI_ENABLE			(1 << 20)

#define MCI_ARGUMENT_REG	0x008

#define MCI_COMMAND_REG		0x00c
#define MCI_CPSM_ENABLE		(1 << 0)
#define MCI_CPSM_CMD(x)		(((x) & 0x3f) << 1)
#define MCI_CPSM_RESPONSE	(1 << 7)
#define MCI_CPSM_LONGRSP	(1 << 8)
#define MCI_CPSM_PENDING	(1 << 9)
#define MCI_CPSM_RSPCRC_NOCHK	(1 << 10)
#define MCI_CPSM_WITHDATA		(1 << 11)
#define MCI_CPSM_RSPWAITTIME	(1 << 12)
#define MCI_CPSM_CMD12_IOABORT	(1 << 13)
#define MCI_RESTORE_SDIOIRQ		(1 << 14)


#define MCI_RESPCMD_REG		0x010
#define MCI_RESPONSE0_REG	0x014
#define MCI_RESPONSE1_REG	0x018
#define MCI_RESPONSE2_REG	0x01c
#define MCI_RESPONSE3_REG	0x020
#define MCI_DATATIMER_REG	0x024
#define MCI_DATALENGTH_REG	0x028
#define MCI_DATACTRL_REG	0x02c
#define MCI_DPSM_ENABLE		(1 << 0)
#define MCI_DPSM_DIRECTION	(1 << 1)
#define MCI_DPSM_STREAM		(1 << 2)
#define MCI_DPSM_BUSMODE(x)				(((x) & 0x3) << 3)
#define MCI_DPSM_BLOCKSIZE(x)			(((x) & 0xfff) << 16)
#define MCI_DPSM_ABORT_EN_MULTIBLOCK	(1 << 6)

#define MCI_DATACNT_REG		0x030
#define MCI_STATUS_REG		0x034
#define MCI_RESPCRCFAIL		(1 << 0)
#define MCI_DATACRCFAIL		(1 << 1)
#define MCI_RESPTIMEOUT		(1 << 2)
#define MCI_DATATIMEOUT		(1 << 3)
#define MCI_RESPEND			(1 << 4)
#define MCI_CMDSENT			(1 << 5)
#define MCI_DATAEND			(1 << 6)
#define MCI_DATABLOCKEND	(1 << 7)
#define MCI_STARTBIT_ERR	(1 << 8)
#define MCI_CMDACTIVE		(1 << 9)
#define MCI_TXACTIVE		(1 << 10)
#define MCI_RXACTIVE		(1 << 11)
#define MCI_FIFOFULL		(1 << 12)
#define MCI_FIFOEMPTY		(1 << 13)
#define MCI_FIFOHALFFULL	(1 << 14)
#define MCI_FIFOHALFEMPTY	(1 << 15)
#define MCI_DATATRANS_FINISH	(1 << 16)
#define MCI_SDIOINT				(1 << 17)

#define MCI_MASK_REG		0x038
#define MCI_RESPCRCFAILMASK	(1 << 0)
#define MCI_DATACRCFAILMASK	(1 << 1)
#define MCI_RESPTIMEOUTMASK	(1 << 2)
#define MCI_DATATIMEOUTMASK	(1 << 3)
#define MCI_RESPENDMASK		(1 << 4)
#define MCI_CMDSENTMASK		(1 << 5)
#define MCI_DATAENDMASK		(1 << 6)
#define MCI_DATABLOCKENDMASK	(1 << 7)
#define MCI_STARTBIT_ERRMASK	(1 << 8)
#define MCI_CMDACTIVEMASK	(1 << 9)
#define MCI_TXACTIVEMASK	(1 << 10)
#define MCI_RXACTIVEMASK	(1 << 11)
#define MCI_FIFOFULLMASK	(1 << 12)
#define MCI_FIFOEMPTYMASK	(1 << 13)
#define MCI_FIFOHALFFULLMASK	(1 << 14)
#define MCI_FIFOHALFEMPTYMASK	(1 << 15)
#define MCI_DATATRANS_FINISHMASK	(1 << 16)
#define MCI_SDIOINTMASK				(1 << 17)

#define MCI_DMACTRL_REG		0x03c
#define MCI_DMA_BUFEN		(1 << 0)
#define MCI_DMA_ADDR(x)		(((x) & 0x7fff) << 1)
#define MCI_DMA_EN			(1 << 16)
#define MCI_DMA_SIZE(x)		(((x) & 0x7fff) << 17)

#define MCI_FIFO_REG		0x040

#if defined(CONFIG_MACH_AK37D) || defined(CONFIG_MACH_AK37E)
#define MCI_PLUGINOFF_REG0		 	(AK_VA_SYSCTRL + 0x80)   //0x0800,0080
#define MCI_PLUGINOFF_REG1		 	(AK_VA_SYSCTRL + 0x84)   //0x0800,0084
#endif

#ifdef CONFIG_MACH_AK39EV330
#define MCI_PLUGINOFF_REG0		 	(AK_VA_SYSCTRL + 0xec)   //0x0800,00EC
#define MCI_PLUGINOFF_REG1		 	(AK_VA_SYSCTRL + 0xf0)   //0x0800,00f0
#endif

#define MCI_PLUGOFF_INT_OFFSET		0
#define MCI_PLUGIN_INT_OFFSET		3
#define MCI_PLUGOFF_STA_OFFSET		8
#define MCI_PLUGIN_STA_OFFSET		11
#define MCI_PLUG_ACTIVE_OFFSET		16
#define MCI_PLUG_STATUS_OFFSET		19

#define AK_FALSE		0
#define AK_TRUE			1

#define MCI_CMDIRQMASKS \
	(MCI_CMDSENTMASK|MCI_RESPENDMASK|		\
	 MCI_RESPCRCFAILMASK|MCI_RESPTIMEOUTMASK)

#define MCI_DATAIRQMASKS \
	(MCI_DATAEND|		\
	 MCI_DATACRCFAILMASK|MCI_DATATIMEOUTMASK)

/*|	\
	 MCI_STARTBIT_ERRMASK)
*/

/*
 * The size of the FIFO in bytes.
 */
#define MCI_FIFOSIZE	4
#define MCI_FIFOHALFSIZE (MCI_FIFOSIZE / 2)

#define NR_SG		16

#define L2BASE		0x2002c000
#define L2FIFO_DMACONF	0x80
#define L2FIFO_CONF1	0x88
#define L2FIFO_ASSIGN1	0x90
#define L2FIFO_INTEN	0x9c

#define L2FIFOBASE	0x48000000
#define L2ADDR(n)	(L2FIFOBASE + 512 * (n))
#define MCI_L2FIFO_NUM	2	/* #6 l2fifo */
#define MCI_L2FIFO_SIZE	512

#define L2DMA_MAX_SIZE	(64*255)

#define L2_DMA_ALIGN	(512)

#define MHz	1000000UL

enum akmci_detect_mode {
	AKMCI_PLUGIN_ALWAY,
	AKMCI_DETECT_MODE_CLK,
	AKMCI_DETECT_MODE_GPIO,
};

enum akmci_xfer_mode {
	AKMCI_XFER_L2DMA,
	//AKMCI_XFER_L2PIO,
};

struct clk;

#define MAX_STATUS_COUNT	(1<<10)
#define MAX_STATUS_MASK 	(MAX_STATUS_COUNT - 1)

#define MCI_SLOT_NUM		2
#define MCI_MODE_MMC_SD		0
#define MCI_MODE_SDIO		1

#define TRANS_DATA_TIMEOUT      0x17D7840

#define MAX_MCI_REQ_SIZE	(65536)

/* l2 fifo limit to 512 bytes */
#define MAX_MCI_BLOCK_SIZE	(512)

struct power_gpio_attribute {
	struct kobj_attribute k_attr;
	int gpio;
	int value;
	bool power_invert;
};

struct akmci_host {
	struct platform_device	*pdev;

	struct mmc_host		*mmc;
	struct mmc_data		*data;
	struct mmc_command	*cmd;
	void __iomem		*base;

	struct clk			*clk;
	unsigned long		 gclk;

	/*
	 *	command output mode
	 *	#define MMC_BUSMODE_OPENDRAIN	1
	 *	#define MMC_BUSMODE_PUSHPULL	2
	 */
	unsigned char		bus_mode;

	/*
	 *  data bus width
	 *	#define MMC_BUS_WIDTH_1		0
	 *	#define MMC_BUS_WIDTH_4		2
	 *	#define MMC_BUS_WIDTH_8		3
	 */
	unsigned char		bus_width;

	unsigned int		bus_clock;

	int			irq_mci;
	int			irq_cd;

	struct power_gpio_attribute gpio_power;
	int			gpio_rst;
	int			gpio_cd;
	int			gpio_wp;

	int 		detect_mode;
	int			xfer_mode;
	int 		data_err_flag;
	unsigned long		clkreg;
	u8			l2buf_id;

	int			card_status;
	unsigned char 		mci_mode;

	unsigned int software_debounce;	/* in msecs, for plugin and plugoff */
	struct delayed_work work;

	struct timer_list		detect_timer;

	unsigned int		data_xfered;
	unsigned int		sg_len;
	struct scatterlist	*sg_ptr;
	unsigned int		sg_off;
	unsigned int		size;

	wait_queue_head_t	intr_data_wait;
	unsigned int 		irq_status;
	unsigned long		pending_events;
	spinlock_t	lock;
};



#endif	/* end __AK_MCI_H__ */

