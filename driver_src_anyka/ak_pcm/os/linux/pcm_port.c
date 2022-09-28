/*
 *  pcm for anyka chip
 *  Copyright (c) by Anyka, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include "pcm_port.h"

void adc_clk_enable(SND_PARAM *clk_param)
{
	clk_prepare_enable(clk_param->sdadc_gclk);
}

void dac_clk_enable(SND_PARAM *clk_param)
{
	clk_prepare_enable(clk_param->sddac_gclk);
}

void adc_clk_disable(SND_PARAM *clk_param)
{
	clk_disable_unprepare(clk_param->sdadc_gclk);
}

void dac_clk_disable(SND_PARAM *clk_param)
{
	clk_disable_unprepare(clk_param->sddac_gclk);
}

/**
 * @brief   set ADC2 sample rate
 * @author
 * @date	2020-06-23 modify
 * @param 	clk_param     refer to SND_PARAM
 * @param   samplerate:  desired rate to be set
 * @return  int
 */
unsigned long adc_set_samplerate(SND_PARAM *clk_param, unsigned long samplerate)
{
	unsigned long sr;

	if (!IS_ERR(clk_param->sdadc_clk))
		pcm_port_debug("get clk_sdadc success p_clk:%p", clk_param->sdadc_clk);
	else
		pcm_port_err("get clk_sdadc fail p_clk:%p", clk_param->sdadc_clk);

	if (clk_prepare_enable(clk_param->sdadc_clk)) {
		pcm_port_err("prepare & enable clk_sdadc fail p_clk:%p", clk_param->sdadc_clk);
		return -ENODEV;
	}
	clk_set_rate(clk_param->sdadc_clk, samplerate);
	sr = clk_get_rate(clk_param->sdadc_clk);
	pcm_port_func("sdadc_clk:%lu", sr);
	//clk_put(clk_param->sdadc_clk);

	return sr;
}

/**
 * @brief   set ADC2 highspeed
 * @author
 * @date	2020-06-23 modify
 * @param 	clk_param     refer to SND_PARAM
 * @param   samplerate:  desired rate to be set
 * @return  int
 */
int adc_set_highspeed(SND_PARAM *clk_param)
{
	unsigned long adchs_clk;
	unsigned long core_pll;

	if (!IS_ERR(clk_param->sdadchs_clk))
		pcm_port_debug("get adchs success p_clk:%p", clk_param->sdadchs_clk);
	else
		pcm_port_err("get adchs fail p_clk:%p", clk_param->sdadchs_clk);

	if (clk_prepare_enable(clk_param->sdadchs_clk)) {
		pcm_port_err("prepare & enable adchs fail p_clk:%p", clk_param->sdadchs_clk);
		return -ENODEV;
	}
	adchs_clk = 56000000;
	clk_set_rate(clk_param->sdadchs_clk, adchs_clk);
	adchs_clk = clk_get_rate(clk_param->sdadchs_clk);
	pcm_port_func("adchs_clk:%lu", adchs_clk);
	//clk_put(clk_param->adchs_clk);

	return 0;
}

/**
 * @brief      set DAC sample rate
 * @author
 * @date
 * @param[in]    samplerate:  desired rate to be set
 * @return  void
 */
unsigned long dac_set_samplerate(SND_PARAM *clk_param, unsigned long samplerate)
{
	unsigned long sr;

	if (!IS_ERR(clk_param->sddac_clk))
		pcm_port_debug("get clk_sddac success p_clk:%p", clk_param->sddac_clk);
	else
		pcm_port_err("get clk_sddac fail p_clk:%p", clk_param->sddac_clk);

	if (clk_prepare_enable(clk_param->sddac_clk)) {
		pcm_port_err("prepare & enable clk_sddac fail p_clk:%p", clk_param->sddac_clk);
		return -ENODEV;
	} else {
		pcm_port_debug("prepare & enable clk_sddac success p_clk:%p", clk_param->sddac_clk);
	}
	clk_set_rate(clk_param->sddac_clk, samplerate);
	sr = clk_get_rate(clk_param->sddac_clk);
	pcm_port_func("sddac_clk:%lu", sr);
	//clk_put(clk_param->sddac_clk);

	return sr;
}

unsigned long dac_cal_hsclk(unsigned long samplerate)
{
	u32 factor;
	unsigned int i;
	unsigned long  dachs_clk;
	const unsigned long rate_list[] = {8000, 16000, 24000, 48000, 96000};
	const unsigned long dac_hclk[sizeof(rate_list)/sizeof(rate_list[0])] = {6, 12, 18, 36, 72};

	for (i=0; i < sizeof(rate_list)/sizeof(rate_list[0]); ++i) {
		if (samplerate <= rate_list[i]) {
			break;
		}
	}
	if (sizeof(rate_list)/sizeof(rate_list[0]) == i) {
		dachs_clk = dac_hclk[i-1]*MHz;
	} else {
		dachs_clk = dac_hclk[i]*MHz;
	}

	pcm_port_func("@dac_cal_hsclk, dachs_clk = %lu",dachs_clk);

	return dachs_clk;
}
int dac_set_highspeed(SND_PARAM *clk_param, unsigned long samplerate)
{
	unsigned long dachs_clk;

	if (!IS_ERR(clk_param->sddachs_clk))
		pcm_port_debug("%s get dachssuccess p_clk:%p", __func__, clk_param->sddachs_clk);
	else
		pcm_port_err("%s get dachs fail p_clk:%p", __func__, clk_param->sddachs_clk);

	if (clk_prepare_enable(clk_param->sddachs_clk)) {
		pcm_port_err("%s prepare & enable dachs fail p_clk:%p", __func__, clk_param->sddachs_clk);
		return -ENODEV;
	}
	dachs_clk = dac_cal_hsclk(samplerate);
	pcm_port_func("set dachs_clk = %lu", dachs_clk);
	clk_set_rate(clk_param->sddachs_clk, dachs_clk);
	dachs_clk = clk_get_rate(clk_param->sddachs_clk);
	pcm_port_func("get dachs_clk:%lu end", dachs_clk);
	//clk_put(clk_param->dachs_clk);
	return 0;
}

unsigned long dac_hw_setformat(SND_PARAM *clk_param, unsigned long sample_rate)
{
	unsigned int sr;

	sr = dac_set_samplerate(clk_param, sample_rate);
	dac_set_highspeed(clk_param, sample_rate);

	return sr;
}

unsigned long adc_hw_setformat(SND_PARAM *clk_param, unsigned long sample_rate)
{
	unsigned int sr;

	sr = adc_set_samplerate(clk_param, sample_rate);
	adc_set_highspeed(clk_param);

	return sr;
}
