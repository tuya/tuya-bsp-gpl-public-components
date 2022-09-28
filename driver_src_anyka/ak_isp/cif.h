/*
 * cif.h
 *
 * cmos interface header file
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
#ifndef __CIF_H__
#define __CIF_H__

#include <linux/videodev2.h>

#include <media/videobuf2-v4l2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>

#include "../include/ak_isp_drv.h"
#include "include_internal/ak_video_priv_cmd_internal.h"

#define AK_CAM_DRV_NAME "ak-camera"

#define VIDEO_NAME		"video-%d-%d"
#define ISP_PARAM_NAME	"isp-param-%d"
#define ISP_STATS_NAME	"isp-stats-%s-%d"

/*DEFAULT_SCLK_FREQ: default sclk frequency, unit:MHZ*/
#define DEFAULT_SCLK_FREQ	24

#define CHN_NUM_PER_INPUT_VIDEO	3
#define MAX_BUFFERS_NUM			4

/*
 * main channel ID is 0
 * sub channel ID is 1
 * ch3 channel ID is 2
 * */
#define IS_MAIN_CHN(chn_id) (chn_id == 0)
#define IS_SUB_CHN(chn_id) (chn_id == 1)
#define IS_CH3_CHN(chn_id) (chn_id == 2)

/*td max couter for reload*/
#define TD_MAX_COUNTER	6

#define ISP_MAX_SUBDEVS			16

/*isp stats*/
#define ak_cam_fh_to_stats_chn(akvfh)		(akvfh->priv)
#define stats_chn_to_input_video(chn)		(chn->input_video)
#define isp_stats_to_input_video(isp_stats)	\
	container_of(isp_stats, struct input_video_attr, isp_stats)

/*isp*/
#define isp_to_input_video(isp)				\
	container_of(isp, struct input_video_attr, isp)

/*sensor*/
#define sensor_to_input_video(sensor)		\
	container_of(sensor, struct input_video_attr, sensor)

/*fh*/
#define ak_cam_fh_to_chn(akvfh)				(akvfh->priv)
#define vfh_to_ak_cam_fh(vfh)				container_of(vfh, struct ak_cam_fh, vfh)

/*chn*/
#define chn_to_input_video(chn)				(chn->input_video)
#define input_video_to_ak_cam(input_video)	(input_video->ak_cam)

/*hw*/
#define notifier_to_hw(notifier)			\
	container_of(notifier, struct hw_attr, notifier)
#define hw_to_ak_cam(hw)					container_of(hw, struct ak_camera_dev, hw)

/*buffer*/
#define vb2_b_to_vb2_v4l2_b(vb2_b)			to_vb2_v4l2_buffer(vb2_b)
#define vb2_v4l2_b_to_ak_b(vb2_v4l2_b)		\
	container_of(vb2_v4l2_b, struct ak_camera_buffer, vb2_v4l2_b)

/*
 * enum pinctrl_name - pinctrl names list of vi pins
 *
 * @PNAME_MIPI0_2LANE:	mipi0 2lane
 * @PNAME_MIPI0_1LANE:	mipi0 1lane
 * @PNAME_MIPI1_2LANE:	mipi1 2lane
 * @PNAME_MIPI1_1LANE:	mipi1 1lane
 * @PNAME_CSI0_SCLK:	sclk0
 * @PNAME_CSI1_SCLK:	sclk1
 * @PNAME_DVP0_12BITS:	dvp0 12bits
 * @PNAME_DVP0_10BITS:	dvp0 10bits
 * @PNAME_DVP0_8BITS:	dvp0 8bits
 * @PNAME_DVP1_12BITS:	dvp1 12bits
 * @PNAME_DVP1_10BITS:	dvp1 10bits
 * @PNAME_DVP1_8BITS:	dvp1 8bits
 */
enum pinctrl_name {
	PNAME_MIPI0_2LANE = 0,
	PNAME_MIPI0_1LANE,
	PNAME_MIPI1_2LANE,
	PNAME_MIPI1_1LANE,
	PNAME_CSI0_SCLK,
	PNAME_CSI1_SCLK,
	PNAME_DVP0_12BITS,
	PNAME_DVP0_10BITS,
	PNAME_DVP0_8BITS,
	PNAME_DVP1_12BITS,
	PNAME_DVP1_10BITS,
	PNAME_DVP1_8BITS,
};

/*
 * @CHN_STATE_CLOSED:	chn be closed
 * @CHN_STATE_OPEND:		chn be opend
 * @CHN_STATE_ALL_BUFFER_INIT:	all vb of the chn had queued
 * @CHN_STATE_NEED_CFG_BUFFER_ADDR:	will need cfg address after all vb queued
 * @CHN_STATE_CFG_BUFFER_ADDR:	had cfg address
 * @CHN_STATE_START_STREAMING:	streaming
 * @CHN_STATE_STOP_STREAMIN:		had be stopd streaming
 */
enum chn_state {
	CHN_STATE_CLOSED = 0,
	CHN_STATE_OPEND,
	CHN_STATE_ALL_BUFFER_INIT,
	CHN_STATE_NEED_CFG_BUFFER_ADDR,
	CHN_STATE_CFG_BUFFER_ADDR,
	CHN_STATE_PRE_START_STREAMING,
	CHN_STATE_START_STREAMING,
	CHN_STATE_STOP_STREAMING,
};

/*
 * enum irq_done_state - isp status of irq
 *
 * @IRQ_FLINE_DONE:	first line download done
 * @IRQ_FEND_DONE:	frame end done
 * @IRQ_ERROR_NOFEND:no frame end but next line come in
 */
enum irq_done_state {
	IRQ_FLINE_DONE = 0,
	IRQ_FEND_DONE,
	IRQ_ERROR_NOFEND,
};

enum isp_stats_type {
	ISP_STATS_3DNR = 0,
	ISP_STATS_AWB,
	ISP_STATS_AF,
	ISP_STATS_AE,
	ISP_STATS_TYPE_NUM
};

enum isp_status {
	ISP_STATUS_OPEN = 0,
	ISP_STATUS_START,
	ISP_STATUS_STOP,
	ISP_STATUS_CLOSE,
};

struct param_struct {
	unsigned int cmd;
	unsigned long arg;
};

/*
 * struct ak_camera_buffer - ak camera private buffer struct
 *
 * @vb:		vb2 v4l2 buffer
 * @queue:	queue of empty buffer
 */
struct ak_camera_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_v4l2_buffer vb2_v4l2_b;
	struct list_head queue;
};

/*
 * statistic-stream
 * @vdev:	video device of af
 * @capture:	capturing list of af data
 * @pend_done_list:	pending done list of af data
 */
struct af_stat {
	struct video_device vdev;
	struct list_head	capture;
	struct list_head	pend_done_list;
};

/*
 * statistic-stream
 * @vdev:	video device of awb
 * @capture:	capturing list of awb data
 * @pend_done_list:	pending done list of awb data
 */
struct awb_stat {
	struct video_device vdev;
	struct list_head	capture;
	struct list_head	pend_done_list;
};

/*
 * statistic-stream
 * @vdev:	video device of 3dnr
 * @capture:	capturing list of 3dnr data
 * @pend_done_list:	pending done list of 3dnr data
 */
struct _3dnr_stat {
	struct video_device vdev;
	struct list_head	capture;
	struct list_head	pend_done_list;
};

/*
 * hw_attr - hardware attribute
 * @dev:		poiter to device
 * @sclk0:	sclk0
 * @sclk1:	sclk1
 * @isp_clk:	isp clk(vclk)
 * @mipi_csi_clk:	mipi controll clk
 * @norm_base:	normal reg address
 * @mipi0_base:	mipi0 reg address
 * @mipi1_base:	mipi1 reg address
 * @irq:		irq of camera
 * @pinctrl:	pointer to camera pinctrl
 * @lock:	lock for camera
 * @v4l2_dev:	v4l2 device
 * @notifier:	notifier for async subdev
 * @input_num:	number of input camera
 * @capturing_input_id:	dual active
 * @type:	the v4l2 type
 * @camera_opend_count:	counter for all chn of all input
 * @dual_mipi_once_had_init:	dual active, mipi io had init
 * @dual_sensors_init:	dual active, sensors had init
 */
struct hw_attr {
	struct device *dev;
	struct clk		*sclk0;
	struct clk		*sclk1;
	struct clk		*isp_clk;
	struct clk		*mipi_csi_clk;

	unsigned long isp_clk_rate;
	unsigned long internal_pclk;
	unsigned long internal_pclk_res;

	void __iomem			*norm_base;
	unsigned char __iomem	*mipi0_base;
	unsigned char __iomem	*mipi1_base;
	int irq;
	struct pinctrl *pinctrl;/*dtb pinctls for dvp or mipi interface*/

	struct spinlock lock;

	struct v4l2_device v4l2_dev;
	struct v4l2_async_notifier notifier;
	//struct v4l2_subdev *subdevs[ISP_MAX_SUBDEVS];/*for sensors*/

	int input_num;
	int capturing_input_id;
	enum v4l2_buf_type type;

	int camera_opend_count;

	wait_queue_head_t set_capture_pause_queue;
	struct mutex capture_pause_lock;
	int set_capture_pause;//set to pause
	int capture_state_pause;//pause now to

	wait_queue_head_t rawdata_capture_wait_queue;
	int rawdata_capture_set;
	int rawdata_capture_done;

	int dual_mipi_once_had_init;

	int dual_sensors_init;

#ifdef CONFIG_MACH_AK39EV330
	unsigned long map0;
	unsigned long map1;
#endif
};

/*
 * sensor_attr
 * @sd:	subdev of sensor
 * @sensor_cbi:	sensor cb
 * @interface:	interface of sensor, dvp, mipi....
 * @io_level:	io voltage level
 * @mipi_mbs:	data speed, MBps
 * @mipi_lanes:	data lanes
 * @reset_gpio:	reset gpio
 * @pwdn_gpio:	pwdn gpio
 * @sclk_mhz:	sclk frequency, MHz
 * @new_fps:		new fps
 * @cur_fps:		current fps
 * @para:		sensor initial
 */
struct sensor_attr {
	struct v4l2_subdev *sd;	/*sensor subdev*/
	struct sensor_cb_info *sensor_cbi;

	int interface;
	int io_level;
	int mipi_mbps;
	int mipi_lanes;
	int reset_gpio;
	int pwdn_gpio;
	int sclk_mhz;

	int new_fps;
	int fps_update;

	enum scan_method input_method;
	AK_ISP_SENSOR_INIT_PARA para;
};

/*
 * isp_attr
 * @vdev:	video device
 * @vlock:	isp lock
 * @af:		af attrubite
 * @awb:		awb attrubite
 * @_3dnr:		3dnr attrubite
 * @isp_struct:	pointer to isp driver private data
 * @isp_param_priv:	pointer to isp param private data
 * @crop:	input data crop
 * @isp_output_format:	isp output format, yuv420p/yuv420sp
 * @isp_mode:	isp mode
 * @isp_id:	isp id
 * @record_fline_count:	record the irq counter for FLINE
 */
struct isp_attr {
	/*only READ/WRITE parampters*/
	struct video_device vdev;
	char *name;

	struct mutex vlock;

	/*statistic-stream*/
	struct af_stat		af;
	struct awb_stat		awb;
	struct _3dnr_stat	_3dnr;

	void *isp_struct;
	void *isp_param_priv;
	enum isp_status isp_status;

	struct v4l2_crop crop;

	int isp_output_format;//yuv420p/yuv420sp
	int isp_mode;
	int isp_param_opend_count;
	int isp_id;
	unsigned int record_fline_count;

	wait_queue_head_t isp_set_capture_pause_queue;
	int isp_set_capture_pause;
	int isp_capture_state_pause;
};

/*
 * chn_attr
 * @chn_id:	chn id
 * @vlock:	chn lock
 * @capture:	list for capturing
 * @pend_done_list: pend done list
 * @alloc_ctx:	vb2 alloc ctx
 * @chn_state:	chn state
 * @phyaddr:		all phyaddr of vb
 * @poll_empty:	is empty while polling in
 * @chn_opend_count:	counter to this chn be opend times
 * @vdev:	video device
 * @video_data_size:	size of the video data
 * @width:	width to the chn resolution
 * @height:	height to the chn resolution
 * @vb_num:	number of vb
 * @input_video:	pointer to input
 */
struct chn_attr {
	int chn_id;
	char *name;
	struct mutex vlock;
	struct list_head	capture;
	struct list_head	pend_done_list;
	struct list_head	drop_list;
	struct vb2_alloc_ctx	*alloc_ctx;
	enum chn_state chn_state;
	unsigned int phyaddr[MAX_BUFFERS_NUM];
	int poll_empty;

	int chn_opend_count;
	int frame_count;
	int drop_frame;

	struct v4l2_format format;
	struct video_device vdev;

	int video_data_size;

	int width;
	int height;
	int vb_num;

	struct input_video_attr *input_video;
};

/*
 * isp_stats_chn_attr
 * @chn_id:	chn id
 * @vlock:	chn lock
 * @capture:	list for capturing
 * @pend_done_list: pend done list
 * @alloc_ctx:	vb2 alloc ctx
 * @chn_state:	chn state
 * @phyaddr:		all phyaddr of vb
 * @poll_empty:	is empty while polling in
 * @chn_opend_count:	counter to this chn be opend times
 * @vdev:	video device
 * @width:	width to the chn resolution
 * @height:	height to the chn resolution
 * @vb_num:	number of vb
 * @input_video:	pointer to input
 */
struct isp_stats_chn_attr {
	int chn_id;
	struct mutex vlock;
	struct list_head	capture;
	struct list_head	pend_done_list;
	struct vb2_alloc_ctx	*alloc_ctx;
	enum chn_state chn_state;
	unsigned int phyaddr[MAX_BUFFERS_NUM];
	int poll_empty;

	int chn_opend_count;

	struct v4l2_format format;
	struct video_device vdev;

	int width;
	int height;
	int vb_num;

	struct input_video_attr *input_video;
};

/*
 * isp_stats_attr
 * @chn:	all statistic chn
 * @record_fend_count:	record counter of irq FEND
 * @stats_count:	counter to statistic
 */
struct isp_stats_attr {
	struct isp_stats_chn_attr chn[ISP_STATS_TYPE_NUM];
	unsigned int record_fend_count;
	unsigned int stats_count;
	struct delayed_work stats_work;
};

/*
 * input_video_attr
 * @list:	this input
 * @input_id:	input id
 * @sensor:	sensor belong to the input
 * @isp:		isp belong to the input
 * @chn:		video chn belong to the input
 * @isp_stats:	isp statistic belong to the input
 * @ak_cam:	pointer to ak camera
 * @asd:		async subdev
 * @input_timestamp:	video timestamp
 * @input_opend_count:	counter to input opend
 * @input_route_init:	route path initial of the input
 * @dual_mode_state_pause_nobuf:	dual active, pause because no buffer
 */
struct input_video_attr {
	struct list_head	list;
	int input_id;
	struct sensor_attr	sensor;
	struct isp_attr		isp;
	struct chn_attr		chn[CHN_NUM_PER_INPUT_VIDEO];
	struct isp_stats_attr isp_stats;/*3dnr,awb,af*/

	struct ak_camera_dev *ak_cam;
	struct v4l2_async_subdev *asd;

	/*all chn share same md work*/
	struct delayed_work md_work;
	struct delayed_work aec_work;

	int rawdata_work_start;
	struct delayed_work rawdata_work;

	struct timeval input_timestamp;

	int input_opend_count;
	int input_route_init;

	int dual_mode_state_pause_nobuf;

	int drop_frame;

	unsigned long irq_done_state;
	unsigned int td_count;
	unsigned int frame_count;
	unsigned int fline_count;
	unsigned int fend_count;
	unsigned int nofend_count;
	unsigned int nofline_count;
};

/*
 * ak_camera_dev
 * @hw:	hardware attrubite
 * @input_num:	number of input
 * @input_head:	list for input
 */
struct ak_camera_dev {
	struct hw_attr hw;
	int input_num;
	struct list_head	input_head;
};

/*
 * struct ak_cam_fh- channel file handle
 *
 * @vfh:	v4l2 file handle
 * @priv:	pointer to private data
 * @queue:	the buffer queue
 */
struct ak_cam_fh {
	struct v4l2_fh vfh;
	void *priv;
	struct vb2_queue queue;
};
#endif
