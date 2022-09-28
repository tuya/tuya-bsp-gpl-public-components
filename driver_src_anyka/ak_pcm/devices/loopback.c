/*
 *  pcm for anyka chip
 *  Copyright (c) by Anyka, Inc.
 *  Create by chen_yanhong 2020-06-19
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
#include "ak_pcm_defs.h"
#include "loopback.h"
#include "ak_pcm_common.h"

static struct akpcm_runtime *g_loopback_rt;

/*
 * update_hw_ptr -
 * update hardware pointer
 *
 * @rt:				pointer to runtime
 */
static inline void update_hw_ptr_unlocked(struct akpcm_runtime *rt)
{
	unsigned int prd_bytes = rt->cfg.period_bytes;

	rt->hw_ptr += prd_bytes;
	if (rt->hw_ptr >= rt->boundary)
		rt->hw_ptr -= rt->boundary;
}

/*
 * set_loopback_timestamp -
 * set timestamp for loopback data
 *
 * @rt:					runtime
 * @prd_bytes:			bytes of one transfer dma
 * @hw_off:				hardware pointer to the cache
 */
static void set_loopback_timestamp(struct akpcm_runtime *rt,
                unsigned int prd_bytes, unsigned int hw_off)
{
	int index = hw_off / prd_bytes;
	unsigned long long *timestamp = rt->timestamp;

	/*
	 * the way of getting timestamp is same as video driver,
	 * then its more easy to do synchronization between audio and video.
	 */
	*(timestamp + index) = get_current_microsecond();

	if ((rt->log_print & AK_PCM_TIMESTAMP_TRACE) || (index == 0)) {
		ak_pcm_debug("ts[%d]:H %lu L %lu", index, (unsigned long)((*(timestamp + index)>>32)&0xFFFFFFFF),
			(unsigned long)((*(timestamp + index))&0xFFFFFFFF));
	}
}

/*
 * update_app_ptr -
 * update app pointer
 *
 * @rt:				pointer to runtime
 * @offset_data:	offset of the app_ptr
 */
static inline void update_app_ptr(struct akpcm_runtime *rt, unsigned int offset_data)
{
	rt->app_ptr += offset_data;
	if(rt->app_ptr >= rt->boundary)
		rt->app_ptr -= rt->boundary;
}

/*
 * loopback_get_single_direct_available -
 * get the available count in single direction.
 * notice that the buffer is two-way buffer but opertation copy data from buffer
 * WILL BE single direction.Need to use again to check if there is more data or not.
 * loopback_read will update the app_ptr then try again
 *
 * @rt:				pointer to runtime
 * @remain:			remain count in the user buffer
 *
 */
static int loopback_get_single_direct_available(struct akpcm_runtime *rt, int remain)
{
	unsigned int hw_ptr = rt->hw_ptr;
	unsigned int app_ptr = rt->app_ptr;
	int available_data = hw_ptr % rt->buffer_bytes - app_ptr % rt->buffer_bytes;

	if (available_data >= 0) {
		available_data = (remain > available_data) ? available_data : remain;
	} else {
		available_data = rt->buffer_bytes - app_ptr % rt->buffer_bytes;
		available_data = (remain > available_data) ? available_data : remain;
	}

	//ak_pcm_func("%d(hw 0x%x app 0x%x)", available_data, hw_ptr, app_ptr);

	return available_data;
}

/*
 * get_loopback_timestamp -
 * get the loopback timestamp according the app_ptr offset
 * |****************|****************|****************|****************|__loopback buffer
 * 0            prd_bytes       2*prd_bytes        3*prd_bytes      4*prd_bytes
 *                                            |
 *                                           app_off
 * timestamp:
 * 0 --> [0, prd_bytes), t0 means the time of buffer[prd_bytes-1]
 * 1 --> [prd_bytes, 2*prd_bytes)
 * 2 --> [2*prd_bytes, 3*prd_bytes)
 * 3 --> [3*prd_bytes, 4*prd_bytes)
 * When index = app_off / prd_bytes, means [index*prd_bytes, (index+1)*prd_bytes) will be timestamp[index]
 * If app_off not 0 means need to rollback sometime from data size as (index+1)*prd_bytes.
 * Notice the data to DAC will be stereo
 *
 * @rt: pointer to runtime
 *
 */
static void get_loopback_timestamp(struct akpcm_runtime *rt)
{
	unsigned int app_off = (rt->app_ptr - 1) % rt->buffer_bytes;
	unsigned int prd_bytes = rt->cfg.period_bytes;
	unsigned long long ts;

	ts = rt->timestamp[app_off / prd_bytes];
	ts -= 1000000 * (prd_bytes -1 - app_off%prd_bytes) / (g_loopback_rt->actual_rate * (rt->cfg.sample_bits / BITS_PER_BYTE) * 2);
	rt->ts = ts;

	if (rt->log_print & (AK_PCM_TIMESTAMP_TRACE | AK_PCM_ISR_TRACE)) {
		ak_pcm_debug("ts:H %lu L %lu", (unsigned long)((ts>>32)&0xFFFFFFFF),
				(unsigned long)((ts)&0xFFFFFFFF));
	}
}

/*
 * copy_to_loopback -
 * copy the buffer data from playback to loopback
 * |****************|****************|****************|****************|__loopback buffer
 * 0            prd_bytes       2*prd_bytes        3*prd_bytes      4*prd_bytes
 * timestamp:
 * 0 --> [0, prd_bytes), t0 means the time of buffer[prd_bytes-1]
 * 1 --> [prd_bytes, 2*prd_bytes)
 * 2 --> [2*prd_bytes, 3*prd_bytes)
 * 3 --> [3*prd_bytes, 4*prd_bytes)
 *
 * @play_rt: pointer to playback runtime
 *
 */
int copy_to_loopback(unsigned char *src_addr, unsigned int playback_hw_off, unsigned long actual_rate)
{
	unsigned int prd_bytes, loopback_hw_off;
	static int log_printer = 0;
	unsigned int log_cycle;

	if (g_loopback_rt == NULL)
		return -EPERM;

	if (!is_runtime_working(g_loopback_rt)) {
		//ak_pcm_err("ERR! loopback not start!");
		return -EPERM; /* Operation not permitted */
	}

	log_cycle = g_loopback_rt->cfg.rate * g_loopback_rt->cfg.sample_bits *2 / (8 * g_loopback_rt->cfg.period_bytes);

	if (g_loopback_rt->actual_rate != actual_rate) {
		ak_pcm_func("actual_rate %ld -> %ld", g_loopback_rt->actual_rate, actual_rate);
		g_loopback_rt->actual_rate = actual_rate;
	}
	prd_bytes = g_loopback_rt->cfg.period_bytes;
	loopback_hw_off = (g_loopback_rt->hw_ptr % g_loopback_rt->buffer_bytes);
	if (loopback_hw_off + prd_bytes > g_loopback_rt->buffer_bytes) {
		/*
		* Since loss part of the playback data will make bad effect.
		* Here return if not enough memory for loopback
		*/
		ak_pcm_err("ERR! loopback over size!(0x%x+0x%x|0x%x)",
			loopback_hw_off, prd_bytes, g_loopback_rt->buffer_bytes);
		return -EPERM;
	}

	set_loopback_timestamp(g_loopback_rt, prd_bytes, loopback_hw_off);
	memcpy(g_loopback_rt->dma_area + loopback_hw_off, (src_addr + playback_hw_off), prd_bytes);
	update_hw_ptr_unlocked(g_loopback_rt);

	if ((g_loopback_rt->log_print & AK_PCM_ISR_TRACE) || (!((log_printer++) % (log_cycle)))) {
		ak_pcm_debug("app 0x%x hw 0x%x", g_loopback_rt->app_ptr, g_loopback_rt->hw_ptr);
	}

	g_loopback_rt->last_isr_count++;

	return 0;
}

/*
 * wake_up_loopback -
 * wake up the blocking object on wq
 *
 * @play_rt:			pointer to runtime
 *
 */
int wake_up_loopback(void)
{
	if (g_loopback_rt == NULL)
		return -EPERM;

	if (!is_runtime_working(g_loopback_rt)) {
		return -EPERM;
	}

	wake_up_interruptible(&(g_loopback_rt->wq));

	return 0;
}


/*
 * loopback_read -
 * device file ops: write
 *
 * @filp:		pointer to device file
 * @buf:		the user buffer pointer
 * @count:		the count of the user buffer
 * @f_pos:		the pointer of offset in the buffer
 *
 */
ssize_t loopback_read(struct akpcm_runtime *rt, char __user *buf, size_t count,
                    loff_t *f_pos, unsigned int f_flags)
{
	unsigned int buffer_bytes = rt->buffer_bytes;
	int available_data;
	int remain_data = count;
	static int log_printer = 0;
	unsigned int log_cycle;

	if (!is_runtime_ready(rt)) {
		return -EPERM;
	}

	if ((!is_runtime_working(rt)) && (is_runtime_ready(rt))) {
		rt->last_transfer_count = 0;
		rt->last_isr_count = 0;
		set_bit(STATUS_WORKING, &(rt->runflag));
	}

	//cache_buff only period_bytes*2
	if (count > rt->cfg.period_bytes)
		count = rt->cfg.period_bytes;

	if (rt->cfg.channels == 1) {
		count = count << 1;
		remain_data = count;
	}

	if (rt->log_print & AK_PCM_IO_TRACE) {
		ak_pcm_debug("IN:app 0x%x hw 0x%x count 0x%x", rt->app_ptr, rt->hw_ptr, count);
	}

	if ((available_data = loopback_get_single_direct_available(rt, remain_data)) == 0) {
		if (f_flags & O_NONBLOCK)
			return -EAGAIN;
	}

	rt->io_trace_in++;

	do {
		if (wait_event_interruptible(rt->wq,
			(((available_data = loopback_get_single_direct_available(rt, remain_data)) > 0)
				|| (!is_runtime_working(rt))))) {
			ak_pcm_err("Err wait available_data");
			return -ERESTARTSYS;
		}
		if (!is_runtime_working(rt)) {
			ak_pcm_func("reading but stop happen");
			return -EPERM;
		}
		memcpy(rt->cache_buff + (count - remain_data), (rt->dma_area + (rt->app_ptr % buffer_bytes)), available_data);
		update_app_ptr(rt, available_data);
		get_loopback_timestamp(rt);
		remain_data -= available_data;
	} while (remain_data > 0);

	if (rt->cfg.channels == 1) {
		stereo_to_mono(rt->cache_buff, rt->cache_buff, count);
		count = count >> 1;
	}

	if (copy_to_user(buf, rt->cache_buff, count)) {
		ak_pcm_err("Err copy to user");
		return -EFAULT;
	}

	log_cycle = rt->cfg.rate * rt->cfg.sample_bits * 2 / (8 * rt->cfg.period_bytes);
	if ((rt->log_print & AK_PCM_IO_TRACE) || (!((log_printer++) % (log_cycle)))) {
		ak_pcm_debug("OUT:app 0x%x hw 0x%x count 0x%x", rt->app_ptr, rt->hw_ptr, count);
	}

	rt->last_transfer_count += count;
	rt->io_trace_out++;

	return count;
}


static void init_loopback_rt(struct akpcm_runtime *rt)
{
	rt->hw_ptr = 0;
	rt->app_ptr = 0;
	g_loopback_rt = rt;

	rt->io_trace_in = 0;
	rt->io_trace_out = 0;

	clear_bit(STATUS_START_STREAM, &(rt->runflag));
	clear_bit(STATUS_WORKING, &(rt->runflag));
	clear_bit(STATUS_PAUSING, &(rt->runflag));
	clear_bit(STATUS_OPENED, &(rt->runflag));
	clear_bit(STATUS_PREPARED, &(rt->runflag));
}

static int loopback_prepare(struct akpcm_runtime *rt);
/*
 * loopback_open -
 * device file ops: open file
 *
 * @inode:		device node
 * @filp:		pointer to device file
 */
int loopback_open(struct akpcm_runtime *rt)
{
	ak_pcm_func("[%d] trying...", get_current()->pid);

	if (is_runtime_opened(rt)) {
		ak_pcm_err("ERR! Already open!");
		return -EPERM; /* Operation not permitted */
	}

	init_loopback_rt(rt);

	set_bit(STATUS_OPENED, &(rt->runflag));

	return 0;
}

/*
 * loopback_close -
 * device file ops: close file
 *
 * @inode:		device node
 * @filp:		pointer to device file
 */
int loopback_close(struct akpcm_runtime *rt)
{
	rt->hw_ptr = 0;
	rt->app_ptr = 0;

	clear_bit(STATUS_WORKING, &(rt->runflag));
	clear_bit(STATUS_PREPARED, &(rt->runflag));
	clear_bit(STATUS_OPENED, &(rt->runflag));

	ak_pcm_func("[%d] successfully", get_current()->pid);

	return 0;
}

/*
 * loopback_prepare -
 * according the param, alloc the memory
 *
 * @rt:		pointer of runtime
 *
 */
static int loopback_prepare(struct akpcm_runtime *rt)
{
	struct akpcm_pars *rt_cfg = &rt->cfg;
	unsigned int request_size = (rt->cfg.periods * rt->cfg.period_bytes);
	unsigned int i;
	int ret = 0;

	if (request_size != rt->buffer_bytes) {
		if (rt->dma_area) {
			vfree(rt->dma_area);
			rt->dma_area = NULL;
		}
		if (rt->cache_buff) {
			vfree(rt->cache_buff);
			rt->cache_buff = NULL;
		}
	}

	if (!rt->dma_area) {
		for (i = 0; i < 64; i++) {
			if ((request_size>>i) == 0) {
				break;
			}
		}
		rt->boundary = request_size << (32-i);
		rt->buffer_bytes = request_size;
		rt->dma_area = vmalloc(rt->buffer_bytes);//kmalloc(rt->buffer_bytes, GFP_KERNEL);
		if (!rt->dma_area) {
			ak_pcm_err("ERR! hw buffer(0x%x) failed", rt->buffer_bytes);
			ret = -ENOMEM;
			goto exit_loopback_prepare;
		}
		memset(rt->dma_area, 0, rt->buffer_bytes);
		ak_pcm_func("hw buffer 0x%p size 0x%x", rt->dma_area, rt->buffer_bytes);
		rt->cache_buff = vmalloc(rt->cfg.period_bytes * 2);//kmalloc(rt->cfg.period_bytes * 2, GFP_KERNEL);
		if (!rt->cache_buff) {
			ak_pcm_err("ERR! app cache alloc(%d) failed", (rt->cfg.period_bytes * 2));
			goto exit_loopback_prepare;
		}
		memset(rt->cache_buff, 0, rt->cfg.period_bytes * 2);
		ak_pcm_func("app cache 0x%p size 0x%x", rt->cache_buff, (rt->cfg.period_bytes * 2));
	}

	rt->app_ptr = 0;
	rt->hw_ptr = 0;

	ak_pcm_func("CH %d PERD %d BYTE %d Total 0x%x bit %d",
			rt_cfg->channels, rt_cfg->periods, rt_cfg->period_bytes, rt->buffer_bytes,
			rt_cfg->sample_bits);

	set_bit(STATUS_PREPARED, &(rt->runflag));

	return 0;

exit_loopback_prepare:
	if (rt->dma_area) {
		vfree(rt->dma_area);
		rt->dma_area = NULL;
	}
	if (rt->cache_buff) {
		vfree(rt->cache_buff);
		rt->cache_buff = NULL;
	}
	return ret;
}

/*
 * loopback_ioctl -
 * device file ops: close file
 *
 * @inode:		device node
 * @cmd:		command
 * @arg;		argument for the command
 *
 */
long loopback_ioctl(struct akpcm_runtime *rt, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned long long ull;

	switch(cmd) {
		case IOC_PREPARE:
			ak_pcm_debug("IOC_PREPARE");
			if (copy_from_user(&(rt->cfg), (void __user *)arg, sizeof(struct akpcm_pars))) {
				return -EFAULT;
			}
			ret = loopback_prepare(rt);
			break;
		case IOC_RSTBUF:
			ak_pcm_debug("IOC_RSTBUF");
			if (is_runtime_working(g_loopback_rt)) {
				clear_bit(STATUS_WORKING, &(rt->runflag));
				wake_up_interruptible(&(g_loopback_rt->wq));
			}
			break;
		case IOC_GETTIMER:
			ull = rt->ts;
			if (rt->log_print & AK_PCM_TIMESTAMP_TRACE) {
				ak_pcm_debug("ts:H %lu L %lu", (unsigned long)((ull>>32)&0xFFFFFFFF),
					(unsigned long)((ull)&0xFFFFFFFF));
			}
			if (copy_to_user((void __user *)arg, &ull, sizeof(unsigned long long))) {
				ret = -EFAULT;
			}
			break;
		case IOC_GET_ACT_RATE:
			if (copy_to_user((void __user *)arg, &(rt->actual_rate), sizeof(unsigned long))) {
				ret = -EFAULT;
			}
			break;
		default:
			ak_pcm_func("unknown 0x%x", cmd);
			ret = -ENOTTY;
			break;
	}

	return ret;
}
