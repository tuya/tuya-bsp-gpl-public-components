/*
 * camera.c
 *
 * low level camera interface
 *
 * Copyright (C) 2020 anyka
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include <mach/map.h>
#include <linux/printk.h>
#include <asm/io.h>

#include "../include/ak_isp_drv.h"
#include "camera.h"

int camera_isp_get_bits_width(void *isp_struct)
{
	return ak_isp_get_bits_width(isp_struct);
}

AK_ISP_PCLK_POLAR camera_isp_get_pclk_polar(void *isp_struct)
{
	return ak_isp_get_pclk_polar(isp_struct);
}

int camera_isp_vi_apply_mode(void *isp_struct,enum isp_working_mode mode)
{
	return ak_isp_vi_apply_mode(isp_struct, mode);
}

int camera_isp_vo_enable_target_lines_done(void *isp_struct, int lines)
{
	return ak_isp_vo_enable_target_lines_done(isp_struct, lines);
}

int camera_isp_vo_enable_irq_status(void *isp_struct, int bit)
{
	return ak_isp_vo_enable_irq_status(isp_struct, bit);
}

int camera_isp_vi_start_capturing(void *isp_struct, int yuv420_type)
{
	return ak_isp_vi_start_capturing(isp_struct, yuv420_type);
}

int camera_isp_vo_set_main_channel_scale(void *isp_struct, int width, int height)
{
	return ak_isp_vo_set_main_channel_scale(isp_struct, width, height);
}

int camera_isp_vo_set_sub_channel_scale(void *isp_struct, int width, int height)
{
	return ak_isp_vo_set_sub_channel_scale(isp_struct, width, height);
}

int camera_isp_vi_set_crop(void *isp_struct, int sx, int sy, int width, int height)
{
	return ak_isp_vi_set_crop(isp_struct, sx, sy, width, height);
}

int camera_isp_vi_stop_capturing(void *isp_struct)
{
	return ak_isp_vi_stop_capturing(isp_struct);
}

int camera_isp_set_td(void *isp_struct)
{
	return ak_isp_set_td(isp_struct);
}

int camera_isp_reload_td(void *isp_struct)
{
	return ak_isp_reload_td(isp_struct);
}

int camera_isp_vo_set_main_buffer_addr
	(void *isp_struct,
	 enum buffer_id id,
	 unsigned long yaddr_main_chan_addr)
{
	return ak_isp_vo_set_main_buffer_addr(isp_struct, id, yaddr_main_chan_addr);
}

int camera_isp_vo_set_sub_buffer_addr
	(void *isp_struct,
	 enum buffer_id id,
	 unsigned long yaddr_sub_chan_addr)
{
	return ak_isp_vo_set_sub_buffer_addr(isp_struct, id, yaddr_sub_chan_addr);
}

int camera_isp_vo_set_ch3_buffer_addr
	(void *isp_struct,
	 enum buffer_id id,
	 unsigned long yaddr_chan3_addr)
{
	return ak_isp_vo_set_ch3_buffer_addr(isp_struct, id, yaddr_chan3_addr);
}

int camera_isp_enable_buffer_main(void *isp_struct)
{
	return ak_isp_enable_buffer_main(isp_struct);
}

int camera_isp_enable_buffer_sub(void *isp_struct)
{
	return ak_isp_enable_buffer_sub(isp_struct);
}

int camera_isp_enable_buffer_ch3(void *isp_struct)
{
	return ak_isp_enable_buffer_ch3(isp_struct);
}

int camera_isp_vo_enable_buffer_main(void *isp_struct, enum buffer_id id)
{
	return ak_isp_vo_enable_buffer_main(isp_struct, id);
}

int camera_isp_vo_enable_buffer_sub(void *isp_struct, enum buffer_id id)
{
	return ak_isp_vo_enable_buffer_sub(isp_struct, id);
}

int camera_isp_vo_enable_buffer_ch3(void *isp_struct, enum buffer_id id)
{
	return ak_isp_vo_enable_buffer_ch3(isp_struct, id);
}

int camera_isp_is_continuous(void *isp_struct)
{
	return ak_isp_is_continuous(isp_struct);
}

int camera_isp_irq_work(void *isp_struct)
{
	return ak_isp_irq_work(isp_struct);
}

int camera_isp_vo_get_using_frame_main_buf_id(void *isp_struct)
{
	return ak_isp_vo_get_using_frame_main_buf_id(isp_struct);
}

int camera_isp_vo_get_using_frame_sub_buf_id(void *isp_struct)
{
	return ak_isp_vo_get_using_frame_sub_buf_id(isp_struct);
}

int camera_isp_vo_get_using_frame_ch3_buf_id(void *isp_struct)
{
	return ak_isp_vo_get_using_frame_ch3_buf_id(isp_struct);
}

int camera_isp_vo_disable_buffer_main(void *isp_struct, enum buffer_id id)
{
	return ak_isp_vo_disable_buffer_main(isp_struct, id);
}

int camera_isp_vo_disable_buffer_sub(void *isp_struct, enum buffer_id id)
{
	return ak_isp_vo_disable_buffer_sub(isp_struct, id);
}

int camera_isp_vo_disable_buffer_ch3(void *isp_struct, enum buffer_id id)
{
	return ak_isp_vo_disable_buffer_ch3(isp_struct, id);
}

int camera_isp_vi_capturing_one(void *isp_struct)
{
	return ak_isp_vi_capturing_one(isp_struct);
}

int camera_isp_vo_check_update_status(void *isp_struct)
{
	return ak_isp_vo_check_update_status(isp_struct);
}

int camera_isp_vo_check_irq_status (void *isp_struct)
{
	return ak_isp_vo_check_irq_status(isp_struct);
}

int camera_isp_vo_clear_update_status(void *isp_struct, int bit)
{
	return ak_isp_vo_clear_update_status(isp_struct, bit);
}

int camera_isp_vo_clear_irq_status(void *isp_struct, int bit)
{
	return ak_isp_vo_clear_irq_status(isp_struct, bit);
}

int camera_isp_pause_isp_capturing(void *isp_struct)
{
	return ak_isp_set_isp_capturing(isp_struct, 0);
}

int camera_isp_resume_isp_capturing(void *isp_struct)
{
	return ak_isp_set_isp_capturing(isp_struct, 1);
}

int camera_isp_vi_get_input_data_format(void *isp_struct, struct input_data_format *idf)
{
	return ak_isp_vi_get_input_data_format(isp_struct, idf);
}

int camera_isp_vi_get_crop(void *isp_struct, int *sx, int *sy, int *width, int *height)
{
	return ak_isp_vi_get_crop(isp_struct, sx, sy, width, height);
}

int camera_isp_set_misc_attr_ex(void *isp_struct, int oneline, int fsden, int hblank, int fsdnum)
{
	return ak_isp_set_misc_attr_ex(isp_struct, oneline, fsden, hblank, fsdnum);
}

int camera_isp_set_ae_fast_struct_default(void *isp_struct, struct ae_fast_struct *ae_fast)
{
	return ak_isp_set_ae_fast_struct_default(isp_struct, ae_fast);
}

/**************************************
 * mipi ipregisters
**************************************/
void camera_mipi_ip_prepare(enum mipi_mode mode)
{
	unsigned long value;

	pr_debug("%s %d\n", __func__, __LINE__);

	/*release mipi controller pin_byte_clk area resest*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value &= ~(1<<27);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*select  pll to generate internal pclk for mipi*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value |= (1<<24);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*select  mipi  sensor*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value |= (1<<26);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

#ifdef CONFIG_MACH_AK37D
	/*select  single/dual mipi  sensor*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	if (mode == MIPI_MODE_SINGLE)
		value &= ~(1<<29);
	else
		value |= (1<<29);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);
#endif

#ifdef CONFIG_MACH_AK37D
#if 0
	/*select  mipi0  sensor*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value &= ~(1<<31);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);
#endif
#endif

	/*share pin config  mipi*/

	/*open mipi controller clock gate*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x1c);
	value &= ~(1<<16);
	__raw_writel(value, AK_VA_SYSCTRL + 0x1c);

	/*release mipi controller reset*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x20);
	value &= ~(1<<16);
	__raw_writel(value, AK_VA_SYSCTRL + 0x20);
#if 0
	/*mipi dphy data 0~3 and clk lane enable*/
	__raw_writeb(0x7d, AK_VA_MIPI1 + 0x00);

	/*ttl register bank disable*/
	__raw_writeb(0x1f, AK_VA_MIPI1 + 0x20);

	if (lanes == 2) {
		/*two lane mode*/
		__raw_writeb(0xf9, AK_VA_MIPI1 + 0xe0);
	} else {
		/*one lane mode*/
		__raw_writeb(0xf8, AK_VA_MIPI1 + 0xe0);
	}

	/*enable err 错误检测使能*/
	//__raw_writeb(0x01, AK_VA_MIPI1 + 0xe1);
	/*disable err 错误检测使能*/
	__raw_writeb(0xe1, AK_VA_MIPI1 + 0xe1);

	/*THS - SETTLE*/
	set_mipi_thssettle(input_video, lanes);

	/*enable mipi mode*/
	__raw_writeb(0x02, AK_VA_MIPI1 + 0xb3);
#endif
}

/*
 * mipi_mhz_to_thssettle -
 * clock frequecy to ths-settle
 *
 * @mhz:			frequecy in MHZh
 */
static int mipi_mhz_to_thssettle(int mhz)
{
	int thssettle;

	if (mhz <= 110)
		thssettle = 0;
	else if (mhz <= 150)
		thssettle = 1;
	else if (mhz <= 200)
		thssettle = 2;
	else if (mhz <= 250)
		thssettle = 3;
	else if (mhz <= 300)
		thssettle = 4;
	else if (mhz <= 400)
		thssettle = 5;
	else if (mhz <= 500)
		thssettle = 6;
	else if (mhz <= 600)
		thssettle = 7;
	else if (mhz <= 700)
		thssettle = 8;
	else if (mhz <= 800)
		thssettle = 9;
	else if (mhz <= 1000)
		thssettle = 0xa;
	else if (mhz <= 1200)
		thssettle = 0xb;
	else if (mhz <= 1400)
		thssettle = 0xc;
	else
		thssettle = 0xd;

	return thssettle;
}

/*
 * set_mipi_thssettle -
 * set ths-settle
 *
 * @lanes:			lanes of mipi
 */
static int set_mipi_thssettle(enum mipi_port port, int mhz, int lanes)
{
	unsigned char __iomem *mipi_base;
	int thssettle;

	thssettle = mipi_mhz_to_thssettle(mhz);

	if (port == MIPI_PORT_0)
		mipi_base = AK_VA_MIPI1;
#ifdef AK_VA_MIPI2
	else if (port == MIPI_PORT_1)
		mipi_base = AK_VA_MIPI2;
#else
	else {
		pr_err("%s port:%d not support!", __func__, port);
		return -1;
	}
#endif

	/*
	 * NOTE: bit[7] must set 1
	 */
	thssettle = 0x80 | (thssettle & 0x0f);
	__raw_writeb(thssettle, mipi_base + 0x40);
	__raw_writeb(thssettle, mipi_base + 0x60);
	if (lanes == 2)
		__raw_writeb(thssettle, mipi_base + 0x80);

	return 0;
}

void camera_mipi_ip_port_cfg(enum mipi_port port, int mhz, int lanes)
{
	unsigned char __iomem *mipi_base;

	if (port == MIPI_PORT_0)
		mipi_base = AK_VA_MIPI1;
#ifdef AK_VA_MIPI2
	else if (port == MIPI_PORT_1)
		mipi_base = AK_VA_MIPI2;
#endif
	else {
		pr_err("%s no port:%d\n", __func__, port);
		return;
	}

	/*0x0d: move from pinctrl to here*/
	__raw_writeb(0x80, mipi_base + 0x0d);//enable pullup

	/*mipi dphy data 0~3 and clk lane enable*/
	__raw_writeb(0x7d, mipi_base + 0x00);

	/*ttl register bank disable*/
	__raw_writeb(0x1f, mipi_base + 0x20);

	if (lanes == 2) {
		/*two lane mode*/
		__raw_writeb(0xf9, mipi_base + 0xe0);
	} else {
		/*one lane mode*/
		__raw_writeb(0xf8, mipi_base + 0xe0);
	}

	/*enable err 错误检测使能*/
	//__raw_writeb(0x01, AK_VA_MIPI1 + 0xe1);
	/*disable err 错误检测使能*/
	__raw_writeb(0xe1, mipi_base + 0xe1);

	/*THS - SETTLE*/
	set_mipi_thssettle(port, mhz, lanes);

	/*enable mipi mode*/
	__raw_writeb(0x02, mipi_base + 0xb3);
}

/**************************************
 * controller registers
**************************************/
/*
 * mipi ip related
 *
 * @mipi_id:	index of mipi port
 * @io_level:	io level
 */
static int mipi_ip_set_io_level(int mipi_id,
		enum camera_io_level io_level)
{
	unsigned char __iomem *mipi_base;

	if (mipi_id == 0)
		mipi_base = AK_VA_MIPI1;
#ifdef AK_VA_MIPI2
	else if (mipi_id == 1)
		mipi_base = AK_VA_MIPI2;
#endif
	else {
		pr_err("%s mipi_id:%d not support!", __func__, mipi_id);
		return -1;
	}

	switch (io_level) {
		case CAMERA_IO_LEVEL_1V8:
			__raw_writeb(0x10, mipi_base + 0xb8);
			break;

		case CAMERA_IO_LEVEL_2V5:
			__raw_writeb(0x08, mipi_base + 0xb8);
			break;

		case CAMERA_IO_LEVEL_3V3:
			__raw_writeb(0x04, mipi_base + 0xb8);
			break;

		default:
			pr_err("%s mipi_id:%d, io_level:%d not defined\n",
					__func__, mipi_id, io_level);
			return -1;
	}

	return 0;
}

/*
 * camera_ctrl_set_dvp_port -
 * set dvp port mode
 *
 */
int camera_ctrl_set_dvp_port(int dvp_port, enum camera_io_level io_level, int bits_width)
{
	unsigned int value;

	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value &= ~(0x01<<25);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*release mipi controller pin_byte_clk area resest*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value &= ~(0x01<<27);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*don't select  pll to generate internal pclk for dvp*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value &= ~(0x01<<24);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*select dvp sensor*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value &= ~(0x01<<26);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*select single sensor*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value &= ~(0x01<<29);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*sharepin had set mipi ip TTL mode*/

	/*set io level*/
	mipi_ip_set_io_level(0, io_level);
	mipi_ip_set_io_level(1, io_level);

	return 0;
}

/*
 * camera_ctrl_set_mipi_csi_pclk -
 * mipi interface intera pclk init
 *
 */
unsigned long camera_ctrl_set_mipi_csi_pclk(unsigned long internal_pclk)
{
#define PERL_PLL (600000000UL)
	int div;
	unsigned long value;
	unsigned long result;

	/*set inter cis_pclk  =  600/8 = 75MHZ ;600/7=85.7M; pclk<= vclk/3*/
	div = PERL_PLL / internal_pclk;
	result = PERL_PLL / div;

	if (result > internal_pclk) {
		div++;
		result = PERL_PLL / div;
	}

	/*hold isp pclk reset*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value |= (1<<25);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*cis_pclk disable*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x18);
	value &= ~(0x01<<28);
	__raw_writel(value, AK_VA_SYSCTRL + 0x18);

	/*wait the state finish*/
	while((__raw_readl(AK_VA_SYSCTRL + 0x18) & (1<<29)) != 0)
	{;}

	/*cis_pclk div cfg*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x18);
	value &= ~(0x3f<<20);
	value |= (div-1)<<20;
	__raw_writel(value, AK_VA_SYSCTRL + 0x18);

	/*cis_pclk enable div valid*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x18);
	value |= 1<<29;
	__raw_writel(value, AK_VA_SYSCTRL + 0x18);

	/*wait the state finish*/
	while((__raw_readl(AK_VA_SYSCTRL + 0x18) & (1<<29)) != 0)
	{;}

	/*cis_pclk enable*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x18);
	value |= 1<<28;
	__raw_writel(value, AK_VA_SYSCTRL + 0x18);

	/*wait the state finish*/
	while((__raw_readl(AK_VA_SYSCTRL + 0x18) & (1<<29)) != 0)
	{;}

	/*release isp pclk reset*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value &= ~(1<<25);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

#ifdef CONFIG_MACH_AK39EV330
#if 0
	/* pclk -> cis2_sclk */
	/*NOTE: only for debug*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x74);
	value |= (0x1<<30) | (0x1<<4);
	__raw_writel(value, AK_VA_SYSCTRL + 0x74);
#endif
#endif
	return result;
}

/*only for single mode that is singe sensor, no used for dual mode*/
void camera_ctrl_mipi_port_sel(enum mipi_port port)
{
	unsigned long value;

	/*select  mipi0  sensor*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	if (port == MIPI_PORT_0)
		value &= ~(1<<31);
	else
		value |= (1<<31);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);
}

void camera_ctrl_set_pclk_polar(int port, int is_rising)
{
	unsigned int value;
	int bit;

#ifdef CONFIG_MACH_AK39EV330
	bit = (port == 0) ? 30:31;
#else//37D
	bit = 30;
#endif
	value = __raw_readl(AK_VA_SYSCTRL + 0x18);
	if (is_rising)
		value |= (0x1 << bit);
	else
		value &= ~(0x1 << bit);
	__raw_writel(value, AK_VA_SYSCTRL + 0x18);
}

void camera_ctrl_set_sclk1(int output_mhz)
{
#ifdef CONFIG_MACH_AK39EV330
	int div;
	int timeout = 0xffff;
	unsigned long regval;
	unsigned long peri_pll = 600;
	void __iomem *peri_pll_ctrl2 = AK_VA_SYSCTRL + 0x18;

	/*cis2_sclk or cis_pclk dissable*/
	regval = __raw_readl(peri_pll_ctrl2);
	regval &= (~(0x1 << 28));
	__raw_writel(regval, peri_pll_ctrl2);

	/*wait the state finish*/
	do {
		regval = __raw_readl(peri_pll_ctrl2);
	} while(((regval & (1<<29)) != 0) && (timeout-- > 0));

	/*cis_pclk div cfg*/
	div = peri_pll / output_mhz;
	regval = __raw_readl(peri_pll_ctrl2);
	regval &= ~(0x3f << 20);
	regval |= (div - 1) << 20;
	__raw_writel(regval, peri_pll_ctrl2);

	/*cis2_sclk or cis_pclk enable div valid*/
	regval = __raw_readl(peri_pll_ctrl2);
	regval |= (0x1<<29);
	__raw_writel(regval, peri_pll_ctrl2);

	/*wait the state finish*/
	timeout = 0xffff;
	do {
		regval = __raw_readl(peri_pll_ctrl2);
	} while(((regval & (1<<29)) != 0) && (timeout-- > 0));

	/*cis2_sclk or cis_pclk enable*/
	regval = __raw_readl(peri_pll_ctrl2);
	regval |= 0x1 << 28;
	__raw_writel(regval, peri_pll_ctrl2);

	/*wait the state finish*/
	timeout = 0xffff;
	do {
		regval = __raw_readl(peri_pll_ctrl2);
	} while(((regval & (1<<29)) != 0) && (timeout-- > 0));
#else//37D
	int div;
	int timeout = 0xffff;
	unsigned long regval;
	unsigned long peri_pll = 600;
	void __iomem *spi_high_speed_clock_ctrl = AK_VA_SYSCTRL + 0x100;

	/*cis1_sclk dissable*/
	regval = __raw_readl(spi_high_speed_clock_ctrl);
	regval &= (~(0x1 << 18));
	__raw_writel(regval, spi_high_speed_clock_ctrl);

	/*wait the state finish*/
	do {
		regval = __raw_readl(spi_high_speed_clock_ctrl);
	} while(((regval & (1<<19)) != 0) && (timeout-- > 0));

	/*cis_pclk div cfg*/
	div = peri_pll / output_mhz;
	regval = __raw_readl(spi_high_speed_clock_ctrl);
	regval &= ~(0x3f << 10);
	regval |= (div - 1) << 10;
	__raw_writel(regval, spi_high_speed_clock_ctrl);

	/*cis2_sclk or cis_pclk enable div valid*/
	regval = __raw_readl(spi_high_speed_clock_ctrl);
	regval |= (0x1<<19);
	__raw_writel(regval, spi_high_speed_clock_ctrl);

	/*wait the state finish*/
	timeout = 0xffff;
	do {
		regval = __raw_readl(spi_high_speed_clock_ctrl);
	} while(((regval & (1<<19)) != 0) && (timeout-- > 0));

	/*cis2_sclk or cis_pclk enable*/
	regval = __raw_readl(spi_high_speed_clock_ctrl);
	regval |= 0x1 << 18;
	__raw_writel(regval, spi_high_speed_clock_ctrl);

	/*wait the state finish*/
	timeout = 0xffff;
	do {
		regval = __raw_readl(spi_high_speed_clock_ctrl);
	} while(((regval & (1<<29)) != 0) && (timeout-- > 0));
#endif
}

/*
 * camera_ctrl_clks_reset - clocks reset
 * reset the clocks that input video relate to.
 * the clocks may include dvp or mipi field.
 *
 * @RETURN none
 */
void camera_ctrl_clks_reset(void)
{
	/*后面移到专门的文件去操作寄存器。*/


	unsigned int value;

	/*hold isp pclk reset*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value |= (1<<25);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*mipi controller pin_byte_clk area resest*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value |= (1<<27);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*close mipi controller clock gate*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x1c);
	value |= 1<<16;
	__raw_writel(value, AK_VA_SYSCTRL + 0x1c);

	/*mipi controller reset*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x20);
	value |= 1<<16;
	__raw_writel(value, AK_VA_SYSCTRL + 0x20);

	/*vclk reset (isp)*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x20);
	value |= (1<<19);
	__raw_writel(value, AK_VA_SYSCTRL + 0x20);

	/*enable those clock*/

	/*release hold isp pclk reset*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value &= ~(1<<25);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*release mipi controller pin_byte_clk area resest*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x14);
	value &= ~(1<<27);
	__raw_writel(value, AK_VA_SYSCTRL + 0x14);

	/*open mipi controller clock gate*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x1c);
	value &= ~(1<<16);
	__raw_writel(value, AK_VA_SYSCTRL + 0x1c);

	/*release mipi controller reset*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x20);
	value &= ~(1<<16);
	__raw_writel(value, AK_VA_SYSCTRL + 0x20);

	/*release vclk reset (isp)*/
	value = __raw_readl(AK_VA_SYSCTRL + 0x20);
	value &= ~(1<<19);
	__raw_writel(value, AK_VA_SYSCTRL + 0x20);
}

