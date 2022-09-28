/*
 * sys_isp.c
 *
 * for debug isp
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

#define NO_ISP "noisp"
#define CACHE_SIZE 64

struct sys_isp_infomation {
	int _3dnr_to_isp_id;
};

static struct sys_isp_infomation sys_isp_info = {
	._3dnr_to_isp_id = 0,
};

/*
 * isp_index_show - show isp current index
 */
static ssize_t isp_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"isp_index:%d\n",
			sys_isp_info._3dnr_to_isp_id);
}

/*
 * isp_index_store- store isp current index
 */
static ssize_t isp_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sys_isp_info._3dnr_to_isp_id = simple_strtol(buf, NULL, 10);
	return count;
}

/*
 * isp_version_store- show isp version
 */
static ssize_t isp_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct input_video_attr *input_video;
	struct isp_attr *isp;
	int r = 0;

	buf[0] = '\0';
	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		/*walk through all input*/
		isp = &input_video->isp;

		/*must the same id*/
		if (sys_isp_info._3dnr_to_isp_id != isp->isp_id)
			continue;

		/*printf to buf*/
		r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
				"version:%s\n",
				ak_isp_get_version(isp->isp_struct));
	}

	return r;
}

/*
 * ae_run_info_show - show ae running info
 */
static ssize_t ae_run_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct input_video_attr *input_video;
	struct isp_attr *isp;
	AK_ISP_AE_RUN_INFO ae_stat;
	int r = 0;

	buf[0] = '\0';
	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		/*walk through all input*/
		isp = &input_video->isp;

		/*must the same id*/
		if (sys_isp_info._3dnr_to_isp_id != isp->isp_id)
			continue;

		/*printf to buf*/
		r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
				"isp-%d:%s\n",
				isp->isp_id,
				(isp->isp_struct) ? "working":"noworking");

		if (isp->isp_struct) {
			/*get ae run info*/
			r = ak_isp_vp_get_ae_run_info(isp->isp_struct, &ae_stat);
			if (r) {
				r = snprintf(buf, PAGE_SIZE, "%s\n", NO_ISP);
				goto end;
			}

			/*printf to buf*/
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "current_calc_avg_lumi:%u\n",
					ae_stat.current_calc_avg_lumi);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "current_calc_avg_compensation_lumi:%u\n",
					ae_stat.current_calc_avg_compensation_lumi);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "current_a_gain:%u\n",
					ae_stat.current_a_gain);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "current_d_gain:%u\n",
					ae_stat.current_d_gain);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "current_isp_d_gain:%u\n",
					ae_stat.current_isp_d_gain);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "current_exp_time:%u\n",
					ae_stat.current_exp_time);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "current_exp_time_step:%u\n",
					ae_stat.current_exp_time_step);
		}
	}

	r = strlen(buf);

end:
	return r;
}

/*
 * frame_rate_show - show frame rate
 */
static ssize_t frame_rate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct input_video_attr *input_video;
	struct isp_attr *isp;
	AK_ISP_FRAME_RATE_ATTR frame_rate;
	int r = 0;

	buf[0] = '\0';
	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		/*walk through all input*/
		isp = &input_video->isp;

		/*must the same id*/
		if (sys_isp_info._3dnr_to_isp_id != isp->isp_id)
			continue;

		/*printf to buf*/
		r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
				"isp-%d:%s\n",
				isp->isp_id,
				(isp->isp_struct) ? "working":"noworking");

		if (isp->isp_struct) {
			/*get frame rate info*/
			r = ak_isp_vp_get_frame_rate(isp->isp_struct, &frame_rate);
			if (r) {
				r = snprintf(buf, PAGE_SIZE, "%s\n", NO_ISP);
				goto end;
			}

			/*printf to buf*/
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "hight_light_frame_rate:%u\n",
					frame_rate.hight_light_frame_rate);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "hight_light_max_exp_time:%u\n",
					frame_rate.hight_light_max_exp_time);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "low_light_frame_rate:%u\n",
					frame_rate.low_light_frame_rate);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "low_light_max_exp_time:%u\n",
					frame_rate.low_light_max_exp_time);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "low_light_to_hight_light_gain:%u\n",
					frame_rate.low_light_to_hight_light_gain);
		}
	}

	r = strlen(buf);

end:
	return r;
}

/*
 * af_stat_info_show - show af statics info
 */
static ssize_t af_stat_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct input_video_attr *input_video;
	struct isp_attr *isp;
	AK_ISP_AF_STAT_INFO af_stat_info;
	int r = 0;

	buf[0] = '\0';
	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		/*walk through all input*/
		isp = &input_video->isp;

		/*must the same id*/
		if (sys_isp_info._3dnr_to_isp_id != isp->isp_id)
			continue;

		/*printf to buf*/
		r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
				"isp-%d:%s\n",
				isp->isp_id,
				(isp->isp_struct) ? "working":"noworking");

		if (isp->isp_struct) {
			int i;

			/*get af*/
			r = ak_isp_vp_get_af_stat_info(isp->isp_struct, &af_stat_info);
			if (r) {
				r = snprintf(buf, PAGE_SIZE, "%s\n", NO_ISP);
				goto end;
			}

			/*printf to buf*/
			for (i = 0;i < 5; i++) {
				r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "af_statics[%d]:%d\n",
						i, af_stat_info.af_statics[i]);
			}
		}
	}

	r = strlen(buf);

end:
	return r;
}

/*
 * awb_stat_info_show - show awb statics info
 */
static ssize_t awb_stat_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct input_video_attr *input_video;
	struct isp_attr *isp;
	AK_ISP_AWB_STAT_INFO awb_stat_info;
	int r = 0;

	buf[0] = '\0';
	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		/*walk through all input*/
		isp = &input_video->isp;

		/*must the same id*/
		if (sys_isp_info._3dnr_to_isp_id != isp->isp_id)
			continue;

		/*printf to buf*/
		r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
				"isp-%d:%s\n",
				isp->isp_id,
				(isp->isp_struct) ? "working":"noworking");

		if (isp->isp_struct) {
			/*get awb*/
			r = ak_isp_vp_get_awb_stat_info(isp->isp_struct, &awb_stat_info);
			if (r) {
				r = snprintf(buf, PAGE_SIZE, "%s\n", NO_ISP);
				goto end;
			}

			/*printf to buf*/
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "r_gain:%d\n",
					awb_stat_info.r_gain);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "g_gain:%d\n",
					awb_stat_info.g_gain);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "b_gain:%d\n",
					awb_stat_info.b_gain);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "r_offset:%d\n",
					awb_stat_info.r_offset);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "g_offset:%d\n",
					awb_stat_info.g_offset);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "b_offset:%d\n",
					awb_stat_info.b_offset);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "current_colortemp_index:%d\n",
					awb_stat_info.current_colortemp_index);
		}
	}

	r = strlen(buf);

end:
	return r;
}

/*
 * 3d_nr_stat_info_show - show 3d nr static info
 */
static ssize_t _3d_nr_stat_info_show(struct device *dev,
		struct device_attribute *attr, char *buf, int start, int end)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct input_video_attr *input_video;
	struct isp_attr *isp;
	AK_ISP_3D_NR_STAT_INFO _3d_nr_stat_info;
	int r = 0;

	buf[0] = '\0';
	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		/*walk through all input*/
		isp = &input_video->isp;

		/*must the same id*/
		if (sys_isp_info._3dnr_to_isp_id != isp->isp_id)
			continue;

		/*printf to buf*/
		r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
				"isp-%d:%s\n",
				isp->isp_id,
				(isp->isp_struct) ? "working":"noworking");

		if (isp->isp_struct) {
			int i, j;

			/*get 3dnr*/
			r = ak_isp_vp_get_3d_nr_stat_info(isp->isp_struct, &_3d_nr_stat_info);
			if (r) {
				r = snprintf(buf, PAGE_SIZE, "%s\n", NO_ISP);
				goto end;
			}

			/*printf to buf*/
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "MD_level:%u\n",
					_3d_nr_stat_info.MD_level);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "start-line:%d, end-line:%d\n",
					start, end);
			for (i = start; i <= end; i++) {
				for (j = 0; j < 32; j++) {
					r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "%u\t",
							_3d_nr_stat_info.MD_stat[i][j]);
				}
				r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "\n");
			}
		}
	}

	r = strlen(buf);

end:
	return r;
}

/*
 * 3d_nr_stat_info_0_show - show 3d nr static info
 */
static ssize_t _3d_nr_stat_info_0_11_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return _3d_nr_stat_info_show(dev,
			attr, buf, 0, 11);
}

/*
 * 3d_nr_stat_info_1_show - show 3d nr static info
 */
static ssize_t _3d_nr_stat_info_12_23_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return _3d_nr_stat_info_show(dev,
			attr, buf, 12, 23);
}

/*
 * ae_attr_show - show ae attribute
 */
static ssize_t ae_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak_camera_dev *ak_cam = dev_get_drvdata(dev);
	struct input_video_attr *input_video;
	struct isp_attr *isp;
	AK_ISP_AE_ATTR ae_attr;
	int r = 0;

	buf[0] = '\0';
	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		isp = &input_video->isp;

		if (sys_isp_info._3dnr_to_isp_id != isp->isp_id)
			continue;

		r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf),
				"isp-%d:%s\n",
				isp->isp_id,
				(isp->isp_struct) ? "working":"noworking");

		if (isp->isp_struct) {
			r = ak_isp_vp_get_ae_attr(isp->isp_struct, &ae_attr);
			if (r) {
				r = snprintf(buf, PAGE_SIZE, "%s\n", NO_ISP);
				goto end;
			}

			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "exp_time_max:%u\n",
					ae_attr.exp_time_max);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "exp_time_min:%u\n",
					ae_attr.exp_time_min);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "d_gain_max:%u\n",
					ae_attr.d_gain_max);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "d_gain_min:%u\n",
					ae_attr.d_gain_min);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "isp_d_gain_max:%u\n",
					ae_attr.isp_d_gain_max);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "isp_d_gain_min:%u\n",
					ae_attr.isp_d_gain_min);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "a_gain_max:%u\n",
					ae_attr.a_gain_max);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "exp_step:%u\n",
					ae_attr.exp_step);
			r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "exp_stable_range:%u\n",
					ae_attr.exp_stable_range);
		}
	}

	r = strlen(buf);

end:
	return r;
}

/*
 * dev_attr_isp_index:	isp index
 * dev_attr_isp_version: isp version
 * dev_attr_ae_run_info: ak_isp_vp_get_ae_rn_info
 * dev_attr_frame_rate: ak_isp_vp_get_frame_rate
 * dev_attr_af_stat_info: ak_isp_vp_ge_af_stat_info
 * dev_attr_awb_stat_info: ak_isp_vp_get_awb_stat_info
 * dev_attr_3d_nr_stat_info_0_11: ak_isp_vp_get_3d_nr_stat_info
 * dev_attr_3d_nr_stat_info_12_23: ak_isp_vp_get_3d_nr_stat_info
 * dev_attr_ae_attr:	ae attribute
 */
static DEVICE_ATTR(isp_index, 0644, isp_index_show, isp_index_store);
static DEVICE_ATTR(isp_version, 0644, isp_version_show, NULL);
static DEVICE_ATTR(ae_run_info, 0644, ae_run_info_show, NULL);
static DEVICE_ATTR(frame_rate, 0644, frame_rate_show, NULL);
static DEVICE_ATTR(af_stat_info, 0644, af_stat_info_show, NULL);
static DEVICE_ATTR(awb_stat_info, 0644, awb_stat_info_show, NULL);
static DEVICE_ATTR(3d_nr_stat_info_0_11, 0644, _3d_nr_stat_info_0_11_show, NULL);
static DEVICE_ATTR(3d_nr_stat_info_12_23, 0644, _3d_nr_stat_info_12_23_show, NULL);
static DEVICE_ATTR(ae_attr, 0644, ae_attr_show, NULL);

static struct attribute *isp_attrs[] = {
	&dev_attr_isp_index.attr,
	&dev_attr_isp_version.attr,
	&dev_attr_ae_run_info.attr,
	&dev_attr_frame_rate.attr,
	&dev_attr_af_stat_info.attr,
	&dev_attr_awb_stat_info.attr,
	&dev_attr_3d_nr_stat_info_0_11.attr,
	&dev_attr_3d_nr_stat_info_12_23.attr,
	&dev_attr_ae_attr.attr,
	NULL,
};

static struct attribute_group isp_attr_grp = {
	.name = "isp",
	.attrs = isp_attrs,
};

int sys_isp_init(struct ak_camera_dev *ak_cam)
{
	int r;
	struct device *dev = ak_cam->hw.dev;

	sys_isp_info._3dnr_to_isp_id = 0;

	r = sysfs_create_group(&dev->kobj, &isp_attr_grp);
	if (r)
		goto fail;

	return 0;

fail:
	return r;
}

int sys_isp_exit(struct ak_camera_dev *ak_cam)
{
	struct device *dev = ak_cam->hw.dev;

	sysfs_remove_group(&dev->kobj, &isp_attr_grp);
	return 0;
}
