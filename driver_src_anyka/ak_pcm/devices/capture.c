/*
 *  pcm capture for anyka chip
 *  Copyright (c) by Anyka, Inc.
 *  Create by huang_haitao 2018-01-31
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
#include "capture.h"
#include "soundcard.h"
#include "ak_pcm_common.h"

/*
 * FULL_THRESTHOLD -
 * full state for one period buffer
 */
#define FULL_THRESTHOLD(rt)		(rt->cfg.period_bytes)

/*
 * clear_capture_ptr_locked -
 * clear all cache for capturer
 * notice need be hold io_lock before call this function.
 *
 * @rt:			pointer to runtime
 */
static void clear_capture_ptr_locked(struct akpcm_runtime *rt)
{
	unsigned long flags;
	spin_lock_irqsave(&rt->ptr_lock,flags);
	/*all ptr is 0 means cache for capture is empty*/
	rt->hw_ptr = 0;
	rt->app_ptr = 0;
	spin_unlock_irqrestore(&rt->ptr_lock,flags);
}

/*
 * update_capture_app_ptr_locked -
 * update capture application ptr when application had read out.
 * notice need be hold io_lock before call this function.
 *
 * @rt:				pointer to runtime
 * @cur_app_pos:	new app ptr
 */
static void update_capture_app_ptr_locked(struct akpcm_runtime *rt, unsigned int offset)
{
	unsigned long flags;
	spin_lock_irqsave(&rt->ptr_lock,flags);

	/* update app_ptr */
	rt->app_ptr += offset;
	if (rt->app_ptr >= rt->boundary)
		rt->app_ptr -= rt->boundary;

	spin_unlock_irqrestore(&rt->ptr_lock,flags);
}

/*
 * get_valid_bytes_locked -
 * capture helper routine
 * get how many data ready to upper layer in capture buffer
 * notice will be try to hold ptr_lock in this function
 *
 * @rt:			pointer to runtime
 */
static unsigned int get_valid_bytes_locked(struct akpcm_runtime *rt, unsigned int *g_hw_off, unsigned int *g_app_off)
{
	unsigned int valid_bytes, hw_off, app_off;
	unsigned long flags;

	spin_lock_irqsave(&rt->ptr_lock, flags);

	hw_off = rt->hw_ptr % rt->buffer_bytes;		/* hw_off, offset of hw_ptr in one buffer_bytes  */
	app_off = rt->app_ptr % rt->buffer_bytes;	/* app_off, offset of app_ptr in one buffer_bytes*/

	/* calculate pend_data & free_space */
	if (hw_off >= app_off) {
		valid_bytes = hw_off - app_off;
	} else {
		valid_bytes = rt->buffer_bytes - app_off + hw_off;
	}

	//ak_pcm_debug("valid_bytes:%u, app_off:%u, hw_ptr:%u",
			//valid_bytes, app_off, rt->hw_ptr);

	spin_unlock_irqrestore(&rt->ptr_lock,flags);

	if (g_hw_off)
		*g_hw_off = hw_off;

	if (g_app_off)
		*g_app_off = app_off;

	return valid_bytes;
}

/*
 * get_capture_free_bytes -
 * capture helper routine
 * get how many free room in capture buffer
 *
 * @rt:			pointer to runtime
 */
static unsigned int get_capture_free_bytes(struct akpcm_runtime *rt)
{
	return (rt->buffer_bytes - get_valid_bytes_locked(rt, NULL, NULL));  /* free_bytes = buffer_bytes - valid_bytes */
}

/*
 * put_user_data -
 * put capture data from dma buffer to user space
 *
 * @rt:			pointer to runtime
 * @buf:		pointer to user space buffer
 * @count:		bytes of buffer
 */
static int put_user_data(struct akpcm_runtime *rt,
		char __user *buf, size_t count)
{
	/* app_off, offset of app_ptr in one buffer_bytes*/
	unsigned int app_off;
	unsigned int hw_off;
	size_t copy_size;
	unsigned char *vaddr = rt->dma_area;

	/* modify under protection */
	copy_size = min(get_valid_bytes_locked(rt,&hw_off, &app_off), count);
	if (rt->buffer_bytes >= (app_off + copy_size)) {
		if (copy_to_user(buf, vaddr+app_off, copy_size)) {
			return -EFAULT;
		}
	} else {
		/* first, copy  rt->buffer_bytes - app_off */
		unsigned int first_bytes = rt->buffer_bytes - app_off;
		if (copy_to_user(buf, vaddr + app_off, first_bytes)) {
			return -EFAULT;
		}
		/* then, copy count - first_bytes*/
		if (copy_to_user(buf+ first_bytes, vaddr, copy_size - first_bytes)) {
			return -EFAULT;
		}
	}

	update_capture_app_ptr_locked(rt, copy_size);

	return 0;
}

/*
 * start_adc -
 * start the ADC for capture function
 *
 * @pcm:		pointer to pcm device
 */
static void start_adc(struct akpcm_runtime *rt)
{
	unsigned int rt_channels = rt->cfg.channels;

	switch (rt->mixer_source) {
		case MIXER_IN_SEL_MIC:
			snd_card_hwparams(rt->snd_dev, SND_SET_MUX, MIXER_IN, MIXER_IN_SEL_MIC); /* set MIXER_IN source value link to MIXER_IN_SEL_MIC */
			rt->mixer_source = MIXER_IN_SEL_MIC;
			break;
		case MIXER_IN_SEL_LINEIN:
			snd_card_hwparams(rt->snd_dev, SND_SET_MUX, MIXER_IN, MIXER_IN_SEL_LINEIN); /* set MIXER_IN source value link to MIXER_IN_SEL_LINEIN */
			rt->mixer_source = MIXER_IN_SEL_LINEIN;
			break;
		default:
			ak_pcm_err("unknow mixer souce %d", rt->mixer_source);
			break;
	}

	snd_card_resume(rt->snd_dev);
}

/*
 * set_capture_timestamp -
 * set timestamp for capture data
 *
 * @rt:					runtime
 * @prd_bytes:			bytes of one transfer dma
 * @hw_off:				hardware pointer to the cache
 */
static void set_capture_timestamp(struct akpcm_runtime *rt,
                unsigned int prd_bytes, unsigned int hw_off)
{
	int index = hw_off / prd_bytes;
	unsigned long long *timestamp = rt->timestamp;

	/*
	 * the way of getting timestamp is same as video driver,
	 * then its more easy to do synchronization between audio and video.
	 */
	*(timestamp + index) = get_current_microsecond();

	if ((rt->log_print & AK_PCM_TIMESTAMP_TRACE) || (index == 0))
		ak_pcm_debug("ts[%d]:H %lu L %lu", index, (unsigned long)((*(timestamp + index)>>32)&0xFFFFFFFF),
			(unsigned long)((*(timestamp + index))&0xFFFFFFFF));
}

/*
 * update_hw_ptr -
 * update hardware pointer for the dma buffer
 *
 * @rt:				pointer to runtime
 */
static void update_hw_ptr(struct akpcm_runtime *rt)
{
	unsigned int prd_bytes = rt->cfg.period_bytes;

	rt->hw_ptr += prd_bytes;
	if (rt->hw_ptr >= rt->boundary)
		rt->hw_ptr -= rt->boundary;
}

/*
 * handle_adc_buffer_full_locked -
 * process the capture buffer is full
 * notice will be try to hold the ptr_lock in this function
 *
 * @pcm:				pointer to pcm devic3
 * @free_bytes:			current free capture bytes
 */
static void handle_adc_buffer_full_locked(struct akpcm_runtime *rt, unsigned int free_bytes)
{
	unsigned long flags;
	unsigned int prd_bytes = rt->cfg.period_bytes;

	spin_lock_irqsave(&rt->ptr_lock, flags);

	rt->app_ptr = rt->hw_ptr - (rt->buffer_bytes - FULL_THRESTHOLD(rt) - prd_bytes);

	if (rt->app_ptr >= rt->boundary)
		rt->app_ptr -= rt->boundary;

	spin_unlock_irqrestore(&rt->ptr_lock,flags);
}

/*
 * capture_isr -
 * DMA transfer isr for capture
 *
 * @data:		private data
 */
void capture_isr(unsigned long data)
{
	struct akpcm_runtime *rt = (struct akpcm_runtime *)data;
	unsigned int prd_bytes = rt->cfg.period_bytes;
	unsigned int hw_off = 0;
	dma_addr_t phyaddr = rt->dma_addr;
	int wakeup_wait_queue = 0;
	unsigned int free_bytes = 0;
	static int log_printer = 0;
	unsigned int log_cycle = rt->cfg.rate * rt->cfg.sample_bits / (8 * prd_bytes);

	if ((rt->log_print & AK_PCM_ISR_TRACE) || (!((log_printer++) % (log_cycle)))) {
		ak_pcm_debug("app 0x%x hw 0x%x free 0x%x", rt->app_ptr, rt->hw_ptr, get_capture_free_bytes(rt));
	}

	/* the previous dma has finished, set timestamp*/
	hw_off = rt->hw_ptr % rt->buffer_bytes;
	set_capture_timestamp(rt, prd_bytes, hw_off);

	dma_sync_single_for_cpu(rt->dev, phyaddr+hw_off, prd_bytes, DMA_FROM_DEVICE);
	update_hw_ptr(rt);
	hw_off = rt->hw_ptr % rt->buffer_bytes;

	if (is_runtime_stream(rt)) {
		/* capture stream is running */
		if (get_valid_bytes_locked(rt, NULL, NULL) >= rt->notify_threshold) {
			wakeup_wait_queue = 1;
		}
		free_bytes = get_capture_free_bytes(rt);
		/* adc buf is full */
		if (free_bytes <= FULL_THRESTHOLD(rt)) {
			handle_adc_buffer_full_locked(rt, free_bytes);
			wakeup_wait_queue = 1;
		}

		dma_sync_single_for_device(rt->dev, phyaddr+hw_off, prd_bytes, DMA_FROM_DEVICE);
		snd_card_dma_xfer(rt->snd_dev, phyaddr+hw_off, prd_bytes);
	} else {
		clear_bit(STATUS_WORKING, &(rt->runflag));
		complete(&(rt->rt_completion));
		wakeup_wait_queue = 1;
		ak_pcm_func("completion capture");
		log_printer = 0;
	}

	if (wakeup_wait_queue)
		wake_up_interruptible(&(rt->wq));

	rt->last_isr_count++;

	//ak_pcm_debug("CP- ptr, hw:%u, app:%u",
		//rt->hw_ptr, rt->app_ptr);
}

/*
 * capture_prepare -
 * prepare for capture(open ADC2, power on MIC).
 *
 * @pcm:		pointer to pcm device
 */
static int capture_prepare(struct akpcm_runtime *rt)
{
	unsigned char *ptr = rt->dma_area;
	unsigned int new_size = (rt->cfg.periods * rt->cfg.period_bytes);
	int i;
	int ret = 0;

	/* allocate memory for loop-buffer */
	if ((ptr) && (new_size != rt->buffer_bytes)) {
		capture_pause(rt);
		dma_unmap_single(rt->dev, rt->dma_addr, rt->buffer_bytes, DMA_FROM_DEVICE);
		kfree(ptr);
		rt->dma_area = ptr = NULL;
	}

	/* alloc dma_addr */
	if (!ptr) {
		ptr = kmalloc(new_size, GFP_KERNEL|GFP_DMA);
		if (!ptr) {
			ak_pcm_err("ERR! hw buffer(0x%x) failed", new_size);
			return -ENOMEM;
		} else {
			dma_addr_t phyaddr;
			phyaddr = dma_map_single(rt->dev, ptr, new_size, DMA_FROM_DEVICE);
			if (dma_mapping_error(rt->dev, phyaddr)) {
				ak_pcm_err("ERR! dma map failed");
				ret = -EPERM;
				goto exit_capture_dma_prepare;
			}
			memset(ptr, 0, new_size);
			rt->dma_addr = phyaddr;
			rt->dma_area = ptr;
			ak_pcm_func("hw buffer: 0x%p/0x%x", ptr, new_size);
		}

		/* reset some parameters */
		clear_capture_ptr_locked(rt);
		rt->buffer_bytes = new_size;
		for(i = 0; i < 64; i++){
			if ((new_size>>i) == 0) {
				break;
			}
		}
		rt->boundary = new_size << (32-i);
		//ak_pcm_debug("capture boundary: 0x%x", rt->boundary);
	}

	set_bit(STATUS_PREPARED, &(rt->runflag));

	ak_pcm_func("successfully");

	return 0;

exit_capture_dma_prepare:
	if (ptr != NULL)
		kfree(ptr);

	return ret;
}

/*
 * capture_start -
 * start operation for capture
 *
 * @pcm:		pointer to pcm device
 */
void capture_start(struct akpcm_runtime *rt)
{
	unsigned int prd_bytes = rt->cfg.period_bytes;
	dma_addr_t phyaddr = rt->dma_addr;

	ak_pcm_func("starting");
	clear_capture_ptr_locked(rt);
	start_adc(rt);
	set_bit(STATUS_START_STREAM, &(rt->runflag)); //set stream running flag
	set_bit(STATUS_WORKING, &(rt->runflag)); //set DMA running flag
	dma_sync_single_for_device(rt->dev, phyaddr, prd_bytes, DMA_FROM_DEVICE);
	snd_card_dma_xfer(rt->snd_dev, phyaddr, prd_bytes);
}

/*
 * capture_resume -
 * resume operation for capture
 *
 * @pcm:		pointer to pcm device
 */
static void capture_resume(struct akpcm_runtime *rt)
{
	if (is_runtime_working(rt))
		return;

	ak_pcm_func("resume...");

	capture_start(rt);
	rt->last_transfer_count = 0;
	rt->last_isr_count = 0;
}

/*
 * capture_pause -
 * pause operation for capture
 *
 * @pcm:		pointer to pcm device
 */
void capture_pause(struct akpcm_runtime *rt)
{
	/* working until start capture DMA */
	if (!is_runtime_working(rt)) {
		ak_pcm_debug("capture is not working now");
		return;
	}

	ak_pcm_func("pausing");

	/* clear capture stream running flag. STOP when current DMA finish */
	clear_bit(STATUS_START_STREAM, &(rt->runflag));
	if (is_runtime_working(rt)) {
		wait_for_completion(&(rt->rt_completion));
		ak_pcm_info("wait capture completion ok");
	}

}

/*
 * set_capture_gain -
 * set analog gain
 *
 * @pcm:		pointer to pcm device
 * @arg:		argument for gain
 */
static int set_capture_gain(struct akpcm_runtime *rt, unsigned long arg)
{
	unsigned int addr;
	unsigned int value;
	int ret = 0;

	/* check set MIC gain or LINE gain */
	if (rt->mixer_source == MIXER_IN_SEL_MIC) {
		addr = MIXER_VOL_MIC;
		ret = get_user(value, (int *)arg);
		if (ret)
			goto set_gain_end;

		if (value < MIC_GAIN_MIN) {
			value = MIC_GAIN_MIN;
		} else if (value > MIC_GAIN_MAX) {
			value = MIC_GAIN_MAX;
		}
		ak_pcm_debug("IOC_SET_GAIN, set MIC gain: %d", value);

		if (rt->mic_gain_cur != value) {
			snd_card_hwparams(rt->snd_dev, SND_SET_GAIN, addr, value);  /* set hw MIC gain */
		}
		rt->mic_gain_cur = value;
	} else {
		addr = MIXER_VOL_LI;
		ret = get_user(value, (int *)arg);
		if (ret)
			goto set_gain_end;

		if (value < LINEIN_GAIN_MIN) {
			value = LINEIN_GAIN_MIN;
		} else if (value > LINEIN_GAIN_MAX) {
			value = LINEIN_GAIN_MAX;
		}

		ak_pcm_debug("IOC_SET_GAIN, set LI gain: %d", value);

		if (rt->linein_gain_cur != value) {
			snd_card_hwparams(rt->snd_dev, SND_SET_GAIN, addr, value);  /* set hw LINEIN gain */
		}
		rt->linein_gain_cur = value;
	}

set_gain_end:
	return ret;
}

/*
 * set_capture_param -
 * set parameters
 *
 * @pcm:		pointer to pcm device
 * @arg:		argument for parameters
 */
static int set_capture_param(struct akpcm_runtime *rt, unsigned long arg)
{
	struct akpcm_pars *rt_cfg = &rt->cfg;
	int ret = 0;
	SND_FORMAT format;

	if (copy_from_user(rt_cfg, (void __user *)arg, sizeof(struct akpcm_pars))) {
		ret = -EFAULT;
		goto set_param_end;
	}
	if (rt_cfg->periods > MAX_TIMESTAMP_CNT) {
		ak_pcm_err("periods too large");
		ret = -EINVAL;
		goto set_param_end;
	}

	if (rt->cfg.period_bytes < 512)
		rt->cfg.period_bytes = 512;

	format.bps = rt->cfg.sample_bits;
	format.channel = rt->cfg.channels;
	format.sample_rate = rt->cfg.rate;
	rt->actual_rate = snd_card_set_format(rt->snd_dev, &format); /* set hw sample rate */
	msleep(60);

	ak_pcm_func("CH %d PERD %d BYTE %d Total 0x%x bit %d actual_rate %ld",
			rt_cfg->channels, rt_cfg->periods, rt_cfg->period_bytes, rt->buffer_bytes,
			rt_cfg->sample_bits, rt->actual_rate);

set_param_end:
	return ret;
}

/*
 * set_capture_source -
 * set source in capture
 * notice the arg will be MIXER_IN_SEL_MUTE to MIXER_IN_SEL_MAX
 *
 * @pcm:		pointer to pcm device
 * @arg:		argument for source
 */
static int set_capture_source(struct akpcm_runtime *rt, unsigned long arg)
{
	unsigned int value = 0;
	int ret = get_user(value, (int *)arg);

	if (ret)
		return ret;

	ak_pcm_debug("IOC_SET_SOURCES value=%d", value);

	/* limit source value */
	if (value < MIXER_IN_SEL_MUTE) {
		value = MIXER_IN_SEL_MUTE;
	} else if (value > MIXER_IN_SEL_MAX){
		value = MIXER_IN_SEL_MAX;
	}

	if (rt->mixer_source != value) {
		ak_pcm_debug("source=%d, src=%x", rt->mixer_source, value);
		snd_card_hwparams(rt->snd_dev, SND_SET_MUX, MIXER_IN, value);  /* set hw source value */
		rt->mixer_source = value;
	}

	return 0;
}

/*
 * init_capture_rt -
 * init capture rt in open
 *
 * @rt:		pointer to akpcm_runtime
 */
static void init_capture_rt(struct akpcm_runtime *rt)
{
	spin_lock_init(&rt->ptr_lock);
	clear_capture_ptr_locked(rt);
	mutex_init(&rt->io_lock);
	init_completion(&(rt->rt_completion));

	rt->io_trace_in = 0;
	rt->io_trace_out = 0;

	clear_bit(STATUS_START_STREAM, &(rt->runflag));
	clear_bit(STATUS_WORKING, &(rt->runflag));
	clear_bit(STATUS_PAUSING, &(rt->runflag));
	clear_bit(STATUS_OPENED, &(rt->runflag));
	clear_bit(STATUS_PREPARED, &(rt->runflag));
}

/*
 * capture_ioctl -
 * capture device file ops: ioctl
 *
 * @flip:			capture device file
 * @cmd:			command
 * @arg;			argument for the command
 */
long capture_ioctl(struct akpcm_runtime *rt, unsigned int cmd, unsigned long arg)
{
	struct akpcm_pars *rt_cfg;
	unsigned int value;
	int ret = 0;
	unsigned long long ull;

	mutex_lock(&rt->io_lock);

	/* commands */
	switch (cmd) {
		case IOC_PREPARE:
			/* set param and allocate mem resource */
			ak_pcm_debug("IOC_PREPARE");
			ret = set_capture_param(rt, arg);
			if(!ret)
				ret = capture_prepare(rt);
			break;
		case IOC_RESUME:
			/* start adc and start dma transfer */
			ak_pcm_debug("IOC_RESUME");
			capture_resume(rt);
			break;
		case IOC_RSTBUF:
			/* stop dma transfer and clear ptr */
			ak_pcm_debug("IOC_RSTBUF, reset buf");
			capture_pause(rt);
			/* reset buffer */
			clear_capture_ptr_locked(rt);
			break;
			/* configures */
		case IOC_GET_PARS:
			/* get rt->cfg */
			ak_pcm_debug("IOC_GET_PARS");
			rt_cfg = &rt->cfg;
			if (copy_to_user((void __user *)arg, rt_cfg,
					sizeof(struct akpcm_pars))) {
				ret = -EFAULT;
			}
			break;
			/* ---------- sources ------------------------------------ */
		case IOC_GET_SOURCES:
			/* get MIXER_IN source */
			value = rt->mixer_source;
			ret = put_user(value, (int *)arg);
			break;
		case IOC_SET_SOURCES:
			/* set MIXER_IN source */
			ret = set_capture_source(rt, arg);
			break;
			/* ---------- GAIN ------------------------------------ */
		case IOC_GET_GAIN:
			/* get gain */
			if (rt->mixer_source == MIXER_IN_SEL_MIC) {
				value = rt->mic_gain_cur;
			} else {
				value = rt->linein_gain_cur;
			}
			ret = put_user(value, (int *)arg);
			break;
		case IOC_SET_GAIN:
			/* set gain */
			set_capture_gain(rt, arg);
			break;
		case IOC_GETTIMER:
			/* the rt->ts is set in capture_read */
			ull = rt->ts;
			if (rt->log_print & AK_PCM_TIMESTAMP_TRACE) {
				ak_pcm_debug("ts:H %lu L %lu", (unsigned long)((ull>>32)&0xFFFFFFFF),
					(unsigned long)((ull)&0xFFFFFFFF));
			}
			if (copy_to_user((void __user *)arg, &ull, sizeof(unsigned long long))) {
				ret = -EFAULT;
			}
			break;
		case IOC_GET_BUF_STATUS:
			/* the status is remain capture data */
			value = get_valid_bytes_locked(rt, NULL, NULL);
			ak_pcm_info("value:%d",value);
			ret = put_user(value, (unsigned int *)arg);
			break;
		case IOC_GET_ACT_RATE:
			if (copy_to_user((void __user *)arg, &(rt->actual_rate), sizeof(unsigned long))) {
				ret = -EFAULT;
			}
			break;
		default:
			ret = -ENOTTY;
			break;
	}

	mutex_unlock(&rt->io_lock);

	return ret;
}

/*
 * capture_read -
 * capture device file ops: read
 *
 * @flip:			capture device file
 * @buf:			buffer to store to
 * @count:			bytes of read
 * @f_pos:			the pointer of offset in the buffer
 */
ssize_t capture_read(struct akpcm_runtime *rt, char __user *buf, size_t count,
                    loff_t *f_pos, unsigned int f_flags)
{
	unsigned int buf_bytes = rt->buffer_bytes;
	unsigned int prd_bytes = rt->cfg.period_bytes;
	unsigned int app_off;
	unsigned long long ts;
	static int log_printer = 0;
	unsigned int log_cycle;

	//ak_pcm_func("IN:app_ptr 0x%x hw_ptr 0x%x", rt->app_ptr, rt->hw_ptr);

	/* check status */
	if(!is_runtime_ready(rt)) {
		return -EPERM;
	}

	/* get valid bytes value and check O_NONBLOCK */
	if (get_valid_bytes_locked(rt, NULL, &app_off) < count) {
		if (f_flags & O_NONBLOCK)
			return -EAGAIN;
	}

	if (rt->notify_threshold != count) {
		ak_pcm_func("notify_threshold 0x%x -> 0x%x", rt->notify_threshold, count);
		rt->notify_threshold = count;
	}

	rt->io_trace_in++;

	/*
	* block on the event.
	* 1. driver get the enough data to put into user data
	* 2. the upper caller make RSTBUF. going to stop
	*/
	if (wait_event_interruptible(rt->wq,
		((get_valid_bytes_locked(rt, NULL, &app_off) >= count)
			|| ((!is_runtime_stream(rt) && (!is_runtime_working(rt)))))) < 0) {
		return -ERESTARTSYS;
	}

	if ((!is_runtime_stream(rt) && (!is_runtime_working(rt)))) {
		ak_pcm_func("reading but stop happen");
		return -EPERM;
	}

	/* copy the app data from l2 buff */
	if (put_user_data(rt, buf, count)) {
		return -EFAULT;
	}

	app_off = (rt->app_ptr-1) % rt->buffer_bytes;
	ts = rt->timestamp[app_off / prd_bytes]; /* the timestamp is setting in capture_isr */
	ts -= 1000000 * (prd_bytes-1 - app_off%prd_bytes) / (rt->actual_rate * rt->cfg.sample_bits / BITS_PER_BYTE); /* sample bits only 16bits, only capture mono data */
	rt->ts = ts;

	log_cycle = rt->cfg.rate * rt->cfg.sample_bits / (8 * prd_bytes);
	if ((rt->log_print & AK_PCM_IO_TRACE) || (!((log_printer++) % (log_cycle)))) {
		ak_pcm_debug("OUT:app 0x%x hw 0x%x ts[%d] H %lu L %lu offset_ts %d count %d",
			rt->app_ptr, rt->hw_ptr, (app_off / prd_bytes), (unsigned long)((ts>>32)&0xFFFFFFFF),
			(unsigned long)((ts)&0xFFFFFFFF), (prd_bytes-1 - app_off%prd_bytes), count);
	}

	rt->last_transfer_count += count;
	rt->io_trace_out++;

	return count;
}

/*
 * capture_open -
 * open capture
 *
 * @inode:			pointer to capturer device node
 * @filp:			capturer device file
 */
int capture_open(struct akpcm_runtime *rt)
{
	int err = 0;
	SND_FORMAT format;

	ak_pcm_func("[%s@%d] runtime 0x%p",
		get_current()->comm ? get_current()->comm : "unknow", get_current()->pid, rt);

	/* don't permit to muilt open */
	if (is_runtime_opened(rt)) {
		ak_pcm_err("ERR! capture device is already open!");
		err = -EPERM; /* Operation not permitted */
		goto capture_open_end;
	}

	/* create capture soundcard */
	err = snd_card_prepare(rt->snd_dev);
	if (err) {
		goto capture_open_end;
	}

	err = snd_card_open(rt->snd_dev);   /* open hw adc */
	if (err) {
		goto capture_open_end;
	}

	init_capture_rt(rt);   /* init capture rt */

	/* set capture_isr */
	snd_card_dma_set_callback(rt->snd_dev, capture_isr, (unsigned long)rt);
	set_bit(STATUS_OPENED, &(rt->runflag));

	/* set default gain from dtb */
	snd_card_hwparams(rt->snd_dev, SND_SET_GAIN, MIXER_VOL_LI, rt->linein_gain_cur);
	snd_card_hwparams(rt->snd_dev, SND_SET_GAIN, MIXER_VOL_MIC, rt->mic_gain_cur);

	/* set MIXER_IN defalut value, link to MIXER_IN_SEL_MIC */
	snd_card_hwparams(rt->snd_dev, SND_SET_MUX, MIXER_IN, MIXER_IN_SEL_MIC);
	rt->mixer_source = MIXER_IN_SEL_MIC;

	//format.bps = rt->cfg.sample_bits;
	//format.channel = rt->cfg.channels;
	//format.sample_rate = rt->cfg.rate;
	//rt->actual_rate = snd_card_set_format(rt->snd_dev, &format); /* set hw sample rate */
	//msleep(60);

	ak_pcm_func("successfully");

capture_open_end:
	return err;
}

/*
 * capture_close -
 * close capture
 *
 * @inode:			pointer to capturer device node
 * @filp:			capturer device file
 */
int capture_close(struct akpcm_runtime *rt)
{
	int mixer_in  = MIXER_IN_SEL_MUTE;

	ak_pcm_func("[%s@%d] runtime 0x%p",
		get_current()->comm ? get_current()->comm : "unknow", get_current()->pid, rt);

	if (!is_runtime_opened(rt)) {
		ak_pcm_err("ERR! capture device is not open!");
		return -EPERM; /* Operation not permitted */
	}
	mutex_lock(&rt->io_lock);

	capture_pause(rt);   /* stop l2 DMA transfer */
	mixer_in = rt->mixer_source;

	/* set MIXER_IN link to MIXER_IN_SEL_MUTE */
	if ((MIXER_IN_SEL_MIC & mixer_in) || (MIXER_IN_SEL_LINEIN & mixer_in))  {
		/* stop process when MIC or LINE_IN is use as a capture device  */
		snd_card_hwparams(rt->snd_dev, SND_SET_MUX, MIXER_IN, MIXER_IN_SEL_MUTE);
		rt->mixer_source = MIXER_IN_SEL_MUTE;
	}

	/* free loop-buffer memory */
	clear_bit(STATUS_PREPARED, &(rt->runflag));

	/* close adc */
	snd_card_close(rt->snd_dev);
	snd_card_release(rt->snd_dev);

	mutex_unlock(&rt->io_lock);

	clear_bit(STATUS_OPENED, &(rt->runflag));
	ak_pcm_func("successfully");

	return 0;
}
