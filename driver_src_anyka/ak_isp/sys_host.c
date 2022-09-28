/*
 * sys_host.c
 *
 * isp host debug
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
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/gpio.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/dma/ipu-dma.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-dma-contig.h>

#include "../include/ak_isp_drv.h"
#include "../include/ak_video_priv_cmd.h"
#include "include_internal/ak_video_priv_cmd_internal.h"
#include "cif.h"
#include <mach/map.h>

#define CACHE_SIZE 64

/*
 * hw_attr_show - show hw attribute
 *
 * @input_num:			num of inputs
 * @capturing_input_id:	dual is active. current capturing input id
 * @camera_opend_count:	counter for all inputs
 * @set_capture_pause:	set to pause
 * @capture_state_pause:state for requesting pause
 * @rawdata_capture_set:set to ready to capture rawdata
 * @rawdata_capture_done:done for capturing rawdata
 * @dual_mipi_once_had_init:dual is active. had inited for once config for mipi
 * @dual_sensors_init:	dual is active. sensors had inited
 */
static ssize_t hw_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct hw_attr *hw = &ak_cam->hw;
	int r;

	r = snprintf(buf, PAGE_SIZE,
			"hw_attr:\n"
			"input_num:%d\n"
			"capturing_input_id:%d\n"
			"camera_opend_count:%d\n"
			"set_capture_pause:%d\n"
			"capture_state_pause:%d\n"
			"rawdata_capture_set:%d\n"
			"rawdata_capture_done:%d\n"
			"dual_mipi_once_had_init:%d\n"
			"dual_sensors_init:%d\n",
			hw->input_num,
			hw->capturing_input_id,
			hw->camera_opend_count,
			hw->set_capture_pause,
			hw->capture_state_pause,
			hw->rawdata_capture_set,
			hw->rawdata_capture_done,
			hw->dual_mipi_once_had_init,
			hw->dual_sensors_init);

	return r;
}

/*
 * input_attr_show - show input video attribute
 *
 * @input_id:		input index
 * @input_timestamp:	lastest timestamp for the input
 * @input_opend_count:	counter for the input opend times
 * @input_route_init:	input route inited
 * @irq_done_state:	irq done state
 * @td_count:		counter for td
 * @frame_count:	counter for frames
 * @fline_count:	counter for line
 * @fend_count:		counter for lend
 * @nofend_count:	counter for had fline but no fend
 * @nofline_count:	counter for had fend but no fline
 */
static ssize_t input_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct input_video_attr *input_video;
	int r;

	buf[0] = '\0';
	list_for_each_entry(input_video, &ak_cam->input_head, list)
		r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
				"input_id:%d\n"
				"input_timestamp.tv_sec:%u\n"
				"input_timestamp.tv_usec:%u\n"
				"input_opend_count:%d\n"
				"input_route_init:%d\n"
				"irq_done_state:%lu\n"
				"td_count:%d\n"
				"frame_count:%d\n"
				"fline_count:%d\n"
				"fend_count:%d\n"
				"nofend_count:%d\n"
				"nofline_count:%d\n",
				input_video->input_id,
				(unsigned int)input_video->input_timestamp.tv_sec,
				(unsigned int)input_video->input_timestamp.tv_usec,
				input_video->input_opend_count,
				input_video->input_route_init,
				input_video->irq_done_state,
				input_video->td_count,
				input_video->frame_count,
				input_video->fline_count,
				input_video->fend_count,
				input_video->nofend_count,
				input_video->nofline_count);

	r = strlen(buf);
	return r;
}

static ssize_t chn_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct input_video_attr *input_video;
	struct chn_attr *chn;
	int chn_id;
	int r;

	buf[0] = '\0';
	list_for_each_entry(input_video, &ak_cam->input_head, list)
		for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
			chn = &input_video->chn[chn_id];
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
					"input_id:%d,chn_id:%d,state:%s\n",
					input_video->input_id,
					chn->chn_id,
					(chn->chn_state == CHN_STATE_START_STREAMING) ? "streaming":"nostreaming");

			if (chn->chn_state == CHN_STATE_START_STREAMING) {
				int i;

				for (i = 0; i < chn->vb_num; i++)
					r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
							"phyaddr-%d:0x%08x\n",
							i,
							chn->phyaddr[i]);

				r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
						"poll_empty:%d\n"
						"chn_opend_count:%d\n"
						"width:%d\n"
						"height:%d\n"
						"vb_num:%d\n",
						chn->poll_empty,
						chn->chn_opend_count,
						chn->width,
						chn->height,
						chn->vb_num);
			}
		}

	r = strlen(buf);
	return r;
}

static ssize_t mipi_ths_settle_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;
	unsigned int ip_ths_settle = 0;
	
	ip_ths_settle = __raw_readl(AK_VA_MIPI1 + 0x60)&0xF;
	r = snprintf(buf, PAGE_SIZE,"mipi_ths_settle:0x%x\n", ip_ths_settle);

	return r;
}

static ssize_t clock_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct hw_attr *hw = &ak_cam->hw;
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "vclk:%lu, internal_pclk:%lu, internal_pclk_res:%lu\n",
			hw->isp_clk_rate, hw->internal_pclk, hw->internal_pclk_res);

	return r;
}

/*
 * dev_attr_hw_attr: hw attribute
 */
static DEVICE_ATTR(hw_attr, 0644, hw_attr_show, NULL);
static DEVICE_ATTR(input_attr, 0644, input_attr_show, NULL);
static DEVICE_ATTR(chn_attr, 0644, chn_attr_show, NULL);
static DEVICE_ATTR(mipi_ths_settle, 0644, mipi_ths_settle_show, NULL);
static DEVICE_ATTR(clock_attr, 0644, clock_attr_show, NULL);

static struct attribute *host_attrs[] = {
	&dev_attr_hw_attr.attr,
	&dev_attr_input_attr.attr,
	&dev_attr_chn_attr.attr,
	&dev_attr_mipi_ths_settle.attr,
	&dev_attr_clock_attr.attr,
	NULL,
};

static struct attribute_group host_attr_grp = {
	.name = "host",
	.attrs = host_attrs,
};

int sys_host_init(struct ak_camera_dev *ak_cam)
{
	int r;
	struct device *dev = ak_cam->hw.dev;

	r = sysfs_create_group(&dev->kobj, &host_attr_grp);
	if (r)
		goto fail;

	return 0;

fail:
	return r;
}

int sys_host_exit(struct ak_camera_dev *ak_cam)
{
	struct device *dev = ak_cam->hw.dev;

	sysfs_remove_group(&dev->kobj, &host_attr_grp);
	return 0;
}
