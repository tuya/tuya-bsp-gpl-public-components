/*
 * video.c
 *
 * isp video data process
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
#include <linux/clk.h>
#include <linux/delay.h>

#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

//#include <media/videobuf2-core.h>
#include "../include/ak_video_priv_cmd.h"
#include "cif.h"
#include "isp_param.h"
#include "camera.h"

#include <mach/map.h>

/*
 * defined: start streaming in irq handler in continues mode.
 * not defined:	first pause then start streaming in continues mode.
 */
#define CNT_MODE_START_CHN_IN_IRQ

/**
 * NOTE: the structure from drivers/pinctrl/core.h
 *
 * struct pinctrl - per-device pin control state holder
 * @node: global list node
 * @dev: the device using this pin control handle
 * @states: a list of states for this device
 * @state: the current state
 * @dt_maps: the mapping table chunks dynamically parsed from device tree for
 *	this device, if any
 * @users: reference count
 */
struct pinctrl {
	struct list_head node;
	struct device *dev;
	struct list_head states;
	struct pinctrl_state *state;
	struct list_head dt_maps;
	struct kref users;
};

static int set_chn_buffer_enable(struct chn_attr *chn, int index);
static inline void set_chn_buffers_addr(struct chn_attr *chn);
static inline void set_chn_buffers_enable(struct chn_attr *chn);
static inline void set_chn_pp_frame_ctrl(struct chn_attr *chn);
static inline void clear_chn_pp_frame_ctrl(struct chn_attr *chn);
static int dual_start_capturing_new_one(struct input_video_attr *input_video);
static int rawdata_capture_one(struct chn_attr *chn);

static bool is_dual_sensor_mode(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	int input_num = hw->input_num;

	if (input_num == 1)
		return false;

	return true;
}

static bool is_first_input(struct input_video_attr *input_video)
{
	int input_id = input_video->input_id;

	if (input_id == 0)
		return true;

	return false;
}

/*
 * Videobuf operations
 */

/*
 * ak_vb2_queue_setup -
 * calculate the __buffer__ (not data) size and number of buffers
 *
 * @vq:				queue of vb2
 * @parq:
 * @count:			return count of filed
 * @num_planes:		number of planes in on frame
 * @size:
 * @alloc_ctxs:
 */
static int ak_vb2_queue_setup(struct vb2_queue *vq,
		const void *parg,
		unsigned int *count, unsigned int *num_planes,
		unsigned int sizes[], void *alloc_ctxs[])
{
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vq);
	struct chn_attr *chn = ak_cam_fh_to_chn(akfh);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s input_id:%d, chn_id:%d *count:%d, sizeimage:%u\n",
			__func__, input_id, chn_id, *count, chn->format.fmt.pix.sizeimage);

	alloc_ctxs[0] = chn->alloc_ctx;
	*num_planes = 1;
	chn->vb_num = *count;
	sizes[0] = chn->format.fmt.pix.sizeimage;

	return 0;
}

/*
 * get_input_scan_method -
 * get input scan method from sensor driver
 *
 */
static void get_input_scan_method(struct input_video_attr *input_video)
{
	int ret;
	struct sensor_attr *sensor = &input_video->sensor;
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;

	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_SCAN_METHOD, &sensor->input_method);
	if (ret) {
		pr_debug("%s get scan method NONE, set PROGRESSIVE.\n", __func__);
		/*default method is progressive*/
		sensor->input_method = SCAN_METHOD_PROGRESSIVE;
	}
}

/*
 * get_dvp_bits_width -
 * get dvp bits width
 *
 */
static int get_dvp_bits_width(struct input_video_attr *input_video)
{
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;

	return camera_isp_get_bits_width(isp_struct);
}

/*
 * set_pinctrl_vi -
 * set pinctrl of vi port
 *
 * @pname:			name of pinctl
 */
static int set_pinctrl_vi(struct input_video_attr *input_video, enum pinctrl_name pname)
{
	char *pnames[] = {
		"mipi0_2lane",
		"mipi0_1lane",
		"mipi1_2lane",
		"mipi1_1lane",
		"csi0_sclk",
		"csi1_sclk",
		"dvp0_12bits",
		"dvp0_10bits",
		"dvp0_8bits",
		"dvp1_12bits",
		"dvp1_10bits",
		"dvp1_8bits",
	};
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct pinctrl_state *pstate;
	int ret;

	pr_debug("%s %d\n", __func__, __LINE__);

	if (pname >= sizeof(pnames)/sizeof(pnames[0])) {
		pr_err("%s pname:%d not support\n", __func__, pname);
		return -1;
	}

	pstate = pinctrl_lookup_state(hw->pinctrl, pnames[pname]);
	if (IS_ERR(pstate)) {
		pr_err("%s pinctrl_lookup_state couldn't find %s state\n"
				, __func__, pnames[pname]);
		return -1;
	}

	/*set pinctrl state to NULL*/
	hw->pinctrl->state = NULL;

	ret = pinctrl_select_state(hw->pinctrl, pstate);
	if (ret) {
		pr_err("%s pinctrl_select_state fail ret:%d\n"
				, __func__, ret);
		return -1;
	}

	return 0;
}

/*
 * dvp_init -
 * dvp interface init
 *
 */
static int dvp_init(struct input_video_attr *input_video)
{
#ifdef CONFIG_MACH_AK39EV330
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
#endif
	struct sensor_attr *sensor = &input_video->sensor;
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	int input_id = input_video->input_id;
	int ret;
	int bits;
	int io_level;
	bool is_first_port = is_first_input(input_video);

	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_IO_LEVEL, &io_level);
	if (ret) {
		pr_err("%s get io_level fail\n", __func__);
		return -1;
	}

	switch (io_level) {
		case IO_LEVEL_1V8:
			io_level = CAMERA_IO_LEVEL_1V8;
			break;

		case IO_LEVEL_2V5:
			io_level = CAMERA_IO_LEVEL_2V5;
			break;

		case IO_LEVEL_3V3:
			io_level = CAMERA_IO_LEVEL_3V3;
			break;

		default:
			io_level = CAMERA_IO_LEVEL_3V3;
			break;
	}

	bits = get_dvp_bits_width(input_video);
	switch (bits) {
		case 12:
			set_pinctrl_vi(input_video, is_first_port ? PNAME_DVP0_12BITS:PNAME_DVP1_12BITS);
			if (is_first_port)
				camera_ctrl_set_dvp_port(input_id, io_level, bits);
			break;
		case 10:
			set_pinctrl_vi(input_video, is_first_port ? PNAME_DVP0_10BITS:PNAME_DVP1_10BITS);
			if (is_first_port)
				camera_ctrl_set_dvp_port(input_id, io_level, bits);
			break;
		case 8:
			set_pinctrl_vi(input_video, is_first_port ? PNAME_DVP0_8BITS:PNAME_DVP1_8BITS);
			if (is_first_port)
				camera_ctrl_set_dvp_port(input_id, io_level, bits);
			break;
		default:
			pr_err("%s bits:%d not support\n", __func__, bits);
			set_pinctrl_vi(input_video, is_first_port ? PNAME_DVP0_8BITS:PNAME_DVP1_8BITS);
			if (is_first_port)
				camera_ctrl_set_dvp_port(input_id, io_level, bits);
			break;
	}

#ifdef CONFIG_MACH_AK39EV330
	if (input_id == 0) {
		hw->map0 = __raw_readl(AK_VA_CAMERA + 0x60);
		hw->map1 = __raw_readl(AK_VA_CAMERA + 0x64);
	}
#endif

	return 0;
}


static int mipi_cfg_once(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	unsigned long flags;
	int mipi_had_init = 0;
	bool is_dual = is_dual_sensor_mode(input_video);

	if (is_dual) {
		spin_lock_irqsave(&hw->lock, flags);
		if (hw->dual_mipi_once_had_init)
			mipi_had_init = 1;
		spin_unlock_irqrestore(&hw->lock, flags);
	}

	if (!mipi_had_init) {
		hw->internal_pclk_res = camera_ctrl_set_mipi_csi_pclk(hw->internal_pclk);
		camera_mipi_ip_prepare(is_dual ? MIPI_MODE_DUAL:MIPI_MODE_SINGLE);
	}

	return 0;
}

/*
 * mipi_init -
 * set mipi port mode
 *
 */
static int mipi_init(struct input_video_attr *input_video)
{
	struct sensor_attr *sensor = &input_video->sensor;
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	int input_id = input_video->input_id;
	int mipi_id = input_id;
	int lanes;
	int mhz;
	int ret;
	bool is_first_port = is_first_input(input_video);

	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_MIPI_LANES, &lanes);
	if (ret) {
		pr_err("%s get lanes fail, ret:%d, mipi_id:%d\n", __func__, ret, mipi_id);
		return -1;
	}

	if (lanes < 0 || lanes > 2) {
		pr_err("%s lanes:%d not support\n", __func__, lanes);
		return -1;
	}

	switch (mipi_id) {
		case 0:
			set_pinctrl_vi(input_video,
					(lanes == 2) ? PNAME_MIPI0_2LANE:PNAME_MIPI0_1LANE);
			break;
		case 1:
			set_pinctrl_vi(input_video,
					(lanes == 2) ? PNAME_MIPI1_2LANE:PNAME_MIPI1_1LANE);
			break;
		default:
			break;
	};

	mipi_cfg_once(input_video);

	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_MIPI_MHZ, &mhz);
	if (ret) {
		pr_err("%s get mipi mhz fail, ret:%d, mipi_id:%d\n", __func__, ret, mipi_id);
		return -EINVAL;
	}

	camera_mipi_ip_port_cfg(is_first_port ? MIPI_PORT_0:MIPI_PORT_1, mhz, lanes);

	return 0;
}

/*
 * set_interface -
 * set dvp or mipi mode
 *
 * @ak_cam:			host struct
 */
static int set_interface(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct sensor_attr *sensor = &input_video->sensor;
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	int interface;
	int ret;
	bool is_first_port = is_first_input(input_video);

	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_INTERFACE, &interface);
	if (ret)
		return -EINVAL;

	if (is_first_port) {
		/*open isp clk gate (another named: VCLK)*/
		if (clk_prepare_enable(hw->isp_clk)) {
			pr_err("%s prepare & enable isp_clk fail\n", __func__);
			return -ENODEV;
		}
	}

	switch (interface) {
		case DVP_INTERFACE:
			pr_err("%s dvp\n", __func__);
			dvp_init(input_video);
			break;

		case MIPI_INTERFACE:
			pr_err("%s mipi\n", __func__);
			mipi_init(input_video);
			break;

		default:
			pr_debug("%s interface:%d not support\n", __func__, interface);
			break;
	}

	return 0;
}

/*
 * set_pclk_polar -
 * set pclk polar
 *
 * NOTE: isp_conf file must had load to isp
 */
static int set_pclk_polar(struct input_video_attr *input_video)
{
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int input_id = input_video->input_id;
	int is_rising;
	int pclk_polar;

	pr_debug("%s input_id:%d\n", __func__, input_id);

	pclk_polar = camera_isp_get_pclk_polar(isp_struct);
	switch (pclk_polar) {
		case POLAR_RISING:
			is_rising = 1;
			break;
		case POLAR_FALLING:
			is_rising = 0;
			break;
		default:
			printk("pclk polar wrong: %d\n", pclk_polar);
			return -1;;
	}

	pr_err("%s pclk edge is %s\n", __func__, is_rising ? "rising":"falling");

	camera_ctrl_set_pclk_polar(input_id, is_rising);

	return 0;
}

/*
 * set_sclk -
 * set sclk enable and config frequency
 *
 * @force_sclk_mhz:		force sclk to the frequcy
 * @RETURN: 0-success, others-fail
 */
static int set_sclk(struct input_video_attr *input_video, int force_sclk_mhz)
{
	struct sensor_attr *sensor = &input_video->sensor;
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct clk		*sclk;
	unsigned long sr;
	int sclk_mhz = force_sclk_mhz;
	bool is_first_port = is_first_input(input_video);

	pr_debug("%s %d\n", __func__, __LINE__);

	if (sclk_mhz <= 0) {
		/*
		 * NOTE: maybe get fail at here.
		 * becasue subdev(sensor) hadnot attach to async notifier。
		 * if fail set default sclk_mhz=24MHZ.
		 */
		sclk_mhz = sensor_cbi->cb->sensor_get_mclk_func(sensor_cbi->arg);
		if (sclk_mhz <= 0) {
			sclk_mhz = DEFAULT_SCLK_FREQ;
			pr_err("%s set default sclk:%dMHZ\n", __func__, sclk_mhz);
		}
	}

	if (is_first_port) {
		if (is_first_port)
			sclk = hw->sclk0;
		else
			sclk = hw->sclk1;

		if (clk_prepare_enable(sclk)) {
			pr_err("%s prepare & enable adchs fail sclk:%p\n", __func__, sclk);
			return -ENODEV;
		}

		clk_set_rate(sclk, sclk_mhz * 1000000);
		pr_debug("%s set rate end\n", __func__);
		sr = clk_get_rate(sclk);
		pr_debug("%s get rate:%lu end\n", __func__, sr);
	} else {
		camera_ctrl_set_sclk1(sclk_mhz);
	}

	return 0;
}

/*
 * set_curmode -
 * set ISP working mode, the mode used by start capturing
 *
 */
static int set_isp_mode(struct input_video_attr *input_video)
{
	struct sensor_attr *sensor = &input_video->sensor;
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	struct isp_attr *isp = &input_video->isp;
	int bus;
	bool is_dual = is_dual_sensor_mode(input_video);

	bus = sensor_cbi->cb->sensor_get_bus_type_func(sensor_cbi->arg);
	switch (bus) {
		case BUS_TYPE_RAW:
			if (is_dual)
				isp->isp_mode = ISP_RGB_OUT;
			else
				isp->isp_mode = ISP_RGB_VIDEO_OUT;
			break;

		case BUS_TYPE_YUV:
			if (is_dual)
				isp->isp_mode = ISP_YUV_OUT;
			else
				isp->isp_mode = ISP_YUV_VIDEO_OUT;
			break;

		default:
			pr_err("%s bus:%d not support\n", __func__, bus);
			break;
	}

	/*TODO: cfg by app*/
	isp->isp_output_format = YUV420_SEMI_PLANAR;

	return 0;
}

/*
 * vi_interface_init -
 * set video capture interface init
 *
 *
 * NOTES: be sure isp_conf had load
 */
static int vi_interface_init(struct input_video_attr *input_video)
{
	int ret;

	/*
	 * set_interface() need isp_conf dvp bits width,
	 * so must had load isp_conf file to ISP before
	 */
	ret = set_interface(input_video);
	if (ret)
		return ret;

	/*
	 * alway need set pclk polar
	 * dvp mode: set pclk input from sensor
	 * mipi mode: set pclk input from internal pll
	 * */
	/*
	 *	app must had load isp_conf file to ISP
	 * */
	set_pclk_polar(input_video);

	set_sclk(input_video, 0);
	set_isp_mode(input_video);

	return 0;
}

static bool any_chn_opend_all_input(struct ak_camera_dev *ak_cam)
{
	struct input_video_attr *input_video;

	list_for_each_entry(input_video, &ak_cam->input_head, list)
		if (input_video->input_opend_count <= 0)
			return false;

	return true;
}

static void dual_sensor_regs_init(struct sensor_attr *sensor)
{
	struct input_video_attr *input_video = sensor_to_input_video(sensor);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct sensor_cb_info *sensor_cbi;
	AK_ISP_SENSOR_INIT_PARA *para;

	if (hw->dual_sensors_init)
		return;

	if (!any_chn_opend_all_input(ak_cam))
		return;

	/*power on sensors, from input_id 0~N*/
	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		sensor = &input_video->sensor;
		sensor_cbi = sensor->sensor_cbi;
		sensor_cbi->cb->sensor_set_power_on_func(sensor_cbi->arg);
	}

	/*
	 * sensor init order:
	 *
	 * 1) some sensor initial slaver first then master.
	 * 2) some sensor initial master first then slave.
	 * 3) some sensor initial slaver first then master, and salver again.
	 * 4) some sensor initial master first then slaver, and master again.
	 *
	 * beause of above reasons, so execute initial sensors 2 times at below,
	 * sensor driver process inital order.
	 */

	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		sensor = &input_video->sensor;
		sensor_cbi = sensor->sensor_cbi;
		para = &sensor->para;
		sensor_cbi->cb->sensor_init_func(sensor_cbi->arg, para);
	}

	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		sensor = &input_video->sensor;
		sensor_cbi = sensor->sensor_cbi;
		para = &sensor->para;
		sensor_cbi->cb->sensor_init_func(sensor_cbi->arg, para);
	}

	hw->dual_sensors_init = 1;
}

static void sensor_regs_init(struct sensor_attr *sensor)
{
	AK_ISP_SENSOR_INIT_PARA *para = &sensor->para;
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;

	sensor_cbi->cb->sensor_set_power_on_func(sensor_cbi->arg);
	sensor_cbi->cb->sensor_init_func(sensor_cbi->arg, para);
}

/*
 * host_set_isp_timing -
 * set timing to isp
 *
 * @arg:			private data
 * @isp_timing:		timing info
 */
static int host_set_isp_timing(void *arg, struct isp_timing_info *isp_timing)
{
	struct chn_attr *chn = (void *)arg;
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;

	pr_debug("%s %d\n", __func__, __LINE__);

	camera_isp_set_misc_attr_ex(
			isp->isp_struct,
			isp_timing->oneline, isp_timing->fsden,
			isp_timing->hblank, isp_timing->fsdnum);
	return 0;
}

/*
 * set_isp_timing_cb -
 * set timing callback to sensor driver
 *
 * @host_arg:			host private data
 */
static int set_isp_timing_cb(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct sensor_attr *sensor = &input_video->sensor;
	struct v4l2_subdev *sd = sensor->sd;
	struct isp_timing_cb_info isp_timing_cb_info = {
		.isp_timing_arg = chn,
		.set_isp_timing = host_set_isp_timing,
	};
	struct v4l2_control ctrl;

	ctrl.id = SET_ISP_MISC_CALLBACK;
	ctrl.value = (int)&isp_timing_cb_info;
	if (v4l2_subdev_call(sd, core, s_ctrl, &ctrl)) {
		pr_err("%s set ctrl.id:%d fail\n", __func__, ctrl.id);
		return -EINVAL;
	}

	return 0;
}

/*
 * set_ae_fast_default -
 * get ae_fast_default from sensor driver, then push them to isp
 *
 */
static int set_ae_fast_default(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct sensor_attr *sensor = &input_video->sensor;
	struct isp_attr *isp = &input_video->isp;
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	struct ae_fast_struct ae_fast;
	int ret;

	pr_debug("%s\n", __func__);

	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_AE_FAST_DEFAULT, &ae_fast);
	if (ret) {
		pr_debug("%s get ae fast default NONE\n", __func__);
		return -1;
	}

	return camera_isp_set_ae_fast_struct_default(isp->isp_struct, &ae_fast);
}

/*
 * ak_vb2_buf_init -
 * vb2 init
 *
 * @vb2_b:				vb2 buffer
 */
static int ak_vb2_buf_init(struct vb2_buffer *vb2_b)
{
	struct vb2_v4l2_buffer *vb2_v4l2_b = vb2_b_to_vb2_v4l2_b(vb2_b);
	struct ak_camera_buffer *ak_b = vb2_v4l2_b_to_ak_b(vb2_v4l2_b);
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vb2_b->vb2_queue);
	struct chn_attr *chn = ak_cam_fh_to_chn(akfh);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	struct sensor_attr *sensor = &input_video->sensor;
	unsigned int yaddr_ch;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int index = vb2_b->index;
	int ret;
	bool is_dual = is_dual_sensor_mode(input_video);

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	/*fill phyaddr for every buf*/
	yaddr_ch = vb2_dma_contig_plane_dma_addr(vb2_b, 0);
	chn->phyaddr[index] = yaddr_ch;

	/*add to list*/
	INIT_LIST_HEAD(&ak_b->queue);

	/*streamon now*/
	if (index == chn->vb_num - 1) {
		if (!input_video->input_route_init) {
			/*no chn had cfg hw, so set share pins, clocks, capture interface and so on*/
			ret = vi_interface_init(input_video);
			if (ret < 0) {
				pr_err("%s vi interface init fail\n", __func__);
				goto done;
			}

			/*set host callback to sensor*/
			set_isp_timing_cb(chn);

			/*set ae fast info*/
			set_ae_fast_default(chn);

			/*do sensor register initial after pins are cfg*/
			if (is_dual)
				dual_sensor_regs_init(sensor);
			else if (isp->isp_status != ISP_STATUS_STOP)
				 /* if application reuse the isp then donot re-config sensor */
				sensor_regs_init(sensor);

			get_input_scan_method(input_video);

			input_video->input_route_init = 1;
		}

		/*set chn state*/
		chn->chn_state = CHN_STATE_ALL_BUFFER_INIT;
	}

	return 0;

done:
	return ret;
}

/*
 * ak_vb2_buf_prepare -
 * vb2 prepare
 *
 * @vb2_b:				vb2 buffer
 */
static int ak_vb2_buf_prepare(struct vb2_buffer *vb2_b)
{
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vb2_b->vb2_queue);
	struct chn_attr *chn = ak_cam_fh_to_chn(akfh);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	return 0;
}

static bool dual_is_pause_nobuf_all_input(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct input_video_attr *t_input_video;

	list_for_each_entry(t_input_video, &ak_cam->input_head, list)
		if (!input_video->dual_mode_state_pause_nobuf)
			return false;

	return true;
}

/*
 * ak_vb2_buf_queue -
 * vb2 queue
 *
 * @vb2_b:				vb2 buffer
 */
static void ak_vb2_buf_queue(struct vb2_buffer *vb2_b)
{
	struct vb2_v4l2_buffer *vb2_v4l2_b = vb2_b_to_vb2_v4l2_b(vb2_b);
	struct ak_camera_buffer *ak_b = vb2_v4l2_b_to_ak_b(vb2_v4l2_b);
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vb2_b->vb2_queue);
	struct chn_attr *chn = ak_cam_fh_to_chn(akfh);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	unsigned long flags;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int index = vb2_b->index;

	size_t new_size;

	pr_debug("%s %d, input_id:%d, chn_id:%d sizeimage:%d\n",
			__func__, __LINE__, input_id, chn_id, chn->format.fmt.pix.sizeimage);

	new_size = chn->format.fmt.pix.sizeimage;

	if (vb2_plane_size(vb2_b, 0) < new_size) {
		pr_err("Buffer #%d too small (%lu < %zu)\n",
				vb2_v4l2_b->vb2_buf.index, vb2_plane_size(vb2_b, 0), new_size);
		goto error;
	}

	vb2_set_plane_payload(vb2_b, 0, new_size);

	if (chn->chn_state == CHN_STATE_START_STREAMING) {
		/*
		 * step1 of streamon is buf_queue, but active start capturing
		 * in start_streaming that is step2 of streamon, so do nothing
		 * before START_STREAMING.
		 *
		 * if the chn_state is START_STREAMING, should set buffer address
		 * & enable as soon as possible.
		 */
		set_chn_buffer_enable(chn, index);

	}

	/*
	 *	after adding to capture list, then the buffer is visible in irq_handle
	 */
	spin_lock_irqsave(&hw->lock, flags);
	list_add_tail(&ak_b->queue, &chn->capture);
	spin_unlock_irqrestore(&hw->lock, flags);

	if (chn->chn_state == CHN_STATE_START_STREAMING &&
			dual_is_pause_nobuf_all_input(input_video)) {
		/*dual mode had stop because nobuf in all input, so need to restart now*/
		dual_start_capturing_new_one(input_video);
	}

error:
	return;
}

/*
 * ak_vb2_wait_prepare -
 * wait prepare
 *
 * @vq:				the queue of vb
 */
static void ak_vb2_wait_prepare(struct vb2_queue *vq)
{
	/*no called*/
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vq);
	struct chn_attr *chn = ak_cam_fh_to_chn(akfh);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	vb2_ops_wait_prepare(vq);
}

/*
 * ak_vb2_wait_finish -
 * wait finish
 *
 * @vq:				the queue of vb
 */
static void ak_vb2_wait_finish(struct vb2_queue *vq)
{
	/*no called*/
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vq);
	struct chn_attr *chn = ak_cam_fh_to_chn(akfh);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	vb2_ops_wait_finish(vq);
}

static void dual_set_current_capture_input(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	int input_id = input_video->input_id;

	hw->capturing_input_id = input_id;
}

static int dual_get_current_capture_input(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;

	return hw->capturing_input_id;
}

static bool check_had_streaming_any_chn_input(struct input_video_attr *input_video)
{
	struct chn_attr *chn;
	int chn_id;

	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];
		if (chn->chn_state == CHN_STATE_START_STREAMING) {
			return true;
		}
	}

	return false;
}

static bool check_had_streaming_any_chn_any_input(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct input_video_attr *t_input_video;

	list_for_each_entry(t_input_video, &ak_cam->input_head, list) {
		if (check_had_streaming_any_chn_input(t_input_video))
			return true;
	}

	return false;
}

static int first_chn_input_isp_init(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int isp_output_format = isp->isp_output_format;
	int isp_mode = isp->isp_mode;
	bool is_dual = is_dual_sensor_mode(input_video);

	if (!is_dual) {
		set_chn_buffers_addr(chn);
		set_chn_buffers_enable(chn);
	}
	set_chn_pp_frame_ctrl(chn);

	camera_isp_vi_apply_mode(isp_struct, isp_mode);
	camera_isp_vo_enable_target_lines_done(isp_struct, 1);
	camera_isp_vo_enable_irq_status(isp_struct,
			(1 << 19) |	//first line
			(1 << 18) |	//target lines done
			(1 << 0));	//all done

	/*
	 * if application reuse the isp then donot call vi_start_capturing to reset
	 * config for isp
	 */
	if (!is_dual) {
		if (isp->isp_status == ISP_STATUS_STOP) {
			//pr_err("ygh %s %d\n",__func__,__LINE__);
			/*in order to update all DMA config & start capturing*/
			ak_isp_vi_start_capturing_one(isp_struct);
			isp->isp_status = ISP_STATUS_START;
			return 0;
		}
	}

	camera_isp_vi_start_capturing(isp_struct, isp_output_format);
	isp->isp_status = ISP_STATUS_START;

	return 0;
}

static int wait_capture_pause_nolock(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	int ret;
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int input_id = input_video->input_id;
	bool is_dual = is_dual_sensor_mode(input_video);

	if (hw->capture_state_pause) {
		ret = 0;
		goto end;
	}

	hw->set_capture_pause = 1;

	if (!is_dual) {
		/*avoid first frame hadnot coming but to stop capuring*/
		int timeout = 1000;
		int frame_count = input_video->frame_count;

		/*check had capuring frames*/
		do {
			msleep(1);
			if (frame_count != input_video->frame_count)
				break;
		} while (timeout-- > 0);

		if (timeout <= 0) {
			pr_info("%s wait timeout input_id:%d\n", __func__, input_id);
			ret = -ETIMEDOUT;
			goto end;
		}

		/*pause ok*/
		camera_isp_pause_isp_capturing(isp_struct);
	}

	pr_debug("%s hw->set_capture_pause = 1\n", __func__);

	ret = wait_event_interruptible_timeout(
				hw->set_capture_pause_queue,
				hw->capture_state_pause,
				msecs_to_jiffies(1000));

	if (ret <= 0) {
		if (ret == 0)
			ret = -ETIMEDOUT;
		else
			ret = -EINTR;
		pr_info("%s wait event ret:%d input_id:%d\n", __func__, ret, input_id);
	} else {
		ret = 0;
	}

end:
	return ret;
}

static inline void flush_enable_buffers(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int chn_id = chn->chn_id;

	if (IS_MAIN_CHN(chn_id))
		camera_isp_enable_buffer_main(isp_struct);
	else if (IS_SUB_CHN(chn_id))
		camera_isp_enable_buffer_sub(isp_struct);
	else if (IS_CH3_CHN(chn_id))
		camera_isp_enable_buffer_ch3(isp_struct);
}

/*
 * ak_vb2_start_streaming -
 * start streaming
 *
 * @q:				the queue of vb
 */
static int ak_vb2_start_streaming(struct vb2_queue *q, unsigned int count)
{
	/*streamon call the function*/
	struct ak_cam_fh *akfh = vb2_get_drv_priv(q);
	struct chn_attr *chn = ak_cam_fh_to_chn(akfh);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
#ifndef CNT_MODE_START_CHN_IN_IRQ
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
#endif
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int done = 0;
	int ret = 0;
	bool is_dual = is_dual_sensor_mode(input_video);
	bool is_first_port = is_first_input(input_video);

	pr_debug("%s %d input_id:%d, chn_id:%d count:%u\n",
			__func__, __LINE__, input_id, chn_id, count);

	/*
	 * initial poll_empty is no true,
	 * isp irq handler post a buffer to pend_done_list by default
	 *
	 */
	chn->poll_empty = 0;

	/*
	 * if no chn_state is START_STREAMING in the input_video then start streaming now.
	 * otherwise start streaming asynchronously.
	 */

	/*find any chn already is START_STREAMING*/
	done = check_had_streaming_any_chn_any_input(input_video);

	if (!done) {
		/*no chn_state is START_STREAMING so start streaming now*/

		if (chn->frame_count > 0)
			/*streamoff->streamon, then drop frames*/
			chn->drop_frame = chn->vb_num * 2;

		first_chn_input_isp_init(chn);

		chn->chn_state = CHN_STATE_START_STREAMING;

		/*
		 * is_dual: is dual sensor need single capture mode.
		 * is_first_port: capturing in the order: sensor0/1/0/1....
		 */
		if (is_dual && is_first_port) {
			ret = dual_start_capturing_new_one(input_video);
		}

	} else if (!is_dual) {
#ifdef CNT_MODE_START_CHN_IN_IRQ
		if (!chn->frame_count)
			/*it is may fail to enable first frame hw buf because irq handler too late.
			 * so drop first frame*/
			chn->drop_frame = 1;
		else if (chn->frame_count > 0)
			/*streamoff->streamon, then drop frames*/
			chn->drop_frame = chn->vb_num * 2;

		/*no chn in any input had streaming & is continue mode*/
		/*some chn_state is START_STREAMING so start streaming asynchronously*/
		chn->chn_state = CHN_STATE_NEED_CFG_BUFFER_ADDR;
#else
		/*
		 * pause isp then config the chn & update isp dma and
		 * resume isp capturing in the end.
		 */
		mutex_lock(&hw->capture_pause_lock);
		do {
			/*do-while to avoid EINTR singal*/
			ret = wait_capture_pause_nolock(input_video);
		} while (ret == -EINTR);
		if (!ret) {
			set_chn_buffers_addr(chn);
			set_chn_buffers_enable(chn);
			set_chn_pp_frame_ctrl(chn);
			flush_enable_buffers(chn);
			camera_isp_resume_isp_capturing(isp_struct);

			chn->chn_state = CHN_STATE_START_STREAMING;
		}

		/*reset flags*/
		hw->capture_state_pause = 0;
		hw->set_capture_pause = 0;
		mutex_unlock(&hw->capture_pause_lock);
#endif
	} else if (!check_had_streaming_any_chn_input(input_video)) {
		/*is dual & isp not init, then to exec first_chn_input_isp_init()*/

		/*
		 * NOTE:
		 * cann't move first_chn_input_isp_init() to irq handler.
		 * because first_chn_input_isp_init() call i2c read/write operations, that
		 * cause kernel WARNNING.
		 *
		 * so stop dual capture then exec first_chn_input_isp_init() in none irq space
		 */
		mutex_lock(&hw->capture_pause_lock);
		ret = wait_capture_pause_nolock(input_video);
		if (!ret) {
			first_chn_input_isp_init(chn);

			chn->chn_state = CHN_STATE_START_STREAMING;

			ret = dual_start_capturing_new_one(input_video);
		}

		/*reset flags*/
		hw->capture_state_pause = 0;
		hw->set_capture_pause = 0;
		mutex_unlock(&hw->capture_pause_lock);
	} else {
		/*is dual & isp had init, then do nothing*/
		chn->chn_state = CHN_STATE_PRE_START_STREAMING;
	}

	return ret;
}

static void clear_all_buf_queue(struct chn_attr *chn)
{
	struct ak_camera_buffer *ak_b, *tmp;

	/*
	 * all buffers from capture are delete, otherwise it cause VB2 warning
	 * */
	list_for_each_entry_safe(ak_b, tmp, &chn->capture, queue) {
		vb2_buffer_done(&ak_b->vb2_v4l2_b.vb2_buf, VB2_BUF_STATE_ERROR);
		list_del_init(&ak_b->queue);
	}

	/*
	 * and all buffers from pend_done_list are delete,
	 * otherwise it cause VB2 warning
	 * */
	list_for_each_entry_safe(ak_b, tmp, &chn->pend_done_list, queue) {
		vb2_buffer_done(&ak_b->vb2_v4l2_b.vb2_buf, VB2_BUF_STATE_ERROR);
		list_del_init(&ak_b->queue);
	}

	list_for_each_entry_safe(ak_b, tmp, &chn->drop_list, queue) {
		vb2_buffer_done(&ak_b->vb2_v4l2_b.vb2_buf, VB2_BUF_STATE_ERROR);
		list_del_init(&ak_b->queue);
	}
}

/*
 * ak_vb2_stop_streaming -
 * stop streaming
 *
 * @q:				the queue of vb
 */
static void ak_vb2_stop_streaming(struct vb2_queue *q)
{
	struct ak_cam_fh *akfh = vb2_get_drv_priv(q);
	struct chn_attr *chn = ak_cam_fh_to_chn(akfh);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	unsigned long flags;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);
	pr_debug("%s %d, R00:0x%x,R04:0x%x,R08:0x%x,R0c:0x%x,R10:0x%x,"
			"R14:0x%x,R18:0x%x,R1c:0x%x,R20:0x%x\n",
			__func__, __LINE__,
			__raw_readl(AK_VA_CAMERA + 0x00),
			__raw_readl(AK_VA_CAMERA + 0x04),
			__raw_readl(AK_VA_CAMERA + 0x08),
			__raw_readl(AK_VA_CAMERA + 0x0c),
			__raw_readl(AK_VA_CAMERA + 0x10),
			__raw_readl(AK_VA_CAMERA + 0x14),
			__raw_readl(AK_VA_CAMERA + 0x18),
			__raw_readl(AK_VA_CAMERA + 0x1c),
			__raw_readl(AK_VA_CAMERA + 0x20)
			);

	spin_lock_irqsave(&hw->lock, flags);

	/*
	 * all channel states are clear
	 * */
	chn->chn_state = CHN_STATE_STOP_STREAMING;
	spin_unlock_irqrestore(&hw->lock, flags);
}

/*
 * ak_vb2_buf_cleanup -
 * vb2 release
 *
 * @vb2_b:				vb2 buffer
 */
static void ak_vb2_buf_cleanup(struct vb2_buffer *vb2_b)
{
	struct vb2_v4l2_buffer *vb2_v4l2_b = vb2_b_to_vb2_v4l2_b(vb2_b);
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vb2_b->vb2_queue);
	struct chn_attr *chn = ak_cam_fh_to_chn(akfh);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct ak_camera_buffer *ak_b = vb2_v4l2_b_to_ak_b(vb2_v4l2_b);
	unsigned long flags;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	spin_lock_irqsave(&hw->lock, flags);

	/* Doesn't hurt also if the list is empty */
	list_del_init(&ak_b->queue);

	spin_unlock_irqrestore(&hw->lock, flags);
}

/*vb2 ops*/
static struct vb2_ops ak_vb2_ops = {
	.queue_setup	= ak_vb2_queue_setup,
	.buf_init		= ak_vb2_buf_init,
	.buf_prepare	= ak_vb2_buf_prepare,
	.buf_queue		= ak_vb2_buf_queue,
	.wait_prepare	= ak_vb2_wait_prepare,
	.wait_finish	= ak_vb2_wait_finish,
	.start_streaming= ak_vb2_start_streaming,
	.stop_streaming	= ak_vb2_stop_streaming,
	.buf_cleanup	= ak_vb2_buf_cleanup,
};

static bool check_chn_multi_opend(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int chn_state = chn->chn_state;

	if (chn->chn_opend_count > 1) {
		pr_warning("%s current multi-opend, input_id:%d, chn_id:%d, chn_state:%d,"
				"chn_opend_count:%d, input_opend_count:%d\n",
				__func__, input_id, chn_id, chn_state,
				chn->chn_opend_count, input_video->input_opend_count);
		return true;
	}

	return false;
}

static int dual_sensor_set_master_slave_flag(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct sensor_attr *sensor = &input_video->sensor;
	struct v4l2_control ctrl;
	struct v4l2_subdev *sd = sensor->sd;
	bool is_first_port = is_first_input(input_video);

	if (is_first_port)
		ctrl.id = SET_MASTER;
	else
		ctrl.id = SET_SLAVER;
	ctrl.value = 0;
	if (v4l2_subdev_call(sd, core, s_ctrl, &ctrl)) {
		pr_err("%s warnning can not set sensor to %s\n",
				__func__, (ctrl.id == SET_MASTER) ? "MASTER":"SLAVER");
	}

	return 0;
}

/*
 * V4L2 file operations
 */

/*
 * ak_cam_file_open -
 * processor when videoX is opened
 *
 * @file:	pointer to videoX file
 */
static int ak_cam_file_open(struct file *file)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct ak_cam_fh *akfh;
	struct vb2_queue *queue;
	struct device *dev = hw->dev;
	unsigned long flags;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;
	bool is_multi_opend;
	bool is_dual = is_dual_sensor_mode(input_video);

	pr_debug("%s %d input_id:%d, chn_id:%d\n",
			__func__, __LINE__, input_id, chn_id);

	/*initing state*/
	//	chn->chn_state = CHN_STATE_INITING;

	/*init: disable the channel buffers*/
	//	disable_current_chn_all_hw_buffer(hw->isp_struct, chn_id);

	spin_lock_irqsave(&hw->lock, flags);

	chn->chn_opend_count++;
	input_video->input_opend_count++;
	hw->camera_opend_count++;//all chn of all sensor input

	is_multi_opend = check_chn_multi_opend(chn);

	spin_unlock_irqrestore(&hw->lock, flags);

	if (is_multi_opend) {
		pr_err("%s current multi-opend, input_id:%d, chn_id:%d, chn_state:%d,"
				"chn_opend_count:%d, input_opend_count:%d\n",
				__func__, input_id, chn_id, chn->chn_state,
				chn->chn_opend_count, input_video->input_opend_count);
		ret = -EBUSY;
		goto mult_opend;
	} else if (chn->chn_state == CHN_STATE_CLOSED) {
		/*when not multi-open the chn_state is CLOSED, then change it*/
		akfh = devm_kzalloc(dev, sizeof(*akfh), GFP_KERNEL);
		if (akfh == NULL) {
			pr_err("%s %d malloc for akfh faile\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto alloc_fail;
		}

		v4l2_fh_init(&akfh->vfh, &chn->vdev);
		v4l2_fh_add(&akfh->vfh);

		queue = &akfh->queue;
		queue->type = hw->type;//V4L2_BUF_TYPE_VIDEO_CAPTURE;
		queue->io_modes = VB2_MMAP | VB2_USERPTR;
		queue->drv_priv = akfh;
		queue->ops = &ak_vb2_ops ;
		queue->mem_ops = &vb2_dma_contig_memops;
		queue->buf_struct_size = sizeof(struct ak_camera_buffer);
		queue->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
		queue->lock = &chn->vlock;

		ret = vb2_queue_init(queue);
		if (ret < 0) {
			pr_err("%s %d vb2_queue_init faile\n", __func__, __LINE__);
			goto queue_init_fail;
		}

		akfh->priv = chn;
		file->private_data = &akfh->vfh;

		chn->chn_state = CHN_STATE_OPEND;
		chn->frame_count = 0;/*reset frame count*/
	} else {
		pr_warning("%s do nothing!! input_id:%d chn_id:%d chn_state:%d\\n",
		__func__, input_id, chn_id, chn->chn_state);
	}

	/*set sensor master/slave*/
	if (is_dual)
		return dual_sensor_set_master_slave_flag(chn);

	return 0;

queue_init_fail:
	v4l2_fh_del(&akfh->vfh);
	v4l2_fh_exit(&akfh->vfh);
	devm_kfree(dev, akfh);
alloc_fail:
mult_opend:
	spin_lock_irqsave(&hw->lock, flags);
	chn->chn_opend_count--;
	input_video->input_opend_count--;
	hw->camera_opend_count--;
	spin_unlock_irqrestore(&hw->lock, flags);
	return ret;
}

static void clear_flags_input(struct input_video_attr *input_video)
{
	input_video->dual_mode_state_pause_nobuf = 0;
}

static void clear_flags_hw(struct hw_attr *hw)
{
	hw->set_capture_pause = 0;
	hw->capture_state_pause = 0;
	hw->dual_sensors_init = 0;
}

/*
 * ak_cam_file_release -
 * processor when videoX is closed
 *
 * @file:			point to videoX file
 */
static int ak_cam_file_release(struct file *file)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct v4l2_fh *vfh = file->private_data;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct device *dev = hw->dev;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	unsigned long flags;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	/*disable the channel buffers*/
	//	disable_current_chn_all_hw_buffer(hw->isp_struct, chn_id);

	spin_lock_irqsave(&hw->lock, flags);

	chn->chn_opend_count--;
	input_video->input_opend_count--;
	hw->camera_opend_count--;

	if (chn->chn_opend_count <= 0) {
		/*current channel no used*/

		clear_all_buf_queue(chn);
		/*
		 * vb2_queue_release() to free v4l2 memory
		 * that alloc by frame_vector_create()
		 */
		vb2_queue_release(&akfh->queue);

		/*reset chn state*/
		chn->chn_state = CHN_STATE_CLOSED;

		/*Release the file handle.*/
		v4l2_fh_del(&akfh->vfh);
		v4l2_fh_exit(&akfh->vfh);
		devm_kfree(dev, akfh);
		file->private_data = NULL;
	}

	if (input_video->input_opend_count <= 0) {
		/*current input video no used*/

		/*flush delay work*/
		flush_delayed_work(&input_video->aec_work);
		flush_delayed_work(&input_video->rawdata_work);

		/*clear flags in this input*/
		clear_flags_input(input_video);
	}

	if (hw->camera_opend_count <= 0) {
		/*all no used*/
		struct input_video_attr *t_input_video;

		/*clocks reset*/
		camera_ctrl_clks_reset();

		/*clear dual flags, because clks reset*/
		hw->dual_mipi_once_had_init = 0;

		/*clks_reset can reset all input clock, so reset all input flag*/
		list_for_each_entry(t_input_video, &ak_cam->input_head, list)
			t_input_video->input_route_init = 0;

		/*clear flags in hw*/
		clear_flags_hw(hw);
	}

	spin_unlock_irqrestore(&hw->lock, flags);
	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	return 0;
}

/*
 * check_done_list_empty - check done_list empty
 * @chn:		pointer to channel
 *
 * @RETURN:	0-no empty, 1-empty
 */
static int check_done_list_empty(struct chn_attr *chn, struct vb2_queue *q)
{
	struct list_head *done_list = &q->done_list;

	if (list_empty(done_list))
		return 1;
	return 0;
}

/*
 * move_to_done_list - move a buffer from pend_done_list to done_list
 * @chn:		pointer to channel
 *
 * @RETURN:	0-fail, 1-success
 */
static int move_to_done_list(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct vb2_buffer *vb2_b;
	struct vb2_v4l2_buffer *vb2_v4l2_b;
	struct ak_camera_buffer *ak_b;
	struct list_head *pend_done_list = &chn->pend_done_list;
	unsigned long flags;
	int move;

	spin_lock_irqsave(&hw->lock, flags);

	if (list_empty(pend_done_list)) {
		/*notice irq post to done_list directly*/
		move = 0;
	} else {
		ak_b = list_first_entry(pend_done_list, struct ak_camera_buffer, queue);
		vb2_v4l2_b = &ak_b->vb2_v4l2_b;
		vb2_b = &vb2_v4l2_b->vb2_buf;
		/*remove a buffer from pend_done_list to v4l2 done_list*/
		list_del_init(&ak_b->queue);
		/*post done*/
		vb2_buffer_done(vb2_b, VB2_BUF_STATE_DONE);

		/*notice irq post to pend_done_list directly but not to done_list*/
		move = 1;
	}

	spin_unlock_irqrestore(&hw->lock, flags);

	return move;
}

/*
 * ak_cam_file_poll -
 * processor when poll camera host
 *
 * @file:			point to videoX file
 * @wait:			poll wait
 */
static unsigned int ak_cam_file_poll(struct file *file, poll_table *wait)
{
	struct v4l2_fh *vfh = file->private_data;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	unsigned long flags;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;
	int empty;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	spin_lock_irqsave(&hw->lock, flags);

	/*1. check done_list if empty*/
	empty = check_done_list_empty(chn, &akfh->queue);

	/*2. get buffer from pend_done_list to done_list*/
	if (empty)
		empty = !move_to_done_list(chn);

	/*
	 * poll_empty = 0: notice irq post to pend_done_list directly not to done_list
	 * poll_empty = 1: notice irq post to done_list directly
	 */
	chn->poll_empty = empty ? 1:0;

	spin_unlock_irqrestore(&hw->lock, flags);

	/*3. poll done_list*/
	ret = vb2_poll(&akfh->queue, file, wait);
	return ret;
}

/*
 * ak_cam_file_mmap -
 * processor when mmap camera host
 *
 * @file:			point to videoX file
 * @wma:			vm struct
 */
static int ak_cam_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct v4l2_fh *vfh = file->private_data;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	return vb2_mmap(&akfh->queue, vma);
}

/*file callbacks*/
static struct v4l2_file_operations ak_cam_file_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
	.open			= ak_cam_file_open,
	.release		= ak_cam_file_release,
	.poll			= ak_cam_file_poll,
	.mmap			= ak_cam_file_mmap,
};

/* -----------------------------------------------------------------------------
 * V4L2 ioctls
 */

/*
 * ak_cam_vidioc_querycap -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @cap:			capability of camera host
 */
static int ak_cam_vidioc_querycap(
		struct file *file, void *fh, struct v4l2_capability *cap)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	/* cap->name is set by the firendly caller:-> */
	strlcpy(cap->card, "AK Camera", sizeof(cap->card));
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

/*
 * ak_cam_vidioc_get_format -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @format:			video data format
 */
static int ak_cam_vidioc_get_format(
		struct file *file, void *fh, struct v4l2_format *format)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	if (format->type != queue->type) {
		pr_err("%s %d input_id:%d, chn_id:%d type err\n",
				__func__, __LINE__, input_id, chn_id);
		return -EINVAL;
	}

	*format = chn->format;

	return 0;
}

struct isp_format_info {
	u32 code;
	u32 pixelformat;
	unsigned int width;
};
static struct isp_format_info formats[] = {
	{ MEDIA_BUS_FMT_Y8_1X8, V4L2_PIX_FMT_YUYV, 12},
};

static void ak_cam_pix_to_mbus(const struct v4l2_pix_format *pix,
		struct v4l2_mbus_framefmt *mbus)
{
	unsigned int i;

	memset(mbus, 0, sizeof(*mbus));
	mbus->width = pix->width;
	mbus->height = pix->height;

	/* Skip the last format in the loop so that it will be selected if no
	 *      * match is found.
	 *           */
	for (i = 0; i < ARRAY_SIZE(formats) - 1; ++i) {
		if (formats[i].pixelformat == pix->pixelformat)
			break;
	}

	mbus->code = formats[i].code;
	mbus->colorspace = pix->colorspace;
	mbus->field = pix->field;
}

static int ak_cam_mbus_to_pix(const struct chn_attr *chn,
		const struct v4l2_mbus_framefmt *mbus,
		struct v4l2_pix_format *pix)
{
	unsigned int bpl = pix->bytesperline;
	unsigned int min_bpl;
	unsigned int i;

	memset(pix, 0, sizeof(*pix));
	pix->width = mbus->width;
	pix->height = mbus->height;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (formats[i].code == mbus->code)
			break;
	}

	if (WARN_ON(i == ARRAY_SIZE(formats)))
		return -1;

	min_bpl = pix->width * formats[i].width / 8;
	bpl = min_bpl;

	pix->pixelformat = formats[i].pixelformat;
	pix->bytesperline = bpl;
	pix->sizeimage = pix->bytesperline * pix->height;
	pix->colorspace = mbus->colorspace;
	pix->field = mbus->field;

	return 0;
}

/*
 * check_ch2_resolution - check ch2 resolution
 * @chn:	pointer to ch2
 *
 * ch2 resolution is half of ch1.
 */
static int check_ch2_resolution(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct chn_attr *chn1 = &input_video->chn[1];
	int ret;

	if ((chn1->width != chn->width * 2) ||
			(chn1->height != chn->height * 2)) {
		pr_err("%s %d fail. ch1_w:%d, ch1_h:%d, ch2_w:%d, ch2_h:%d\n",
				__func__, __LINE__, chn1->width, chn1->height, chn->width, chn->height);
		ret = -EINVAL;
	} else {
		pr_info("%s %d success. ch1_w:%d, ch1_h:%d, ch2_w:%d, ch2_h:%d\n",
				__func__, __LINE__, chn1->width, chn1->height, chn->width, chn->height);
		ret = 0;
	}

	return ret;
}

/*
 * ak_cam_vidioc_set_format -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @format:			video data format
 */
static int ak_cam_vidioc_set_format(
		struct file *file, void *fh, struct v4l2_format *format)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct v4l2_pix_format *pix = &format->fmt.pix;
	struct v4l2_mbus_framefmt fmt;
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret = 0;

	pr_debug("%s %d, input_id:%d, chn_id:%d,pix->width:%d, pix->height:%d\n",
			__func__, __LINE__, input_id, chn_id, pix->width, pix->height);

	if (check_chn_multi_opend(chn)) {
		pr_err("%s chn multi opend\n", __func__);
		return 0;
	}

	chn->width = pix->width;
	chn->height = pix->height;

	if (chn_id == 0)
		camera_isp_vo_set_main_channel_scale(isp_struct, pix->width, pix->height);
	else if (chn_id == 1)
		camera_isp_vo_set_sub_channel_scale(isp_struct, pix->width, pix->height);
	else if (chn_id == 2) {
		/*notes: ch2 resolution is half of ch1, only check that at here*/
		ret = check_ch2_resolution(chn);
		if (ret)
			return ret;
	} else {
		pr_err("%s %d chn_id:%d fail\n", __func__, __LINE__, chn_id);
		return -EINVAL;
	}

	ak_cam_pix_to_mbus(pix, &fmt);
	ret = ak_cam_mbus_to_pix(chn, &fmt, pix);
	if (ret) {
		pr_err("%s %d mbus_to_pix fail\n", __func__, __LINE__);
		return ret;
	}

	chn->video_data_size = pix->sizeimage;

	chn->format = *format;

	return ret;
}

/*
 * ak_cam_vidioc_try_format -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @format:			video data format
 */
static int ak_cam_vidioc_try_format(
		struct file *file, void *fh, struct v4l2_format *format)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	struct v4l2_subdev_format fmt;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	if (format->type != queue->type)
		return -EINVAL;

	ak_cam_pix_to_mbus(&format->fmt.pix, &fmt.format);
	return ak_cam_mbus_to_pix(chn, &fmt.format, &format->fmt.pix);
}

static bool check_isp_opend_input(struct input_video_attr *input_video)
{
	struct isp_attr *isp = &input_video->isp;

	if (isp->isp_param_opend_count <= 0)
		return false;

	return true;
}

/*
 * ak_cam_vidioc_cropcap -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @cropcap:		capibility of crop
 */
static int ak_cam_vidioc_cropcap(
		struct file *file, void *fh, struct v4l2_cropcap *cropcap)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct sensor_attr *sensor = &input_video->sensor;
	struct v4l2_subdev *sd = sensor->sd;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	if (sd == NULL) {
		pr_err("%s %d  subdev is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!check_isp_opend_input(input_video)) {
		pr_err("%s %d  isp is not opend this input\n", __func__, __LINE__);
		return -ENXIO;
	}


	return v4l2_subdev_call(sd, video, cropcap, cropcap);
}

/*
 * ak_cam_vidioc_get_crop -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @crop:			return crop
 */
static int ak_cam_vidioc_get_crop(
		struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	*crop = isp->crop;
	return 0;
}

/*
 * ak_cam_vidioc_set_crop -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @crop:			crop to set
 */
static int ak_cam_vidioc_set_crop(
		struct file *file, void *fh, const struct v4l2_crop *crop)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	struct sensor_attr *sensor = &input_video->sensor;
	struct v4l2_subdev *sd = sensor->sd;
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	struct v4l2_cropcap cropcap;
	void *isp_struct = isp->isp_struct;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int left, top;
	int total_left, total_top;
	int ret;
	int cwidth, cheight;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	if (check_chn_multi_opend(chn)) {
		pr_err("%s chn multi opend\n", __func__);
		return 0;
	}

	ret = v4l2_subdev_call(sd, video, s_crop, crop);
	if (ret < 0) {
		goto done;
	}

	if (sensor_cbi->cb->sensor_get_valid_coordinate_func(sensor_cbi->arg, &left, &top)) {
		left = 0;
		top = 0;
	}
	total_left = left + crop->c.left;
	total_top = top + crop->c.top;

	pr_debug("%s chn_id:%d left:%d, top:%d, c.left:%d, c.top:%d\n",
			__func__, chn_id, left, top, crop->c.left, crop->c.top);

	if (v4l2_subdev_call(sd, video, cropcap, &cropcap)) {
		pr_err("%s get cropcap fail\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	cwidth = crop->c.width;
	cheight = crop->c.height;
	if (cwidth + total_left > cropcap.bounds.width + cropcap.bounds.left)
		cwidth = cropcap.bounds.width + cropcap.bounds.left - total_left;
	if (cheight + total_top > cropcap.bounds.height + cropcap.bounds.top)
		cheight = cropcap.bounds.height+ cropcap.bounds.top - total_top;
	if (cwidth <= 0) {
		pr_err("%s cwidth low than zero(%d,%d,%d,%d,%d).\n",
				__func__, cwidth,
				crop->c.left, crop->c.width,
				cropcap.bounds.left, cropcap.bounds.width);
		ret = -EINVAL;
		goto done;
	}

	if (cheight <= 0) {
		pr_err("%s cheight low than zero(%d,%d,%d,%d,%d).\n",
				__func__, cheight,
				crop->c.top, crop->c.height,
				cropcap.bounds.top, cropcap.bounds.height);
		ret = -EINVAL;
		goto done;
	}

	ret = camera_isp_vi_set_crop(isp_struct, total_left, total_top, cwidth, cheight);
	if (ret < 0) {
		goto done;
	}

	if (isp->isp_status == ISP_STATUS_STOP) {
		/*reuse isp*/
		if ((isp->crop.c.top != crop->c.top) ||
				(isp->crop.c.left != crop->c.left) ||
				(isp->crop.c.width != crop->c.width) ||
				(isp->crop.c.height != crop->c.height)) {
			/*if crop is changed then should reset 3dnr*/
			camera_isp_set_td(isp_struct);
			input_video->td_count = 0;
			set_bit(IRQ_ERROR_NOFEND, &input_video->irq_done_state);
		}
	}

	isp->crop = *crop;

done:
	return ret;
}

/*
 * get_sensor_id -
 * get sensor ID, no I2C operations
 *
 * @chn:		point to channel attr
 * @sensor_id:	point to sensor id structure
 */
static int get_sensor_id(struct chn_attr *chn, struct priv_sensor_id *sensor_id)
{
	int ret;
	struct v4l2_control ctrl;
	int chn_id = chn->chn_id;
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct sensor_attr *sensor = &input_video->sensor;
	struct v4l2_subdev *sd = sensor->sd;

	ctrl.id = GET_SENSOR_ID;
	ret = v4l2_subdev_call(sd, core, g_ctrl, &ctrl);
	if (ret) {
		pr_err("%s chn_id:%d fail\n", __func__, chn_id);
		sensor_id->sensor_id = 0;
		return -1;
	}
	sensor_id->sensor_id = ctrl.value;
	return 0;
}

/*
 * get_phyaddr
 * get physical address. the physical address is backup at queue
 *
 * @chn:			channel
 * @phyaddr:		return physical address
 */
static int get_phyaddr(struct chn_attr *chn, struct priv_phyaddr *phyaddr)
{
	int index = phyaddr->phyaddr;
	int chn_id = chn->chn_id;

	if (index >= MAX_BUFFERS_NUM) {
		pr_err("%s chn_id:%d invalid index:%d\n", __func__, chn_id, index);
		return -1;
	}
	phyaddr->phyaddr = chn->phyaddr[index];
	return 0;
}

/*
 * ak_cam_vidioc_get_param -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @parm:			parameter of stream
 */
static int ak_cam_vidioc_get_param(
		struct file *file, void *fh, struct v4l2_streamparm *parm)
{
	enum ak_video_priv_cmd ak_cmd;
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct param_struct *param;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret = 0;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	param = (void *)parm->parm.raw_data;
	switch(param->cmd) {
		case GET_SENSOR_ID:
			ret = get_sensor_id(chn, (void *)param);
			break;

		case GET_PHYADDR:
			ret = get_phyaddr(chn, (void *)param);
			break;

		default:
			pr_err("%s input_id:%d, chn_id:%d cmd:%d not support\n",
					__func__, input_id, chn_id, ak_cmd);
			ret = -EINVAL;
			break;
	};

	return ret;
}

/*
 * ak_cam_vidioc_set_param -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @parm:			parameter of stream
 */
static int ak_cam_vidioc_set_param(
		struct file *file, void *fh, struct v4l2_streamparm *parm)
{
	enum ak_video_priv_cmd ak_cmd;
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	//	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct param_struct *param;
	//	int *parm_type;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret = 0;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	param = (void *)parm->parm.raw_data;
	switch(param->cmd) {
#ifdef CHNS_DONT_DYNAMIC_ENABLE
		case SET_CHNS_AT_SAME_TIME:
			//ret = set_chns_at_same_time(ak_cam, (void *)parm_type);
			break;
#endif

		case SET_CAPTURE_RAWDATA:
			input_video->rawdata_work_start = 1;
			break;

		default:
			pr_err("%s input_id:%d, chn_id:%d cmd:%d not support\n", __func__, input_id, chn_id, ak_cmd);
			ret = -EINVAL;
			break;
	};

	return ret;
}

/*
 * ak_cam_vidioc_reqbufs -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @rb:				request buffers of the v4l2
 */
static int ak_cam_vidioc_reqbufs(
		struct file *file, void *fh, struct v4l2_requestbuffers *rb)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	ret = vb2_reqbufs(queue, rb);

	return ret;
}

/*
 * ak_cam_vidioc_querybuf -
 *
 * @file:		file of the camera device
 * @fh:			private data
 * @v4l2_b:		the buffer of v4l2
 */
static int ak_cam_vidioc_querybuf(
		struct file *file, void *fh, struct v4l2_buffer *v4l2_b)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	ret = vb2_querybuf(queue, v4l2_b);

	return ret;
}

/*
 * ak_cam_vidioc_qbuf -
 *
 * @file:		file of the camera device
 * @fh:			private data
 * @v4l2_b:		the buffer of v4l2
 */
static int ak_cam_vidioc_qbuf(struct file *file, void *fh, struct v4l2_buffer *v4l2_b)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	ret = vb2_qbuf(queue, v4l2_b);

	return ret;
}

/*
 * ak_cam_vidioc_dqbuf -
 *
 * @file:		file of the camera device
 * @fh:			private data
 * @v4l2_b:		the buffer of v4l2
 */
static int ak_cam_vidioc_dqbuf(struct file *file, void *fh, struct v4l2_buffer *v4l2_b)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	ret = vb2_dqbuf(queue, v4l2_b, file->f_flags);
	/*set video data size, md follow the video*/
	v4l2_b->reserved = chn->video_data_size;

	return ret;
}

/*
 * ak_cam_vidioc_streamon -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @type:			type of video bus
 */
static int ak_cam_vidioc_streamon(
		struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	if (check_chn_multi_opend(chn)) {
		pr_err("%s chn multi opend\n", __func__);
		return 0;
	}

	ret = vb2_streamon(queue, type);

	return ret;
}

/*
 * ak_cam_vidioc_streamoff -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @type:			type of video bus
 */
static int ak_cam_vidioc_streamoff(
		struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	struct hw_attr *hw = &ak_cam->hw;
	struct isp_attr *isp = &input_video->isp;
	//void *isp_struct = isp->isp_struct;
	unsigned long flags;
	unsigned int streaming;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	bool is_dual = is_dual_sensor_mode(input_video);

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	if (check_chn_multi_opend(chn)) {
		pr_err("%s chn multi opend\n", __func__);
		goto done;
	}

	streaming = vb2_is_streaming(queue);
	if (!streaming)
		goto done;

	clear_chn_pp_frame_ctrl(chn);

	spin_lock_irqsave(&hw->lock, flags);
	/*
	 * clear_all_buf_queue() must exec fisrt than vb2_streamoff(),
	 * otherwise cause kernel WARNING in vb2_streamoff().
	 */
	clear_all_buf_queue(chn);
	spin_unlock_irqrestore(&hw->lock, flags);

	vb2_streamoff(queue, type);

	if (!is_dual) {
		if (!check_had_streaming_any_chn_any_input(input_video)) {
			//camera_isp_vi_stop_capturing(isp_struct);
			isp->isp_status = ISP_STATUS_STOP;
		}
	}

done:
	return 0;
}

/*
 * ak_cam_vidioc_enum_input -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @input:			input info
 */
static int ak_cam_vidioc_enum_input(
		struct file *file, void *fh, struct v4l2_input *input)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	if (input->index > 0)
		return -EINVAL;

	strlcpy(input->name, "camera", sizeof(input->name));
	input->type = V4L2_INPUT_TYPE_CAMERA;

	return 0;
}

/*
 * ak_cam_vidioc_g_input -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @input:			input info
 */
static int ak_cam_vidioc_g_input(struct file *file, void *fh, unsigned int *input)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	*input = 0;

	return 0;
}

/*
 * ak_cam_vidioc_s_input -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @input:			input info
 */
static int ak_cam_vidioc_s_input(struct file *file, void *fh, unsigned int input)
{
	struct chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);
	return input == 0 ? 0 : -EINVAL;
}

/*v4l2 ops*/
static const struct v4l2_ioctl_ops ak_cam_ioctl_ops = {
	.vidioc_querycap		= ak_cam_vidioc_querycap,
	.vidioc_g_fmt_vid_cap	= ak_cam_vidioc_get_format,
	.vidioc_s_fmt_vid_cap	= ak_cam_vidioc_set_format,
	.vidioc_try_fmt_vid_cap	= ak_cam_vidioc_try_format,
	.vidioc_g_fmt_vid_out	= ak_cam_vidioc_get_format,
	.vidioc_s_fmt_vid_out	= ak_cam_vidioc_set_format,
	.vidioc_try_fmt_vid_out	= ak_cam_vidioc_try_format,
	.vidioc_cropcap			= ak_cam_vidioc_cropcap,
	.vidioc_g_crop			= ak_cam_vidioc_get_crop,
	.vidioc_s_crop			= ak_cam_vidioc_set_crop,
	.vidioc_g_parm			= ak_cam_vidioc_get_param,
	.vidioc_s_parm			= ak_cam_vidioc_set_param,
	.vidioc_reqbufs			= ak_cam_vidioc_reqbufs,
	.vidioc_querybuf		= ak_cam_vidioc_querybuf,
	.vidioc_qbuf			= ak_cam_vidioc_qbuf,
	.vidioc_dqbuf			= ak_cam_vidioc_dqbuf,
	.vidioc_streamon		= ak_cam_vidioc_streamon,
	.vidioc_streamoff		= ak_cam_vidioc_streamoff,
	.vidioc_enum_input		= ak_cam_vidioc_enum_input,
	.vidioc_g_input			= ak_cam_vidioc_g_input,
	.vidioc_s_input			= ak_cam_vidioc_s_input,
};

/*
 * fline_state -
 * check first line done process:
 * pre-FEND should happen before
 *
 */
static void fline_state(struct input_video_attr *input_video)
{
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;

	if (test_bit(IRQ_FLINE_DONE, &input_video->irq_done_state)) {
		pr_debug("NOFEND\n");
		input_video->nofend_count++;
		camera_isp_set_td(isp_struct);
		input_video->td_count = 0;
		set_bit(IRQ_ERROR_NOFEND, &input_video->irq_done_state);
	}

	set_bit(IRQ_FLINE_DONE, &input_video->irq_done_state);
}

/*
 * fend_state -
 * check frame end done process:
 * pre-FLINE should happen before
 *
 * @ak_cam:			point to camera host
 */
static void fend_state(struct input_video_attr *input_video)
{
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;

	if (!test_bit(IRQ_FLINE_DONE, &input_video->irq_done_state)) {
		pr_debug("NOFLINE\n");
		input_video->nofline_count++;
	}

	if (test_bit(IRQ_ERROR_NOFEND, &input_video->irq_done_state)) {
		if (input_video->td_count++ >= TD_MAX_COUNTER) {
			input_video->td_count = 0;
			clear_bit(IRQ_ERROR_NOFEND, &input_video->irq_done_state);
			camera_isp_reload_td(isp_struct);
		}
	}

	set_bit(IRQ_FEND_DONE, &input_video->irq_done_state);

	clear_bit(IRQ_FLINE_DONE, &input_video->irq_done_state);
	clear_bit(IRQ_FEND_DONE, &input_video->irq_done_state);
}

/*
 * set_chn_buffer_addr -
 * set one hareware buffer address about the channel
 *
 */
static int set_chn_buffer_addr(struct chn_attr *chn, unsigned int yaddr_ch, int index)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int chn_id = chn->chn_id;

	if (IS_MAIN_CHN(chn_id))
		camera_isp_vo_set_main_buffer_addr(isp_struct,
				BUFFER_ONE + index, yaddr_ch);
	else if (IS_SUB_CHN(chn_id))
		camera_isp_vo_set_sub_buffer_addr(isp_struct,
				BUFFER_ONE + index, yaddr_ch);
	else if (IS_CH3_CHN(chn_id))
		camera_isp_vo_set_ch3_buffer_addr(isp_struct,
				BUFFER_ONE + index, yaddr_ch);
	else {
		pr_err("%s chn_id:%d err\n", __func__, chn_id);
		return -1;
	}

#if 0 //for debug pp_frame_ctrl
	if (chn_id == 0 && index == 0) {
		int i;
		for (i = 0; i < MAX_BUFFERS_NUM; i++)
			camera_isp_vo_set_sub_buffer_addr(g_sensor_id,
					BUFFER_ONE + i, yaddr_ch);
	}
#endif
	return 0;
}

/*
 * set_chn_buffer_enable -
 * set one hareware buffer enable about the channel
 *
 */
static int set_chn_buffer_enable(struct chn_attr *chn, int index)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int chn_id = chn->chn_id;

	if (IS_MAIN_CHN(chn_id))
		camera_isp_vo_enable_buffer_main(isp_struct, BUFFER_ONE + index);
	else if (IS_SUB_CHN(chn_id))
		camera_isp_vo_enable_buffer_sub(isp_struct, BUFFER_ONE + index);
	else if (IS_CH3_CHN(chn_id))
		camera_isp_vo_enable_buffer_ch3(isp_struct, BUFFER_ONE + index);
	else {
		pr_err("%s chn_id:%d err\n", __func__, chn_id);
		return -1;
	}

	return 0;
}

/* set_chn_buffers_addr - set chn hw buffers address
 *
 * @RETURN: none
 */
static inline void set_chn_buffers_addr(struct chn_attr *chn)
{
	unsigned int yaddr_ch;
	int i;
	int vb_num = chn->vb_num;

	for (i = 0; i < vb_num; i++) {
		yaddr_ch = chn->phyaddr[i];
		set_chn_buffer_addr(chn, yaddr_ch, i);
	}
}

/* set_chn_buffers_enable - set chn hw buffers enable
 *
 * @RETURN: none
 */
static inline void set_chn_buffers_enable(struct chn_attr *chn)
{
	int i;
	int vb_num = chn->vb_num;

	for (i = 0; i < vb_num; i++)
		set_chn_buffer_enable(chn, i);
}

/*thist frame ctrl must call before start capturing*/
static inline void set_chn_pp_frame_ctrl(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
//	struct sensor_attr *sensor = &input_video->sensor;
	int ch_frame_ctrl;
	int chn_id = chn->chn_id;
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;

	/*
	 * always enable current channel's frame ctrl
	 */
#if 0
	/*comment below: isp hw id not increase when ch_frame_ctrl = 0xaa*/
	if (sensor->input_method == SCAN_METHOD_INTERLACED)
		ch_frame_ctrl = 0xaa;
	else
#endif
		ch_frame_ctrl = 0xff;

	isp_set_pp_frame_ctrl_single(isp_struct, ch_frame_ctrl, chn_id);
}

static inline void clear_chn_pp_frame_ctrl(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int chn_id = chn->chn_id;

	isp_set_pp_frame_ctrl_single(isp_struct, 0x00, chn_id);
}

/*
 * chns_async_enable -
 * enable all channels that is ASNC state
 *
 * NOTE: call in irq
 *
 */
static inline int chns_async_enable(struct input_video_attr *input_video)
{
	struct chn_attr *chn;
	int chn_id;

	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];

		//pr_err("%s %d chn_id:%d\n", __func__, __LINE__, chn_id);
		switch (chn->chn_state) {
			case CHN_STATE_NEED_CFG_BUFFER_ADDR:
				/*
				 * isp working now.
				 *
				 * because hw buffer addr is not cpu-domain registers,
				 * so call set_chn_buffer_addr() and delay a frame for finishing
				 * dma download the hw buffers address.
				 *
				 * then it is safe after that, set_chn_en_framei to set buffer enalbe & pp_frame
				 */
				set_chn_buffers_addr(chn);
				chn->chn_state = CHN_STATE_CFG_BUFFER_ADDR;
				break;

			case CHN_STATE_CFG_BUFFER_ADDR:
				set_chn_buffers_enable(chn);
				set_chn_pp_frame_ctrl(chn);
				/*flush enable hw id, then it's ok to calc first current_buffer_id in next irq_work*/
				flush_enable_buffers(chn);
				chn->chn_state = CHN_STATE_START_STREAMING;
				break;

			case CHN_STATE_START_STREAMING:
				/*the channel is streaming*/
				break;

			default:
				pr_debug("%s chn_state:%d chn_id:%d err\n", __func__, chn->chn_state, chn_id);
				break;
		}
	}
	return 0;
}

static void isp_aec(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;

	if (input_video->rawdata_work_start) {
		input_video->rawdata_work_start = 0;
		schedule_delayed_work(&input_video->rawdata_work, 0);
	}

	/*do not do aec if current capturing raw*/
	if (hw->rawdata_capture_set)
		return;

	//if (camera_isp_is_continuous(isp_struct))
		schedule_delayed_work(&input_video->aec_work, 0);
}

static void isp_irq_work(struct input_video_attr *input_video)
{
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;

	//if (camera_isp_is_continuous(isp_struct))
		camera_isp_irq_work(isp_struct);
}

/*
 * get_chn_buf_id -
 * return hareware buffer identify at the chn_id
 *
 */
static int get_chn_buf_id(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int chn_id = chn->chn_id;
	int id;

	if (IS_MAIN_CHN(chn_id))
		id = camera_isp_vo_get_using_frame_main_buf_id(isp_struct);
	else if (IS_SUB_CHN(chn_id))
		id = camera_isp_vo_get_using_frame_sub_buf_id(isp_struct);
	else if (IS_CH3_CHN(chn_id))
		id = camera_isp_vo_get_using_frame_ch3_buf_id(isp_struct);
	else
		id = -1;

	return id;
}

/*
 * get_active_entry_by_id -
 * get current active buffer
 *
 * @chn:			point to channel attr
 * @id:				id from isp
 */
	static inline struct ak_camera_buffer *
get_active_entry_by_id(struct chn_attr *chn, int id)
{
	struct ak_camera_buffer *ak_b;

	list_for_each_entry(ak_b, &chn->capture, queue) {
		if (id == ak_b->vb2_v4l2_b.vb2_buf.index)
			return ak_b;
	}

	return NULL;
}

/*
 * list_entries_num -
 * calac number of list
 *
 * @head:			point to list
 */
static inline int list_entries_num(struct list_head *head)
{
	int i;
	struct list_head *list = head;

	for (i = 0; list->next != head; i++)
		list = list->next;

	return i;
}

/*
 * pop_one_buf_from_pend_done_list - pop one buffer from pend done list
 * @chn:		pointer to channel
 *
 * @RETURN:	0-success, others-fail
 */
static inline int pop_one_buf_from_pend_done_list(struct chn_attr *chn)
{
	struct vb2_buffer *vb2_b;
	struct vb2_v4l2_buffer *vb2_v4l2_b;
	struct ak_camera_buffer *ak_b;
	struct list_head *pend_done_list = &chn->pend_done_list;
	int chn_id = chn->chn_id;
	int index;

	if (list_empty(pend_done_list)) {
		/*shoud never to here*/
		pr_debug("pop empty\n");
		return -EINVAL;
	}

	/*remove the oldest buffer from pend done list*/
	ak_b = list_first_entry(pend_done_list, struct ak_camera_buffer, queue);
	list_del_init(&ak_b->queue);

	/*enable hw buffer & add the buffer to capture*/
	vb2_v4l2_b = &ak_b->vb2_v4l2_b;
	vb2_b = &vb2_v4l2_b->vb2_buf;
	index = vb2_b->index;

	set_chn_buffer_enable(chn, index);

	/*
	 *	after adding to capture list, then the buffer is visible in irq_handle
	 */
	list_add_tail(&ak_b->queue, &chn->capture);

	pr_debug("pop chn_id:%d, id:%d\n", chn_id, vb2_b->index);

	return 0;
}

static inline int pop_one_buf_from_drop_list(struct chn_attr *chn)
{
	struct vb2_buffer *vb2_b;
	struct vb2_v4l2_buffer *vb2_v4l2_b;
	struct ak_camera_buffer *ak_b;
	struct list_head *drop_list = &chn->drop_list;
	int chn_id = chn->chn_id;
	int index;

	if (list_empty(drop_list)) {
		/*shoud never to here*/
		pr_debug("pop empty\n");
		return -EINVAL;
	}

	/*remove the oldest buffer from pend done list*/
	ak_b = list_first_entry(drop_list, struct ak_camera_buffer, queue);
	list_del_init(&ak_b->queue);

	/*enable hw buffer & add the buffer to capture*/
	vb2_v4l2_b = &ak_b->vb2_v4l2_b;
	vb2_b = &vb2_v4l2_b->vb2_buf;
	index = vb2_b->index;

	set_chn_buffer_enable(chn, index);

	/*
	 *	after adding to capture list, then the buffer is visible in irq_handle
	 */
	list_add_tail(&ak_b->queue, &chn->capture);

	pr_debug("pop chn_id:%d, id:%d\n", chn_id, vb2_b->index);

	return 0;
}

/*
 * disable_chn_hw_buffer -
 * disable hardware buffer about the channel
 *
 * @index:			hardware buffer index about the channel
 */
static int disable_chn_hw_buffer(struct chn_attr *chn, int index)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int chn_id = chn->chn_id;

	if (IS_MAIN_CHN(chn_id))
		camera_isp_vo_disable_buffer_main(isp_struct, BUFFER_ONE + index);
	else if (IS_SUB_CHN(chn_id))
		camera_isp_vo_disable_buffer_sub(isp_struct, BUFFER_ONE + index);
	else if (IS_CH3_CHN(chn_id))
		camera_isp_vo_disable_buffer_ch3(isp_struct, BUFFER_ONE + index);
	else
		return -1;

	return 0;
}

static void pop_one_buf_to_capture(struct input_video_attr *input_video)
{
	struct chn_attr *chn;
	int num;
	int chn_id;

	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		/*
		 * streaming means some hw buffer id corresponding
		 * to this chn_id had enable
		 * */
		chn = &input_video->chn[chn_id];

		if (chn->chn_state != CHN_STATE_START_STREAMING)
			continue;

		num = list_entries_num(&chn->capture);
		if (num <= 1) {
			if (pop_one_buf_from_drop_list(chn))
				/*
				 * run to here indicate less than 2 active hw buffer,
				 * so move one buffer from to capture list.
				 * no need check return value at here. if fail I cannot do anything.
				 */
				pop_one_buf_from_pend_done_list(chn);
		}
	}
}

static bool is_drop_frame_chn(struct chn_attr *chn)
{
	if (chn->drop_frame > 0) {
		chn->drop_frame--;
		pr_debug("drop one frame, chn_id:%d\n",chn->chn_id);
		return true;
	}

	return false;
}

/*
 * irq_handle_continous_mode -
 * irq_handle for continous mode.
 * get current HW id which DMA finish, set the corresponding vb buffer DONE
 *
 */
static int irq_handle_continous_mode(struct input_video_attr *input_video)
{
	struct sensor_attr *sensor = &input_video->sensor;
	struct vb2_v4l2_buffer *vb2_v4l2_b;
	struct vb2_buffer *vb2_b;
	struct ak_camera_buffer *ak_b;
	struct chn_attr *chn;
	struct timeval *timestamp = &input_video->input_timestamp;
	int post_done = 0;
	int chn_id;
	int id;
	int drop = 0;

	input_video->frame_count++;

	v4l2_get_timestamp(timestamp);

	/*check all channels*/
	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];

		/*
		 * START_STREAMING means some hw buffer id corresponding
		 * to this chn_id had enable
		 */
		if (chn->chn_state != CHN_STATE_START_STREAMING)
			continue;

		chn->frame_count++;

		if (is_drop_frame_chn(chn)) {
			drop = 1;
			pr_debug("drop chn_id:%d\n",chn_id);
		}

		if (sensor->input_method == SCAN_METHOD_INTERLACED) {
			/*skip*/
			if (chn->frame_count & 0x1)
				continue;
		}

		/*get hw buffer id corresponding to this chn_id*/
		id = get_chn_buf_id(chn);
		if (id < 0)
			continue;

		ak_b = get_active_entry_by_id(chn, id & 0x7f);
		if (!ak_b) {
			pr_debug("NO active\n");
			continue;
		}

		vb2_v4l2_b = &ak_b->vb2_v4l2_b;
		vb2_b = &vb2_v4l2_b->vb2_buf;

		/*fill vb*/
		vb2_v4l2_b->timestamp = *timestamp;
		vb2_v4l2_b->field = NORMAL_FRAME;
		vb2_v4l2_b->sequence = chn->phyaddr[vb2_b->index];
		if (drop) {
			/*remove the buffer to drop_list*/
			list_move_tail(&ak_b->queue, &chn->drop_list);
		} else if (chn->poll_empty) {
			/*poll had call into so remove the buffer to done_list*/
			chn->poll_empty = 0;
			list_del_init(&ak_b->queue);
			vb2_buffer_done(vb2_b, VB2_BUF_STATE_DONE);
		} else {
			/*remove the buffer to pend_done_list*/
			list_move_tail(&ak_b->queue, &chn->pend_done_list);
		}

		/*disable hw buffer*/
		disable_chn_hw_buffer(chn, vb2_b->index);
		pr_debug("irqcnt chn_id:%d: post done, id:%d\n", chn_id, vb2_b->index);

		post_done = 1;
	}

	return 0;
}

static struct ak_camera_buffer *get_first_entry(struct chn_attr *chn)
{
	struct ak_camera_buffer *ak_b;

	list_for_each_entry(ak_b, &chn->capture, queue) {
		return ak_b;
	}

	return NULL;
}

static int irq_handle_single_mode(struct input_video_attr *input_video)
{
	struct chn_attr *chn;
	struct ak_camera_buffer *ak_b;
	struct vb2_v4l2_buffer *vb2_v4l2_b;
	struct vb2_buffer *vb2_b;
	struct timeval *timestamp = &input_video->input_timestamp;
	int chn_id;
	int drop = 0;

	v4l2_get_timestamp(timestamp);

	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];

		/*START_STREAMING means the chn is ready in single mode*/
		if (chn->chn_state != CHN_STATE_START_STREAMING)
			continue;

		if (is_drop_frame_chn(chn)) {
			drop = 1;
			pr_debug("drop chn_id:%d\n",chn_id);
		}

		/*get first buffer in list*/
		ak_b = get_first_entry(chn);
		if (!ak_b) {
			pr_debug("%s NO ak_b\n", __func__);
			continue;
		}

		vb2_v4l2_b = &ak_b->vb2_v4l2_b;
		vb2_b= &vb2_v4l2_b->vb2_buf;

		/*fill vb2_v4l2_b*/
		vb2_v4l2_b->timestamp = *timestamp;
		vb2_v4l2_b->field = NORMAL_FRAME;
		vb2_v4l2_b->sequence = chn->phyaddr[vb2_b->index];

		if (drop) {
			/*remove the buffer to drop_list*/
			list_move_tail(&ak_b->queue, &chn->drop_list);
		} else if (chn->poll_empty) {
			/*poll had call into so remove the buffer to done_list*/
			chn->poll_empty = 0;
			list_del_init(&ak_b->queue);
			vb2_buffer_done(vb2_b, VB2_BUF_STATE_DONE);
		} else {
			/*remove the buffer to pend_done_list*/
			list_move_tail(&ak_b->queue, &chn->pend_done_list);
		}
	}

	return 0;
}

static bool check_dual_mode_and_not_mine_frame(struct input_video_attr *input_video)
{
	int input_id = input_video->input_id;
	int current_capturing_input_id;
	bool is_dual = is_dual_sensor_mode(input_video);

	if (is_dual) {
		current_capturing_input_id = dual_get_current_capture_input(input_video);
		if (current_capturing_input_id != input_id)
			return true;
	}

	return false;
}

static int dual_start_capturing_new_one(struct input_video_attr *input_video)
{
#ifdef CONFIG_MACH_AK39EV330
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
#endif
	struct isp_attr *isp = &input_video->isp;
	struct ak_camera_buffer *ak_b;
	struct chn_attr *chn;
	void *isp_struct = isp->isp_struct;
	unsigned int yaddr_ch;
	int hw_buf_index = 0;//index alway is zero in single mode
	int input_id = input_video->input_id;
#ifdef CONFIG_MACH_AK39EV330
	int isp_id = isp->isp_id;
#endif
	int chn_id;
	int index;
	int num = 0;

	pr_debug("%s input_id:%d\n", __func__, input_id);

	/*1. set buffer addr*/
	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];

		/*now to update new chn that had ready capturing*/
		if (chn->chn_state == CHN_STATE_PRE_START_STREAMING)
			chn->chn_state = CHN_STATE_START_STREAMING;

		/*START_STREAMING means the chn is ready in single mode*/
		if (chn->chn_state != CHN_STATE_START_STREAMING)
			continue;

		/*get first buffer in list*/
		ak_b = get_first_entry(chn);
		if (!ak_b) {
			pr_debug("%s NO ak_b input_id:%d chn_id:%d\n", __func__, input_id, chn_id);
			continue;
		}

		index = ak_b->vb2_v4l2_b.vb2_buf.index;
		yaddr_ch = chn->phyaddr[index];

		set_chn_buffer_addr(chn, yaddr_ch, hw_buf_index);
		set_chn_buffer_enable(chn, hw_buf_index);

		/*
		 * need set pp_frame_ctrl every time,
		 * because it open chn dynamically in same input in dual mode
		 */
		set_chn_pp_frame_ctrl(chn);

		num++;
	}

	if (!num) {
		input_video->dual_mode_state_pause_nobuf = 1;
		return -EINVAL;
	}

	input_video->dual_mode_state_pause_nobuf = 0;
	dual_set_current_capture_input(input_video);

#ifdef CONFIG_MACH_AK39EV330
	/*bugfix: data maps active for dvp0 & dvp1 at same time*/
	if (isp_id == 0) {
		__raw_writel(hw->map0, AK_VA_CAMERA + 0x60);
		__raw_writel(hw->map1, AK_VA_CAMERA + 0x64);
	} else if (isp_id == 1) {
		__raw_writel(0x76543210, AK_VA_CAMERA + 0x60);
		__raw_writel(0xba98, AK_VA_CAMERA + 0x64);
	}
#endif

	return camera_isp_vi_capturing_one(isp_struct);
}

static int dual_start_capturing_other_new_one(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct input_video_attr *t_input_video;
	int input_id = input_video->input_id;
	int t_input_id;

	list_for_each_entry(t_input_video, &ak_cam->input_head, list) {
		t_input_id = t_input_video->input_id;

		if (t_input_id != input_id)
			return dual_start_capturing_new_one(t_input_video);
	}

	return -EINVAL;
}

static void dual_start_next_frame(struct input_video_attr *input_video)
{
	int current_capturing_input_id;
	int input_id = input_video->input_id;
	bool is_dual = is_dual_sensor_mode(input_video);

	pr_debug("%s input_id:%d\n", __func__, input_id);

	if (!is_dual)
		/*do nothing for continue mode*/
		return;

	current_capturing_input_id = dual_get_current_capture_input(input_video);
	if (current_capturing_input_id != input_id) {
		/*caller should not pass other input_video*/
		return;
	}

	/*
	 * start capturing another input now
	 *
	 * dual mode: is pingpong capturing, sensor0/1/0/1....
	 */
	if (dual_start_capturing_other_new_one(input_video))
	{
		/*if fail to other input than start myself input*/ 
		dual_start_capturing_new_one(input_video);
	}
}

static int irq_process_flags(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	int done = 0;

	if (hw->rawdata_capture_set) {
		hw->rawdata_capture_done = 1;
		wake_up_interruptible(&(hw->rawdata_capture_wait_queue));
		done = 1;
		pr_info("%s rawdata\n", __func__);
	}

	return done;
}

static bool is_drop_frame(struct input_video_attr *input_video)
{
	if (input_video->drop_frame > 0) {
		input_video->drop_frame--;
		return true;
	}

	return false;
}

static bool check_capture_pause_flag(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct isp_attr *isp = &input_video->isp;

	if (hw->set_capture_pause) {
		hw->capture_state_pause = 1;
		wake_up_interruptible(&(hw->set_capture_pause_queue));
		pr_debug("%s %d pause\n", __func__, __LINE__);
		return true;
	}

	if (isp->isp_set_capture_pause) {
		/*pause all input in dual mode*/
		isp->isp_capture_state_pause = 1;
		wake_up_interruptible(&(isp->isp_set_capture_pause_queue));
		pr_debug("%s %d pause\n", __func__, __LINE__);
		return true;
	}

	return false;
}

static void frame_data_process(struct input_video_attr *input_video)
{
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;

	if (irq_process_flags(input_video))
		return;

	if (camera_isp_is_continuous(isp_struct)) {
		if (!is_drop_frame(input_video))
			irq_handle_continous_mode(input_video);

		/*
		 * async enable chn should after frame data process,
		 * because it check START_STREAMING in frame data process
		 */
		chns_async_enable(input_video);
		check_capture_pause_flag(input_video);
	} else {/*single mode*/
		if (!is_drop_frame(input_video))
			irq_handle_single_mode(input_video);
		if (!check_capture_pause_flag(input_video))
			dual_start_next_frame(input_video);
	}
#if 0
	else if (hw->capture_rawdata_start) {
		/*process capturing rawdata in continue mode*/
		hw->capture_rawdata_done = 1;
		wake_up_interruptible(&(hw->capture_rawdata_wait_queue));
	}
#endif
}

static int aec_process(struct input_video_attr *input_video)
{
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;

	ak_isp_awb_work(isp_struct);
	ak_isp_ae_work(isp_struct);

	return 0;
}

static int set_sensor_fps_async(struct sensor_attr *sensor)
{
	struct input_video_attr *input_video = sensor_to_input_video(sensor);
	int input_id = input_video->input_id;
	int new_fps = sensor->new_fps;
	int fps_update = sensor->fps_update;
	int ret = 0;

	if (fps_update) {
		struct v4l2_control ctrl;
		struct v4l2_subdev *sd = sensor->sd;

		ctrl.id = SET_FPS_DIRECT;
		ctrl.value = new_fps;
		if (v4l2_subdev_call(sd, core, s_ctrl, &ctrl)) {
			pr_err("%s inpud_id:%d set fps:%d fail\n", __func__, input_id, new_fps);
			ret = -EINVAL;
		}

		sensor->fps_update = 0;
		pr_debug("%s input_id:%d set fps:%d success\n", __func__, input_id, new_fps);
	}

	return ret;
}

static void aec_work(struct work_struct *work)
{
	struct input_video_attr *input_video =
		container_of(work, struct input_video_attr, aec_work.work);
	struct sensor_attr *sensor = &input_video->sensor;

	set_sensor_fps_async(sensor);

	aec_process(input_video);
}

static void rawdata_work(struct work_struct *work)
{
	struct input_video_attr *input_video =
		container_of(work, struct input_video_attr, rawdata_work.work);
	struct chn_attr *chn = &input_video->chn[0];/*main chn*/

	if (chn->chn_state != CHN_STATE_START_STREAMING) {
		pr_err("%s main chn not working, cannot capture rawdata\n", __func__);
		return;
	}

	rawdata_capture_one(chn);
}

static int get_input_data_format(struct chn_attr *chn, struct rawdata_header *header)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	struct input_data_format idf;
	int sx, sy, width, height;
	int ret;

	header->magic = RAWDATA_HEADER_MAGIC;
	header->header_size = RAWDATA_HEADER_SIZE;
	ret = camera_isp_vi_get_crop(isp_struct, &sx, &sy, &width, &height);
	if (ret)
		return -EINVAL;
	ret = camera_isp_vi_get_input_data_format(isp_struct, &idf);
	if (ret)
		return -EINVAL;

	switch (idf.df) {
		case BAYER_RAW_DATA:
			header->format = BAYER_RAWDATA;
			header->bits_width = idf.data_width;
			break;
		case YUV422_DATA:
			header->format = YUV422_16B_DATA;
			header->bits_width = 16;
			break;
		default:
			return -EINVAL;
			break;
	}

	header->width = width;
	header->height = height;
	header->rawdata_size = header->width * header->height * header->bits_width / 8;

	return 0;
}

static void rawdata_insert_header(
		struct chn_attr *chn, struct ak_camera_buffer *ak_b,
		struct rawdata_header *header)
{
	struct vb2_v4l2_buffer *vb2_v4l2_b = &ak_b->vb2_v4l2_b;
	struct vb2_buffer *vb2_b = &vb2_v4l2_b->vb2_buf;
	unsigned char *vaddr = (void *)CONFIG_PAGE_OFFSET +\
						   (chn->phyaddr[vb2_b->index] - CONFIG_PHYS_OFFSET);
	int header_size = header->header_size;
	int i;

	/*move space for header*/
	for (i = header->rawdata_size - 1; i >= 0; i--)
		*(vaddr + i + header_size) = *(vaddr + i);

	/*clear header size*/
	memset(vaddr, 0, header_size);

	/*fill header*/
	memcpy(vaddr, header, sizeof(*header));

	pr_debug("magic:0x%x, header_size:%d, format:%d, rawdata_size:%d,"
			"bits_width:%d, width:%d, height:%d\n",
			header->magic, header->header_size, header->format, header->rawdata_size,
			header->bits_width, header->width, header->height);
}

static void rawdata_clean_header(
		struct chn_attr *chn, struct ak_camera_buffer *ak_b,
		struct rawdata_header *header)
{
	struct vb2_v4l2_buffer *vb2_v4l2_b = &ak_b->vb2_v4l2_b;
	struct vb2_buffer *vb2_b = &vb2_v4l2_b->vb2_buf;
	unsigned char *vaddr = (void *)CONFIG_PAGE_OFFSET +\
						   (chn->phyaddr[vb2_b->index] - CONFIG_PHYS_OFFSET);
	int header_size = header->header_size;

	/*clear header size*/
	memset(vaddr, 0, header_size);
}

static void rawdata_vb_done(struct ak_camera_buffer *ak_b)
{
	struct vb2_v4l2_buffer *vb2_v4l2_b = &ak_b->vb2_v4l2_b;
	struct vb2_buffer *vb2_b = &vb2_v4l2_b->vb2_buf;

	v4l2_get_timestamp(&vb2_v4l2_b->timestamp);

	/*set RAWDATA flag*/
	vb2_v4l2_b->field = RAWDATA_FRAME;

	/*remove a buffer from capture or pend_done_list*/
	list_del_init(&ak_b->queue);

	/*post done*/
	vb2_buffer_done(vb2_b, VB2_BUF_STATE_DONE);
}

static int rawdata_wait_app_process_finish(
		struct chn_attr *chn, int index, int timeout)
{
	struct ak_camera_buffer *ak_b;
	int msec = 100;

	do {
		msleep(msec);
		timeout -= msec;

		list_for_each_entry(ak_b, &chn->capture, queue)
			if (index == ak_b->vb2_v4l2_b.vb2_buf.index)
				return 0;
		list_for_each_entry(ak_b, &chn->pend_done_list, queue)
			if (index == ak_b->vb2_v4l2_b.vb2_buf.index)
				return 0;
		list_for_each_entry(ak_b, &chn->drop_list, queue)
			if (index == ak_b->vb2_v4l2_b.vb2_buf.index)
				return 0;
	} while (timeout > 0);

	return -ETIMEDOUT;
}

static int rawdata_wait_all_buf_in_capture_list(struct chn_attr *chn)
{
	struct ak_camera_buffer *ak_b = NULL;
	int retry = 5;
	int msec = 200;
	int count;

	do {
		count = 0;
		msleep(msec);

		list_for_each_entry(ak_b, &chn->capture, queue) {
			count++;
		}

		list_for_each_entry(ak_b, &chn->pend_done_list, queue) {
			count++;
		}

		list_for_each_entry(ak_b, &chn->drop_list, queue) {
			count++;
		}
	} while (retry-- > 0 &&
			count != chn->vb_num);

	/*pop all buf from pend to capture*/
	while (!pop_one_buf_from_pend_done_list(chn));

	if (count != chn->vb_num) {
		pr_err("%s wait all buf in capture list fail\n", __func__);
		return -EBUSY;
	}

	return 0;
}

static struct ak_camera_buffer *get_vb_in_capture_list(struct chn_attr *chn, int index)
{
	struct ak_camera_buffer *ak_b = NULL;

	list_for_each_entry(ak_b, &chn->capture, queue) {
		if (index == ak_b->vb2_v4l2_b.vb2_buf.index)
			return ak_b;
	}

	return NULL;
}

static void rawdata_set_addr(struct chn_attr *chn, struct ak_camera_buffer *ak_b, int index)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	bool is_dual = is_dual_sensor_mode(input_video);

	if (is_dual) {
		struct vb2_v4l2_buffer *vb2_v4l2_b = &ak_b->vb2_v4l2_b;
		struct vb2_buffer *vb2_b = &vb2_v4l2_b->vb2_buf;
		unsigned int yaddr_ch;

		yaddr_ch = vb2_dma_contig_plane_dma_addr(vb2_b, 0);
		set_chn_buffer_addr(chn, yaddr_ch, index);
	}
}

static int rawdata_wait_capture_done(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	int ret;

	/*wait for captuing rawdata finish*/
	ret = wait_event_interruptible_timeout(
			hw->rawdata_capture_wait_queue,
			hw->rawdata_capture_done,
			msecs_to_jiffies(1000));
	if (ret <= 0) {
		if (ret == 0)
			ret = -ETIMEDOUT;
		else
			ret = -EINTR;
		pr_debug("%s wait event ret:%d\n", __func__, ret);
	} else {
		ret = 0;
	}

	return ret;
}

static int __rawdata_capture_one_nolock(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct isp_attr *isp = &input_video->isp;
	struct ak_camera_buffer *ak_b = NULL;
	struct rawdata_header header;
	void *isp_struct = isp->isp_struct;
	int index = 0;
	bool is_dual = is_dual_sensor_mode(input_video);

	/*wait all buf in capture list*/
	if (rawdata_wait_all_buf_in_capture_list(chn))
		return -ETIMEDOUT;

	/*get vb0 buf ptr*/
	ak_b = get_vb_in_capture_list(chn, index);
	if (!ak_b)
		return -ETIMEDOUT;

	/*get input data format*/
	if (get_input_data_format(chn, &header)) {
		pr_err("%s get input data format fail\n", __func__);
		return -EINVAL;
	}

	/*set hw buf0 addr&enable */
	rawdata_set_addr(chn, ak_b, index);

	/*set isp mode*/
	camera_isp_vi_apply_mode(isp_struct, ISP_RAW_OUT);

	/*set the current input that capturing for dual*/
	if (is_dual)
		dual_set_current_capture_input(input_video);

	/*set capture raw flag*/
	hw->rawdata_capture_done = 0;
	hw->rawdata_capture_set = 1;

#ifdef CONFIG_MACH_AK37D
	if (is_dual) {
		/*select  single/dual mipi  sensor*/
		/*37D dual mode get raw upload fail, so reset to single*/
		unsigned long value;
		value = __raw_readl(AK_VA_SYSCTRL + 0x14);
		value &= ~(1<<29);
		__raw_writel(value, AK_VA_SYSCTRL + 0x14);
	}
#endif

	/*start isp capturing*/
	camera_isp_resume_isp_capturing(isp_struct);

	/*wait isp finish capture rawdata*/
	if (rawdata_wait_capture_done(chn))
		return -ETIMEDOUT;

	/*insert header*/
	rawdata_insert_header(chn, ak_b, &header);

	/*done the rawdata vb*/
	rawdata_vb_done(ak_b);

	/*wait app process the rawdata*/
	if (rawdata_wait_app_process_finish(chn, index, 600)) {
		pr_err("rawdata_wait_app_process_finish fail\n");
		return -ETIMEDOUT;
	}

	/*clean header*/
	rawdata_clean_header(chn, ak_b, &header);

	return 0;
}

static void rawdata_resume_to_normal(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int isp_mode = isp->isp_mode;
	bool is_dual = is_dual_sensor_mode(input_video);

	/*resume isp mode*/
	camera_isp_vi_apply_mode(isp_struct, isp_mode);

	/*reset rawdata flags*/
	hw->rawdata_capture_done = 0;
	hw->rawdata_capture_set = 0;

	/*reset pause flags*/
	hw->capture_state_pause = 0;
	hw->set_capture_pause = 0;

	/*
	 * drop frame
	 *
	 * if hw id is wrong, application get the rawdata to encode,
	 * that make encoder no responding
	 */
	input_video->drop_frame = MAX_BUFFERS_NUM;

#ifdef CONFIG_MACH_AK37D
	if (is_dual) {
		/*select  single/dual mipi  sensor*/
		/*37D resume to dual*/
		unsigned long value;
		value = __raw_readl(AK_VA_SYSCTRL + 0x14);
		value |= (1<<29);
		__raw_writel(value, AK_VA_SYSCTRL + 0x14);
	}
#endif

	if (is_dual) {
		dual_start_capturing_new_one(input_video);
	} else {
		camera_isp_resume_isp_capturing(isp_struct);
	}

}

static int rawdata_capture_one_nolock(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	int ret;
#if (defined(CONFIG_MACH_AK37D) || defined(CONFIG_MACH_AK39EV330))
	bool is_dual = is_dual_sensor_mode(input_video);
#endif

	pr_debug("%s\n", __func__);

#if (defined(CONFIG_MACH_AK37D) || defined(CONFIG_MACH_AK39EV330))
	/*H3D & H3B dual mode not support rawdata*/
	if (is_dual) {
		pr_info("%s dual not support rawdata\n", __func__);
		return -EINVAL;
	}
#endif

	if (wait_capture_pause_nolock(input_video)) {
		pr_info("%s wait for capturing pause fail\n", __func__);

		/*reset pause flags*/
		hw->capture_state_pause = 0;
		hw->set_capture_pause = 0;

		return -EBUSY;
	}

	ret =__rawdata_capture_one_nolock(chn);

	rawdata_resume_to_normal(chn);

	return ret;
}

static int rawdata_capture_one(struct chn_attr *chn)
{
	struct input_video_attr *input_video = chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;

	pr_debug("%s\n", __func__);

	mutex_lock(&hw->capture_pause_lock);
	rawdata_capture_one_nolock(chn);
	mutex_unlock(&hw->capture_pause_lock);

	return -EINVAL;
}

/*video-stream init*/
int input_video_init(struct input_video_attr *input_video)
{
	int chn_id;
	void *err_alloc_ctx = NULL;
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct v4l2_device *v4l2_dev = &hw->v4l2_dev;
	struct chn_attr *chn;
	struct video_device *vdev;
	struct device *dev = hw->dev;

	pr_debug("%s %d\n", __func__, __LINE__);

	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];
		chn->chn_id = chn_id;
		chn->input_video = input_video;

		/*set state*/
		chn->chn_state = CHN_STATE_CLOSED;

		/* list of video-buffers */
		INIT_LIST_HEAD(&chn->capture);
		INIT_LIST_HEAD(&chn->pend_done_list);
		INIT_LIST_HEAD(&chn->drop_list);

		chn->alloc_ctx = vb2_dma_contig_init_ctx(dev);
		err_alloc_ctx = chn->alloc_ctx;
		if (IS_ERR(chn->alloc_ctx)) {
			pr_err("%s %d chn_id:%d get alloc_ctx:%p err\n",
					__func__, __LINE__, chn_id, chn->alloc_ctx);
			goto error_alloc_ctx;
		}

#if 0
		mutex_init(&chn->queue_lock);
		mutex_init(&chn->stream_lock);
		mutex_init(&chn->fmt_lock);
#endif

		vdev = &chn->vdev;

		mutex_init(&chn->vlock);

		/*
		 * Provide a mutex to v4l2 core. It will be used
		 * to protect all fops and v4l2 ioctls.
		 */
		vdev->lock = &chn->vlock;//video device lock
		vdev->v4l2_dev = v4l2_dev;
		vdev->fops = &ak_cam_file_fops;
		snprintf(vdev->name, sizeof(vdev->name), "input_video%d-%d",
				input_video->input_id, chn_id);
		vdev->vfl_type = VFL_TYPE_GRABBER;
		vdev->release = video_device_release_empty;
		vdev->ioctl_ops = &ak_cam_ioctl_ops;

		video_set_drvdata(vdev, chn);
	}

	INIT_DELAYED_WORK(&input_video->aec_work, aec_work);
	INIT_DELAYED_WORK(&input_video->rawdata_work, rawdata_work);

	return 0;

error_alloc_ctx:
	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];
		if (!IS_ERR(chn->alloc_ctx))
			vb2_dma_contig_cleanup_ctx(chn->alloc_ctx);
		chn->alloc_ctx = NULL;
		chn->chn_id = 0;
		chn->input_video = NULL;
	}

	return PTR_ERR(err_alloc_ctx);
}

/*video-stream uninit*/
int input_video_uninit(struct input_video_attr *input_video)
{
	int chn_id;
	struct chn_attr *chn;

	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];
		if (!IS_ERR(chn->alloc_ctx))
			vb2_dma_contig_cleanup_ctx(chn->alloc_ctx);
		chn->alloc_ctx = NULL;
		chn->chn_id = 0;
		chn->input_video = NULL;
	}

	return 0;
}

/*video-stream register*/
int input_video_register(struct input_video_attr *input_video)
{
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct device *dev = hw->dev;
	struct video_device *vdev;
	struct chn_attr *chn;
	char *name;
	int input_id = input_video->input_id;
	int chn_id;
	int ret;
	bool is_first_port = is_first_input(input_video);

	pr_debug("%s\n", __func__);

	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];
		vdev = &chn->vdev;

		/*alloc memory for name*/
		name = devm_kzalloc(dev, strlen(VIDEO_NAME), GFP_KERNEL);
		if (!name) {
			pr_err("%s alloc name fail. input_id:%d, chn_id:%d\n",
					__func__, input_id, chn_id);
			ret = -ENOMEM;
			goto error_alloc;
		}
		snprintf(name, strlen(VIDEO_NAME), VIDEO_NAME, input_id, chn_id);
		/*set video node name*/
		vdev->dev.init_name = name;

		/*register video devide*/
		ret = video_register_device(vdev, vdev->vfl_type, -1);
		if (ret < 0) {
			pr_err("%s register video device fail. chn_id:%d, ret:%d\n", __func__, chn_id, ret);
			goto error_register_video_dev;
		}

		chn->name = name;
	}

	/*
	 * 1. set sclk by default frequecy
	 * output sclk in order to probe sensor ID by sensor driver
	 */

	/*
	 * NOTE: set_sclk may set default sclk=24MHZ at here.
	 * becasue subdev(sensor) hadnot attach to async notifier。
	 * if fail then set default sclk=24MHZ.
	 */
	set_sclk(input_video, DEFAULT_SCLK_FREQ);
	if (is_first_port)
		set_pinctrl_vi(input_video, PNAME_CSI0_SCLK);
	else
		set_pinctrl_vi(input_video, PNAME_CSI1_SCLK);

	return 0;

error_register_video_dev:
devm_kfree(dev, name);
error_alloc:
	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];
		vdev = &chn->vdev;

		if (video_is_registered(vdev)) {
			name = chn->name;

			/*unregister video device*/
			video_unregister_device(vdev);

			/*free memory for name*/
			devm_kfree(dev, name);
		}
	}

	return ret;
}

/*video-stream unregister*/
int input_video_unregister(struct input_video_attr *input_video)
{
	struct chn_attr *chn;
	struct video_device *vdev;
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct device *dev = hw->dev;
	char *name;
	int chn_id;

	for (chn_id = 0; chn_id < CHN_NUM_PER_INPUT_VIDEO; chn_id++) {
		chn = &input_video->chn[chn_id];
		vdev = &chn->vdev;

		if (video_is_registered(vdev)) {
			name = chn->name;
			chn->name = NULL;

			/*unregister video device*/
			video_unregister_device(vdev);

			/*free memory for name*/
			devm_kfree(dev, name);
		}
	}

	return 0;
}

void input_video_process_irq_start(struct input_video_attr *input_video)
{
	int fine = 0;
	unsigned int stat, update_stat;
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int input_id = input_video->input_id;

	pr_debug("%s input_id:%d\n", __func__, input_id);

	if (check_dual_mode_and_not_mine_frame(input_video))
		/*dual & capturing now is not mine*/
		return;

	if (!isp_param_check_working(isp))
		/*isp is closed so no longer call it*/
		return;

	/*get state*/
	update_stat = camera_isp_vo_check_update_status(isp_struct);
	stat = camera_isp_vo_check_irq_status(isp_struct);

	/*FRAME_ALL_DONE*/
	if (stat & (1 << 0)) {
		/*down frame end*/
		pr_debug("FEND%d\n", input_id);
		fine = 1;
		input_video->fend_count++;
		fend_state(input_video);
		pop_one_buf_to_capture(input_video);
		isp_irq_work(input_video);
		frame_data_process(input_video);
	}

	/*FIRST_LINE_START*/
	if (update_stat & (1 << 17)) {
		/*new frame start*/
		pr_debug("FLINE%d\n", input_id);
		fine = 1;
		input_video->fline_count++;
		fline_state(input_video);
		isp_aec(input_video);
	}

	/*TARGET_LINES_DONE*/
	if (update_stat & (1 << 16)) {
		/*target lines done*/
		pr_debug("TARGET DONE\n");
		fine = 1;
	}

#if 0
	if (!fine) {
		pr_debug("Do none, update_stat:0x%x, stat:0x%x\n", update_stat, stat);
		pr_debug("%s %d, R00:0x%x,R04:0x%x,R08:0x%x,R0c:0x%x,"
				"R10:0x%x,R14:0x%x,R18:0x%x,R1c:0x%x,R20:0x%x\n",
				__func__, __LINE__,
				__raw_readl(AK_VA_CAMERA + 0x00),
				__raw_readl(AK_VA_CAMERA + 0x04),
				__raw_readl(AK_VA_CAMERA + 0x08),
				__raw_readl(AK_VA_CAMERA + 0x0c),
				__raw_readl(AK_VA_CAMERA + 0x10),
				__raw_readl(AK_VA_CAMERA + 0x14),
				__raw_readl(AK_VA_CAMERA + 0x18),
				__raw_readl(AK_VA_CAMERA + 0x1c),
				__raw_readl(AK_VA_CAMERA + 0x20)
				);
		stat = __raw_readl(AK_VA_CAMERA + 0x1c);
		__raw_writel(stat | (1<<17)|(1<<16),AK_VA_CAMERA + 0x1c);
	}
#endif
	/*clearing stat always in the end*/
	camera_isp_vo_clear_update_status(isp_struct, update_stat & (0x3 << 16));
	camera_isp_vo_clear_irq_status(isp_struct, 0xfffe | (stat & 1));
}

void input_video_process_irq_end(struct input_video_attr *input_video)
{
	int input_id = input_video->input_id;
	pr_debug("%s input_id:%d\n", __func__, input_id);
}
