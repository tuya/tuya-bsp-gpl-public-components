/*
 * (C) Copyright 2008
 * Texas Instruments, <www.ti.com>
 * Sukumar Ghorai <s-ghorai@ti.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation's version 2 of
 * the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <config.h>
#include <common.h>
#include <mmc.h>
#include <part.h>
#include <i2c.h>
#include <twl4030.h>
#include <twl6030.h>
#include <palmas.h>
#include <asm/io.h>
#ifdef CONFIG_H3_B_CODE
#include <asm/arch-ak39ev33x/ak_mmc_host_def.h>
#include <asm/arch-ak39ev33x/ak_cpu.h>
#include <asm/arch-ak39ev33x/ak_types.h>
#include <asm/arch-ak39ev33x/ak_l2buf.h>
#else
#ifdef CONFIG_37_E_CODE
#include <asm/arch-ak37e/ak_mmc_host_def.h>
#include <asm/arch-ak37e/ak_cpu.h>
#include <asm/arch-ak37e/ak_types.h>
#include <asm/arch-ak37e/ak_l2buf.h>
#else
#include <asm/arch-ak37d/ak_mmc_host_def.h>
#include <asm/arch-ak37d/ak_cpu.h>
#include <asm/arch-ak37d/ak_types.h>
#include <asm/arch-ak37d/ak_l2buf.h>
#endif
#endif

#include <asm/errno.h>
#include <asm/ak_sdhsmmc.h>
#include <fdtdec.h>
#include <libfdt.h>


DECLARE_GLOBAL_DATA_PTR;

/* If we fail after 1 second wait, something is really bad */
#define MAX_RETRY_MS	1000


struct ak_sdhsmmc_data {
	struct akhsmmc *base_addr;
	int cd_gpio;
	int wp_gpio;
	int dev_index; // cdh:add
};

int g_power_on_pin = 0;

enum ak_mmc_cd_mode g_mmc_plug_mod;
unsigned int g_mmc_plug_gpio_index = 0;

struct ak_sdhsmmc_data *g_pCurSdDevice = AK_NULL;

static bool send_cmd(struct mmc *mmc, unsigned char cmd_index, unsigned char resp ,unsigned int arg);
static int ak_mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data);

static struct mmc sdhsmmc_dev[3];
static struct ak_sdhsmmc_data sdhsmmc_dev_data[3];

static T_U32 s_SdRegClkCtrl    = SD_CLK_CTRL_REG;          // (clock) control reg
static T_U32 s_SdRegArgument   = SD_ARGUMENT_REG;          // command arg reg
static T_U32 s_SdRegCmd        = SD_CMD_REG;               // command control reg
//static T_U32 s_SdRegRespCmd    = SD_RESP_CMD_REG;          // command response reg
static T_U32 s_SdRegResp0      = SD_RESP_REG0;             // response reg 0
static T_U32 s_SdRegResp1      = SD_RESP_REG1;             // response reg 1
static T_U32 s_SdRegResp2      = SD_RESP_REG2;             // response reg 2
static T_U32 s_SdRegResp3      = SD_RESP_REG3;             // response reg 3
static T_U32 s_SdRegDataTim    = SD_DATA_TIM_REG;          // data timer reg
static T_U32 s_SdRegDataLen    = SD_DATA_LEN_REG;          // data length reg
static T_U32 s_SdRegDataCtrl   = SD_DATA_CTRL_REG;         // data control reg
//static T_U32 s_SdRegDataCount  = SD_DATA_COUT_REG;         // data remain counter reg
static T_U32 s_SdRegStatus     = SD_INT_STAT_REG;          // command/data status reg
//static T_U32 s_SdRegIntEnable  = SD_INT_ENABLE;            // interrupt enable reg
//static T_U32 s_SdRegDmaMode    = SD_DMA_MODE_REG;          // data buffer configure reg(0x2002x03C), CPU/DMA mode select
static T_U32 s_SdRegCpuMode    = SD_CPU_MODE_REG;          // data reg(0x2002x040)



extern int gpio_set_pin_as_gpio (unsigned int pin);
extern void gpio_set_pin_dir( unsigned long pin, unsigned char dir);
extern int gpio_direction_output(int gpio, int value);
extern void gpio_set_pin_level(int pin, unsigned char level);


/**
 * @brief check and set select wp and cd detect gpio.
 *
 * check select wp and cd detect gpio,if exist this gpio, return this gpio index
 * @author CaoDonghua
 * @date 2016-09-30
 * @param[in] mmcdev_index sd device index.
 * @return int selected detect gpio failed or successfully 
 * @retval  returns gpio index on success
 * @retval  return negative one if failed
 */
static int ak_mmc_setup_gpio_in(int gpio, const char *label)
{
	//unsigned int value = 0;
	int gpio_tmp = -1;
	
	/* check and set mmc_wp pin, but no wp pin */
	if (strcmp(label, "mmc_wp") == 0) {
		return -1;
	}

	/*as for H3D AK376XD dsi platform, sd_detect pin use AK_GPIO_77*/
	if (strcmp(label, "mmc_cd") == 0) {
#ifdef TF_DET_USE_GPIO
		gpio_set_pin_as_gpio (gpio);
		gpio_set_pin_dir(gpio, 0);
#endif
		gpio_tmp = gpio;
		return gpio_tmp;
	}

	return 0;
}

/**
 * @brief get anyka gpio pin input level.
 *
 * get anyka gpio pin input level for sd card wp or cd detect
 * @author CaoDonghua
 * @date 2016-09-30
 * @param[in] gpio gpio pin index.
 * @return int selected gpio input level high 1 or low 0
 * @retval  return high level 1
 * @retval  return low level 0
 */
static int ak_gpio_get_value(unsigned gpio)
{
	debug("cdh:%s, reg:0x%x, offset:%d\n", __func__, (GPIO_IN_REG1 + (gpio/32)*0x4), gpio%32);
	return (readl(GPIO_IN_REG1 + (gpio/32)*0x4) >> gpio%32) & 0x1;
}

#ifdef TF_DET_USE_MCK
static int ak_mck_get_tfplug_status(void)
{
	int status[3];
	int i, index;
	int value;

	/* when bit[17]=0 not active, bit[20]=0, mci2 plug in */
	for(i=0, index=0; ; i++){
		value = readl(SD_PLUG_IN_OFF_REG);
		if(value&(0x1<<17)){
			mdelay(10);
			continue;
		}
		status[index] = (value>>20)&0x1;
		index++;
		if (index == 0x3){
			break;
		}
	}

	if((status[0] == 0x0) && (status[0] == status[1]) && (status[1] == status[2])){
		debug("cdh:%s, line:%d mck det TF card ok!\n", __func__, __LINE__);
		return 0;
	}

	return -1;
}
#endif

/**
 * @brief get sd card cd detect gpio pin level.
 *
 * get sd card cd detect gpio pin level for card plug or unplug
 * @author CaoDonghua
 * @date 2016-09-30
 * @param[in] mmcdev_index sd device index.
 * @return int sd gpio input level high 1 or low 0
 * @retval  return high level 1
 * @retval  return low level 0
 */
static int ak_mmc_getcd(struct mmc *mmc)
{
	
#ifdef TF_DET_USE_GPIO
		int cd_gpio = ((struct ak_sdhsmmc_data *)mmc->priv)->cd_gpio;
		return ak_gpio_get_value(cd_gpio);
#endif
	
#ifdef TF_DET_USE_MCK
		return ak_mck_get_tfplug_status();
#endif
	return 0; 
}

/**
 * @brief get sd wp detect pin gpio level.
 *
 * get sd wp from gpio pin input direction level
 * @author CaoDonghua
 * @date 2016-09-30
 * @param[in] mmcdev_index sd device index.
 * @return int wp gpio input level high 1 or low 0
 * @retval  return high level 1
 * @retval  return low level 0
 */
static int ak_mmc_getwp(struct mmc *mmc)
{
	int wp_gpio = ((struct ak_sdhsmmc_data *)mmc->priv)->wp_gpio;
	return ak_gpio_get_value(wp_gpio);
}


/**
 * @brief config selected sd module interface share pin.
 *
 * config selected sd module interface share pin..
 * @author CaoDonghua
 * @date 2016-09-30
 * @param[in] mmcdev_index sd device index.
 * @return int config selected sd module interface share pin failed or successfully 
 * @retval  return zero on success
 * @retval  return a non-zero if failed
 */

#ifdef CONFIG_H3_B_CODE
static int ak_mmc_sd_sharepin_cfg(int mmcdev_index)
{
	unsigned int value = 0;
	unsigned int mci_sel = 0;
	
	if (mmcdev_index == 0) { // cdh:mmc/sd
	    value = REG32(SHARE_PIN_CFG3_REG);
		printf("mmc/sd share pin!\n");
        /*as for H3 AK3918E platform,only use MCI0_D[0],that's say,we only have 1bit bus_width*/
		value &= ~((1<<6)|(1<<7)|(1<<8)); 
		value |= ((1<<6)|(1<<7)|(1<<8));
        REG32(SHARE_PIN_CFG3_REG) = value;
	}else if (mmcdev_index == 1) { // cdh:sdio
        value = REG32(SHARE_PIN_CFG0_REG);
		value |= (1<<28);
        REG32(SHARE_PIN_CFG0_REG) = value;
        
		printf("sdio share pin!\n");
		value = REG32(SHARE_PIN_CFG2_REG);
		value &= ~((3<<16)|(3<<18)|(3<<22)|(3<<24)); // cdh:enable mci1 module clock
		value |= ((2<<16)|(2<<18)|(2<<22)|(2<<24));
        REG32(SHARE_PIN_CFG2_REG) = value;

        value = REG32(SHARE_PIN_CFG3_REG);
        value &= ~((1<<20)|(1<<21)); // cdh:enable mci1 module clock
        value |= ((1<<20)|(1<<21));
        REG32(SHARE_PIN_CFG3_REG) = value;
	}else {
		printf("%s: Err no such dev index:%d!\n", __func__, mmcdev_index);
		return -1;
	}
	

//	gpio_set_pin_as_gpio(g_power_on_pin);
//	gpio_direction_output(g_power_on_pin, 1);
//	mdelay(100);
	//gpio_direction_output(g_power_on_pin, 0);

	return 0;
}

#else

#ifdef CONFIG_37_E_CODE
static int ak_mmc_sd_sharepin_cfg(int mmcdev_index)
{
	unsigned int value = 0;
	unsigned int mci_sel = 0;
	
	if (mmcdev_index == 0) { // cdh:mmc/sd
		printf("sd1 share pin!\n");
        // todo
        /*as for H3 AK3918E platform,only use MCI0_D[0],that's say,we only have 1bit bus_width*/
		value = REG32(SHARE_PIN_CFG6_REG);
		value &= ~((0x7<<24)|(0x7<<27)); 
		value |= ((0x1<<24)|(0x1<<27));
		REG32(SHARE_PIN_CFG6_REG) = value;

		value = REG32(SHARE_PIN_CFG7_REG);
		value &= ~((0x7<<0)|(0x7<<3)|(0x7<<6)|(0x7<<9)|(0x7<<12)|(0x7<<15)|(0x7<<18)|(0x7<<21)); 
		value |= ((0x1<<0)|(0x1<<3)|(0x1<<6)|(0x1<<9)|(0x1<<12)|(0x1<<15)|(0x1<<18)|(0x1<<21));
		REG32(SHARE_PIN_CFG7_REG) = value;

	}else if (mmcdev_index == 1) { // cdh:sdio
		printf("sd2 share pin!\n");
#if 0
		value = REG32(SHARE_PIN_CFG6_REG);
		value &= ~(0x3F80);  
		REG32(SHARE_PIN_CFG6_REG) = value;
#endif		
		/*
		* sd2 sharepin set as sd2, four buse width
		* share pin module include attribute pu or pd
		*/
#if 1
		mci_sel = REG32(SHARE_PIN_CFG1_REG);
		mci_sel &= ~((0x7<<18) | (0x7<<21) | (0x7<<24) | (0x7<<27));
		mci_sel |= ((0x2<<18) | (0x2<<21) | (0x2<<24) | (0x2<<27));
		REG32(SHARE_PIN_CFG1_REG) = mci_sel;



		mci_sel = REG32(SHARE_PIN_CFG2_REG);
		mci_sel &= ~((0x7<<0) | (0x7<<3));
		mci_sel |= ((0x2<<0) | (0x2<<3));
		REG32(SHARE_PIN_CFG2_REG) = mci_sel;

		/*
		* sd2 enable all pin pull-up
		* sd2 set pin drv 00
		* sd2 ie enable
		*/
		//debug("sd2 share pin#, default en_pu reg:0x%x, en_pu_sel:0x%x!\n", REG32(PU_PD_ENABLE_CFG1_REG), REG32(PU_PD_SEL_CFG1_REG));
#if 1	
		//pupd enable
        value = REG32(PU_PD_ENABLE_CFG0_REG);
        value |= (0x3f<<16);
        REG32(PU_PD_ENABLE_CFG0_REG) = value;

        // 0:pull-up    1:pull-down 
        value = REG32(PU_PD_SEL_CFG0_REG);
        value &= ~(0x3f<<16);
        REG32(PU_PD_SEL_CFG0_REG) = value;
		
		//printf("sd2 share pin#, config. en_pu reg:0x%x, en_pu_sel:0x%x!\n", REG32(PU_PD_ENABLE_CFG1_REG), REG32(PU_PD_SEL_CFG1_REG));
		//debug("sd2 share pin#, config. ie_en reg:0x%x!\n", REG32(PAD_IE_CFG1_REG));
#endif
		/*
		* sd2 use GPIO77 as sd_det, use GPIO81 as gpio control sd2 power
		*/
#if 0		
		value = REG32(SHARE_PIN_CFG4_REG);
		value &= ~((0x3<<18)|(0x3<<26)); 
		REG32(SHARE_PIN_CFG4_REG) = value;


		/*
		* sd2 power on, GPIO81 output low
		*/
		value = REG32(GPIO_DIR_REG3);
		value |= (0x1<<17); 
		REG32(GPIO_DIR_REG3) = value;

		value = REG32(GPIO_OUT_REG3);
		value &= ~(0x1<<17); 
		REG32(GPIO_OUT_REG3) = value;
#endif
		
#else
		/* set sd2 sharepin and pupd */
		ak_group_config(ePIN_AS_SDIO);

		/* set gpio77 sharepin as gpio and as sd power ctrl, output low */
		gpio_set_pin_as_gpio(AK_GPIO_77);
		gpio_direction_input(AK_GPIO_77);

		/* set gpio81 sharepin as gpio and as sd_det */
		gpio_set_pin_as_gpio(AK_GPIO_81);
		gpio_direction_output(AK_GPIO_81, AK_GPIO_OUT_LOW);
#endif
		

		
		debug("sd2 gpio81 pin BIT17, low power on!\n");

		
	
		//debug("sd2 share pin#, share1 reg:0x%x, share2 reg:0x%x!\n", REG32(SHARE_PIN_CFG2_REG), REG32(SHARE_PIN_CFG4_REG));
		//debug("sd2 share pin#, en_pu reg:0x%x, en_pu_sel:0x%x!\n", REG32(PU_PD_ENABLE_CFG1_REG), REG32(PU_PD_SEL_CFG1_REG));
		//debug("sd2 share pin#, drv1 reg:0x%x, drv2:0x%x!\n", REG32(PAD_DRV_CFG2_REG), REG32(PAD_DRV_CFG3_REG));
		//debug("sd2 share pin#, ie1 reg:0x%x, ie2:0x%x!\n", REG32(PAD_IE_CFG1_REG), REG32(PAD_IE_CFG2_REG));
	}
	else if (mmcdev_index == 2) { // cdh:sd3
		printf("sd3 share pin!\n");
		// todo
#if 1
		value = REG32(SHARE_PIN_CFG0_REG);
		value &= ~((0x7<<0) | (0x7<<3) | (0x7<<6) | (0x7<<9) | (0x7<<12) | (0x7<<15));
		value |= ((0x2<<0) | (0x2<<3) | (0x2<<6) | (0x2<<9) | (0x2<<12) | (0x2<<15));
		REG32(SHARE_PIN_CFG0_REG) = value;
#else
		value = REG32(SHARE_PIN_CFG5_REG);
		value &= ~((0x7<<24) | (0x7<<27));
		value |= ((0x2<<24) | (0x2<<27));
		REG32(SHARE_PIN_CFG5_REG) = value;


		value = REG32(SHARE_PIN_CFG6_REG);
		value &= ~((0x7<<0) | (0x7<<3) | (0x7<<6) | (0x7<<9));
		value |= ((0x2<<0) | (0x2<<3) | (0x2<<6) | (0x2<<9));
		REG32(SHARE_PIN_CFG6_REG) = value;

#endif

		/* enable mck pd */
		value = REG32(PU_PD_ENABLE_CFG2_REG);
		value &= ~((1<<23)|(1<<24)|(1<<25)|(1<<27)|(1<<28));
		REG32(PU_PD_ENABLE_CFG2_REG) = value;

		value = REG32(PU_PD_ENABLE_CFG2_REG);
		value |= (0x1<<26); 
		REG32(PU_PD_ENABLE_CFG2_REG) = value;

	}else {
		printf("%s: Err no such dev index:%d!\n", __func__, mmcdev_index);
		return -1;
	}

//	gpio_set_pin_as_gpio(g_power_on_pin);
//	gpio_direction_output(g_power_on_pin, 1);
//	gpio_set_pin_level(g_power_on_pin, 1);
	
	return 0;
}


#else

static int ak_mmc_sd_sharepin_cfg(int mmcdev_index)
{
	unsigned int value = 0;
	unsigned int mci_sel = 0;

	if (mmcdev_index == 2)
	{
		//<XGPIO_091 XGPIO_092 XGPIO_093 XGPIO_094 XGPIO_095 XGPIO_096> 无法配置上下拉，除了 XGPIO_094 为下拉之外， 其他为上拉

		//pupd disable
		value = REG32(PU_PD_ENABLE_CFG2_REG);
		value |= ((1<<23)|(1<<24)|(1<<25)|(1<<27)|(1<<28));
		REG32(PU_PD_ENABLE_CFG2_REG) = value;

		value = REG32(PU_PD_ENABLE_CFG2_REG);
		value &= ~(0x1<<26); 
		REG32(PU_PD_ENABLE_CFG2_REG) = value;

		//direct input 输入方向
		//<XGPIO_091 XGPIO_092 XGPIO_093 XGPIO_094 XGPIO_095 XGPIO_096>;
		gpio_set_pin_as_gpio(91);
		gpio_set_pin_as_gpio(92);
		gpio_set_pin_as_gpio(93);
		gpio_set_pin_as_gpio(94);
		gpio_set_pin_as_gpio(95);
		gpio_set_pin_as_gpio(96);

		gpio_direction_input(91);
		gpio_direction_input(92);
		gpio_direction_input(93);
		gpio_direction_input(94);
		gpio_direction_input(95);
		gpio_direction_input(96);
	}

//	gpio_set_pin_as_gpio(g_power_on_pin);
//	gpio_direction_output(g_power_on_pin, 1);
//	mdelay(100);
	//gpio_direction_output(g_power_on_pin, 0);

	if (mmcdev_index == 0) { // cdh:mmc/sd
		printf("sd1 share pin!\n");
        // todo
        /*as for H3 AK3918E platform,only use MCI0_D[0],that's say,we only have 1bit bus_width*/
		value = REG32(SHARE_PIN_CFG1_REG);
		value &= ~((0x3<<18)|(0x3<<20)|(0x3<<22)); 
		value |= ((0x1<<18)|(0x1<<20)|(0x1<<22));
		REG32(SHARE_PIN_CFG1_REG) = value;

	}else if (mmcdev_index == 1) { // cdh:sdio
		printf("sd2 share pin!\n");
#if 0
		value = REG32(SHARE_PIN_CFG6_REG);
		value &= ~(0x3F80);  
		REG32(SHARE_PIN_CFG6_REG) = value;
#endif
		/*
		* sd2 sharepin set as sd2, four buse width
		* share pin module include attribute pu or pd
		*/
#if 1
		mci_sel = REG32(SHARE_PIN_CFG2_REG);
		mci_sel &= ~((0x3<<6) | (0x3<<8) | (0x3<<10) | (0x3<<12) | (0x3<<14) | (0x3<<16));
		mci_sel |= ((0x1<<6) | (0x1<<8) | (0x1<<10) | (0x1<<12) | (0x1<<14) | (0x1<<16));
		REG32(SHARE_PIN_CFG2_REG) = mci_sel;

		/*
		* sd2 enable all pin pull-up
		* sd2 set pin drv 00
		* sd2 ie enable
		*/
		//debug("sd2 share pin#, default en_pu reg:0x%x, en_pu_sel:0x%x!\n", REG32(PU_PD_ENABLE_CFG1_REG), REG32(PU_PD_SEL_CFG1_REG));
#if 1
		/* enable mck pd */
		value = REG32(PU_PD_ENABLE_CFG1_REG);
		value &= ~((1<<8)|(1<<7)|(1<<6)|(1<<5)|(1<<3));
		REG32(PU_PD_ENABLE_CFG1_REG) = value;

		value = REG32(PU_PD_ENABLE_CFG1_REG);
		value |= (0x1<<4); 
		REG32(PU_PD_ENABLE_CFG1_REG) = value;
		
		//debug("sd2 share pin#, config. en_pu reg:0x%x, en_pu_sel:0x%x!\n", REG32(PU_PD_ENABLE_CFG1_REG), REG32(PU_PD_SEL_CFG1_REG));
		//debug("sd2 share pin#, config. ie_en reg:0x%x!\n", REG32(PAD_IE_CFG1_REG));
#endif
		/*
		* sd2 use GPIO77 as sd_det, use GPIO81 as gpio control sd2 power
		*/
//		value = REG32(SHARE_PIN_CFG4_REG);
//		value &= ~((0x3<<18)|(0x3<<26)); 
//		REG32(SHARE_PIN_CFG4_REG) = value;

		/*
		* sd2 power on, GPIO81 output low
		*/
//		value = REG32(GPIO_DIR_REG3);
//		value |= (0x1<<17); 
//		REG32(GPIO_DIR_REG3) = value;

//		value = REG32(GPIO_OUT_REG3);
//		value &= ~(0x1<<17); 
//		REG32(GPIO_OUT_REG3) = value;

#else
		/* set sd2 sharepin and pupd */
		ak_group_config(ePIN_AS_SDIO);

		/* set gpio77 sharepin as gpio and as sd power ctrl, output low */
		gpio_set_pin_as_gpio(AK_GPIO_77);
		gpio_direction_input(AK_GPIO_77);

		/* set gpio81 sharepin as gpio and as sd_det */
		gpio_set_pin_as_gpio(AK_GPIO_81);
		gpio_direction_output(AK_GPIO_81, AK_GPIO_OUT_LOW);
#endif
		debug("sd2 gpio81 pin BIT17, low power on!\n");

		//debug("sd2 share pin#, share1 reg:0x%x, share2 reg:0x%x!\n", REG32(SHARE_PIN_CFG2_REG), REG32(SHARE_PIN_CFG4_REG));
		//debug("sd2 share pin#, en_pu reg:0x%x, en_pu_sel:0x%x!\n", REG32(PU_PD_ENABLE_CFG1_REG), REG32(PU_PD_SEL_CFG1_REG));
		//debug("sd2 share pin#, drv1 reg:0x%x, drv2:0x%x!\n", REG32(PAD_DRV_CFG2_REG), REG32(PAD_DRV_CFG3_REG));
		//debug("sd2 share pin#, ie1 reg:0x%x, ie2:0x%x!\n", REG32(PAD_IE_CFG1_REG), REG32(PAD_IE_CFG2_REG));
	}
	else if (mmcdev_index == 2) { // cdh:sd3
		printf("sd3 share pin!\n");
		// todo
		value = REG32(SHARE_PIN_CFG5_REG);
		value &= ~((0x3<<14)|(0x3<<16)|(0x3<<18)|(0x3<<20)|(0x3<<22)|(0x3<<24)); 
		value |= ((0x2<<14)|(0x2<<16)|(0x3<<18)|(0x3<<20)|(0x3<<22)|(0x3<<24));
		REG32(SHARE_PIN_CFG5_REG) = value;
		
		value = REG32(SHARE_PIN_CFG6_REG);
		value &= ~((0x3<<0)|(0x3<<2)|(0x3<<4)|(0x3<<6)); 
		value |= ((0x2<<0)|(0x3<<2)|(0x3<<4)|(0x1<<6));
		REG32(SHARE_PIN_CFG6_REG) = value;

		/* enable mck pd */
		value = REG32(PU_PD_ENABLE_CFG2_REG);
		value &= ~((1<<23)|(1<<24)|(1<<25)|(1<<27)|(1<<28));
		REG32(PU_PD_ENABLE_CFG2_REG) = value;

		value = REG32(PU_PD_ENABLE_CFG2_REG);
		value |= (0x1<<26); 
		REG32(PU_PD_ENABLE_CFG2_REG) = value;

	}else {
		printf("%s: Err no such dev index:%d!\n", __func__, mmcdev_index);
		return -1;
	}
	
	return 0;
}
#endif
#endif


/**
 * @brief open selected sd module system clock.
 *
 * open selected sd module system clock, first do it.
 * @author CaoDonghua
 * @date 2016-09-30
 * @param[in] mmcdev_index sd device index.
 * @return int open selected sd module system clock failed or successfully 
 * @retval  return zero on success
 * @retval  return a non-zero if failed
 */
static int ak_mmc_sd_system_clk(int mmcdev_index)
{
	unsigned int value = 0;

	if (mmcdev_index == 0) {
		value = readl(CLOCK_CTRL_REG);
		value &= ~(0x1<<1);	// cdh:enable mci0 module clock
		writel(value, CLOCK_CTRL_REG);
	}else if (mmcdev_index == 1) {
		printf("set sd2 open clk!\n");
		value = readl(CLOCK_CTRL_REG);
		value &= ~(0x1<<2); // cdh:enable mci1 module clock
		writel(value, CLOCK_CTRL_REG);
	}else if (mmcdev_index == 2) {
		value = readl(CLOCK_CTRL2_REG);
		value &= ~(0x1<<3); // cdh:enable mci2 module clock
		writel(value, CLOCK_CTRL2_REG);
	}else {
		printf("%s: Err no such dev index:%d!\n", __func__, mmcdev_index);
		return -1;
	}

	debug("cdh:%s, open mci2 clk:0x%x\n", __func__, readl(CLOCK_CTRL_REG));
	
	return 0;
}

/**
 * @brief init sd card board.
 *
 * init sd card board include open sd module system clock , soft reset sd controller and set interface share pin.
 * @author CaoDonghua
 * @date 2016-09-30
 * @param[in] clk The main clock of sd card.
 * @param[in] asic current asic freq
 * @param[in] pwr_save Set this parameter true to enable power save
 * @return int return ainit sd card board success or failed
 * @retval return zero on success
 * @retval return a non-zero if failed
 */
static int ak_mmc_board_init(struct mmc *mmc)
{
	int devindex;
	unsigned int value = 0;
	
	devindex = ((struct ak_sdhsmmc_data *)mmc->priv)->dev_index;

#if 0
	/* config selected mmc/sd bus interface share pin */ 
	if (ak_mmc_sd_sharepin_cfg(devindex) == -1) {
		printf("%s:Err sharepin cfg!\n", __func__);
		return -1;
	}
#endif	

	/* config selected mmc/sd module system clock */ 
    if (ak_mmc_sd_system_clk(devindex) == -1) {
		printf("%s:Err system clk cfg!\n", __func__);
		return -1;
    }

	/* reset mmc_sd /sdio module controller */ 
	if (devindex == 0) {
		value = REG32(RESET_CTRL_REG);
		value |= MMC_SOFTRESET;
		REG32(RESET_CTRL_REG) = value;
		value &= ~MMC_SOFTRESET;
		REG32(RESET_CTRL_REG) = value;
	}else if (devindex == 1) {
		printf("set sd2 reset!\n");
		value = REG32(RESET_CTRL_REG);
		value |= SDIO_SOFTRESET;
		REG32(RESET_CTRL_REG) = value;
		value &= ~SDIO_SOFTRESET;
		REG32(RESET_CTRL_REG) = value;
	}else if (devindex == 2){
		value = REG32(CLOCK_CTRL2_REG);
		value |= SD3_SOFTRESET;
		REG32(CLOCK_CTRL2_REG) = value;
		value &= ~SD3_SOFTRESET;
		REG32(CLOCK_CTRL2_REG) = value;
	}

	debug("cdh:%s, open mci2 reset:0x%x\n", __func__, readl(RESET_CTRL_REG));

#if 1
	/* config selected mmc/sd bus interface share pin */ 
	if (ak_mmc_sd_sharepin_cfg(devindex) == -1) {
		printf("%s:Err sharepin cfg!\n", __func__);
		return -1;
	}
#endif
	
	return 0;
}


/**
 * @brief Set sd card controller output mclk clock.
 *
 * The clock must be less than 400khz when the sd controller in identification mode.
 * @author CaoDonghua
 * @date 2016-09-30
 * @param[in] clk The main clock of sd card.
 * @param[in] asic current asic freq
 * @param[in] pwr_save Set this parameter true to enable power save
 * @return void
 */
static void ak_set_clock(struct mmc *mmc, unsigned int clk, unsigned int asic, bool pwr_save)
{
    unsigned char clk_div_l,clk_div_h;
    unsigned int  asic_freq,reg_value,tmp;
	struct ak_sdhsmmc_data *priv_data = (struct ak_sdhsmmc_data *)mmc->priv;

	debug("want set asic clk = %d, wanted sdmmc clk = %d\n", asic, clk);
	 
    if (0 == clk){
        return;
    }
    
    asic_freq = asic;
    if (asic_freq < clk*2) {
        clk_div_l = clk_div_h = 0;
    }else {
        /* clk = asic / ((clk_div_h+1) + (clk_div_l+1))
        	*   note:clk_div_h and clk_div_l present high and low level cycle time
		*/
        tmp = asic_freq / clk;
        if (asic_freq % clk){
            tmp += 1;
        }
        
        tmp -= 2;
        clk_div_h = tmp/2;
        clk_div_l = tmp - clk_div_h;
    }
    
    reg_value = (clk_div_l<<CLK_DIV_L_OFFSET)|(clk_div_h<<CLK_DIV_H_OFFSET) | SD_CLK_ENABLE | FALLING_TRIGGER | SD_INTERFACE_ENABLE;
    if (pwr_save) {
        reg_value |= PWR_SAVE_ENABLE;
	}
	REG32((T_U32)priv_data->base_addr + s_SdRegClkCtrl) = reg_value;
	
    /* calculate real clock */
    tmp = asic_freq/(clk_div_h+clk_div_l+2);
    //debug("asic clk = %d, real sdmmc clk = %d, reg_clk:0x%x\n",asic_freq, tmp, REG32(s_SdRegClkCtrl));
}


/**
 * @brief send sd command.
 *
 * The clock must be less than 400khz when the sd controller in identification mode.
 * @author CaoDonghua
 * @date 2016-09-30
 * @param[in] cmd_index The command index.
 * @param[in] rsp The command response:no response ,short reponse or long response
 * @param[in] arg The cmd argument.
 * @return bool cmd send failed or successfully 
 * @retval  AK_TRUE: CMD sent successfully
 * @retval  AK_FALSE: CMD sent failed
 */
static bool send_cmd(struct mmc *mmc, unsigned char cmd_index, unsigned char resp, unsigned int arg)
{
    unsigned int cmd_value = 0;
    unsigned int status;
	struct ak_sdhsmmc_data *priv_data = (struct ak_sdhsmmc_data *)mmc->priv;

    if (cmd_index == SD_CMD(1) || cmd_index == SD_CMD(41) || cmd_index == SD_CMD(5)) {
        cmd_value = CPSM_ENABLE | ( resp << WAIT_REP_OFFSET) | ( cmd_index << CMD_INDEX_OFFSET) | RSP_CRC_NO_CHK;
    }
    else {
        cmd_value = CPSM_ENABLE | ( resp << WAIT_REP_OFFSET ) | ( cmd_index << CMD_INDEX_OFFSET) ;
    }
    
    REG32((T_U32)priv_data->base_addr + s_SdRegArgument) = arg;
    REG32((T_U32)priv_data->base_addr + s_SdRegCmd) = cmd_value;

    if (SD_NO_RESPONSE == resp) {
        while(1) {
            status = REG32((T_U32)priv_data->base_addr + s_SdRegStatus); 
            if (status & CMD_SENT){
                return AK_TRUE;
            }
        }
    }else if ((SD_SHORT_RESPONSE == resp) ||(SD_LONG_RESPONSE == resp)) {
        while(1) {
            status = REG32((T_U32)priv_data->base_addr + s_SdRegStatus); 
            if ((status & CMD_TIME_OUT)||(status & CMD_CRC_FAIL)) {
                printf("send cmd %d error, status = %x\n", cmd_index, status);
                return AK_FALSE;       
            }
            else if (status & CMD_RESP_END) {
                return AK_TRUE;
            }
        }
    }
    else {
        printf("error requeset!\n");
        return AK_FALSE;
    }                   
}

/**
 * @brief send sd acmd.
 *
 * ALL the ACMDS shall be preceded with APP_CMD command cmd55
 * @author CaoDonghua
 * @date 2016-09-26
 * @param[in] cmd_index The command index.
 * @param[in] rsp The command response:no response ,short reponse or long response
 * @param[in] arg The cmd argument.
 * @return T_BOOL acmd send failed or successfully
 * @retval  AK_TRUE: ACMD sent successfully
 * @retval  AK_FALSE: ACMD sent failed
 */
static bool send_acmd(struct mmc *mmc, unsigned char cmd_index, unsigned char resp, unsigned int arg)
{
    unsigned int rca;
 
    rca = mmc->rca;
    send_cmd(mmc, SD_CMD(55), SD_SHORT_RESPONSE, rca << 16);
    return (send_cmd(mmc, cmd_index, resp, arg));
}


/**
* @brief Check sd controller ready status
*
* Check if sd controller is transferring now
* @author CaoDonghua
* @date 2016-09-26
* @return T_BOOL sd controller is busy or ready
* @retval  AK_TRUE: sd controller is transferring
* @retval  AK_FALSE: sd controller is not transferring
*/
static bool sd_trans_busy(struct mmc *mmc)
{
    unsigned int status;
	struct ak_sdhsmmc_data *priv_data = (struct ak_sdhsmmc_data *)mmc->priv;
	
    status = REG32((T_U32)priv_data->base_addr +s_SdRegStatus); 
    if ((status & TX_ACTIVE) || (status & RX_ACTIVE)) {
        return  AK_TRUE;
    }
    else {
        return AK_FALSE;
    }
}


/*@brief set sd controller data register.
*
* Set timeout value,transfer size,transfer direction,bus mode,data block len
* @author CaoDonghua
* @date 2016-09-26
* @param[in] len transfer size
* @param[in] blk_size  block length
* @param[in] dir  transfer direction
* @return T_VOID
* @retval  none
*/
static void set_data_reg(struct mmc *mmc, unsigned int len, unsigned int blk_size, unsigned char bus_mode , unsigned char dir)
{
    unsigned int reg_value;
	struct ak_sdhsmmc_data *priv_data = (struct ak_sdhsmmc_data *)mmc->priv;

 	REG32((T_U32)priv_data->base_addr + s_SdRegDataTim) = SD_DAT_MAX_TIMER_V;
    REG32((T_U32)priv_data->base_addr + s_SdRegDataLen) = len;

    reg_value = SD_DATA_CTL_ENABLE | ( dir << SD_DATA_CTL_DIRECTION_OFFSET ) \
                | (bus_mode << SD_DATA_CTL_BUS_MODE_OFFSET) \
                | (blk_size << SD_DATA_CTL_BLOCK_LEN_OFFSET );
   
    REG32((T_U32)priv_data->base_addr + s_SdRegDataCtrl) = reg_value;
}





/**
* @brief sd controller send data 
*
* sd controller send data ,use cpu mode
* @author CaoDonghua
* @date 2016-09-26
* @param[in] buf the pointer of array which will be sent to card 
* @param[in] len the size of data which will be sent to card 
* @return T_BOOL send failed or successfully
* @retval  AK_TRUE: send  successfully
* @retval  AK_FALSE: send failed
*/
static bool sd_tx_data_cpu(struct mmc *mmc, unsigned char buf[], unsigned int len)
{
    unsigned int status;
    unsigned int offset = 0;
	struct ak_sdhsmmc_data *priv_data = (struct ak_sdhsmmc_data *)mmc->priv;
    
    while (1) {
        status = REG32((T_U32)priv_data->base_addr + s_SdRegStatus);
        if ((offset<len) && (status&DATA_BUF_EMPTY)) {
            REG32((T_U32)priv_data->base_addr + s_SdRegCpuMode) = (buf[offset])|(buf[offset+1]<<8)|(buf[offset+2]<<16)|(buf[offset+3]<<24);
            offset += 4;
        }
        
        if ((status&DATA_TIME_OUT) || (status&DATA_CRC_FAIL)) {
            printf("crc error or timeout, status is %x\n",status); 
            return AK_FALSE;
        }
        
        if (!(status & TX_ACTIVE)) {
            break;
        }
    }

    return AK_TRUE;
}




/**
* @brief sd controller receive data 
* 
* sd controller receive data ,use cpu mode
* @author CaoDonghua
* @date 2016-09-26
* @param[in] buf the pointer of array to save received data
* @param[in] len the size of data received
* @return T_BOOL receive failed or successfully
* @retval  AK_TRUE: receive  successfully
* @retval  AK_FALSE: receive failed
*/
static bool sd_rx_data_cpu(struct mmc *mmc, unsigned char buf[], unsigned int len)
{
    unsigned int status;
    unsigned int buf_tmp;
    unsigned int i;
    unsigned int offset, size;
	struct ak_sdhsmmc_data *priv_data = (struct ak_sdhsmmc_data *)mmc->priv;

    offset = 0;
    size = len;
    
    while (1) {
        status = REG32((T_U32)priv_data->base_addr + s_SdRegStatus);
        if ((status & DATA_BUF_FULL)) {
            buf_tmp  = REG32((T_U32)priv_data->base_addr + s_SdRegCpuMode); 
            for (i=0; i<4; i++) {
                buf[offset+i] = (T_U8)((buf_tmp >> (i*8)) & 0xff);
            }
            
            offset += 4;
            size -= 4;
        }
        
        if ((size>0) && (size<4) && (status&DATA_END)) {
            buf_tmp = REG32((T_U32)priv_data->base_addr + s_SdRegCpuMode);
            for (i=0; i<size; i++) {
                buf[offset+i] = (T_U8)((buf_tmp >> (i*8)) & 0xff);
            }
            
            size = 0;           
        }
        
        if ((status&DATA_TIME_OUT) || (status&DATA_CRC_FAIL)) {
            printf("crc error or timeout, status is %x\n", status); 
            return AK_FALSE;
        }
        
        if (!(status&RX_ACTIVE)) {
            break;
        }
    }

    return AK_TRUE;
}




/**
 * @brief SD read or write data use cpu mode
 *
 * read or write data transfer with use cpu mode
 * @author CaoDonghua
 * @date 2016-09-26
 * @param[in] ram_addr used as data buffer
 * @param[in] size transfer bytes
 * @param[in] dir   transfer direction
 * @return T_BOOL transfer failed or successfully
 * @retval  AK_TRUE: transfer successfully
 * @retval  AK_FALSE: transfer failed
 */
static bool sd_trans_data_cpu(struct mmc *mmc, struct mmc_data *data, unsigned char *ram_addr, unsigned int size, unsigned char dir)
{  
    bool ret = AK_TRUE;
	T_eBUS_MODE   BusMode;
	
    /* configue bus width */
	switch (mmc->bus_width) {
	case 4:
		BusMode = USE_FOUR_BUS;
		break;
	case 1:
	default:
		BusMode = USE_ONE_BUS;
		break;
	}
	
    /* set data transfer controller reg config */ 
	set_data_reg(mmc, size, data->blocksize, BusMode, dir);

	/* tx or rx data transfer use cpu mode  */ 
    if (SD_DATA_CTL_TO_HOST == dir) {   
    	/* receive data with cpu mode */
        if(!sd_rx_data_cpu(mmc, ram_addr, size)) {
            ret = AK_FALSE;
        }
    }
    else {	
    	/* send data with cpu mode */
        if(!sd_tx_data_cpu(mmc, ram_addr, size)) {
            ret = AK_FALSE;
        }
    }
    
    return ret;
}




/**
* @brief  sd read data operation
*
*  read data or block operation
* @author CaoDonghua
* @date 2016-09-26
* @param[in] mmc  mmc device handle.
* @param[in] cmd   mmc cmd handle.
* @param[in] data  mmc data handle.
* @return int return read date or block operation success or failed
* @retval returns zero on success
* @retval return a non-zero if failed
*/
static int ak_mmc_read_data(struct mmc *mmc, struct mmc_data *data, char *buf, unsigned int size)
{
	int ret = 0;

	/*  rx transfer data with cpu mode */
    if (!sd_trans_data_cpu(mmc, data, (unsigned char *)buf, size, SD_DATA_CTL_TO_HOST)){
    	printf("%s:read data failed!\n", __func__);
		ret = -1;
    }
    
    return ret;
}

/**
* @brief  sd write data operation
*
*  write data or block operation
* @author CaoDonghua
* @date 2016-09-26
* @param[in] mmc  mmc device handle.
* @param[in] data   mmc data handle.
* @param[in] buf   mmc write data buffer.
* @param[in] size  mmc write data size.
* @return int return write data or block operation success or failed
* @retval returns zero on success
* @retval return a non-zero if failed
*/
static int ak_mmc_write_data(struct mmc *mmc, struct mmc_data *data, char *buf, unsigned int size)
{
	int ret = 0;

	/*  tx transfer data with cpu mode */
    if (!sd_trans_data_cpu(mmc, data, (unsigned char *)buf, size, SD_DATA_CTL_TO_CARD)){
    	printf("%s:write data failed!\n", __func__);
		ret = -1;
    }
    
    return ret;
}

/**
* @brief  send cmd and with or without data operation
*
*  include only send cmd with response and read or write block operation
* @author CaoDonghua
* @date 2016-09-26
* @param[in] mmc  mmc device handle.
* @param[in] cmd   mmc cmd handle.
* @param[in] data  mmc data handle.
* @return int return send sd cmd and read or write block success or failed
* @retval returns zero on success
* @retval return a non-zero if failed
*/
static int ak_mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd,
			struct mmc_data *data)
{
	unsigned char flags = 0;
	int ret = 0;
	struct ak_sdhsmmc_data *priv_data = (struct ak_sdhsmmc_data *)mmc->priv;
	
	/* notes:add delay while get card scr command, wait 50 ms */
	if (cmd->cmdidx == SD_CMD_APP_SEND_SCR)
		udelay(50000); 

	/* according cmd resp_type to set respone argument */
	if (cmd->resp_type == MMC_RSP_NONE){ 
		flags = SD_NO_RESPONSE;
	}
	else if ((cmd->resp_type & MMC_RSP_136)){
		flags = SD_LONG_RESPONSE;
	} 
	else if ((cmd->resp_type & MMC_RSP_OPCODE)){
		flags = SD_SHORT_RESPONSE;
	}
	else{
		flags = SD_NO_RESPONSE;
	}

	/* first check sd controller bus busy state */
    if (sd_trans_busy(mmc)) {
        return -1;
    }

	/* ACMD23 set the number of write blocks to be pre-erased before writing for multi-blocks write */
    if(cmd->cmdidx == SD_CMD(25)){
        if (send_acmd(mmc, 23, SD_SHORT_RESPONSE, data->blocks) == AK_FALSE ){
            printf("set the number of write blocks to be pre-erased failed!\n");
            return AK_FALSE;
        }
    }

    /* normal send sd cmd */
    if (!send_cmd(mmc, cmd->cmdidx, flags, cmd->cmdarg)) {
        printf("block rw command %d is failed!\n", cmd->cmdidx);
        return -1;
    }
	/* get send sd cmd  response*/
	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			/* get long cmd  response*/
			cmd->response[3] = REG32((T_U32)priv_data->base_addr + s_SdRegResp0); 
			cmd->response[2] = REG32((T_U32)priv_data->base_addr + s_SdRegResp1); 
			cmd->response[1] = REG32((T_U32)priv_data->base_addr + s_SdRegResp2); 
			cmd->response[0] = REG32((T_U32)priv_data->base_addr + s_SdRegResp3); 
		} else {
			/* get short cmd  response*/
			cmd->response[0] = REG32((T_U32)priv_data->base_addr + s_SdRegResp0); 
		}
	}

	/* data控制数据，data->flags控制读写方向*/ 
	if (data && (data->flags & MMC_DATA_READ)) {
		ret = ak_mmc_read_data(mmc, data, data->dest, data->blocksize * data->blocks);
	} else if (data && (data->flags & MMC_DATA_WRITE)) {
		ret = ak_mmc_write_data(mmc, data,  (char *)data->src, data->blocksize * data->blocks);
	}

    return ret;
}

/**
* @brief  set sd controller clock and sd card bus width
*
*  init set sd controller clock and sd card bus width
* @author CaoDonghua
* @date 2016-09-26
* @param[in] mmc  mmc device handle.
* @return int return set sd controller clock and sd card bus width init success or failed
* @retval returns none
*/

extern unsigned long get_asic_freq(void);

static void ak_mmc_set_ios(struct mmc *mmc)
{
	unsigned int arg_value = 0;

	if (mmc->iosset_flag == 0x1) {
		/* SD_POWER_SAVE_ENABLE */
		ak_set_clock(mmc, mmc->clock, get_asic_freq(), SD_POWER_SAVE_ENABLE);
	}else if (mmc->iosset_flag == 0x2){
		switch (mmc->bus_width) {
		case 4:
			arg_value = SD_BUS_WIDTH_4BIT;
			break;
		case 1:
		default:
			arg_value = SD_BUS_WIDTH_1BIT;
			break;
		}

		/* configue sd card device bus width */
		if (!send_acmd(mmc, SD_CMD(6), SD_SHORT_RESPONSE, arg_value)) {
	        printf("%s: sd card cfg bus wide err!\n", __func__);
	    } 
	}else {
		printf("cdh:mmc->iosset_flag no valid!\n");
	}
    
}

/**
* @brief  sd  interface and controller init
*
*  init select sd interface share pin and controller clock and soft reset 
* @author CaoDonghua
* @date 2016-09-26
* @param[in] mmc  mmc device handle.
* @return int return anyka sd  interface and controller init success or failed
* @retval returns zero on success
* @retval return a non-zero if failed
*/
static int ak_mmc_init_setup(struct mmc *mmc)
{
	/* initial sd interface sharepin 
	*   and open this sd system module clock 
	*/ 
	ak_mmc_board_init(mmc);

	/* initial sd interface default clock from get current asic clock
	*   and open this sd system module clock 
	*   and enable power save
	*/ 
	ak_set_clock(mmc, SD_IDENTIFICATION_MODE_CLK, get_asic_freq(), SD_POWER_SAVE_ENABLE);
	mdelay(100);
	
	return 0;
}

/**
* @brief anyka mmc/sd controller init
*
*  init anyka mmc/sd controller features
* @author CaoDonghua
* @date 2016-09-26
* @param[in] dev_index  mmc bus device index.
* @param[in] host_caps_mask mmc controller capacity.
* @param[in] f_max mmc controller support max speed.
* @param[in] cd_gpio  mmc controller sd card plug/unplug detect.
* @param[in] wp_gpio mmc controller sd card protect detect.
* @return int return anyka mmc/sd controller init success or failed
* @retval returns zero on success
* @retval return a non-zero if failed
*/
int ak_sdhsmmc_init(int dev_index, uint host_caps_mask, uint f_max, int cd_gpio,
		int wp_gpio)
{
	struct mmc *mmc = &sdhsmmc_dev[dev_index];
	struct ak_sdhsmmc_data *priv_data = &sdhsmmc_dev_data[dev_index];

	/* set mmc/sd controller support capacity,as for H3 AK3918E platform,We use 1bit bus_width */
	uint host_caps_val = MMC_MODE_HS_52MHz | MMC_MODE_HS | MMC_MODE_HC;

	sprintf(mmc->name, "mmc_sd");
	mmc->send_cmd = ak_mmc_send_cmd;
	mmc->set_ios  = ak_mmc_set_ios;
	mmc->init 	  = ak_mmc_init_setup;
	mmc->priv 	  = priv_data;

	/* select the dev mmc bus register base address */
	switch (dev_index) {
	case 0:
		priv_data->base_addr = (struct akhsmmc *)AK_SDHSMMC1_BASE;
		break;
	case 1:
		priv_data->base_addr = (struct akhsmmc *)AK_SDHSMMC2_BASE;
		break;
	case 2:
		priv_data->base_addr = (struct akhsmmc *)AK_SDHSMMC3_BASE;
		break;
	default:
		priv_data->base_addr = (struct akhsmmc *)AK_SDHSMMC1_BASE;
		break;
	}

	//debug("cdh:%s, line:%d, dev_index:0x%x, sd_base:0x%x\n", __func__, __LINE__, dev_index, priv_data->base_addr);
	priv_data->dev_index = dev_index;

	/* check if has cd detect pin, and set match handle function, cd_gpio=AK_GPIO57 */
	priv_data->cd_gpio = ak_mmc_setup_gpio_in(cd_gpio, "mmc_cd");
	if (priv_data->cd_gpio != -1) {
		debug("yes cd_gpio=%d\n", priv_data->cd_gpio);
		mmc->getcd = ak_mmc_getcd;
	}

	/* check if has cd detect pin, and set match handle function, cd_gpio=AK_GPIO57 
	*  if no wp , we set wp_gpio = 0xFF
	*/
	priv_data->wp_gpio = ak_mmc_setup_gpio_in(wp_gpio, "mmc_wp"); 
	if (priv_data->wp_gpio != -1) {
		debug("yes wp_gpio=%d\n", priv_data->wp_gpio);
		mmc->getwp = ak_mmc_getwp; 
	}
	debug("sd det pin:%d, wp pin:%d\n", priv_data->cd_gpio, priv_data->wp_gpio);

	/* set mmc/sd controller parameters
	*  host_caps:support high speed and high capacity
	*  f_min:mini speed 400kHz
	*  f_max:max speed 52MHz
	*/
	mmc->host_caps = host_caps_val & ~host_caps_mask;
	mmc->f_min = 400000; 

	if (f_max != 0) 
		mmc->f_max = f_max;
	else {
		if (mmc->host_caps & MMC_MODE_HS) {
			if (mmc->host_caps & MMC_MODE_HS_52MHz)
				mmc->f_max = 52000000;
			else
				mmc->f_max = 26000000;
		} else
			mmc->f_max = 20000000;
	}

	/* set mmc/sd controller parameters
	*  b_max:max block cnt every times operation,default 4KB
	*  voltages:support 2.6v ~ 3.4v
	*/
	mmc->b_max    = SD_DMA_BLOCK_4K; // cdh: support for multi block max cnts
	mmc->voltages = SD_DEFAULT_VOLTAGE;

	/* register mmc device to mmc core */
	mmc_register(mmc);
	debug("cdh:%s, init end!\n", __func__);

	return 0;
}


bool sd_init_info(int idex, struct mmc_cd_info *mmc_cd)
{
	unsigned int ret = 0;// i;
	int node = -1;
	const void *s_fdt_dtb = (const void *)gd->fdt_blob;
	unsigned int array[3] = {0};

	ret = fdt_check_header(s_fdt_dtb);
	if (0 != ret)
	{
		printf("fdt_check_header err:%d\n", ret);
		return false;
	}

	//node = fdtdec_get_node_by_compatible((const void *)s_fdt_dtb, 0, g_compatible[idex]);
	node = fdtdec_next_compatible(s_fdt_dtb, 0, COMPAT_ANYKA_H3D_MCI0+idex);
	if(node < 0)
	{
		printf("fdt err:%d\n", node);
		return false;
	}

	if (!fdtdec_get_is_enabled((const void *)s_fdt_dtb, node))
	{
		//printf("fdtdec_get_is_enabled 0\n");
		return false;
	}


	if(!fdtdec_get_bool((const void *)s_fdt_dtb, node, "cap-sd-highspeed"))
	{
		//printf("fdtdec_get_bool 0\n");
		return false;
	}

	if(fdtdec_get_bool((const void *)s_fdt_dtb, node, "cap-sdio-irq"))
	{
		//printf("fdtdec_get_bool sdio 0\n");
		return false;
	}

	ret = fdtdec_get_int_array((const void *)s_fdt_dtb, node, "power-pins", array, 3);
    if (ret < 0)
    {
        printf("fdt err:%d, %s, line:%d\n", ret, __func__, __LINE__);
        return false;
    }

    g_power_on_pin = array[1];

	if(fdtdec_get_bool((const void *)s_fdt_dtb, node, "cd_clk")){
		//printf("fdtdec_get_bool mmc cd mode as cd_clk_mode OK\n");
		mmc_cd->mmc_plug_mod = CD_CLK_MODE;
		return true;
	}else{
		ret = fdtdec_get_int_array((const void *)s_fdt_dtb, node, "detect-gpio", array, 3);
		if(ret == 0){
			//printf("fdtdec_get_bool detect-gpio:%d OK\n", array[1]);
			mmc_cd->mmc_plug_mod = DETECT_GPIO_MODE;
			mmc_cd->mmc_plug_gpio_index = array[1];
			return true;
		}else{
			if(fdtdec_get_bool((const void *)s_fdt_dtb, node, "non-removable")){
				//printf("fdtdec_get_bool non-removable OK\n");
				mmc_cd->mmc_plug_mod = NON_REMOVABLE_MODE;
				return true;
			}else{
				//printf("fdtdec_get_bool non-removable fail\n");
				return false;
			}
		}
	}
}

