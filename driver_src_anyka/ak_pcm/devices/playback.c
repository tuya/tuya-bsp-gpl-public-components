/*
 *  pcm for anyka chip
 *  Copyright (c) by Anyka, Inc.
 *  Create by panqihe 2014-06-09
 *  Modifiy by chen_yanhong 2020-06-19
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
#include "os_common.h"
#include "playback.h"
#include "soundcard.h"
#include "loopback.h"

/*
 * FILL_PTHRESHOLD:		minimum bytes in buffer
 * WRITE_PTHRESHOLD:	minimum empty bytes space to write
 */
#define FILL_PTHRESHOLD(rt)		(rt->cfg.period_bytes)
#define WRITE_PTHRESHOLD(rt)	(FILL_PTHRESHOLD(rt) + rt->cfg.period_bytes)

#define DAC_FILL_MAX_TIME		(2)

/*
 * clear_playback_ptr_locked -
 * to empty playbakc buffer
 * notice will be try to hold ptr_lock in this function
 *
 * @rt:		pointer to runtime
 */
static inline void clear_playback_ptr_locked(struct akpcm_runtime *rt)
{
	unsigned long flags;
	spin_lock_irqsave(&rt->ptr_lock,flags);
	if (!is_runtime_stream(rt)) {
		rt->hw_ptr = 0;
		rt->app_ptr = 0;
	}
	//ak_pcm_debug("bd: 0x%x hw 0x%x app 0x%x",rt->boundary,rt->hw_ptr,rt->app_ptr);
	spin_unlock_irqrestore(&rt->ptr_lock,flags);
}

/*
 * update_playback_app_ptr_locked -
 * update playback application ptr when application had write in.
 * notice will be try to hold ptr_lock in this function
 *
 * @rt:				pointer to runtime
 * @offset:			offset will be add in app_ptr
 *
 */
static inline void update_playback_app_ptr_locked(struct akpcm_runtime *rt, unsigned int offset)
{
	unsigned long flags;

	spin_lock_irqsave(&rt->ptr_lock,flags);

	rt->app_ptr += offset;
	if (rt->app_ptr >= rt->boundary)  //rt->boundary是一个很大的数
		rt->app_ptr -= rt->boundary;

	spin_unlock_irqrestore(&rt->ptr_lock,flags);
}

/*
 * get_pend_bytes_locked -
 * playback helper routine, get pend bytes to play
 * get how many data sent to DAC in playback buffer
 * notice will be try to hold ptr_lock in this function
 *
 * @rt:			pointer to runtime
 * @g_hw_off:	pointer to update the last hw_offset to upper caller
 * @g_app_off:	pointer to update the last app_off to upper caller
 *
 */
static inline unsigned int get_pend_bytes_locked(struct akpcm_runtime *rt, unsigned int *g_hw_off, unsigned int *g_app_off)
{
	unsigned int pend_bytes;
	unsigned int hw_off;
	unsigned int app_off;
	unsigned long flags;

	spin_lock_irqsave(&rt->ptr_lock,flags);

	hw_off = rt->hw_ptr % rt->buffer_bytes; /* hw_off, offset of hw_ptr in one buffer_bytes  */
	app_off = rt->app_ptr % rt->buffer_bytes; /* app_off, offset of app_ptr in one buffer_bytes*/

	/* calculate pend_data & free_space */
	if (app_off >= hw_off) {
		pend_bytes = app_off - hw_off;
	} else {
		pend_bytes = rt->buffer_bytes - hw_off + app_off;
	}

	//ak_pcm_debug("pend_bytes:0x%x, buffer_bytes: 0x%x",pend_bytes, rt->buffer_bytes);
	//ak_pcm_debug("offset hw_off=%u, app_off=%u", hw_off, app_off);

	spin_unlock_irqrestore(&rt->ptr_lock,flags);

	if (g_hw_off)
		*g_hw_off = hw_off;

	if (g_app_off)
		*g_app_off = app_off;

	return pend_bytes;
}

/*
 * get_playback_free_bytes -
 * get how many free room in playback buffer
 *
 * @rt:			pointer to runtime
 * @g_hw_off:	pointer to update the last hw_offset to upper caller
 * @g_app_off:	pointer to update the last app_off to upper caller
 *
 */
static inline unsigned int get_playback_free_bytes(struct akpcm_runtime *rt, unsigned int *g_hw_off, unsigned int *g_app_off)
{
	return (rt->buffer_bytes - get_pend_bytes_locked(rt, g_hw_off, g_app_off));   /* free_bytes = buffer_bytes - use_bytes */
}

/*
 * get_user_data -
 * get playback data to here from user space
 *
 * @rt:			pointer to runtime
 * @buf:		pointer to user space buffer
 * @count:		bytes of buffer
 * @app_off:	pointer as the last app_offset
 */
static int get_user_data(struct akpcm_runtime *rt, char *buf,
                        size_t count, unsigned int *app_off)
{
	unsigned char *vaddr = rt->dma_area;
	unsigned int first_bytes;

	size_t copy_size = min(get_playback_free_bytes(rt, NULL, app_off), count);

	//ak_pcm_debug("app_off:%u, count=%u, copy_size=%u", *app_off, count ,copy_size);

	if (rt->buffer_bytes >= (*app_off + copy_size)) {
		memcpy(vaddr + *app_off, buf, copy_size);
	} else {
		/* first, copy	rt->buffer_bytes - app_off */
		first_bytes = rt->buffer_bytes - *app_off;
		memcpy(vaddr + *app_off, buf, first_bytes);
			/* then, copy count - first_bytes*/
		memcpy(vaddr , buf+ first_bytes, copy_size - first_bytes);
	}

	update_playback_app_ptr_locked(rt, copy_size);

	return 0;
}

/*
 * update_hw_ptr -
 * update hardware buffer pointer //TODO:need the check add lock or not
 *
 * @rt: pointer to runtime
 */
static inline void update_hw_ptr(struct akpcm_runtime *rt)
{
	unsigned int prd_bytes = rt->cfg.period_bytes;

	rt->hw_ptr += prd_bytes;

	if (rt->hw_ptr >= rt->boundary)
		rt->hw_ptr -= rt->boundary;
}

/*
 * update_playback_stream -
 * start next dma transfer to play
 *
 * @rt:		pointer to runtime
 */
static inline void update_playback_stream(struct akpcm_runtime *rt)
{
	unsigned char *vaddr = rt->dma_area;
	dma_addr_t phyaddr = rt->dma_addr;
	unsigned int prd_bytes = rt->cfg.period_bytes; //2K
	unsigned int buf_bytes = rt->buffer_bytes; // period(=16) * 2K
	unsigned int hw_off = 0;

	get_pend_bytes_locked(rt, &hw_off, NULL);

	dma_sync_single_for_device(rt->dev, (phyaddr + hw_off), prd_bytes, DMA_TO_DEVICE);
	/* next DMA */
	snd_card_dma_xfer(rt->snd_dev, phyaddr+hw_off, prd_bytes);
}

/*
 * playback_stop -
 * stop the playback
 *
 * @rt:		pointer to runtime
 */
static inline void playback_stop(struct akpcm_runtime *rt)
{
	while(snd_card_dma_status(rt->snd_dev));

	/* complete playback_pause */
	complete(&(rt->rt_completion));

	/* clear hw_ptr and app_ptr */
	clear_playback_ptr_locked(rt);
	clear_bit(STATUS_WORKING, &(rt->runflag));
}

/*
 * fill_data_to_da_unlocked -
 * fill empty data to da buffer.
 * then the playback runnging always.
 *
 * @rt:				pointer to runtime
 * @pend_bytes:		pend bytes size to play
 */
static inline unsigned int fill_data_to_da_unlocked(struct akpcm_runtime *rt,
		unsigned int pend_bytes)
{
	unsigned char *vaddr = rt->dma_area;
	unsigned int prd_bytes = rt->cfg.period_bytes;
	unsigned int hw_off = (rt->hw_ptr % rt->buffer_bytes);
	unsigned int fill_bytes = prd_bytes - pend_bytes;

	/* fill a period of user data with 0s */
	memset(vaddr + hw_off + pend_bytes, 0, fill_bytes);
	/*
	 * forword app ptr
	 * causion: take care not to conflict with playback_write
	 */
	rt->app_ptr = rt->hw_ptr + prd_bytes;
	if (rt->app_ptr >= rt->boundary)
		rt->app_ptr -= rt->boundary;

	return prd_bytes;
}

/*
 * playback_start_dac -
 * start the DAC for playback
 *
 * @rt:				pointer to runtime
 */
static void playback_start_dac(struct akpcm_runtime *rt)
{
	int dstsrc = 0;

	/* output to HP */
	dstsrc = rt->mixer_source;
	ak_pcm_info("dstsrc %d", dstsrc);
	snd_card_hwparams(rt->snd_dev, SND_SET_MUX, MIXER_OUT, dstsrc); /* set MIXER_OUT to dac */
	rt->mixer_source = dstsrc;
}

/*
 * playback_stop_hw
 * stop output process for timer_stop_output
 *
 * @rt:				pointer to runtime
 */
static void playback_stop_hw(struct akpcm_runtime *rt)
{
	if (!is_runtime_stream(rt)) {
		snd_card_close(rt->snd_dev);    /* close hw dac */
		ak_pcm_info("close hp channel");
		snd_card_hwparams(rt->snd_dev, SND_SET_MUX, MIXER_OUT, MIXER_OUT_SEL_MUTE);   /* disconnect MIXER_OUT */
		rt->mixer_source = MIXER_OUT_SEL_MUTE;
	}

	return;
}

/////////////////////////////////////////////////////
/*
 * playback_isr -
 * playback L2DMA ISR post-process
 *
 * @data:			private data
 */
void playback_isr(unsigned long data)
{
	struct akpcm_runtime *rt = (struct akpcm_runtime *)data;
	unsigned int prd_bytes = rt->cfg.period_bytes;
	unsigned int pend_bytes = 0;
	unsigned int hw_off = (rt->hw_ptr % rt->buffer_bytes);
	unsigned char *vaddr = rt->dma_area;
	static unsigned int prev_pend_bytes = 0;
	static unsigned int dac_fill_times = 0;
	static int log_printer = 0;
	unsigned int log_cycle = rt->cfg.rate * rt->cfg.sample_bits * 2/ (8 * prd_bytes);

	copy_to_loopback(rt->dma_area, hw_off, rt->actual_rate);

	//ak_pcm_debug("PB+ hw:%u, app:%u",rt->hw_ptr, rt->app_ptr);
	update_hw_ptr(rt);

	rt->remain_app_bytes = rt->remain_app_bytes - prd_bytes;

	/* calculate pend_data & elapse */
	pend_bytes = get_pend_bytes_locked(rt, NULL, NULL);

	if ((rt->log_print & AK_PCM_ISR_TRACE) || (!((log_printer++) % (log_cycle)))) {
		ak_pcm_debug("app 0x%x hw 0x%x pend 0x%x", rt->app_ptr,  rt->hw_ptr, pend_bytes);
	}

	/* user data in da buf is not enough */
	if (pend_bytes <= FILL_PTHRESHOLD(rt)) {
		if ((pend_bytes != prev_pend_bytes) && (!pend_bytes)) {
			ak_pcm_func("pending %d then reset dac_fill", pend_bytes);
			dac_fill_times = 0;
		}
		fill_data_to_da_unlocked(rt, pend_bytes);
	}

	/*
	* 1. Normally,star the DMA set STATUS_START_STREAM and STATUS_WORKING.
	* 3. When IOC_RSTBUF.clear STATUS_START_STREAM then set STATUS_PAUSING.
	*    Then update 2 more 0 frame data to DAC.Then clear STATUS_PAUSING.
	*    Next isr will stop the dma
	* 3. otherwise stop the DMA flow.
	*/
	if (is_runtime_stream(rt) && (pend_bytes <= rt->buffer_bytes)) {
		/* playback stream is running */
		//ak_pcm_debug("play_str");
		update_playback_stream(rt);
	} else if (!is_runtime_stream(rt) && is_runtime_pausing(rt) && !is_runtime_ready(rt) && ((pend_bytes)||((dac_fill_times++) < DAC_FILL_MAX_TIME))) {
		if ((!pend_bytes) && (dac_fill_times <= DAC_FILL_MAX_TIME)) {
			ak_pcm_func("closing with fill 0 @%d", dac_fill_times);
		}
		if (dac_fill_times == DAC_FILL_MAX_TIME) {
			clear_bit(STATUS_PAUSING, &(rt->runflag));
			dac_fill_times++;
		}
		update_playback_stream(rt);
	} else if (!is_runtime_stream(rt) && is_runtime_pausing(rt) && is_runtime_ready(rt)) {
		ak_pcm_debug("play_str fill 0");
		/* fill a period of user data with 0s */
		memset(vaddr + hw_off, 0, prd_bytes);

		clear_bit(STATUS_PAUSING, &(rt->runflag));
		update_playback_stream(rt);
	} else {
		/* playback stream is not running, stop it */
		playback_stop(rt);
		ak_pcm_debug("playback stopped");
		log_printer = 0;
	}

	prev_pend_bytes = pend_bytes;
	rt->last_isr_count++;

	wake_up_interruptible(&(rt->wq));
	wake_up_loopback();
	//ak_pcm_debug("PB- hw:%u, app:%u",rt->hw_ptr, rt->app_ptr);
}

/*
 * playback_prepare -
 * prepare to playback function include alloc the memory and args to buttom layer
 *
 * @pcm:		pointer to pcm device
 */
static int playback_prepare(struct akpcm_runtime *rt)
{
	unsigned char *ptr = rt->dma_area;
	unsigned int rt_rate = rt->cfg.rate;
	unsigned int new_size = (rt->cfg.periods * rt->cfg.period_bytes);
	unsigned int i;
	int ret = 0;
	dma_addr_t phyaddr;
	SND_FORMAT format;

	/* allocate memory for loop-buffer */
	if ((ptr) && (new_size != rt->buffer_bytes)) {
		dma_unmap_single(rt->dev, rt->dma_addr, rt->buffer_bytes, DMA_TO_DEVICE);
		kfree(ptr);
		rt->dma_area = ptr = NULL;

		if (rt->cache_buff) {
			vfree(rt->cache_buff);
			rt->cache_buff = NULL;
		}
	}

	/* alloc dma_addr and cache buffer */
	if (!ptr) {
		ptr = kmalloc(new_size, GFP_KERNEL|GFP_DMA);
		if (!ptr) {
			ak_pcm_err("ERR! hw buffer(0x%x) failed", new_size);
			return -ENOMEM;
		}

		phyaddr = dma_map_single(rt->dev, ptr, new_size, DMA_TO_DEVICE);
		if (dma_mapping_error(rt->dev, phyaddr)) {
			ak_pcm_err("ERR! dma map failed");
			ret = -EPERM;
			goto exit_playback_dma_prepare;
		}
		rt->dma_addr = phyaddr;
		rt->dma_area = ptr;
		ak_pcm_func("hw buffer: 0x%p/0x%x", ptr, new_size);

		/* reset some parameters */
		clear_playback_ptr_locked(rt);
		rt->buffer_bytes = new_size;
		for (i = 0; i < 64; i++) {
			if ((new_size>>i) == 0) {
				break;
			}
		}
		rt->boundary = new_size << (32-i);
		rt->cache_buff = vmalloc(new_size*2);//kmalloc(new_size*2, GFP_KERNEL);
		if (rt->cache_buff == NULL) {
			ak_pcm_err("ERR! cache buffer(0x%x) failed", (new_size*2));
			ret = -ENOMEM;
			goto exit_playback_cache_prepare;
		}
		memset(rt->cache_buff, 0, (new_size*2));
		ak_pcm_func("cache buffer: 0x%p/0x%x", rt->cache_buff, rt->boundary);
	}

	format.bps = rt->cfg.sample_bits;
	format.channel = rt->cfg.channels;
	format.sample_rate = rt->cfg.rate;
	rt->actual_rate = snd_card_set_format(rt->snd_dev, &format);
	ak_pcm_func("rate %d actual_rate %ld", rt_rate, rt->actual_rate);

	if (rt->actual_rate < 0)
		return -ENODEV;

	set_bit(STATUS_PREPARED, &(rt->runflag));

	return 0;

exit_playback_cache_prepare:
	dma_unmap_single(rt->dev, rt->dma_addr, rt->buffer_bytes, DMA_TO_DEVICE);
exit_playback_dma_prepare:
	if (ptr != NULL) {
		kfree(ptr);
		rt->dma_area = NULL;
	}

	return ret;
}

/*
 * playback_start_dma -
 * start to playback
 *
 * @rt:				pointer to runtime
 */
static void playback_start_dma(struct akpcm_runtime *rt)
{
	unsigned int prd_bytes = rt->cfg.period_bytes;
	unsigned int hw_off;
	dma_addr_t phyaddr = rt->dma_addr;

	/*no enough data then fill zero*/
	if (get_pend_bytes_locked(rt, &hw_off, NULL) < prd_bytes) {
		memset(rt->dma_area, 0, prd_bytes);
		rt->app_ptr += prd_bytes;
	}
	init_completion(&(rt->rt_completion));

	/* set bit should before call of snd_card_dma_xfer()*/
	set_bit(STATUS_START_STREAM, &(rt->runflag));
	set_bit(STATUS_WORKING, &(rt->runflag));

	dma_sync_single_for_device(rt->dev, phyaddr+hw_off, prd_bytes, DMA_TO_DEVICE);

	/* start dma transfer */
	snd_card_dma_xfer(rt->snd_dev, phyaddr+hw_off, prd_bytes);
	rt->remain_app_bytes = 0;
	ak_pcm_debug("hw 0x%x prd_bytes 0x%x", hw_off, prd_bytes);
}

/*
 * playback_resume -
 * resume playback
 *
 * @rt:				pointer to runtime
 */
static void playback_resume(struct akpcm_runtime *rt)
{
	/* check status */
	if (is_runtime_working(rt)) {
		return;
	}

	/* start playback dac */
	ak_pcm_info("start again");
	playback_start_dac(rt);
	/* start playback dma */
	playback_start_dma(rt);
	rt->last_transfer_count = 0;
	rt->last_isr_count = 0;
}


/*
 * playback_pause -
 * pause operation for playback
 *
 * @rt:				pointer to runtime
 */
int playback_pause(struct akpcm_runtime *rt)
{
	/* check status */
	if (!is_runtime_working(rt)) {
		ak_pcm_err("playback is not working now");
		return 0;
	}

	ak_pcm_func("trying...");

	/* here just clear playback stream running flag. */
	clear_bit(STATUS_START_STREAM, &(rt->runflag));
	set_bit(STATUS_PAUSING, &(rt->runflag));

	/* wait playback_isr finish */
	if (is_runtime_working(rt)) {
		wait_for_completion(&(rt->rt_completion));
		ak_pcm_info("wait playback completion ok");
	}

	/* clear rt->hw_ptr and rt->app_ptr */
	clear_playback_ptr_locked(rt);

	return 0;
}

/*
 * set_playback_param -
 * set player parameters from the user space
 *
 * @rt:				pointer to runtime
 * @arg:			arguments of player
 */
static int set_playback_param(struct akpcm_runtime *rt, unsigned long arg)
{
	struct akpcm_pars *rt_cfg = &rt->cfg;

	/* set rt->cfg from app layer */
	if (copy_from_user(rt_cfg, (void __user *)arg, sizeof(struct akpcm_pars))) {
		return -EFAULT;
	}

	ak_pcm_func("playback param prds %d prdbyte %d ch %d bit %d rate %d",
			rt_cfg->periods, rt_cfg->period_bytes, rt_cfg->channels,
			rt_cfg->sample_bits, rt_cfg->rate);

	return 0;
}

/*
 * get_playback_source -
 * get playback source to user space
 *
 * @rt:				pointer to runtime
 * @arg:			pointer to mixer_source send to upper layer
 */
static int get_playback_source(struct akpcm_runtime *rt, unsigned long arg)
{
	unsigned int value = rt->mixer_source;  /* get source value */

	return put_user(value, (int *)arg);
}

/*
 * set_playback_source -
 * set playback source
 *
 * @rt:				pointer to runtime
 * @arg:			arguments from player between MIXER_OUT_SEL_MUTE and MIXER_OUT_SEL_MAX
 */
static int set_playback_source(struct akpcm_runtime *rt, unsigned long arg)
{
	int ret = 0;
	unsigned int value = 0;

	ret = get_user(value, (int *)arg);
	if (ret)
		return ret;

	/* limit source value */
	if (value < MIXER_OUT_SEL_MUTE) {
		value = MIXER_OUT_SEL_MUTE;
	} else if (value > MIXER_OUT_SEL_MAX){
		value = MIXER_OUT_SEL_MAX;
	}

	if (rt->mixer_source != value) {
		ret = snd_card_hwparams(rt->snd_dev, SND_SET_MUX, MIXER_OUT, value); /* set hw source value */
		if (ret)
			return ret;

		ak_pcm_info("playback source:%d -> %d", rt->mixer_source, value);
		rt->mixer_source = value;
	}

	return ret;
}

/*
 * get_playback_gain -
 * get playback analog gain
 *
 * @rt:				pointer to runtime
 * @arg:			the pointer to store gain value from user space
 */
static int get_playback_gain(struct akpcm_runtime *rt, unsigned long arg)
{
	unsigned int value = 0;

	value = rt->hp_gain_cur;  /* get hp gain */

	return put_user(value, (int *)arg);
}

/*
 * set_playback_gain -
 * set playback analog gain
 *
 * @rt:				pointer to runtime
 * @arg:			the pointer which store the gain value from user sqace.
 */
static int set_playback_gain(struct akpcm_runtime *rt, unsigned long arg)
{
	int ret = 0;
	unsigned int value;

	ret = get_user(value, (int *)arg);
	if (ret)
		return -EINVAL;

	/* limit hp gain value */
	if (value < HEADPHONE_GAIN_MIN) {
		value = HEADPHONE_GAIN_MIN;
	} else if (value > HEADPHONE_GAIN_MAX) {
		value = HEADPHONE_GAIN_MAX;
	}

	ak_pcm_info("playback HP gain:%d", value);

	if (rt->hp_gain_cur != value) {
		rt->hp_gain_cur = value;   /* save hp gain */
		return snd_card_hwparams(rt->snd_dev, SND_SET_GAIN, MIXER_VOL_HP, value);   /* set hw hp gain */
	}

	return 0;
}

/*
 * init_playback_rt -
 * init playback rt in open
 *
 * @rt:				pointer to runtime
 */
static void init_playback_rt(struct akpcm_runtime *rt)
{
	spin_lock_init(&rt->ptr_lock);
	clear_playback_ptr_locked(rt);

	/* set rt->cfg */
	mutex_init(&rt->io_lock);
	rt->mixer_source = MIXER_OUT_SEL_MUTE;
	rt->remain_app_bytes = 0;

	rt->io_trace_in = 0;
	rt->io_trace_out = 0;

	clear_bit(STATUS_START_STREAM, &(rt->runflag));
	clear_bit(STATUS_WORKING, &(rt->runflag));
	clear_bit(STATUS_PAUSING, &(rt->runflag));
	clear_bit(STATUS_OPENED, &(rt->runflag));
	clear_bit(STATUS_PREPARED, &(rt->runflag));
}

/*
 * ak_get_status_value -
 * ak_get_status_value in IOC_GET_BUF_STATUS
 *
 * @rt:				pointer to runtime
 */
static unsigned long ak_get_status_value(struct akpcm_runtime *rt)
{
	long remain_app_bytes = 0;
	unsigned long value = 0;
	unsigned int period_bytes = rt->cfg.period_bytes;
	long temp = 0;
	
	remain_app_bytes = rt->remain_app_bytes;
	temp = 0 - period_bytes * DAC_FILL_MAX_TIME;

	if (remain_app_bytes > 0) {
		value = remain_app_bytes;       /* if remain_app_bytes > 0, the value is remain_app_bytes */
	} else if(remain_app_bytes > temp) {
		value = period_bytes;           /* when app_data finish, need to run more DAC_FILL_MAX_TIME isr */
	} else {
		value = 0;                      /* after app_data finish and run more DAC_FILL_MAX_TIME isr, notifiy application layer finish */
	}
	ak_pcm_debug("remain_app_bytes:%ld temp:%ld value:%ld\n",remain_app_bytes,temp,value);

	return value;
}

/*
 * playback_ioctl -
 * device file ops: ioctl
 *
 * @filp:		pointer to device file
 * @cmd:		command
 * @arg:		argument of command
 */
long playback_ioctl(struct akpcm_runtime *rt, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned long value = 0;
	struct akpcm_pars *rt_cfg;
	unsigned long long ull;

	mutex_lock(&rt->io_lock);
	if (!rt) {
		ak_pcm_err("play_rt is NULL");
		mutex_unlock(&rt->io_lock);
		return -EPERM;
	}

	/* commands */
	switch(cmd){
	case IOC_PREPARE:
		ak_pcm_debug("IOC_PREPARE");
		/* set param and allocate mem resource */
		ret = set_playback_param(rt, arg);
		if(!ret)
			ret = playback_prepare(rt);
		break;
	case IOC_RESUME:
		/* start dac and start dma transfer */
		ak_pcm_debug("IOC_RESUME");
		playback_resume(rt);
		break;
	case IOC_RSTBUF:
		/* stop dma transfer and clear ptr */
		ak_pcm_func("IOC_RSTBUF");
		playback_pause(rt);
		break;
	/* configures */
	case IOC_GET_PARS:
		/* get rt->cfg */
		ak_pcm_debug("IOC_GET_PARS");
		rt_cfg = &rt->cfg;
		if (copy_to_user((void __user *)arg, rt_cfg, sizeof(struct akpcm_pars))) {
			ret = -EFAULT;
		}
		break;
		/* ---------- sources ------------------------------------ */
	case IOC_GET_SOURCES:
		/* get MIXER_OUT source */
		ak_pcm_debug("IOC_GET_SOURCES");
		ret = get_playback_source(rt, arg);
		break;
	case IOC_SET_SOURCES:
		/* set MIXER_OUT source */
		ak_pcm_debug("IOC_SET_SOURCES");
		ret = set_playback_source(rt, arg);
		break;
	/* ---------- GAIN ------------------------------------ */
	case IOC_GET_GAIN:
		/* get hp gain */
		ak_pcm_debug("IOC_GET_GAIN");
		ret = get_playback_gain(rt, arg);
		break;
	case IOC_SET_GAIN:
		/* set hp gain */
		ret = set_playback_gain(rt, arg);
		break;
	case IOC_SET_SPEAKER:
		/* control PA */
		if (copy_from_user(&value, (void __user *)arg, sizeof(value))) {
			ret = -EFAULT;
			goto pb_ioc_end;
		}
		ak_pcm_debug("IOC_SET_SPEAKER, value=%lu", value);
		snd_card_hwparams(rt->snd_dev, SND_SPEAKER_CTRL, 0, value);
		break;
	case IOC_GETTIMER:
		ull = get_current_microsecond();
		if (copy_to_user((void __user *)arg, &ull, sizeof(unsigned long long))) {
			ret = -EFAULT;
		}
		break;
	case IOC_GET_BUF_STATUS:
		/* the status is remain app data */
		//value = rt->dac_user_data_playing;
		value = ak_get_status_value(rt);
		ret = put_user(value, (unsigned int *)arg);
		break;
	case IOC_GET_ACT_RATE:
		if (copy_to_user((void __user *)arg, &(rt->actual_rate), sizeof(unsigned long))) {
			ret = -EFAULT;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

pb_ioc_end:
	mutex_unlock(&rt->io_lock);
	return ret;
}

/*
 * playback_write -
 * device file ops: write
 *
 * @filp:		pointer to device file
 * @buf:		buffer to be writen
 * @count:		count to be writen
 * @f_pos:		the pointer of offset in the buffer
 */
ssize_t playback_write(struct akpcm_runtime *rt, const char __user *buf,
		size_t count, loff_t *f_pos, unsigned int f_flags)
{
	unsigned int pnd_bytes = 0, app_off = 0, hw_off = 0;
	static int log_printer = 0;
	unsigned int log_cycle;

	if (!is_runtime_ready(rt)) {
		return -EPERM;
	}

	if (get_playback_free_bytes(rt, &hw_off, &app_off) < (count + WRITE_PTHRESHOLD(rt))) {
		if (f_flags & O_NONBLOCK)
			return -EAGAIN;
	}

	rt->io_trace_in++;

	//ak_pcm_debug("%s:%d 0x%p count:%d rt->cfg.channels:%d\n",__func__,__LINE__,rt->cache_buff,count,rt->cfg.channels);
	if(!rt->cache_buff) {
		return -EPERM;
	}
	copy_from_user(rt->cache_buff,buf,count);  /* copy user data to cache buffer */

	/* convert to stereo data if app_data is mono data */
	if(rt->cfg.channels == 1) {
		count = count << 1;
		mono_to_stereo(rt->cache_buff, rt->cache_buff, count);
	}

	/* calculate pend_data & free_space */
	if (wait_event_interruptible(rt->wq,
		(get_playback_free_bytes(rt, &hw_off, &app_off) >= (count + WRITE_PTHRESHOLD(rt)))) < 0) {
		ak_pcm_debug("playback write wakeup by signal!");
		return -ERESTARTSYS;
	}

	/* copy data to L2 buffer */
	if (get_user_data(rt, rt->cache_buff, count, &app_off)) {
		return -EFAULT;
	}

	/* get app_off - hw_off value */
	pnd_bytes = get_pend_bytes_locked(rt,NULL, NULL);
	rt->remain_app_bytes = pnd_bytes;

	if(rt->cfg.channels == 1) {
		count = count >> 1;
	}

	log_cycle = rt->cfg.rate * rt->cfg.sample_bits * 2/ (8 * rt->cfg.period_bytes);
	if ((rt->log_print & AK_PCM_IO_TRACE) || (!((log_printer++) % (log_cycle)))) {
		ak_pcm_debug("OUT:app 0x%x hw 0x%x pnd_bytes 0x%x count 0x%x", rt->app_ptr, rt->hw_ptr, pnd_bytes, count);
	}

	rt->last_transfer_count += count;
	rt->io_trace_out++;

	return count;
}

/*
 * playback_open -
 * device file ops: open file
 *
 * @inode:		device node
 * @filp:		pointer to device file
 */
int playback_open(struct akpcm_runtime *rt)
{
	int ret = 0;

	ak_pcm_func("[%s@%d] runtime 0x%p",
		get_current()->comm ? get_current()->comm : "unknow", get_current()->pid, rt);

	/* don't permit to muilt open */
	if (is_runtime_opened(rt)) {
		ak_pcm_err("ERR! plackback device is already open!");
		return -EPERM; /* Operation not permitted */
	}

	ret = snd_card_prepare(rt->snd_dev);	/* create soundcard  */
	if (ret) {
		ak_pcm_err("ERR! Fail to prepare sndcard");
		return -ENOMEM;
	}

	ret = snd_card_open(rt->snd_dev);     /* ues soundcard to open dac */
	if (ret)
		goto pb_open_end;

	init_playback_rt(rt); /* init playback rt */

	/* set playback_isr */
	snd_card_dma_set_callback(rt->snd_dev, playback_isr, (unsigned long)rt);

	/* set default gain from dtb */
	snd_card_hwparams(rt->snd_dev, SND_SET_GAIN, MIXER_VOL_HP, rt->hp_gain_cur);

	/* set MIXER_OUT defalut value, link to MIXER_OUT_SEL_MUTE */
	snd_card_hwparams(rt->snd_dev, SND_SET_MUX, MIXER_OUT, MIXER_OUT_SEL_MUTE);
	rt->mixer_source = MIXER_OUT_SEL_MUTE;

	snd_card_hwparams(rt->snd_dev, SND_SPEAKER_CTRL, 0, 1);  /* open PA */

	set_bit(STATUS_OPENED, &(rt->runflag));

	return 0;

pb_open_end:
	return ret;
}

/*
 * playback_close -
 * device file ops: close file
 *
 * @inode:		device node
 * @filp:		pointer to device file
 */
int playback_close(struct akpcm_runtime *rt)
{
	ak_pcm_func("[%s@%d] runtime 0x%p",
		get_current()->comm ? get_current()->comm : "unknow", get_current()->pid, rt);

	if (!rt) {
		ak_pcm_err("rt is NULL");
		return -EPERM;
	}

	mutex_lock(&rt->io_lock);
	if (!is_runtime_opened(rt)) {
		ak_pcm_err("ERR! plackback device is not open");
		mutex_unlock(&rt->io_lock);
		return -EPERM;
	}
	clear_bit(STATUS_PREPARED, &(rt->runflag));

	snd_card_hwparams(rt->snd_dev, SND_SPEAKER_CTRL, 0, 0);  /* disable PA */
	playback_pause(rt);   /* stop l2 DMA transfer */
	playback_stop_hw(rt); /* stop dac transfer */
	//snd_card_hwparams(rt->snd_dev, SND_SPEAKER_CTRL, 0, 0);  /* disable PA */

	snd_card_release(rt->snd_dev);              /* destory soundcard */
	clear_bit(STATUS_OPENED, &(rt->runflag));
	mutex_unlock(&rt->io_lock);

	ak_pcm_func("successfully");

	return 0;
}
