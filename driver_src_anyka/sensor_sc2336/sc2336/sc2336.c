/*
 * sc2336 Camera Driver
 *
 * Copyright anyka
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-image-sizes.h>
#include "../include/ak_video_priv_cmd.h"
#include "../include_internal/ak_video_priv_cmd_internal.h"
#include "../include/ak_isp_drv.h"

#include "../include/ak_sensor.h"
#include "../include/sensor_cmd.h"
#include "../common/sensor_sys.h"
#include "../common/sensor_i2c.h"

/*
 *
SENSOR_PWDN_LEVEL:	the io level when sensor power down
SENSOR_RESET_LEVEL:	the io level when sensor reset
SENSOR_I2C_ADDR:	the sensor slave address, 7 bits width.
					the i2c bus left shift 1bits.
SENSOR_ID:	the sensor ID.
SENSOR_MCLK:	mclk, unit：MHz。24MHz or 27MHz mclk for most sensors.
				NOTE: 27MHz mclk is inaccurate.
SENSOR_REGADDR_BYTE:	sensor register address bytes width, 1 byte or 2 bytes.
SENSOR_DATA_BYTE:		register's value bytes width, 1 byte or 2bytes.
SENSOR_OUTPUT_WIDTH:	use for isp cut window MAX width.
SENSOR_OUTPUT_HEIGHT:	use for isp cut window MAX height.
SENSOR_VALID_OFFSET_X:	use for isp cut window left offset.
SENSOR_VALID_OFFSET_Y:	use for isp cut window top offset.
SENSOR_BUS_TYPE:		sensor output data type.
SENSOR_IO_INTERFACE:	sensor output data bus type.
SENSOR_IO_LEVEL:		sensor output data io level.
MAX_FPS:				current sensor configurtion support max fps.
EXP_EFFECT_FRAMES:		!!IMPORT!! frequency for update exp_time
		0 - adjust exp_time for every frames
		1 - adjust exp_time for 1/2 frames
		2 - adjust exp_time for 1/3 frames
		and so on.
A_GAIN_EFFECT_FRAMES:	!!IMPORT!! frequency for update a_gain
		0 - adjust a_gain for every frames
		1 - adjust a_gain for 1/2 frames
		2 - adjust a_gain for 1/3 frames
		and so on.
D_GAIN_EFFECT_FRAMES:	!!IMPORT!! frequency for update d_gain
		0 - adjust d_gain for every frames
		1 - adjust d_gain for 1/2 frames
		2 - adjust d_gain for 1/3 frames
		and so on.
DELAY_FLAG
 *
 * */
#define SENSOR_PWDN_LEVEL		0
#define SENSOR_RESET_LEVEL		0
#define SENSOR_I2C_ADDR			0x30
#define ACTUAL_SENSOR_ID		0xcb3a
#define SENSOR_ID				0x2336
#define SENSOR_MCLK				24
#define SENSOR_REGADDR_BYTE		2
#define SENSOR_DATA_BYTE		1
#define SENSOR_OUTPUT_WIDTH		1920
#define SENSOR_OUTPUT_HEIGHT	1080
#define SENSOR_VALID_OFFSET_X	0
#define SENSOR_VALID_OFFSET_Y	0
#define SENSOR_BUS_TYPE			BUS_TYPE_RAW
#define SENSOR_IO_INTERFACE		MIPI_INTERFACE
#define SENSOR_IO_LEVEL			IO_LEVEL_1V8
#define MIPI_MBPS				375
#define MIPI_LANES				2
#define MAX_FPS					30
#define EXP_EFFECT_FRAMES		1	/*adjust exp_time for 1/2 frames*/
#define A_GAIN_EFFECT_FRAMES	1	/*adjust a_gain for every frames*/
#define D_GAIN_EFFECT_FRAMES	1	/*adjust d_gain for every frames*/

#define DELAY_FLAG		(0xffff)

/*
 * Struct
 */
struct regval_list {
	u8 reg_num;
	u8 value;
};

struct sc2336_win_size {
	char				*name;
	u32				width;
	u32				height;
	const struct regval_list	*regs;
};

/*
 * store sensor fps informations
 * @current_fps: current fps
 * @to_fps: should changed to the fps
 * @to_fps_value: the fps paramter
 */
struct sensor_fps_info {
	int current_fps;
	int to_fps;
	int to_fps_value;
	int reg_fps_value;
};

/*
 * the host callbacks for sensor
 * @isp_timing_cb_info: isp timing callback.
 * 	a few sensor need the callback when fps had changed
 */
struct host_callbacks {
	struct isp_timing_cb_info isp_timing_cb_info;
};

/*
 * aec paramters. some sensors need record paramters.
 */
struct sc2336_aec_parms {
	int curr_again_level;
	int curr_again_10x;
	int r0x3e02_value;
	int r0x3e01_value;
	int r0x3e00_value;
	int target_exp_ctrl;
	int curr_2x_dgain;
	int curr_corse_gain;

	int r0x3e08_corse_gain;
	int r0x3e09_fine_gain;
    int reg_frame_hts;
	int reg_frame_vts;
	int pclk_freq;
	int calc_vts_tmp;
};

static const struct v4l2_ctrl_ops sc2336_ctrl_ops;
static const struct v4l2_ctrl_config config_sensor_get_id = {
	.ops	= &sc2336_ctrl_ops,
	.id		= SENSOR_GET_ID,
	.name	= "get sensor id",
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.min	= 0,
	.max	= 0xfffffff,
	.step	= 1,
	.def	= 0,
	.flags	= V4L2_CTRL_FLAG_VOLATILE,/*set V4L2_CTRL_FLAG_VOLATILE avoid get cached*/
};

/*
 * the sensor struct
 * @client: point to i2c client
 * @subdev: v4l2 subdev struct
 * @hdl: 	v4l2 control handler
 * @cfmt_code:
 * @win:	default window
 * @gpio_reset:	gpio number for reset pin
 * @gpio_pwdn:	gpio number for power down pin
 * @cb_info:	sensor callbacks
 * @fps_info:	fps informations
 * @hcb:		host callbacks
 * @aec_parms:	aec paramters
 */
struct sc2336_priv {
	struct i2c_client *client;
	struct v4l2_subdev		subdev;
	struct v4l2_ctrl_handler	hdl;
	u32	cfmt_code;
	const struct sc2336_win_size	*win;

	int gpio_reset;
	int gpio_pwdn;
	struct sensor_cb_info cb_info;
	struct sensor_fps_info fps_info;

	struct host_callbacks hcb;
	struct sc2336_aec_parms aec_parms;

	AK_ISP_SENSOR_INIT_PARA para;

	struct v4l2_ctrl *ctrl_sensor_get_id;
};

/*default aec of fast boot*/
static struct ae_fast_struct ae_fast_default = {
	.sensor_exp_time = 2152,
	.sensor_a_gain	= 256,
	.sensor_d_gain	= 256,
	.isp_d_gain		= 256,
	{
		.r_gain = 445,
		.g_gain = 256,
		.b_gain = 396,
		.r_offset = 0,
		.g_offset = 0,
		.b_offset = 0
	}
};

static AK_ISP_SENSOR_CB sc2336_cb;

static int dvp = 0;

module_param(dvp, int, 0644);

/*
 * check_id - check hardware sensor ID whether it meets this driver
 * 0-no check, force to meet
 * others-check, if not meet return fail
 */
static int check_id = 0;
module_param(check_id, int, 0644);

static void sc2336_set_fps_async(struct sc2336_priv *priv);
static int __sc2336_sensor_probe_id_func(struct i2c_client *client);  //use IIC bus
static int sc2336_sensor_read_id_func(void *arg);   //no use IIC bus
static int sc2336_sensor_get_resolution_func(void *arg, int *width, int *height);
static int sc2336_sensor_set_fps_func(void *arg, const int fps);
static int sc2336_fps_to_vts(struct sc2336_priv *priv, const int fps);
static int call_sensor_sys_init(void);
static int call_sensor_sys_deinit(void);

static u32 sc2336_codes[] = {
	MEDIA_BUS_FMT_YUYV8_2X8,
	MEDIA_BUS_FMT_UYVY8_2X8,
	MEDIA_BUS_FMT_RGB565_2X8_BE,
	MEDIA_BUS_FMT_RGB565_2X8_LE,
};

/*
 * General functions
 */
static struct sc2336_priv *to_sc2336(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct sc2336_priv,
			    subdev);
}

static struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct sc2336_priv, hdl)->subdev;
}

/*
 * XXXX_s_stream -
 * set steaming enable/disable
 * soc_camera_ops functions
 *
 * @sd:				subdev
 * @enable:			enable flags
 */
static int sc2336_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int sc2336_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	int ret = 0;

	switch (ctrl->id) {
		case SENSOR_GET_ID:
			ctrl->val = SENSOR_ID;
			break;

		default:
			ret = -EINVAL;
			pr_err("%s id:%d no support\n", __func__, ctrl->id);
			break;
	}

	return ret;
}

/*
 * XXXX_s_stream -
 * set ctrls
 * soc_camera_ops functions
 *
 * @ctrl:			pointer to ctrl
 */
static int sc2336_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

/*
 * XXXX_s_power -
 * set power operation
 * soc_camera_ops functions
 *
 * @sd:			subdev
 * @on:			power flags
 */
static int sc2336_core_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

/*
 * XXXX_get_sensor_id -
 * get sensor ID
 * private callback functions
 *
 * @ctrl:			pointer to ctrl
 */
static int sc2336_get_sensor_id(struct v4l2_control *ctrl)
{
	ctrl->value = sc2336_sensor_read_id_func(NULL);   //no use IIC bus
	return 0;
}

/*
 * XXXX_get_sensor_id -
 * get sensor callback
 * private callback functions
 *
 * @sd:				subdev
 * @ctrl:			pointer to ctrl
 */
static int sc2336_get_sensor_cb(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sc2336_priv *priv = to_sc2336(client);

	ctrl->value = (int)&priv->cb_info;
	return 0;
}

static int sc2336_get_max_exp_for_fps(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sc2336_priv *priv = to_sc2336(client);
	int fps = ctrl->value;
	int vts = sc2336_fps_to_vts(priv, fps);

	ctrl->value = vts * 2 - 10;
	return 0;
}

/*
 * XXXX_core_g_ctrl -
 * get control
 * core functions
 *
 * @sd:				subdev
 * @ctrl:			pointer to ctrl
 */
static int sc2336_core_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret;

	switch (ctrl->id) {
		case GET_SENSOR_ID:
			ret = sc2336_get_sensor_id(ctrl);
			break;

		case GET_SENSOR_CB:
			ret = sc2336_get_sensor_cb(sd, ctrl);
			break;

		case GET_MAX_EXP_FOR_FPS:
			ret = sc2336_get_max_exp_for_fps(sd, ctrl);
			break;

		default:
			pr_err("%s cmd:%d not support\n", __func__, ctrl->id);
			ret = -1;
			break;
	}

	return ret;
}

/*
 * XXXX_set_isp_timing_cb -
 * set isp timing callback function
 * private callback functions
 *
 * @sd:				subdev
 * @ctrl:			pointer to ctrl
 */
static int sc2336_set_isp_timing_cb(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sc2336_priv *priv = to_sc2336(client);
	struct isp_timing_cb_info *isp_timing_cb_info = (void *)ctrl->value;

	memcpy(&priv->hcb.isp_timing_cb_info,
			isp_timing_cb_info,
			sizeof(struct isp_timing_cb_info));
	return 0;
}

/*
 * XXXX_set_fps_direct -
 * set sensor fps directly, synchronously.
 * private callback functions
 *
 * @sd:				subdev
 * @ctrl:			pointer to ctrl
 */
static int sc2336_set_fps_direct(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sc2336_priv *priv = to_sc2336(client);
	int fps = ctrl->value;

	sc2336_sensor_set_fps_func(priv, fps);
	sc2336_set_fps_async(priv);
	return 0;
}

/*
 * XXXX_core_s_ctrl -
 * set core s_ctrl
 * core functions
 *
 * @sd:				subdev
 * @ctrl:			pointer to ctrl
 */
static int sc2336_core_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret;

	switch (ctrl->id) {
		case SET_ISP_MISC_CALLBACK:
			ret = sc2336_set_isp_timing_cb(sd, ctrl);
			break;

		case SET_FPS_DIRECT:
			ret = sc2336_set_fps_direct(sd, ctrl);
			break;

		default:
			pr_err("%s cmd:%d not support\n", __func__, ctrl->id);
			ret = -1;
			break;
	}

	return ret;
}

static int sc2336_store_initial_regs(struct sc2336_priv *priv, void *arg)
{
	struct i2c_client *client = priv->client;
	AK_ISP_SENSOR_INIT_PARA *sensor_para = arg;
	void *p_reg_info;
	int ret;

	p_reg_info = sensor_para->reg_info;
	sensor_para->reg_info =
		kmalloc(sizeof(AK_ISP_SENSOR_REG_INFO) * sensor_para->num, GFP_KERNEL);
	if (!sensor_para->reg_info) {
		pr_err("%s alloc1 fail\n", __func__);
		ret = -ENOMEM;
		goto alloc1_fail;
	} else {
		if (copy_from_user(sensor_para->reg_info, p_reg_info,
					sizeof(AK_ISP_SENSOR_REG_INFO) * sensor_para->num)) {
			pr_err("%s cp2 fail\n", __func__);
			ret = -EFAULT;
			goto cp2_fail;
		}

		if (priv->para.reg_info) {
			devm_kfree(&client->dev, priv->para.reg_info);
			priv->para.reg_info = NULL;
			priv->para.num = 0;
		}

		/*store sensor configurtion*/
		priv->para.num = sensor_para->num;
		priv->para.reg_info = devm_kzalloc(&client->dev,
				sizeof(AK_ISP_SENSOR_REG_INFO)*sensor_para->num, GFP_KERNEL);
		if (!priv->para.reg_info) {
			pr_err("%s alloc2 fail\n", __func__);
			goto alloc2_fail;
		}
		memcpy(priv->para.reg_info, sensor_para->reg_info,
				sizeof(AK_ISP_SENSOR_REG_INFO)*sensor_para->num);

		pr_debug("%s para.num;%d\n", __func__, priv->para.num);
	}

	kfree(sensor_para->reg_info);
	return 0;

alloc2_fail:
cp2_fail:
	kfree(sensor_para->reg_info);
alloc1_fail:
	return ret;
}

static long sc2336_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sc2336_priv *priv = to_sc2336(client);
	int ret = 0;

	pr_debug("%s\n", __func__);

	switch (cmd) {
		case AK_SENSOR_SET_INIT:
			sc2336_store_initial_regs(priv, arg);
			break;

		case AK_SENSOR_GET_MAX_EXP_FOR_FPS: {
			struct sensor_max_exp_for_fps *exp_for_fps = arg;
			struct v4l2_control ctrl;

			ctrl.value = exp_for_fps->fps;
			ret = sc2336_get_max_exp_for_fps(sd, &ctrl);
			if (!ret)
				exp_for_fps->max_exp = ctrl.value;
			break;
		}

		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}

/*
 * XXXX_get_fmt -
 * get format
 * pad functions
 *
 * @sd:				subdev
 * @cfg:			pointer to pad config
 * @format:			return format
 */
static int sc2336_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client  *client = v4l2_get_subdevdata(sd);
	struct sc2336_priv *priv = to_sc2336(client);
	static struct sc2336_win_size win = {
		.name = "default",
		.width = SENSOR_OUTPUT_WIDTH,
		.height = SENSOR_OUTPUT_HEIGHT,
		.regs = NULL,
	};

	if (format->pad)
		return -EINVAL;

	if (!priv->win) {
		priv->win = &win;
		priv->cfmt_code = MEDIA_BUS_FMT_UYVY8_2X8;
	}

	mf->width	= priv->win->width;
	mf->height	= priv->win->height;
	mf->code	= priv->cfmt_code;

	switch (mf->code) {
	case MEDIA_BUS_FMT_RGB565_2X8_BE:
	case MEDIA_BUS_FMT_RGB565_2X8_LE:
		mf->colorspace = V4L2_COLORSPACE_SRGB;
		break;
	default:
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_UYVY8_2X8:
		mf->colorspace = V4L2_COLORSPACE_JPEG;
	}
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

/*
 * XXXX_set_fmt -
 * set format
 * pad functions
 *
 * @sd:				subdev
 * @cfg:			pointer to pad config
 * @format:			format to set
 */
static int sc2336_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;

	if (format->pad)
		return -EINVAL;

	mf->field	= V4L2_FIELD_NONE;

	switch (mf->code) {
	case MEDIA_BUS_FMT_RGB565_2X8_BE:
	case MEDIA_BUS_FMT_RGB565_2X8_LE:
		mf->colorspace = V4L2_COLORSPACE_SRGB;
		break;
	default:
		mf->code = MEDIA_BUS_FMT_UYVY8_2X8;
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_UYVY8_2X8:
		mf->colorspace = V4L2_COLORSPACE_JPEG;
	}

	if (cfg)
		cfg->try_fmt = *mf;
	return 0;
}

/*
 * XXXX_set_fmt -
 * set format
 * pad functions
 *
 * @sd:				subdev
 * @cfg:			pointer to pad config
 * @sel:			return selection
 */
static int sc2336_get_selection(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_selection *sel)
{
	sel->r.left = SENSOR_VALID_OFFSET_X;
	sel->r.top = SENSOR_VALID_OFFSET_Y;
	sel->r.width = SENSOR_OUTPUT_WIDTH;
	sel->r.height = SENSOR_OUTPUT_HEIGHT;

	return 0;
}

/*
 * XXXX_enum_bus -
 * enum bus
 * pad functions
 *
 * @sd:				subdev
 * @cfg:			pointer to pad config
 * @code:			return code of bus type
 */
static int sc2336_enum_mbus_code(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(sc2336_codes))
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_YUYV8_2X8;

	return 0;
}

/*
 * XXXX_g_crop -
 * get crop
 * video functions
 *
 * @sd:				subdev
 * @a:				return crop
 */
static int sc2336_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= SENSOR_VALID_OFFSET_X;
	a->c.top	= SENSOR_VALID_OFFSET_Y;
	a->c.width	= SENSOR_OUTPUT_WIDTH;
	a->c.height	= SENSOR_OUTPUT_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

/*
 * XXXX_cropcap -
 * get crop capbilities
 * video functions
 *
 * @sd:				subdev
 * @a:				return crop capbilities
 */
static int sc2336_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	sc2336_sensor_get_resolution_func(NULL, &a->bounds.width, &a->bounds.height);
	a->bounds.left			= SENSOR_VALID_OFFSET_X;
	a->bounds.top			= SENSOR_VALID_OFFSET_Y;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	pr_err("%s bounds.width:%d, bounds.height:%d\n", __func__, a->bounds.width, a->bounds.height);

	return 0;
}

/*
 * XXXX_video_probe -
 * do some process in subdev probe
 * locale functions
 *
 * @sd:				subdev
 * @a:				return crop capbilities
 */
static int sc2336_video_probe(struct i2c_client *client)
{
	struct sc2336_priv *priv = to_sc2336(client);
	int ret;

	ret = v4l2_ctrl_handler_setup(&priv->hdl);

	return ret;
}

/*ctrl ops*/
static const struct v4l2_ctrl_ops sc2336_ctrl_ops = {
	.g_volatile_ctrl	= sc2336_g_volatile_ctrl,
	.s_ctrl				= sc2336_s_ctrl,
};

/*core ops*/
static struct v4l2_subdev_core_ops sc2336_subdev_core_ops = {
	.s_power	= sc2336_core_s_power,
	.g_ctrl		= sc2336_core_g_ctrl,
	.s_ctrl		= sc2336_core_s_ctrl,
	.ioctl		= sc2336_core_ioctl,
};

/*
 * XXXX_g_mbus_config -
 * get buf config
 * video functions
 *
 * @sd:				subdev
 * @cfg:			return config
 */
static int sc2336_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cfg->flags = V4L2_MBUS_PCLK_SAMPLE_RISING | V4L2_MBUS_MASTER |
		V4L2_MBUS_VSYNC_ACTIVE_HIGH | V4L2_MBUS_HSYNC_ACTIVE_HIGH |
		V4L2_MBUS_DATA_ACTIVE_HIGH;
	cfg->type = V4L2_MBUS_PARALLEL;
	cfg->flags = 0;

	return 0;
}

/*
 * XXXX_s_crop -
 * set crop
 * video functions
 *
 * @sd:				subdev
 * @crop:			crop to set
 */
static int sc2336_s_crop(struct v4l2_subdev *sd, const struct v4l2_crop *crop)
{
	printk(KERN_ERR "%s %d, left:%d, top:%d, width:%d, height:%d\n",
			__func__, __LINE__, crop->c.left, crop->c.top, crop->c.width, crop->c.height);
	return 0;
}

/*video ops*/
static struct v4l2_subdev_video_ops sc2336_subdev_video_ops = {
	.s_stream	= sc2336_s_stream,
	.cropcap	= sc2336_cropcap,
	.g_crop		= sc2336_g_crop,
	.g_mbus_config	= sc2336_g_mbus_config,
	.s_crop		= sc2336_s_crop,
};

/*pad ops*/
static const struct v4l2_subdev_pad_ops sc2336_subdev_pad_ops = {
	.enum_mbus_code = sc2336_enum_mbus_code,
	.get_fmt	= sc2336_get_fmt,
	.set_fmt	= sc2336_set_fmt,
	.get_selection	= sc2336_get_selection,
};

/*sensor driver subdev ops*/
static struct v4l2_subdev_ops sc2336_subdev_ops = {
	.core	= &sc2336_subdev_core_ops,
	.video	= &sc2336_subdev_video_ops,
	.pad	= &sc2336_subdev_pad_ops,
};

/*
 * sensor_of_parse -
 * parse node of device
 *
 * @client:			pointor to i2c client
 * @priv:			sensor struct
 */
static int sensor_of_parse(struct i2c_client *client, struct sc2336_priv *priv)
{
	struct device_node *np = client->dev.of_node;

	/*it is exist reset but lack of pwdn gpio in most case*/
	priv->gpio_reset = of_get_named_gpio(np, "reset-gpio", 0);
	priv->gpio_pwdn = of_get_named_gpio(np, "pwdn-gpio", 0);

	if (priv->gpio_reset >= 0) {
		devm_gpio_request(&client->dev, priv->gpio_reset, "sensor-reset");
	}

	if (priv->gpio_pwdn >= 0) {
		devm_gpio_request(&client->dev, priv->gpio_pwdn, "sensor-pwdn");
	}

	return 0;
}

/*
 * XXXX_match - check sensor id for match driver
 * @priv:			pointer to pirvate structure
 *
 * @RETURN:	0-match fail; 1-match success
 */
static int sc2336_match(struct sc2336_priv *priv)
{
	int pid;

	if (!check_id)
		return 1;

	priv->cb_info.cb->sensor_set_power_on_func(priv);
	pid = priv->cb_info.cb->sensor_probe_id_func(priv);
	if (pid <= 0) {
		pr_err("%s fail\n", __func__);
		return 0;
	}

	return 1;
}

/*
 * XXXX_probe -
 * driver probe after platform probe ok
 * i2c_driver functions
 *
 * @client:			pointor to i2c client
 * @did:			driver ids
 */
static int sc2336_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct sc2336_priv	*priv;
	struct i2c_adapter	*adapter = to_i2c_adapter(client->dev.parent);
	int			ret;

	pr_err("%s %s %s\n", __func__, __DATE__, __TIME__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&adapter->dev,
			"sc2336: I2C-Adapter doesn't support SMBUS\n");
		return -EIO;
	}

	priv = devm_kzalloc(&client->dev, sizeof(struct sc2336_priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&adapter->dev,
			"Failed to allocate memory for private data!\n");
		return -ENOMEM;
	}

	/*parse of node*/
	sensor_of_parse(client, priv);

	/*
	 * the current sensor slave address to client.
	 * the address from dts may incorrect
	 * */
	client->addr = SENSOR_I2C_ADDR;
	priv->cb_info.cb = &sc2336_cb;
	priv->cb_info.arg = priv;
	priv->client = client;

	if (!client->dev.of_node) {
		dev_err(&client->dev, "Missing platform_data for driver\n");
		ret = -EINVAL;
		goto err_clk;
	}

	if (!sc2336_match(priv)) {
		ret = -ENODEV;
		goto err_clk;
	}

	/*subdev init*/
	v4l2_i2c_subdev_init(&priv->subdev, client, &sc2336_subdev_ops);
	priv->subdev.flags |= /*V4L2_SUBDEV_FL_HAS_EVENTS | */V4L2_SUBDEV_FL_HAS_DEVNODE;
	v4l2_ctrl_handler_init(&priv->hdl, 2);
	v4l2_ctrl_new_std(&priv->hdl, &sc2336_ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&priv->hdl, &sc2336_ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, 0);

	/*private ctrl*/
	priv->ctrl_sensor_get_id = v4l2_ctrl_new_custom(&priv->hdl, &config_sensor_get_id, NULL);
	if (!priv->ctrl_sensor_get_id)
		pr_err("creat ctrl_sensor_get_id fail\n");

	priv->subdev.ctrl_handler = &priv->hdl;
	if (priv->hdl.error) {
		dev_err(&client->dev, "Hdl error\n");
		ret = priv->hdl.error;
		goto err_clk;
	}

	ret = sc2336_video_probe(client);
	if (ret < 0) {
		dev_err(&client->dev, "OV2640 probe fail\n");
		goto err_videoprobe;
	}

	ret = v4l2_async_register_subdev(&priv->subdev);
	if (ret < 0) {
		dev_err(&client->dev, "v4l2 async register subdev fail, ret:%d\n", ret);
		goto err_videoprobe;
	}

	dev_err(&adapter->dev, "sc2336 Probed success, subdev:%p\n", &priv->subdev);

	/*ak platform need export some informations from sensor*/
	call_sensor_sys_init();

	return 0;

err_videoprobe:
	v4l2_ctrl_handler_free(&priv->hdl);
err_clk:
	return ret;
}

/*
 * XXXX_remove -
 * driver probe after platform remove ok
 * i2c_driver functions
 *
 * @client:			pointor to i2c client
 */
static int sc2336_remove(struct i2c_client *client)
{
	struct sc2336_priv       *priv = to_sc2336(client);

	pr_err("%s %d\n", __func__, __LINE__);

	if (!priv) {
		pr_err("%s had remove\n", __func__);
		return 0;
	}

	/*unregister async subdev*/
	v4l2_async_unregister_subdev(&priv->subdev);
	/*unregister subdev*/
	v4l2_device_unregister_subdev(&priv->subdev);
	/*unregister handler*/
	v4l2_ctrl_handler_free(&priv->hdl);
	/*unregister sys node*/
	call_sensor_sys_deinit();
	return 0;
}

static const struct i2c_device_id sc2336_id[] = {
	{ "sc2336", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc2336_id);

static const struct of_device_id sc2336_of_match[] = {
	/*donot changed compatible, must the same as dts*/
	{.compatible = "anyka,sensor0", },
	{},
};
MODULE_DEVICE_TABLE(of, sc2336_of_match);

static struct i2c_driver sc2336_i2c_driver = {
	.driver = {
		.name = "sc2336",
		.of_match_table = of_match_ptr(sc2336_of_match),
	},
	.probe    = sc2336_probe,
	.remove   = sc2336_remove,
	.id_table = sc2336_id,
};

module_i2c_driver(sc2336_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for sc2336 sensor");
MODULE_AUTHOR("anyka");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.1");

/*sensor i2c write*/
static int sc2336_write(const struct i2c_client *client, int reg, int value)
{
	struct i2c_transfer_struct trans = {
		.client = client,
		.reg_bytes = SENSOR_REGADDR_BYTE,
		.value_bytes = SENSOR_DATA_BYTE,
		.reg = reg,
		.value = value,
	};

	return i2c_write(&trans);
}

/*sensor i2c read*/
static int sc2336_read(const struct i2c_client *client, int reg)
{
	struct i2c_transfer_struct trans = {
		.client = client,
		.reg_bytes = SENSOR_REGADDR_BYTE,
		.value_bytes = SENSOR_DATA_BYTE,
		.reg = reg,
		.value = 0,
	};

	pr_debug("%s client:%p, addr:%x\n", __func__, client, client->addr);
	return i2c_read(&trans);
}

static void sc2336_agc_mode(struct sc2336_priv *priv)
{
	int value;
	struct i2c_client *client = priv->client;

	value = sc2336_read(client, 0x3e03) & 0xf0;
	value |= 0xb;
	sc2336_write(client, 0x3e03, value);
}

/*
 * The sensor must set callbacks
 *
 * */
static int sc2336_sensor_init_func(void *arg, const AK_ISP_SENSOR_INIT_PARA *npara)
{
	/*cleaned_0x54_SC500AI_24MInput_MIPI_2lane_396Mbps_10bit_2880x1620_15fps.ini*/
	int i;
	AK_ISP_SENSOR_REG_INFO *preg_info;
	struct sc2336_priv *priv = arg;
	struct i2c_client *client = priv->client;
	AK_ISP_SENSOR_INIT_PARA *para = &priv->para;

	if (para->num <= 0 && npara)
		para = (void *)npara;

	preg_info = para->reg_info;
	for (i = 0; i < para->num; i++) {
#if 0
		{
			int value;

			value = sc2336_read(client, preg_info->reg_addr);
			pr_err("reg:%x read value:%x\n", preg_info->reg_addr, value);
		}
#endif
		sc2336_write(client, preg_info->reg_addr, preg_info->value);
#if 0
		{
			int value;

			value = sc2336_read(client, preg_info->reg_addr);
			pr_err("reg:%x write value:%x, read back value:%x\n",
					preg_info->reg_addr, preg_info->value, value);
		}
#endif

		preg_info++;
	}

	sc2336_agc_mode(priv);
    
	priv->aec_parms.reg_frame_vts =
		sc2336_read(client,0x320e) << 8 | sc2336_read(client,0x320f);

    priv->aec_parms.reg_frame_hts =
		sc2336_read(client,0x320c) << 8 | sc2336_read(client,0x320d);

	priv->aec_parms.pclk_freq = priv->aec_parms.reg_frame_hts *
		priv->aec_parms.reg_frame_vts * MAX_FPS;

	priv->aec_parms.calc_vts_tmp = (priv->aec_parms.reg_frame_vts * MAX_FPS * 100); 
	
	printk("MarJ pclk %d vts %d hts %d calc_tmp %d\n", 
			priv->aec_parms.pclk_freq, 
			priv->aec_parms.reg_frame_vts, 
			priv->aec_parms.reg_frame_hts,  
			priv->aec_parms.calc_vts_tmp);

	priv->fps_info.current_fps = MAX_FPS;
	priv->fps_info.to_fps = priv->fps_info.current_fps;
	priv->fps_info.to_fps_value = priv->aec_parms.reg_frame_vts;
	priv->fps_info.reg_fps_value = priv->aec_parms.reg_frame_vts;

#if 0
	int value;
	value = sc2336_read(client, 0x320e)&0x7f;
	pr_err("reg:%04x , read back value:%04x\n", 0x320e, value);

	value = sc2336_read(client, 0x320f)&0xff;
	pr_err("reg:%04x , read back value:%04x\n", 0x320f, value);

	value = sc2336_read(client, 0x320c)&0xff;
	pr_err("reg:%04x , read back value:%04x\n", 0x320c, value);

	value = sc2336_read(client, 0x320d)&0xff;
	pr_err("reg:%04x , read back value:%04x\n", 0x320d, value);
#endif
	return 0;
}

/*read sensor register*/
static int sc2336_sensor_read_reg_func(void *arg, const int reg_addr)
{
	struct sc2336_priv *priv = arg;
	struct i2c_client *client = priv->client;

	return sc2336_read(client, reg_addr);
}
/*write sensor register*/
static int sc2336_sensor_write_reg_func(void *arg, const int reg_addr, int value)
{
	struct sc2336_priv *priv = arg;
	struct i2c_client *client = priv->client;

	return sc2336_write(client, reg_addr, value);
}
/*read sensor register, NO i2c ops*/
static int sc2336_sensor_read_id_func(void *arg)   //no use IIC bus
{
	return SENSOR_ID;
}

static int aec_parms_init(struct sc2336_priv *priv)
{
	priv->aec_parms.curr_again_level = 0;
	priv->aec_parms.curr_again_10x = -1;
	priv->aec_parms.r0x3e02_value = 0;
	priv->aec_parms.r0x3e01_value = 0;
	priv->aec_parms.r0x3e00_value = 0;
	priv->aec_parms.target_exp_ctrl = 0;
	priv->aec_parms.curr_2x_dgain = 0;
	priv->aec_parms.curr_corse_gain = -1;

	priv->aec_parms.reg_frame_vts = 0;
	priv->aec_parms.reg_frame_hts = 0;
	priv->aec_parms.pclk_freq = 0;
	priv->aec_parms.calc_vts_tmp = 0;
	priv->aec_parms.r0x3e08_corse_gain = 0;
	priv->aec_parms.r0x3e09_fine_gain = 0;

	return 0;
}

/*set sensor again*/
static int sc2336_sensor_update_a_gain_func(void *arg, const unsigned int a_gain)
{
	/*
	 * max again < 25 times = 8192
	 * */
	struct sc2336_priv *priv = arg;
	struct i2c_client *client = priv->client;
	int const gain = a_gain * 1000 / 256;
    int ana_gain = 0x00;
    int dig_gain = 0x00;
    int dig_fine_gain = 0x80;
    int x = 0,range = 0;
    x = gain / 1000;
    range = 0xff - 0x80;

    if (gain < 1000) {
        
        ana_gain = 0x00;
        dig_gain = 0x00;
        dig_fine_gain = 0x80;
    
    } else if (gain < 2000) {
        
        ana_gain = 0x00;
        dig_gain = 0x00;
        dig_fine_gain = (gain - 1000) * range / (2000 - 1000) + 0x80;
        
    } else if (gain < 4000) {
        
        ana_gain = 0x08;
        dig_gain = 0x00;
        dig_fine_gain = (gain - 2000) * range / (4000 - 2000) + 0x80;
        
    } else if (gain < 8000) {
            
        ana_gain = 0x09;
        dig_gain = 0x00;
        dig_fine_gain = (gain - 4000) * range / (8000 - 4000) + 0x80;
        
    } else if (gain < 16000) {
        
        ana_gain = 0x0b;
        dig_gain = 0x00;
        dig_fine_gain = (gain - 8000) * range / (16000 - 8000) + 0x80;
 
    } else if (gain < 32000) {

        ana_gain = 0x0f;
        dig_gain = 0x00;
        dig_fine_gain = (gain - 16000) * range / (32000 - 16000) + 0x80;
    
    } else {
        
        ana_gain = 0x1f;
        dig_gain = 0x00;
        dig_fine_gain = 0x80;
        
    }

    sc2336_write(client, 0x3e09,ana_gain);
    dig_fine_gain = dig_fine_gain & 0xfc;
    sc2336_write(client, 0x3e06,dig_gain);
    sc2336_write(client, 0x3e07,dig_fine_gain);
	return A_GAIN_EFFECT_FRAMES;
}
/*set sensor dgain*/
static int sc2336_sensor_update_d_gain_func(void *arg, const unsigned int d_gain)
{
	/*
	 * max dgain < 32 times = 4096
	 * */
#if 0
	struct sc2336_priv *priv = arg;
	struct i2c_client *client = priv->client;
	int const gain = d_gain * 1000 / 256;
    int dig_gain = 0x00;
    int dig_fine_gain = 0x80;
    int x = 0,range = 0;
    x = gain / 1000;
    range = 0xfe - 0x80;
  
    if (x < 1) {
        
        dig_gain = 0x00;
        dig_fine_gain = 0x80;
        
    }else if (x < 2) {
        
        dig_gain = 0x00;
        dig_fine_gain = (gain - 1000) * range / (1984 - 1000) + 0x80;
        
    }else if (x < 4) {
         
        dig_gain = 0x01;
        dig_fine_gain = (gain - 2000) * range / (3969 - 2000) + 0x80;
        
    }else {
        
        dig_gain = 0x03;
        dig_fine_gain = 0x80;
        
    }
    
    dig_fine_gain = dig_fine_gain & 0xfc;
    
    sc2336_write(client, 0x3e06,dig_gain);
    sc2336_write(client, 0x3e07,dig_fine_gain);
#endif
	return D_GAIN_EFFECT_FRAMES;
}
/*set sensor exp time*/
static int __set_reg_exp_time(struct sc2336_priv *priv, int exp_time)
{
	int ret = 0;
	struct i2c_client *client = priv->client;

	if(exp_time < 1) exp_time = 1;
	if(exp_time > 8191) exp_time = 8191;    //2^13
	
	pr_debug("%s, exp:%d\n", __func__, exp_time);

	priv->aec_parms.r0x3e00_value = (exp_time>>12)&0x0f;
	priv->aec_parms.r0x3e01_value = (exp_time>>4)&0xff;
	priv->aec_parms.r0x3e02_value = ((exp_time)&0xf)<<4;

	sc2336_write(client, 0x3e00, priv->aec_parms.r0x3e00_value);
	sc2336_write(client, 0x3e01, priv->aec_parms.r0x3e01_value);
	sc2336_write(client, 0x3e02, priv->aec_parms.r0x3e02_value);

	//priv->aec_parms.target_exp_ctrl = exp_time;

	return ret;
}

static int __set_reg_frame_vts(struct sc2336_priv *priv, int vts)
{
	int ret = 0;
	struct i2c_client *client = priv->client;
	if(vts < 1)
		vts = 1;
	
	pr_debug("%s, vts:%d\n", __func__, vts);

	sc2336_write(client, 0x320e, (vts >> 8));
	sc2336_write(client, 0x320f, (vts & 0xff));

	priv->fps_info.reg_fps_value = vts;

	return ret;
}

static int __get_reg_frame_vts(struct sc2336_priv *priv)
{
	struct i2c_client *client = priv->client;

	return (sc2336_read(client, 0x320e)) << 8 | sc2336_read(client, 0x320f);
}

static void update_frame_vts_and_exp_ctrl(struct sc2336_priv *priv)
{
	struct i2c_client *client = priv->client;

	/*
	 * exp_time=1 mean half line exptime
	 * max exp_time = {R0x320e[6:0],R0x320f} - 10
	 *
	 * now:
	 * at max_exptime_30fps
	 * = {R0x320e[6:0],R0x320f} - 10
	 * = 0x470 - 10
	 * = 1136 -10
	 * = 1126
	 */

	int const cur_frame_vts     = __get_reg_frame_vts(priv);
	int const target_frame_vts  = priv->fps_info.to_fps_value;

    int const max_exp_ctrl      = priv->fps_info.to_fps_value - 4;   //Vts-4

    if(target_frame_vts <= 0)
        return ;
    
	int target_exp_ctrl = 0;
	if (priv->aec_parms.target_exp_ctrl <= max_exp_ctrl) {
		target_exp_ctrl = priv->aec_parms.target_exp_ctrl;
		//pr_err("priv->target_exp_ctrl <= max_exp_ctrl, use priv->aec_parms.target_exp_ctrl:%d\n",priv->aec_parms.target_exp_ctrl);
	} else {
		target_exp_ctrl = max_exp_ctrl;
		//pr_err("priv->target_exp_ctrl > max_exp_ctrl, use max_exp_ctrl:%d\n", max_exp_ctrl);
	}

	//pr_err("VTS = %d->%d \n", cur_frame_vts, target_frame_vts);
	//pr_err("EXP = %d/%d \n", target_exp_ctrl, priv->aec_parms.target_exp_ctrl);
    if(target_frame_vts <= 0)
        return ;
	if (target_frame_vts >= cur_frame_vts) {
		if (target_frame_vts != cur_frame_vts)
			__set_reg_frame_vts(priv, target_frame_vts);
		__set_reg_exp_time(priv, target_exp_ctrl);
	} else {
		__set_reg_exp_time(priv, target_exp_ctrl);
		__set_reg_frame_vts(priv, target_frame_vts);
	}
}

/*set sensor exp time*/
static int sc2336_sensor_updata_exp_time_func(void *arg, unsigned int exp_time)
{
	struct sc2336_priv *priv = arg;
	struct i2c_client *client = priv->client;

	priv->aec_parms.target_exp_ctrl = exp_time;
	update_frame_vts_and_exp_ctrl(priv);

	return EXP_EFFECT_FRAMES;
}
/*sensor timer*/
static int sc2336_sensor_timer_func(void *arg)
{
	return 0;
}
/*standby in*/
static int sc2336_sensor_set_standby_in_func(void *arg)
{
	return 0;
}
/*standby out*/
static int sc2336_sensor_set_standby_out_func(void *arg)
{
	return 0;
}
/*low level read sensor ID, user i2c ops*/
static int sc2336_sensor_probe_id_func(void *arg)  //use IIC bus
{
	struct sc2336_priv *priv = arg;
	struct i2c_client *client = priv->client;
	int id;
	int value;

	value = sc2336_read(client, 0x3107);
	id = value << 8;

	value = sc2336_read(client, 0x3108);
	id |= value;

	pr_err("id:%x\n", id);

	if (id == ACTUAL_SENSOR_ID)
		return SENSOR_ID;

	return 0;
}
/*get resolution*/
static int sc2336_sensor_get_resolution_func(void *arg, int *width, int *height)
{
	*width = SENSOR_OUTPUT_WIDTH;
	*height = SENSOR_OUTPUT_HEIGHT;
	return 0;
}
/*get mclk*/
static int sc2336_sensor_get_mclk_func(void *arg)
{
	return SENSOR_MCLK;
}
/*get current fps*/
static int sc2336_sensor_get_fps_func(void *arg)
{
	struct sc2336_priv *priv = arg;

	return priv->fps_info.current_fps;
}
/*get valid coordinate*/
static int sc2336_sensor_get_valid_coordinate_func(void *arg, int *x, int *y)
{
	*x = SENSOR_VALID_OFFSET_X;
	*y = SENSOR_VALID_OFFSET_Y;
	return 0;
}
/*get bus type*/
static enum sensor_bus_type sc2336_sensor_get_bus_type_func(void *arg)
{
	return SENSOR_BUS_TYPE;
}
static int get_ae_fast_default(struct ae_fast_struct *ae_fast)
{
	*ae_fast = ae_fast_default;
	return 0;
}
/*get self definded params*/
static int sc2336_sensor_get_parameter_func(void *arg, int param, void *value)
{
	int ret = 0;
	struct sc2336_priv *priv = arg;

	switch (param) {
		case GET_INTERFACE:
			*((int *)value) = dvp ? DVP_INTERFACE:MIPI_INTERFACE;
			break;

		case GET_IO_LEVEL:
			*((int *)value) = SENSOR_IO_LEVEL;
			break;

		case GET_MIPI_MHZ:
			*((int *)value) = MIPI_MBPS;
			break;

		case GET_MIPI_LANES:
			*((int *)value) = MIPI_LANES;
			break;

		case GET_RESET_GPIO:
			*((int *)value) = priv->gpio_reset;
			break;

		case GET_PWDN_GPIO:
			*((int *)value) = priv->gpio_pwdn;
			break;

		case GET_AE_FAST_DEFAULT:
			get_ae_fast_default(value);
			break;

		case GET_SCAN_METHOD:
			ret = -1;
		default:
			pr_err("%s param:%d not support\n", __func__, param);
			ret = -1;
			break;
	}

	return ret;
}
/*sensor power on*/
static int sc2336_sensor_set_power_on_func(void *arg)
{
	struct sc2336_priv *priv = arg;

	if (priv->gpio_pwdn >= 0) {
		gpio_direction_output(priv->gpio_pwdn, !SENSOR_PWDN_LEVEL);
	}

	if (priv->gpio_reset >= 0) {
		gpio_direction_output(priv->gpio_reset, !SENSOR_RESET_LEVEL);
		msleep(50);
		gpio_direction_output(priv->gpio_reset, SENSOR_RESET_LEVEL);
		msleep(50);
		gpio_direction_output(priv->gpio_reset, !SENSOR_RESET_LEVEL);
		msleep(100);
	}

	aec_parms_init(priv);

	return 0;
}
/*sensor power off*/
static int sc2336_sensor_set_power_off_func(void *arg)
{
	struct sc2336_priv *priv = arg;

	if (priv->gpio_pwdn >= 0) {
		gpio_direction_input(priv->gpio_pwdn);
	}

	if (priv->gpio_reset >= 0) {
		gpio_direction_input(priv->gpio_reset);
	}

	return 0;
}
/*set sensor fps*/
static int sc2336_fps_to_vts(struct sc2336_priv *priv, const int fps)
{
	/*
	 * sc2336:
	 *
	 * mipi:
	 * HTS=(0x320c,0x320d)*2
	 * VTS=(0x320e[6:0],0x320f)
	 * fps = pclk / HTS / VTS
	 *
	 * now pclk=74976000hz
	 * HTS=0x898=2200
	 * VTS=0x470=1136 at 30fps
	 *
	 * so :
	 * vts = 74976000 / 2200 / fps = 34080 / fps
	 *     = (34080*100) / (fps*100)
	 *     = (3408000UL) / vts;//vts=fps*100
	 *
	 */
	int vts;

	pr_debug("%s %d fps:%d tmp:%d\n", __func__, __LINE__, fps, priv->aec_parms.calc_vts_tmp);

	switch (fps) {
		case 12:
			vts = 1250;
			break;

		case 14:
			vts = 1428;
			break;

		default:
			vts = fps * 100;
			break;
	}

	vts = (priv->aec_parms.calc_vts_tmp) / vts;
	// vts = (3408000UL) / vts;

	return vts;
}

/*set sensor fps*/
static int sc2336_sensor_set_fps_func(void *arg, const int fps)
{
	int tmp;
	struct sc2336_priv *priv = arg;

	tmp = sc2336_fps_to_vts(priv, fps);

	if (tmp > 0) {
		priv->fps_info.to_fps_value = tmp;
		priv->fps_info.to_fps = fps;
	}
	return 0;
}

static void sc2336_set_fps_async(struct sc2336_priv *priv)
{
	struct i2c_client *client = priv->client;

	if (priv->fps_info.to_fps != priv->fps_info.current_fps) {
		update_frame_vts_and_exp_ctrl(priv);
		priv->fps_info.current_fps = priv->fps_info.to_fps;
	}
}

static AK_ISP_SENSOR_CB sc2336_cb = {
	.sensor_init_func							= sc2336_sensor_init_func,
	.sensor_read_reg_func						= sc2336_sensor_read_reg_func,
	.sensor_write_reg_func						= sc2336_sensor_write_reg_func,
	.sensor_read_id_func						= sc2336_sensor_read_id_func,
	.sensor_update_a_gain_func					= sc2336_sensor_update_a_gain_func,
	.sensor_update_d_gain_func					= sc2336_sensor_update_d_gain_func,
	.sensor_updata_exp_time_func				= sc2336_sensor_updata_exp_time_func,
	.sensor_timer_func							= sc2336_sensor_timer_func,
	.sensor_set_standby_in_func					= sc2336_sensor_set_standby_in_func,
	.sensor_set_standby_out_func				= sc2336_sensor_set_standby_out_func,
	.sensor_probe_id_func						= sc2336_sensor_probe_id_func,
	.sensor_get_resolution_func					= sc2336_sensor_get_resolution_func,
	.sensor_get_mclk_func						= sc2336_sensor_get_mclk_func,
	.sensor_get_fps_func						= sc2336_sensor_get_fps_func,
	.sensor_get_valid_coordinate_func			= sc2336_sensor_get_valid_coordinate_func,
	.sensor_get_bus_type_func					= sc2336_sensor_get_bus_type_func,
	.sensor_get_parameter_func					= sc2336_sensor_get_parameter_func,
	.sensor_set_power_on_func					= sc2336_sensor_set_power_on_func,
	.sensor_set_power_off_func					= sc2336_sensor_set_power_off_func,
	.sensor_set_fps_func						= sc2336_sensor_set_fps_func,
};

static int sensor_id_func(void)
{
	return sc2336_sensor_read_id_func(NULL);
}

static char *sensor_if_func(void)
{
	static char ifstr[16] = "dvp";

	if (!dvp)
		sprintf(ifstr, "mipi%d", MIPI_LANES);

	return ifstr;
}

static int call_sensor_sys_init(void)
{
	return sensor_sys_init(sensor_id_func, sensor_if_func);
}

static int call_sensor_sys_deinit(void)
{
	sensor_sys_exit();
	return 0;
}
