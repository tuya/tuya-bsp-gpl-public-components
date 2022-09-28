/*
 * pcm for anyka chip
 * Copyright (c) by Anyka, Inc.
 * Create by wushangsong 2020-06-20
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*
 * User Guide
 * ①. os driver layer MUST use snd_card_init to update the hardware resource for the sound card when probe.
 *    Then call snd_card_deinit when remove.
 * ②. when open the device(playback/capture..), please notice:
 *  **_open: use snd_card_prepare to prepare the software resource in sound card(like L2 buffer).
 *           Then use the snd_card_open to open the hardware.
 * ③. snd_card_dma_set_callback for the callback function when L2 finish.
 *    Before call snd_card_dma_xfer to transfer the data between host and controller. need the call callback first.
 * ④. snd_card_resume for operation when resume. such as clean the L2 buffer.Not compulsive.
 * ⑤. snd_card_hwparams and snd_card_set_format on params to hardware.
 * ⑥. snd_card_dma_status provide the way device(playback/capture..) to check the device L2 buffer.
 *
 */

#include "pcm_port.h"
#include "ak_pcm_defs.h"
#include "soundcard.h"

AK_SND_CARD g_ak_snd_card;

static struct snd_card_ops adc_function_ops =
{
	adc_open,
	adc_close,
	adc_setformat,
	analog_sethwparam,
};

static struct snd_controller adc_controller =
{
	.l2_buf_id = BUF_NULL,
	.ops = &adc_function_ops,
};

static struct snd_card_ops dac_function_ops =
{
	dac_open,
	dac_close,
	dac_setformat,
	analog_sethwparam,
};

static struct snd_controller dac_controller =
{
	.l2_buf_id = BUF_NULL,
	.ops = &dac_function_ops,
};

struct ak_snd_card g_ak_snd_card =
{
	.controllers[SND_CARD_DAC_PLAYBACK] = &dac_controller,
	.controllers[SND_CARD_ADC_CAPTURE] = &adc_controller,
	.controllers[SND_CARD_I2S_SEND] = NULL,
	.controllers[SND_CARD_I2S_RECV] = NULL,
	.controllers[SND_CARD_PDM_I2S_RECV] = NULL,
	.param = NULL,
};

/*
 * @brief give dma direction base on the device type
 * @param dev: the sound card device, refer to enum snd_card_type
 * @date 2020-09-15
 * @return l2_dma_transfer_direction_t the direction on this device
 */
static l2_dma_transfer_direction_t snd_card_data_direction(snd_dev dev)
{
	l2_dma_transfer_direction_t dirct = MEM2BUF;

	switch (dev) {
		case SND_CARD_DAC_PLAYBACK:
		case SND_CARD_I2S_SEND:
			dirct = MEM2BUF;
			break;
		case SND_CARD_ADC_CAPTURE:
		case SND_CARD_I2S_RECV:
			dirct = BUF2MEM;
			break;
		default:
			ak_pcm_err("ERR!Unsupport type!");
	}

	return dirct;
}

/*
 * @brief init the param for the sound card. mostly hardware resources.
 * @param param: hardware resource for the upper devices driver.
 * @date 2020-09-15
 * @return 0 if successfully, otherwise negative value.
 */
int snd_card_init(SND_PARAM *param)
{
	if (!param) {
		return -EINVAL;
	}

	g_ak_snd_card.param = param;

	return 0;
}

/*
 * @brief deinit the sound card.No availability ops on this function now.
 * @param NULL.
 * @date 2020-09-15
 * @return 0 if successfully, otherwise negative value.
 */
int snd_card_deinit(void)
{
	return 0;
}

/*
 * @brief prepare the hardware resource(like L2 buffer .etc) for the sound card
 * @param dev: the device of the sound card device
 * @date 2020-09-15
 * @return 0 if successfully, otherwise negative value.
 */
int snd_card_prepare(snd_dev dev)
{
	unsigned char l2_buf_id = BUF_NULL;

	if (dev >= SND_CARD_TYPE_MAX)
		return -EINVAL;

	switch (dev) {
		case SND_CARD_DAC_PLAYBACK:
		case SND_CARD_I2S_SEND:
			l2_buf_id = l2_alloc(ADDR_DAC);
			break;
		case SND_CARD_ADC_CAPTURE:
		case SND_CARD_I2S_RECV:
			l2_buf_id = l2_alloc(ADDR_ADC);
			break;
		default:
			ak_pcm_err("ERR!Unsupport type!");
	}

	if (l2_buf_id == BUF_NULL)
		return -EBUSY;

	g_ak_snd_card.controllers[dev]->l2_buf_id = l2_buf_id;

	ak_pcm_func("DEV %d -> L2: %d", dev, l2_buf_id);

	return 0;
}

/*
 * @brief release the hardware resource(like L2 buffer .etc) for the sound card
 * @param dev: the device of the sound card device
 * @date 2020-09-15
 * @return 0 if successfully, otherwise negative value.
 */
int snd_card_release(snd_dev dev)
{
	if (dev >= SND_CARD_TYPE_MAX)
		return -EINVAL;

	if (g_ak_snd_card.controllers[dev] == NULL)
		return -EINVAL;

	switch (dev) {
		case SND_CARD_DAC_PLAYBACK:
		case SND_CARD_I2S_SEND:
			l2_free((l2_device_t)ADDR_DAC);
			break;
		case SND_CARD_ADC_CAPTURE:
		case SND_CARD_I2S_RECV:
			l2_free((l2_device_t)ADDR_ADC);
			break;
		default:
			ak_pcm_err("ERR!Unsupport type!");
	}

	g_ak_snd_card.controllers[dev]->l2_buf_id = BUF_NULL;

	return 0;
}

/*
 * @brief open the specific sound card device
 * @param dev: the device of the sound card device
 * @date 2020-09-15
 * @return 0 if successfully, otherwise negative value.
 */
int snd_card_open(snd_dev dev)
{
	int ret = 0;

	if (dev >= SND_CARD_TYPE_MAX)
		return -EINVAL;

	if (g_ak_snd_card.controllers[dev] == NULL)
		return -EINVAL;

	ret = g_ak_snd_card.controllers[dev]->ops->open();
	if (ret)
		return ret;

	l2_clr_status(g_ak_snd_card.controllers[dev]->l2_buf_id);

	return ret;
}

/*
 * @brief close the specific sound card device
 * @param dev: the device of the sound card device
 * @date 2020-09-15
 * @return 0 if successfully, otherwise negative value.
 */
int snd_card_close(snd_dev dev)
{
	if (dev >= SND_CARD_TYPE_MAX)
		return -EINVAL;

	if (g_ak_snd_card.controllers[dev] == NULL)
		return -EINVAL;

	return g_ak_snd_card.controllers[dev]->ops->close();
}

/*
 * @brief resume the specific sound card device
 * @param dev: the device of the sound card device
 * @date 2020-09-15
 * @return 0 if successfully, otherwise negative value.
 */
int snd_card_resume(snd_dev dev)
{
	int ret = 0;

	if (dev >= SND_CARD_TYPE_MAX)
		return -EINVAL;

	if (g_ak_snd_card.controllers[dev] == NULL)
		return -EINVAL;

	if (g_ak_snd_card.controllers[dev]->l2_buf_id == BUF_NULL)
		return -EINVAL;

	l2_clr_status(g_ak_snd_card.controllers[dev]->l2_buf_id);
	ak_pcm_func("L2 status 0x%x", l2_get_status(g_ak_snd_card.controllers[dev]->l2_buf_id));

	return ret;
}

/*
 * @brief set analog gain, channel source, pa of the sound device
 * @param dev: the device of the sound card device
 * @param type: refer to snd_hwparams_type
 * @param addr: type of the addr
 * @param  param
 * @date 2020-09-15
 * @return 0 if successfully, otherwise negative value.
 */
int snd_card_hwparams(snd_dev dev, enum snd_hwparams_type type, int addr, unsigned int param)
{
	if (dev >= SND_CARD_TYPE_MAX)
		return -EINVAL;

	if (g_ak_snd_card.controllers[dev] == NULL)
		return -EINVAL;

	return g_ak_snd_card.controllers[dev]->ops->hwparam(type, addr, param);
}

/*
 * @brief Set sound sample rate, channel, bits per sample of the sound device
 * @param dev: the device of the sound card device
 * @param format: refer to SND_FORMAT
 * @date 2020-09-15
 * @return the actural rate for base on the format
 */
unsigned long snd_card_set_format(snd_dev dev, SND_FORMAT *format)
{
	if (dev >= SND_CARD_TYPE_MAX)
		return -EINVAL;

	if (g_ak_snd_card.controllers[dev] == NULL)
		return -EINVAL;

	return g_ak_snd_card.controllers[dev]->ops->set_format(format);
}

/*
 * @brief start data tranferring between memory and l2 buffer with dma mode
 * @param dev: the device of the sound card device
 * @param ram_addr: the memory address
 * @param length: the size of data to be transfered
 * @date 2020-09-15
 * @return 0 if successfully, otherwise negative value.
 */
int snd_card_dma_xfer(snd_dev dev, unsigned long ram_addr, int length)
{
	l2_dma_transfer_direction_t dirct;

	if ((dev >= SND_CARD_TYPE_MAX) || (length < 0))
		return -EINVAL;

	if (g_ak_snd_card.controllers[dev] == NULL)
		return -EINVAL;

	dirct = snd_card_data_direction(dev);

	l2_combuf_dma(ram_addr, g_ak_snd_card.controllers[dev]->l2_buf_id, length, dirct, 1);

	return 0;
}

/*
 * @brief set dma callback function
 * @param dev: the device of the sound card device
 * @param func: DMA finish call this function
 * @param data: arg to the function when callback
 * @date 2020-09-15
 * @return 0 if successfully, otherwise negative value.
 */
int snd_card_dma_set_callback(snd_dev dev, void (*func)(unsigned long), unsigned long data)
{
	if ((dev >= SND_CARD_TYPE_MAX) || (!func))
		return -EINVAL;

	if (g_ak_snd_card.controllers[dev] == NULL)
		return -EINVAL;

	if (g_ak_snd_card.controllers[dev]->l2_buf_id == BUF_NULL)
		return -EINVAL;

	return l2_set_dma_callback(g_ak_snd_card.controllers[dev]->l2_buf_id, func, (unsigned long)data);
}

/*
 * @brief get dma status/assert l2_buf_id has been prepared
 * @param dev: the device of the sound card device
 * @date 2020-09-15
 * @return the l2 status.
 */
int snd_card_dma_status(snd_dev dev)
{
	return l2_get_status(g_ak_snd_card.controllers[dev]->l2_buf_id);
}
