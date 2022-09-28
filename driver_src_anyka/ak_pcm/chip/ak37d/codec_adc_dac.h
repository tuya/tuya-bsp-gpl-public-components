/*
 * @file codec_adc_dac.h
 * @brief sound operation interface
 * Copyright (C) 2010 Anyka (Guangzhou) Software Technology Co., LTD
 * @author	wushangsong
 * @date  2020-06-23
 * @version 1.0
 */
#ifndef __CODEC_ADC_DAC_H__
#define __CODEC_ADC_DAC_H__

#include "ak_pcm_defs.h"

#define DOUBLE_MIC_GAIN					1

#define ENABLE							1
#define DISABLE							0

//pAddress0800	 0x08000000-0x080000FF
#define CPUPLLCHN_CLOCK_CTRL_REG		0x0004
#define ASIC_CLOCK_DIV_REG				0x0008
#define PERI_CLOCK_DIV_REG_1			0x0014
#define PERI_CLOCK_DIV_REG_2			0x0018

#define SAR_ADC_CLK_CTRL_REG			0x000C
#define HIGHSPEED_CLOCK_CTRL_REG		0x0010
#define CLOCK_GATE_CTRL_REG				0x001c
#define SOFT_RST_CTRL_REG				0x0020
#define MULTIPLE_FUN_CTRL_REG1			0x0058
#define FADEOUT_CTRL_REG				0x0070

#define ADC1_CONF_REG1					0x0098

#define ANALOG_AUDIO_CODEC_CFG1_REG		0x009c
#define ANALOG_AUDIO_CODEC_CFG2_REG		0x00A0
#define ANALOG_AUDIO_CODEC_CFG3_REG		0x00A4

#define DAC_ADC_AUDDIO_CLK_REG			0x01BC

//pAddress2011 0x2011000-0x20118008
#define DAC_CTRL_CFG_REG				0x0000
#define DAC_CTRL_I2S_CFG_REG			0x0004
#define CPU_DATA_REG					0x0008

#define ADC2_CTRL_CONFIG_REG			0x8000
#define ADC2_DATA_REG					0x8004

//ADC2_CTRL_CONFIG_REG(0x20118000)
#define WORD_LENGTH_MASK				(0xF << 8)
#define ADC2_WORD_LENGTH(val)			(((val-1)&0xF) << 8)
#define I2S_EN							(1 << 4)
#define CH_POLARITY_SEL					(1 << 3)
#define HOST_RD_INT_EN					(1 << 2)
#define ADC2MODE_L2_EN					(1 << 1)
#define ADC2_CTRL_EN					(1 << 0)

//DAC_CTRL_CFG_REG(0x20110000)
#define ARM_INT							(1 << 3) //ARM interrupt enable
#define MUTE							(1 << 2) //repeat to sent the Last data to DAC
#define FORMAT							(1 << 4) //1 is used memeory saving format.
#define L2_EN							(1 << 1)
#define DAC_CTRL_EN						(1 << 0)

//DAC_CTRL_I2S_CFG_REG(0x20110004)
#define LR_CLK							(1 << 6)
#define POLARITY_SEL					(1 << 5)
#define I2S_CONFIG_WORDLENGTH_MASK		(0x1F << 0)

//SOFT_RST_CTRL_REG(0x08000020)
#define DAC_SOFT_RST					((1 << 26)|(1 << 28))
#define ADC2_SOFT_RST					((1 << 27)|(1 << 29))

//MULTIPLE_FUN_CTRL_REG1(0x08000058)
#define IN_DAAD_EN						(1 << 25) //ENABLE INTERNAL DAC ADC via i2s

/* FADEOUT_CTRL_REG, reg:(0x0080 0070)*/
#define OSR_MASK						(0x7 << 0)
#define OSR(value)						(((value)&0x7) << 0)
#define ADC2_OSR_BIT					31
#define DAC_FILTER_EN					(1 << 3)

/* ADC/DAC clock control, reg:(0x0800000C)*/
#define DAC_DIV_VLD						(1 << 29)
#define DAC_CLK_EN						(1 << 28)
#define MASK_DAC_DIV_FRAC				(0xFFF << 12)
#define DAC_DIV_FRAC(val)				(((val)&0xFFF) << 12)
#define MASK_DAC_DIV_INT				(0xFF << 4)
#define DAC_DIV_INT(val)				(((val)&0xFF) << 4)

/* ADC/DAC HIGHSPEED clock control, reg:(0x0800 0010) */
#define ADC2_HSDIV_VLD					(1 << 29)
#define ADC2_HCLK_EN					(1 << 28)
#define MASK_ADC2_HCLK_DIV				(0x3F << 20)
#define ADC2_HCLK_DIV(val)				(((val)&0x3F) << 20)
#define DAC_HSDIV_VLD					(1 << 19)
#define DAC_HCLK_EN						(1 << 18)
#define MASK_DAC_HCLK_DIV				(0xFF << 10)
#define DAC_HCLK_DIV(val)				(((val)&0xFF) << 10)
#define ADC2_DIV_VLD					(1 << 9)
#define ADC2_CLK_EN						(1 << 8)
#define MASK_ADC2_DIV					(0xFF << 0)
#define ADC2_DIV(val)					((val)&0xFF)

//ANALOG_AUDIO_CODEC_CFG1_REG(0x0080 009c)
#define DISCHG_HP						(29)
#define PD_HP_CTRL						(25)
#define PD2_HP							(1 << 24)
#define PD1_HP							(1 << 23)
#define MASK_HP_GAIN					(0x1F << 18)
#define HP_GAIN(val)					(((val)&0x1F) << 18)
#define HP_GAIN_OFF						18
#define PRE_EN							(1 << 17)
#define HP_IN							(14)
#define RST_DAC							(1 << 13)
#define PD_OP							(1 << 12)
#define PD_CK							(1 << 11)
#define MASK_DIS_CHG_VCM2				(0x1F << 6)
#define DIS_CHG_VCM2(val)				(((val)&0x1f) << 6)
#define EN_VP2V5						(1 << 5)
#define PON_VP							(1 << 4)
#define PL_VCM3							(1 << 2)
#define PD_VCM3							(1 << 1)
#define PD_BIAS							(1 << 0)

//ANALOG_AUDIO_CODEC_CFG2_REG(0x0080 00A0)
#define VREF_TEST_EN					(1 << 31)
#define ADC_IN_SEL						(29)
#define LIMEN							(1 << 28)
#define PD_S2D							(1 << 27)
#define PD_ADC2							(1 << 26)
#define LINE_GAIN						(22)
#define LINE_POWER						(21)
#define PD_LINE_R						(1 << 21)
#define PD_MICN							(1 << 20)
#define PD_MICP							(1 << 19)
#define MIC_GAINBST						(1 << 18)
#define MIC_GAIN						(15)
#define VREF_SEL						(1 << 14)
#define MASK_ANTIPOP_EN					(0xF << 10)
#define ANTIPOP_EN(val)					(((val)&0xF) << 10)
#define AN1_WK_EN						(1 << 9)
#define AN0_WK_EN						(1 << 8)
#define EN_BATDIV						(1 << 7)
#define AIN1_SEL						(1 << 6)
#define AIN0_SEL						(1 << 5)
#define BAT_SEL							(1 << 4)
#define PD_SARADCREF					(1 << 3)
#define SARADC_IBCTRL					(1 << 2)
#define SARADC_RESET					(1 << 1)
#define PD_SARADC						(1 << 0)

//ANALOG_AUDIO_CODEC_CFG3_REG(0x0080 00A4)
#define HP_VCM_BUF_POWER_ON				(1 << 22)

#define HP_IN_MUTE						0
#define HP_IN_MIC_OFFSET				0
#define HP_IN_LINEIN_OFFSET				1
#define HP_IN_DAC_OFFSET				2

#define ADC_IN_MUTE						0
#define ADC_IN_MIC_OFFSET				1
#define ADC_IN_LINEIN_OFFSET			2
#define ADC_IN_DAC_OFFSET				0

#endif //__CODEC_ADC_DAC_H__
