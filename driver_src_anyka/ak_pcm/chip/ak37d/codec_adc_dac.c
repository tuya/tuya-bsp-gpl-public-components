/*
 * @file codec_adc_dac.c
 * @brief the source code of DA/AD controller
 * This file is the source code of DA/AD controller
 * Copyright (C) 2010 Anyka (GuangZhou) Software Technology Co., Ltd.
 * @author wushangsong
 * @date 2020-06-23
 * @version 1.0
 */

#include "pcm_port.h"
#include "codec_adc_dac.h"
#include "soundcard.h"

extern AK_SND_CARD g_ak_snd_card;

#define AK_SND_VCM2_WAIT_TIME_MS		(20)

static void adc2_dac_share_open(SND_PARAM *codec)
{
	unsigned int reg_value;

	//BGR->VCM2
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	reg_value &= ~(PD_SARADCREF | SARADC_IBCTRL);
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);

	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value &= ~MASK_DIS_CHG_VCM2;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG3_REG);
	reg_value |= VREF_TEST_EN;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG3_REG);

	//open VREF1.5V
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	reg_value &= ~MASK_ANTIPOP_EN;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);

	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	reg_value |= ANTIPOP_EN(1);
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);

	//power on bias generator
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value &= ~PD_BIAS;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	//Disable the pull-down 2Kohm resistor to VCM3
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value &= ~PL_VCM3;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	//Power on VCM3
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value &= ~PD_VCM3;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG3_REG);
	reg_value &= ~HP_VCM_BUF_POWER_ON;
	__raw_writel(reg_value, codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG3_REG);

	//power on nmos part of head phone driver
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value &= ~PD1_HP;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	//power on pmos part of head phone driver
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value &= ~PD2_HP;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	//power on the integrator in DAC
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value &= ~PD_OP;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	//reset DAC output to middle volatge
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value |= RST_DAC;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	//wait vcm2 charging fully
	msleep(AK_SND_VCM2_WAIT_TIME_MS);

	//disable BGR output VREF, close fast charge
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG3_REG);
	reg_value &= ~VREF_TEST_EN;
	__raw_writel(reg_value, codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG3_REG);

	//to normal
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value &= ~RST_DAC;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	//power on the DAC clk
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value &= ~PD_CK;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
}

static void adc2_dac_share_close(SND_PARAM *codec)
{
	unsigned int reg_value;

	//enable vcm2 discharge
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value |= MASK_DIS_CHG_VCM2;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	//PowerOff Vcm3
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value |= PD_VCM3;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	//SetVcm3powdown
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value |= PL_VCM3;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	//power off bias generator
	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value |= PD_BIAS;
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
}

static int codec_dac_open(SND_PARAM *codec)
{
	int i;
	unsigned long reg_val = 0;

	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_CFG_REG);
	reg_val |= MUTE;
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_CFG_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_CFG_REG);
	reg_val &= ~DAC_CTRL_EN;
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_CFG_REG);

	reg_val = __raw_readl(codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG);
	reg_val &= ~DAC_CLK_EN;
	__raw_writel(reg_val,codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG);

	reg_val = __raw_readl(codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG);
	reg_val |= DAC_DIV_VLD;
	__raw_writel(reg_val,codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG);

	i = 0;
	while(__raw_readl(codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG) & (DAC_DIV_VLD))
	{
		++i;
		if(i > 100000)
		{
			ak_pcm_debug("set da clk reg fail");
			return -EINVAL;
		}
	}

	// to enable DAC CLK
	reg_val = __raw_readl(codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG);
	reg_val |= DAC_CLK_EN;
	__raw_writel(reg_val,codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG);

	//soft reset DAC
	reg_val = __raw_readl(codec->sysctrl_base + SOFT_RST_CTRL_REG);
	reg_val &= ~(DAC_SOFT_RST);
	__raw_writel(reg_val,codec->sysctrl_base + SOFT_RST_CTRL_REG);

	reg_val = __raw_readl(codec->sysctrl_base + SOFT_RST_CTRL_REG);
	reg_val |= DAC_SOFT_RST;
	__raw_writel(reg_val,codec->sysctrl_base + SOFT_RST_CTRL_REG);

	//to enable internal dac/adc via i2s
	reg_val = __raw_readl(codec->sysctrl_base + MULTIPLE_FUN_CTRL_REG1);
	reg_val |= IN_DAAD_EN;
	__raw_writel(reg_val,codec->sysctrl_base + MULTIPLE_FUN_CTRL_REG1);

	//I2S config reg
	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_I2S_CFG_REG);
	reg_val &= (~I2S_CONFIG_WORDLENGTH_MASK);
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_I2S_CFG_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_I2S_CFG_REG);
	reg_val |= (16<<0);
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_I2S_CFG_REG);

	//config I2S interface DAC
	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_CFG_REG);
	reg_val |= DAC_CTRL_EN;
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_CFG_REG);

	//enable accept data from l2
	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_CFG_REG);
	reg_val |= L2_EN;
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_CFG_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_CFG_REG);
	reg_val |= FORMAT;
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_CFG_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_CFG_REG);
	reg_val &= ~ARM_INT;
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_CFG_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_I2S_CFG_REG);
	reg_val &= ~POLARITY_SEL;
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_I2S_CFG_REG);

	reg_val = __raw_readl(codec->sysctrl_base + FADEOUT_CTRL_REG);
	reg_val |= DAC_FILTER_EN;
	__raw_writel(reg_val,codec->sysctrl_base + FADEOUT_CTRL_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_CFG_REG);
	reg_val &= ~MUTE;
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_CFG_REG);

	return 0;
}

static int codec_dac_close(SND_PARAM *codec)
{
	unsigned long reg_val = 0;

	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_CFG_REG);
	reg_val &= (~L2_EN);
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_CFG_REG);

	reg_val = __raw_readl(codec->sysctrl_base + MULTIPLE_FUN_CTRL_REG1);
	reg_val &= (~(IN_DAAD_EN));
	__raw_writel(reg_val,codec->sysctrl_base + MULTIPLE_FUN_CTRL_REG1);

	reg_val = __raw_readl(codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG);
	reg_val &= (~(DAC_CLK_EN|DAC_DIV_VLD));
	__raw_writel(reg_val,codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + DAC_CTRL_CFG_REG);
	reg_val &= (~DAC_CTRL_EN);
	__raw_writel(reg_val,codec->adda_cfg_base + DAC_CTRL_CFG_REG);

	return 0;
}

static int codec_adc_open(SND_PARAM *codec)
{
	unsigned long reg_val = 0;

	adc2_dac_share_open(codec);

	reg_val = __raw_readl(codec->sysctrl_base + MULTIPLE_FUN_CTRL_REG1);
	reg_val |= IN_DAAD_EN;
	__raw_writel(reg_val,codec->sysctrl_base + MULTIPLE_FUN_CTRL_REG1);

	reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	reg_val |= LIMEN;
	__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);
	reg_val &= ~HOST_RD_INT_EN;
	__raw_writel(reg_val,codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);
	reg_val |= CH_POLARITY_SEL;
	__raw_writel(reg_val,codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);
	reg_val &= ~I2S_EN;
	__raw_writel(reg_val,codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);

	reg_val = __raw_readl(codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);
	reg_val &= ~WORD_LENGTH_MASK;
	reg_val |= ADC2_WORD_LENGTH(16); //16bit as word
	__raw_writel(reg_val,codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);

	reg_val = __raw_readl(codec->sysctrl_base + SOFT_RST_CTRL_REG);
	reg_val |= (ADC2_SOFT_RST);
	__raw_writel(reg_val,codec->sysctrl_base + SOFT_RST_CTRL_REG);

	reg_val = __raw_readl(codec->sysctrl_base + SOFT_RST_CTRL_REG);
	reg_val &= ~(ADC2_SOFT_RST);
	__raw_writel(reg_val,codec->sysctrl_base + SOFT_RST_CTRL_REG);

	reg_val = __raw_readl(codec->sysctrl_base + SOFT_RST_CTRL_REG);
	reg_val |= (ADC2_SOFT_RST);
	__raw_writel(reg_val,codec->sysctrl_base + SOFT_RST_CTRL_REG);

	//Enable adc controller
	reg_val = __raw_readl(codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);
	reg_val |= ADC2_CTRL_EN;
	__raw_writel(reg_val,codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);

	//Enable l2
	reg_val = __raw_readl(codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);
	reg_val |= ADC2MODE_L2_EN;
	__raw_writel(reg_val,codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);

	//Power on adc2 conversion
	reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	reg_val &= ~PD_S2D;
	__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);

	reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	reg_val &= ~PD_ADC2;
	__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);

	return 0;
}

static int codec_adc_close(SND_PARAM *codec)
{
	unsigned long reg_val = 0;

	//disable l2
	reg_val = __raw_readl(codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);
	reg_val &= ~ADC2MODE_L2_EN;
	__raw_writel(reg_val,codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);

	//disable adc2 clk
	reg_val = __raw_readl(codec->sysctrl_base + HIGHSPEED_CLOCK_CTRL_REG);
	reg_val &= ~ADC2_CLK_EN;
	__raw_writel(reg_val,codec->sysctrl_base + HIGHSPEED_CLOCK_CTRL_REG);

	//disable ADC2 interface
	reg_val = __raw_readl(codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);
	reg_val &= ~ADC2_CTRL_EN;
	__raw_writel(reg_val,codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG);

	//Power off adc2
	reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	reg_val |= PD_ADC2|PD_S2D;
	__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);

	reg_val = __raw_readl(codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG);
	reg_val |= (DAC_DIV_VLD);
	__raw_writel(reg_val,codec->sysctrl_base + SAR_ADC_CLK_CTRL_REG);

	return 0;
}

/**
 * @brief power on HP
 * @author
 * @date	2020-06-23 modify
 * @param 	bOn  poweron or poweroff
 * @return  void
 */
static int set_hp_power(bool bOn)
{
	int ret = 0;
	SND_PARAM *codec = g_ak_snd_card.param;

	ak_pcm_func("hp power:%d", bOn);

	if (bOn) {
		adc2_dac_share_open(codec);
	} else {
		adc2_dac_share_close(codec);
	}

	return ret;
}

/**
 * @brief  open a dac device
 * @author
 * @date   2020-06-23
 * @return int
 * @retval true  open successful
 * @retval false open failed
 */
int dac_open(void)
{
	SND_PARAM *codec = g_ak_snd_card.param;

	ak_pcm_func("opening...");

	dac_clk_enable(codec);

	return codec_dac_open(codec);
}

/**
 * @brief   Close a dac device
 * @author
 * @date    2020-06-23
 * @return  int
 * @retval  true close successful
 * @retval  false close failed
 */
int dac_close(void)
{
	SND_PARAM *codec = g_ak_snd_card.param;

	ak_pcm_func("closing...");

	return codec_dac_close(codec);
}

/**
 * @brief   Set sound sample rate, channel, bits per sample of the sound device
 * @author
 * @date    2020-06-23 modify
 * @param   format     refer to snd_format
 * @return  unsigned long
 * @retval  actual_rate
 */
unsigned long dac_setformat(SND_FORMAT *format)
{
	SND_PARAM *codec = g_ak_snd_card.param;

	return dac_hw_setformat(codec, format->sample_rate);
}

/**
 * @brief  select HP in signal
 * @author
 * @date   2020-06-23 modify
 * @param  signal: signal desired
 * @return void
 */
static void set_hp_in(unsigned long signal)
{
	unsigned long reg_val;
	SND_PARAM *codec = g_ak_snd_card.param;
	unsigned long reg = 0;

	ak_pcm_debug("set hp in");

	if (!(signal))
		reg |= HP_IN_MUTE;
	if (MIXER_OUT_SEL_DAC & signal)
		reg |= 1 << HP_IN_DAC_OFFSET;
	if (MIXER_OUT_SEL_LINEIN & signal)
		reg |= 1 << HP_IN_LINEIN_OFFSET;
	if (MIXER_OUT_SEL_MIC & signal)
		reg |= 1 << HP_IN_MIC_OFFSET;

	reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_val &= ~(0x7 << HP_IN);
	reg_val |= ((reg & 0x7) << HP_IN);
	__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
}

/**
 * @brief   get HP in signal
 * @author
 * @date	2020-06-23 modify
 * @return  int
 */
static int get_hp_in(void)
{
	unsigned long reg_val;
	SND_PARAM *codec = g_ak_snd_card.param;
	unsigned long reg;
	unsigned int  ret;

	ak_pcm_debug("get hp in");

	reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg = (reg_val >> 14) & 0x7;

	if (!(reg))
		ret = MIXER_OUT_SEL_MUTE;
	if (reg & (1 << HP_IN_DAC_OFFSET))
		ret |= MIXER_OUT_SEL_DAC;
	if (reg & (1 << HP_IN_LINEIN_OFFSET))
		ret |= MIXER_OUT_SEL_LINEIN;
	if (reg & (1 << HP_IN_MIC_OFFSET))
		ret |= MIXER_OUT_SEL_MIC;

	return ret;
}

/**
 * @brief   set ADC2 int signal
 * @author
 * @date	2020-06-23 modify
 * @param   signal:  DAC|LINEIN|MIC
 * @return  void
 */
static void set_adc2_in(unsigned long signal)
{
	unsigned long reg_val;
	SND_PARAM *codec = g_ak_snd_card.param;
	unsigned long reg;

	ak_pcm_debug("set adc2 in");

	if (!signal)
		reg = ADC_IN_MUTE;
	if (MIXER_IN_SEL_DAC & signal)
		reg |= 1 << ADC_IN_DAC_OFFSET;
	if (MIXER_IN_SEL_LINEIN & signal)
		reg |= 1 << ADC_IN_LINEIN_OFFSET;
	if (MIXER_IN_SEL_MIC & signal)
		reg |= 1 << ADC_IN_MIC_OFFSET;

	reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	reg_val &= ~(0x7 << ADC_IN_SEL);
	reg_val |= ((reg&0x7) << ADC_IN_SEL);
	__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
}

/**
 * @brief   power on linein
 * @author
 * @date	2020-06-23 modify
 * @param   bOn:  0|1|
 * @return  void
 */
static int set_linein_power(bool bOn)
{
	int ret = 0;
	unsigned long reg_val = 0;
	SND_PARAM *codec = g_ak_snd_card.param;

	ak_pcm_debug("set linein power");

	if (bOn) {
		reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
		reg_val &= ~(1 << LINE_POWER); //power on the channel
		__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	} else {
		reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
		reg_val |= (1 << LINE_POWER); //power off the channel
		__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	}
	return ret;
}

/**
 * @brief   power on mic
 * @author
 * @date	2020-06-23 modify
 * @param   bOn: 1-connet DAC to lineout; 0-disconnect
 * @return  void
 */
static int set_mic_power(bool bOn)
{
	int ret = 0;
	unsigned long reg_val = 0;
	SND_PARAM *codec = g_ak_snd_card.param;

	ak_pcm_debug("set mic power");

	//NOTE:
	//mono: bit20 on/off
	//diff: bit19 & bit19 on/off
	if (bOn) {
		//power on mic interface
		reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
#if DOUBLE_MIC_GAIN
		reg_val &= (~(PD_MICP));
		reg_val	|= (PD_MICN);  //power on diff mic
#else
		reg_val &= (~((PD_MICP) | (PD_MICN)));  //power on mono mic
#endif
		__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	} else {
		reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
		reg_val |= (PD_MICP) | (PD_MICN);  //power off mono mic
		__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	}

	return ret;
}

/**
 * @brief   set HP gain
 * @author
 * @date	2020-06-23 modify
 * @param   gain: 0~5
 * @return  void
 */
static int set_hp_gain(unsigned long gain)
{
	int ret = 0;
	unsigned long reg_value;
	//for H240
	unsigned long gain_table[6] = {0x10, 0x08, 0x04, 0x02, 0x01, 0x00};
	SND_PARAM *codec = g_ak_snd_card.param;

	if (gain < HEADPHONE_GAIN_MIN) {
		gain = HEADPHONE_GAIN_MIN;
	} else if (gain > HEADPHONE_GAIN_MAX) {
		gain = HEADPHONE_GAIN_MAX;
	}
	ak_pcm_debug("IOC_SET_HP_GAIN, gain=%lu", gain);

	reg_value = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);
	reg_value &= ~MASK_HP_GAIN;
	reg_value |= HP_GAIN(gain_table[gain]);
	__raw_writel(reg_value,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG1_REG);

	return ret;
}

/**
 * @brief   set mic gain
 * @author
 * @date	2020-06-23 modify
 * @param   gain: 0~7
 * @return  void
 */
static int set_mic_gain(unsigned long gain)
{
	int ret = 0;
	unsigned long reg_val = 0;
	SND_PARAM *codec = g_ak_snd_card.param;

	if (gain < MIC_GAIN_MIN) {
		gain = MIC_GAIN_MIN;
	} else if (gain > MIC_GAIN_MAX) {
		gain = MIC_GAIN_MAX;
	}
	ak_pcm_debug("IOC_SET_MIC_GAIN, gain=%lu", gain);

	reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	reg_val &= ~(0x7<<MIC_GAIN);

#if DOUBLE_MIC_GAIN
	reg_val |= (MIC_GAINBST); //use double mic gain
#else
	reg_val &= ~(MIC_GAINBST); //not use double Mic gain
#endif
	reg_val |= ((gain & 0x7) << MIC_GAIN);

	__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);

	return ret;
}

/**
 * @brief   set linein gain
 * @author
 * @date	2020-06-23 modify
 * @param   gain: 0~15
 * @return  void
 */
static int set_linein_gain(unsigned long gain)
{
	unsigned long reg_val = 0;
	int ret = 0;
	SND_PARAM *codec = g_ak_snd_card.param;

	if (gain < LINEIN_GAIN_MIN) {
		gain = LINEIN_GAIN_MIN;
	} else if (gain > LINEIN_GAIN_MAX) {
		gain = LINEIN_GAIN_MAX;
	}
	ak_pcm_debug("IOC_SET_LINENIN_GAIN, gain=%lu", gain);

	reg_val = __raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);
	reg_val &= ~(0xF<<LINE_GAIN);
	reg_val |= ((gain&0xF)<<LINE_GAIN);
	__raw_writel(reg_val,codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG);

	return ret;
}

/**
 * @brief  	set analog gain
 * @author
 * @date	2020-06-23 modify
 * @param  	addr
 * @param  	gain
 * @retval  true set successful
 * @retval  EINVAL set failed
 */
static int set_analog_gain(int addr, unsigned long gain)
{
	int ret = 0;

	ak_pcm_debug("dac set analog gain");

	switch (addr)
	{
		case MIXER_VOL_MIC:
			ret = set_mic_gain(gain);
			break;
		case MIXER_VOL_LI:
			ret = set_linein_gain(gain);
			break;
		case MIXER_VOL_HP:
			ret = set_hp_gain(gain);
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

/**
 * @brief   set channel source
 * @author
 * @date    2020-06-23 modify
 * @param   addr
 * @param   src
 * @retval  void
 */
static int ak_set_channel_source(int addr, int src)
{
	int pa_ctrl;
	int ret = 0;
	static int pre_mixer_out = MIXER_OUT_SEL_MAX;

	SND_PARAM *codec = g_ak_snd_card.param;

	ak_pcm_debug("IOC_SET_SOURCE, src=%d", src);
	ak_pcm_debug("addr=%d, src=%d", addr, src);

	switch (addr) {
		case MIXER_OUT:
			if (pre_mixer_out == src) {
				ak_pcm_func("MIXER_OUT already @%d", pre_mixer_out);
				return 0;
			}
			pre_mixer_out = src;
			/*
			 * 1. first close pa
			 * 2. set hp power
			 * 3. open pa
			 *
			 */
			if (codec->speaker_gpio >= 0) {
				gpio_direction_output(codec->speaker_gpio, !codec->speaker_en_level);
			}

			set_hp_power(ENABLE);

			if (codec->speaker_gpio >= 0) {
				gpio_direction_output(codec->speaker_gpio, codec->speaker_en_level);
			}

			set_hp_in(src);

			/* set the power according to the route status */
			if (MIXER_OUT_SEL_LINEIN & src)
				set_linein_power(ENABLE);

			if (MIXER_OUT_SEL_MIC & src)
				set_mic_power(ENABLE);
			break;
		case MIXER_IN:
			set_adc2_in(src);
			if (MIXER_IN_SEL_LINEIN & src)
				set_linein_power(ENABLE);
			if (MIXER_IN_SEL_MIC & src)
				set_mic_power(ENABLE);
			break;
		default:
			ak_pcm_debug("Invalid mixer addr!");
			return -EINVAL;
	}

	return ret;
}

/**
 * @brief   set speaker
 * @author
 * @date    2020-06-23 modify
 * @param   value
 * @return  void
 */
static int set_speaker(unsigned int value)
{
	int ret = 0;
	SND_PARAM *codec = g_ak_snd_card.param;
	static char save_hp_source;

	ak_pcm_debug("IOC_SET_SPEAKER, value=%d", value);

	if (codec->speaker_gpio >= 0) {
		if (value) {
			gpio_direction_output(codec->speaker_gpio, codec->speaker_en_level);
		} else {
			gpio_direction_output(codec->speaker_gpio, !codec->speaker_en_level);
		}
	}

	if (value) {
		if (get_hp_in() == MIXER_OUT_SEL_MUTE)
			set_hp_in(save_hp_source);
	} else {
		save_hp_source = get_hp_in();
		set_hp_in(MIXER_OUT_SEL_MUTE);
	}

	return ret;
}

/**
 * @brief   set dev power
 * @author
 * @date    2020-06-23 modify
 * @param   addr
 * @param   gain
 * @retval  true set successful
 * @retval  EINVAL set failed
 */
static int set_dev_power(int addr, bool bOn)
{
	int ret = 0;

	ak_pcm_debug("set analog power");

	switch (addr)
	{
		case DEV_POWER_MIC:
			ret = set_mic_power(bOn);
			break;
		case DEV_POWER_LINEIN:
			ret = set_linein_power(bOn);
			break;
		case DEV_POWER_HP:
			ret = set_hp_power(bOn);
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

/**
 * @brief   open a adc device
 * @author
 * @date    2020-06-13 modify
 * @return  int
 * @retval  true close successful
 * @retval  false close failed
 */
int adc_open(void)
{
	SND_PARAM *codec = g_ak_snd_card.param;

	ak_pcm_func("opening: ADC2 0x%x Analog 0x%x clkctrl 0x%x",
		__raw_readl(codec->adda_cfg_base + ADC2_CTRL_CONFIG_REG),
		__raw_readl(codec->sysctrl_base + ANALOG_AUDIO_CODEC_CFG2_REG),
		__raw_readl(codec->sysctrl_base + HIGHSPEED_CLOCK_CTRL_REG));

	adc_clk_enable(codec);

	return codec_adc_open(codec);
}

/**
 * @brief  close a adc device
 * @author
 * @date   2020-06-13 modify
 * @return int
 * @retval true  open successful
 * @retval false open failed
 */
int adc_close(void)
{
	SND_PARAM *codec = g_ak_snd_card.param;

	ak_pcm_func("closing...");

	return codec_adc_close(codec);
}

/**
 * @brief   Set sound sample rate, channel, bits per sample of the sound device
 * @author
 * @date    2020-06-23 modify
 * @param 	format     refer to snd_format
 * @return  unsigned long
 * @retval  actual_rate
 */
unsigned long adc_setformat(SND_FORMAT *format)
{
	SND_PARAM *codec = g_ak_snd_card.param;

	return adc_hw_setformat(codec, format->sample_rate);
}

/**
 * @brief  Set analog gain, channel source, pa, power of the sound device
 * @author
 * @date   2020-06-23
 * @param  type     refer to snd_hwparams_type
 * @param  addr     type of the addr
 * @param  param
 * @return int
 * @retval true set successful
 * @retval -EINVAL set failed
 */
int analog_sethwparam(enum snd_hwparams_type type, int addr, unsigned int param)
{
	int ret = 0;

	switch(type)
	{
		case SND_SET_GAIN:
			ak_pcm_func("SET_GAIN who %d gain %d", addr, param);
			ret = set_analog_gain(addr,param);
			break;
		case SND_SET_MUX:
			ak_pcm_func("SET_MUX mixer %d src %d", addr, param);
			ret = ak_set_channel_source(addr, param);
			break;
		case SND_SPEAKER_CTRL:
			ak_pcm_func("SPEAKER_CTRL %d", param);
			ret = set_speaker(param);
			break;
		case SND_SET_POWER:
			ak_pcm_func("SET_POWER who %d->%d", addr, param);
			ret = set_dev_power(addr, (bool)param);
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}
