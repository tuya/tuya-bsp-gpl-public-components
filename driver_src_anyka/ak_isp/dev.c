/*
 * dev.c
 *
 * master interface for isp device
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
#include <linux/init.h>
#include <linux/module.h>
//#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
//#include <linux/sched.h>
//#include <linux/dma/ipu-dma.h>
//#include <linux/delay.h>

#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#if 0
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-dma-contig.h>
#endif
#include <linux/of_graph.h>

#include <mach/map.h>

/*private camere include files*/
#if 0
#include "include/ak_isp_drv.h"
#include "include/ak_video_priv_cmd.h"
#include "include_internal/ak_video_priv_cmd_internal.h"
#include "include/ak_isp_char.h"
#include "ak_host.h"
#else
#include "cif.h"
#include "../include/ak_video_priv_cmd.h"
#include "video.h"
#include "isp_param.h"
#include "isp_stats.h"
#include "sys_isp.h"
#include "sys_host.h"
#include "sys_sensor.h"
#endif

static unsigned long internal_pclk = 75000000;//defalut 75MHz
module_param(internal_pclk, ulong, S_IRUGO);

/*
 * get_sensor_cb -
 * all sensor callbacks should through host driver.
 * all channels sensor callback ponit to an same sensor.
 *
 * @ak_cam:				point to camera host
 */
static struct sensor_cb_info *get_sensor_cb(struct v4l2_subdev *sd)
{
	int ret;
	struct v4l2_control ctrl;

	ctrl.id = GET_SENSOR_CB;
	ret = v4l2_subdev_call(sd, core, g_ctrl, &ctrl);
	if (ret) {
		pr_err("%s get subdev call fail\n", __func__);
		return NULL;
	}

	/*all sensor must be implemented*/
	if ((void *)ctrl.value == NULL) {
		pr_err("%s get subdev to sensor cb fail\n", __func__);
		return NULL;
	}

	return (struct sensor_cb_info *)ctrl.value;
}

static int fill_sensor_attr(struct sensor_attr *sensor, struct v4l2_subdev *sd)
{
	int ret;
	struct sensor_cb_info *sensor_cbi;
	int interface;
	int io_level;
	int mipi_mbps;
	int mipi_lanes;
	int reset_gpio;
	int pwdn_gpio;
	int sclk_mhz;

	sensor_cbi = get_sensor_cb(sd);
	if (!sensor_cbi) {
		pr_err("%s sensor_cbi is NULL\n", __func__);
		ret = -EPERM;
		goto error;
	}
	sensor->sd = sd;
	sensor->sensor_cbi = sensor_cbi;

	/*interface*/
	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_INTERFACE, &interface);
	if (ret) {
		pr_err("%s get sensor interface fail\n", __func__);
		interface = -1;
		goto error;
	}

	/*io_level*/
	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_IO_LEVEL, &io_level);
	if (ret) {
		pr_info("%s get sensor io_level fail\n", __func__);
		io_level = -1;
	}

	/*mipi_mbps*/
	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_MIPI_MHZ, &mipi_mbps);
	if (ret) {
		pr_info("%s get sensor mipi_mbps fail\n", __func__);
		mipi_mbps = -1;
	}

	/*mipi_lanes*/
	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_MIPI_LANES, &mipi_lanes);
	if (ret) {
		pr_info("%s get sensor mipi_lanes fail\n", __func__);
		mipi_lanes = -1;
	}

	/*reset_gpio*/
	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_RESET_GPIO, &reset_gpio);
	if (ret) {
		pr_info("%s get sensor reset_gpio fail\n", __func__);
		reset_gpio = -1;
	}

	/*pwdn_gpio*/
	ret = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_PWDN_GPIO, &pwdn_gpio);
	if (ret) {
		pr_info("%s get sensor pwdn_gpio fail\n", __func__);
		pwdn_gpio = -1;
	}

	/*sclk_mhz*/
	sclk_mhz = sensor_cbi->cb->sensor_get_mclk_func(sensor_cbi->arg);
	if (sclk_mhz <= 0) {
		pr_err("%s get sensor sclk_mhz invalid, sclk_mhz:%d\n", __func__, sclk_mhz);
		goto error;
	}

	/*check those paramters below*/

	if (interface == DVP_INTERFACE && io_level < 0) {
		pr_err("%s set dvp_interface but not set io_level, fail\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	if (interface == MIPI_INTERFACE && mipi_lanes < 0) {
		pr_err("%s set mipi_interface but not set mipi_lanes, fail\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	/*sensor subdev*/
	sensor->sd = sd;

	sensor->interface	= interface;
	sensor->mipi_mbps	= mipi_mbps;
	sensor->io_level	= io_level;
	sensor->mipi_lanes	= mipi_lanes;
	sensor->reset_gpio	= reset_gpio;
	sensor->pwdn_gpio	= pwdn_gpio;
	sensor->sclk_mhz	= sclk_mhz;

	return 0;

error:
	return ret;
}

/*
 * notifier_bound - processor when subdev ready to bound
 *
 * @notifier:		point to async notifier
 * @sd:				subdev
 * @asd:			async subdev
 */
static int notifier_bound(struct v4l2_async_notifier *notifier,
		struct v4l2_subdev *sd,
		struct v4l2_async_subdev *asd)
{
	struct hw_attr *hw = notifier_to_hw(notifier);
	struct ak_camera_dev *ak_cam = hw_to_ak_cam(hw);
	struct input_video_attr *input_video;
	struct sensor_attr *sensor;

	pr_debug("%s %d subdev:%p asd:%p sd->name:%s\n",
			__func__, __LINE__, sd, asd, sd->name);

	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		if (input_video->asd == asd) {
			/*match input video*/
			sensor = &input_video->sensor;

			/*fill sensor attr structure*/
			if (!fill_sensor_attr(sensor, sd))
				return sys_sensor_init(sensor);
		}
	}

	return -ENXIO;
}

/*
 * notifier_complete - processor when subdev bound finish
 *
 * @notifier:			point to async notifier
 */
static int notifier_complete(struct v4l2_async_notifier *notifier)
{
	struct hw_attr *hw = notifier_to_hw(notifier);

	return v4l2_device_register_subdev_nodes(&hw->v4l2_dev);
}

/*
 * parse_clock - isp clock init
 *
 * @pdev:			platform device struct
 * @ak_cam:			host sturct
 */
static int parse_clock(struct platform_device *pdev, struct ak_camera_dev *ak_cam)
{
	void __iomem *norm_base;
	unsigned char __iomem *mipi0_base, *mipi1_base;
	struct device *dev = &pdev->dev;
	struct resource iomem;
	struct device_node *node = dev->of_node;
	struct clk *sclk0, *sclk1, *isp_clk, *mipi_csi_clk;
	struct hw_attr *hw = &ak_cam->hw;
	int ret;
	int irq;

	pr_debug("%s %d\n", __func__, __LINE__);

	/*get normal res*/
	ret = of_address_to_resource(node, 0, &iomem);
	if (ret) {
		pr_err("%s %d could not get normal resource\n", __func__, __LINE__);
		goto error;
	}
	norm_base = devm_ioremap_resource(dev, &iomem);
	if (IS_ERR(norm_base)) {
		pr_err("%s %d could not get io resource\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error;
	}

	sclk0 = devm_clk_get(&pdev->dev, "csi_sclk");
	//sclk0 = devm_clk_get(&pdev->dev, "csi_sclk0");
	if (IS_ERR(sclk0)) {
		pr_err("%s %d could not get csi_sclk\n", __func__, __LINE__);
		ret = -EINVAL;
		goto error;
	}

	isp_clk = devm_clk_get(&pdev->dev, "isp_clk");
	if (IS_ERR(isp_clk)) {
		pr_err("%s %d could not get isp_clk\n", __func__, __LINE__);
		ret = -EINVAL;
		goto error;
	}

	mipi_csi_clk = devm_clk_get(&pdev->dev, "mipi_csi_clk");
	if (IS_ERR(mipi_csi_clk)) {
		pr_err("%s %d could not get mipi_csi_clk\n", __func__, __LINE__);
		ret = -EINVAL;
		goto error;
	}

	irq = irq_of_parse_and_map(node, 0);
	if (irq <= 0) {
		pr_err("%s %d could not get irq\n", __func__, __LINE__);
		ret = -EINVAL;
		goto error;
	}

	mipi0_base = AK_VA_MIPI1;
#ifdef AK_VA_MIPI2
	mipi1_base = AK_VA_MIPI2;
#else
	mipi1_base = NULL;
#endif

	hw->norm_base = norm_base;
	hw->mipi0_base = mipi0_base;
	hw->mipi1_base = mipi1_base;
	hw->sclk0 = sclk0;
	hw->sclk1 = sclk1;
	hw->isp_clk = isp_clk;
	hw->mipi_csi_clk = mipi_csi_clk;
	hw->irq = irq;

	hw->isp_clk_rate = clk_get_rate(isp_clk);
	hw->internal_pclk = internal_pclk;

	/*init isp output format*/
	//	hw->isp_output_format = YUV420_SEMI_PLANAR;
	/*set sensor callbacks*/
	//	hw->host_sensor_cb_info.cb = &sensor0_cb;
	//	hw->host_sensor_cb_info.arg = ak_cam;

	return 0;

error:
	return ret;
}

/*
 * create_input_nodes - create nodes for the input channel
 *
 * @input_video:		pointer to input video channel
 *
 * @RETURN: 0-success, others-fail
 */
static int create_input_nodes(struct input_video_attr *input_video)
{
	struct isp_attr *isp = &input_video->isp;
	struct isp_stats_attr *isp_stats = &input_video->isp_stats;
	int ret;

	/*video-stream init*/
	ret = input_video_init(input_video);
	if (ret) {
		pr_err("%s video_chns_init fail\n", __func__);
		goto end;
	}

	/*video-stream register*/
	ret = input_video_register(input_video);
	if (ret) {
		pr_err("%s video_chns_register fail\n", __func__);
		goto error_video_chns_reg;
	}

	/*isp-param init*/
	ret = isp_param_init(isp);
	if (ret) {
		pr_err("%s isp_param_init fail\n", __func__);
		goto error_isp_param_init;
	}

	/*isp-param register*/
	ret = isp_param_register(isp);
	if (ret) {
		pr_err("%s isp_param_register fail\n", __func__);
		goto error_isp_param_reg;
	}

	/*isp-stats init*/
	ret = isp_stats_init(isp_stats);
	if (ret) {
		pr_err("%s isp_stats_init fail\n", __func__);
		goto error_isp_stats_init;
	}

	/*isp-stats register*/
	ret = isp_stats_register(isp_stats);
	if (ret) {
		pr_err("%s isp_stats_register fail\n", __func__);
		goto error_isp_stats_reg;
	}

	return 0;

error_isp_stats_reg:
	isp_stats_uninit(isp_stats);
error_isp_stats_init:
	isp_param_unregister(isp);
error_isp_param_reg:
	isp_param_uninit(isp);
error_isp_param_init:
	input_video_unregister(input_video);
error_video_chns_reg:
	input_video_uninit(input_video);
end:
	return ret;
}

/*
 * release_input_nodes - release nodes for the input channel
 *
 * @input_video:		pointer to input video channel
 *
 * @RETURN: 0-success, others-fail
 */
static int release_input_nodes(struct input_video_attr *input_video)
{
	struct isp_attr *isp = &input_video->isp;
	struct isp_stats_attr *isp_stats = &input_video->isp_stats;

	/*isp-stats unregister*/
	isp_stats_unregister(isp_stats);

	/*isp-stats uninit*/
	isp_stats_uninit(isp_stats);

	/*isp-param unregister*/
	isp_param_unregister(isp);

	/*isp-param uninit*/
	isp_param_uninit(isp);

	/*video-stream unregister*/
	input_video_unregister(input_video);

	/*video-stream uninit*/
	input_video_uninit(input_video);

	return 0;
}

/*
 * create_inputs_nodes - create nodes for the all input channel
 *
 * @ak_cam:		pointer to ak camera device
 *
 * @RETURN: 0-success, others-fail
 */
static int create_inputs_nodes(struct ak_camera_dev *ak_cam)
{
	int ret;
	int input_id;
	struct hw_attr *hw = &ak_cam->hw;
	struct v4l2_async_notifier *notifier = &hw->notifier;
	struct input_video_attr *input_video, *t_input_video;

	pr_debug("%s\n", __func__);

	/*register v4l2 device*/
	ret = v4l2_device_register(hw->dev, &hw->v4l2_dev);
	if (ret) {
		pr_err("%s v4l2_device_register fail, ret:%d\n", __func__, ret);
		goto error_register_v4l2_dev;
	}

	ak_cam->input_num = 0;
	INIT_LIST_HEAD(&ak_cam->input_head);

	for (input_id = 0; input_id < hw->input_num; input_id++) {
		/*alloc input video structure*/
		//input_video = devm_kzalloc(hw->dev, sizeof(*input_video), GFP_KERNEL);
		input_video = devm_kzalloc(hw->dev, sizeof(struct input_video_attr), GFP_KERNEL);
		if (!input_video) {
			pr_err("%s alloc for input_video fail, input_id:%d\n",
					__func__, input_id);
			ret = -ENOMEM;
			goto error_alloc;
		}

		/*fill info*/
		input_video->input_id = input_id;
		input_video->ak_cam = ak_cam;
		/*subdev bounding used to match input video*/
		input_video->asd = notifier->subdevs[input_id];

		/*create nodes*/
		ret = create_input_nodes(input_video);
		if (ret) {
			pr_err("create nodes for input_id:%d fail, ret:%d\n", input_id, ret);
			goto error_create_nodes;
		}

		/*add to list*/
		list_add_tail(&input_video->list, &ak_cam->input_head);
	}

	return 0;

error_create_nodes:
	devm_kfree(hw->dev, input_video);
error_alloc:
	list_for_each_entry_safe(input_video, t_input_video, &ak_cam->input_head, list) {
		/*release nodes*/
		ret = release_input_nodes(input_video);
		if (ret) {
			pr_err("release nodes for input_id:%d fail, ret:%d\n", input_video->input_id, ret);
			return ret;
		}

		/*delete from list*/
		list_del_init(&input_video->list);

		/*release memory*/
		devm_kfree(hw->dev, input_video);
	}

	v4l2_device_unregister(&hw->v4l2_dev);
error_register_v4l2_dev:
	return ret;
}

/*
 * release_inputs_nodes - release nodes for the all input channel
 *
 * @ak_cam:		pointer to ak camera device
 *
 * @RETURN: 0-success, others-fail
 */
static int release_inputs_nodes(struct ak_camera_dev *ak_cam)
{
	struct input_video_attr *input_video, *t_input_video;
	struct hw_attr *hw = &ak_cam->hw;
	int ret;

	pr_debug("%s\n", __func__);

	list_for_each_entry_safe(input_video, t_input_video, &ak_cam->input_head, list) {
		/*release nodes*/
		ret = release_input_nodes(input_video);
		if (ret) {
			pr_err("release nodes for input_id:%d fail, ret:%d\n", input_video->input_id, ret);
			return ret;
		}

		/*delete from list*/
		list_del_init(&input_video->list);

		/*release memory*/
		devm_kfree(hw->dev, input_video);
	}

	v4l2_device_unregister(&hw->v4l2_dev);

	return 0;
}

/*
 * of_parse_nodes -
 * parse nodes and create correspond subdevs and async subdev
 *
 * @dev:			point to device
 * @notifier:		point to the async_notifier
 */
static int of_parse_nodes(struct device *dev,
		struct v4l2_async_notifier *notifier)
{
	struct device_node *node = NULL;

	notifier->subdevs = devm_kcalloc(
			dev, ISP_MAX_SUBDEVS, sizeof(*notifier->subdevs), GFP_KERNEL);
	if (!notifier->subdevs) {
		pr_err("%s %d malloc subdevs fail\n", __func__, __LINE__);
		return -ENOMEM;
	}

	while (notifier->num_subdevs < ISP_MAX_SUBDEVS &&
			(node = of_graph_get_next_endpoint(dev->of_node, node))) {
		struct v4l2_async_subdev *asd;

		asd = devm_kzalloc(dev, sizeof(*asd), GFP_KERNEL);
		if (!asd) {
			pr_err("%s %d malloc asd fail, num_subdevs:%d\n",
					__func__, __LINE__, notifier->num_subdevs);
			of_node_put(node);
			return -ENOMEM;
		}

		notifier->subdevs[notifier->num_subdevs] = asd;

		asd->match.of.node = of_graph_get_remote_port_parent(node);
		of_node_put(node);
		if (!asd->match.of.node) {
			pr_err("bad remote port parent, num_subdevs:%d\n", notifier->num_subdevs);
			return -EINVAL;
		}

		asd->match_type = V4L2_ASYNC_MATCH_OF;
		notifier->num_subdevs++;
	}

	return notifier->num_subdevs;
}

/*
 * ak_camera_interrupt -
 * irq hander of camera host
 *
 * @irq:			the number of camera host
 * @data:			private data
 */
static irqreturn_t ak_camera_interrupt(int irq, void *data)
{
	struct ak_camera_dev *ak_cam = data;
	struct hw_attr *hw = &ak_cam->hw;
	struct input_video_attr *input_video;
	struct isp_attr *isp;
	struct isp_stats_attr *isp_stats;
	int capturing_input_id = hw->capturing_input_id;
	int match = 0;

	/*
	 * capturing_input_id: must set the id before start capturing
	 */
	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		if (input_video->input_id == capturing_input_id) {
			match = 1;
			break;
		}
	}

	if (!match) {
		pr_err("%s cannot find capturng_input_id:%d\n", __func__, capturing_input_id);
		goto end;
	}

	isp = &input_video->isp;
	isp_stats = &input_video->isp_stats;

	input_video_process_irq_start(input_video);
	isp_param_process_irq_start(isp);
	isp_stats_process_irq_start(isp_stats);

	isp_stats_process_irq_end(isp_stats);
	isp_param_process_irq_end(isp);
	input_video_process_irq_end(input_video);

end:
	return IRQ_HANDLED;
}

/*
 * ak_camera_probe -
 * processor when camera host driver probe
 *
 * @pdev:			platform device
 */
static int ak_camera_probe(struct platform_device *pdev)
{
	struct ak_camera_dev *ak_cam;
	struct hw_attr *hw;
	int ret;

	pr_debug("%s %d\n", __func__, __LINE__);

	/*alloc the global  struct*/
	ak_cam = devm_kzalloc(&pdev->dev, sizeof(*ak_cam), GFP_KERNEL);
	if (!ak_cam) {
		pr_err("%s allocate memory for ak_cam fail\n", __func__);
		ret = -ENOMEM;
		goto error_alloc;
	}

	/*store hw info*/
	hw = &ak_cam->hw;
	hw->dev = &pdev->dev;
	hw->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	hw->pinctrl = devm_pinctrl_get(hw->dev);
	if (IS_ERR_OR_NULL(hw->pinctrl)) {
		pr_err("%s get pinctrl:%p,%ld, dev:%p, fail",
				__func__, hw->pinctrl, PTR_ERR(hw->pinctrl), hw->dev);
		ret = PTR_ERR(hw->pinctrl);
		goto error_pinctrl;
	}
	platform_set_drvdata(pdev, ak_cam);

	/*parse related clocks*/
	spin_lock_init(&hw->lock);
	ret = parse_clock(pdev, ak_cam);
	if (ret) {
		pr_err("%s parse_clock fail, ret:%d\n", __func__, ret);
		goto error_parse_clock;
	}

#if 0
	hw->clock_init = 1;
	hw->capture_rawdata_start = 0;
	hw->capture_rawdata_done = 0;
	init_waitqueue_head(&(hw->capture_rawdata_wait_queue));
#endif

	pr_debug("%s %s %s\n", __func__, __DATE__, __TIME__);

	/*parse video node*/
	ret = of_parse_nodes(&pdev->dev, &hw->notifier);
	if (ret < 0) {
		pr_err("%s of_parse_nodes fail\n", __func__);
		goto error_parse_nodes;
	}
	pr_info("number of ak video subdev: %d\n", ret);
	hw->input_num = ret;
	pr_info("%s %d\n", __func__, __LINE__);

	/*create video & subdev nodes*/
	ret = create_inputs_nodes(ak_cam);
	if (ret < 0) {
		pr_err("%s create_inputs_nodes fail\n", __func__);
		goto error_create_inputs_nodes;
	}
	pr_info("%s %d\n", __func__, __LINE__);

	/*register async notifier*/
	hw->notifier.bound		= notifier_bound;
	hw->notifier.complete	= notifier_complete;
	ret = v4l2_async_notifier_register(&hw->v4l2_dev, &hw->notifier);
	if (ret) {
		pr_err("%s v4l2_async_notifier_register fail\n", __func__);
		goto error_notifier_register;
	}

	/*request irq procession*/
	ret = devm_request_irq(
			&pdev->dev, hw->irq, ak_camera_interrupt, 0, "ak_camera", ak_cam);
	if (ret) {
		pr_err("%s %d could not request irq, ret:%d\n", __func__, __LINE__, ret);
		goto error_request_irq;
	}

	mutex_init(&hw->capture_pause_lock);
	init_waitqueue_head(&(hw->set_capture_pause_queue));
	init_waitqueue_head(&(hw->rawdata_capture_wait_queue));

	/*sys debug interface*/
	sys_isp_init(ak_cam);
	sys_host_init(ak_cam);

	return 0;

error_request_irq:
	v4l2_async_notifier_unregister(&hw->notifier);
error_notifier_register:
	release_inputs_nodes(ak_cam);
error_create_inputs_nodes:
error_parse_nodes:
error_parse_clock:
error_pinctrl:
	devm_kfree(hw->dev, ak_cam);
error_alloc:
	return ret;
}

static void sys_sensors_exit(struct ak_camera_dev *ak_cam)
{
	struct input_video_attr *input_video;

	list_for_each_entry(input_video, &ak_cam->input_head, list) {
		sys_sensor_exit(&input_video->sensor);
	}
}

/*
 * ak_camera_remove -
 * prcessor when camera host driver removed
 *
 * @pdev				platform device
 */
static int ak_camera_remove(struct platform_device *pdev)
{
	struct ak_camera_dev *ak_cam = platform_get_drvdata(pdev);
	struct hw_attr *hw = &ak_cam->hw;

	pr_debug("%s %d\n", __func__, __LINE__);

	/*sys debug interface*/
	sys_host_exit(ak_cam);
	sys_isp_exit(ak_cam);
	/*
	 * sys_sesnor_exit should before v4l2_async_notifier_unregister
	 * where free sensor's struct device
	 */
	sys_sensors_exit(ak_cam);

	/*release notifier*/
	v4l2_async_notifier_unregister(&hw->notifier);

	/*release all inputs nodes*/
	release_inputs_nodes(ak_cam);

	/*release the global*/
	devm_kfree(hw->dev, ak_cam);

	return 0;
}

/*camera host ids*/
static const struct of_device_id ak_of_match[] = {
	{ .compatible = "anyka,ak37d-vi" },
	{ .compatible = "anyka,ak39ev330-vi" },
	{ }
};

MODULE_DEVICE_TABLE(of, ak_of_match);

static struct platform_driver ak_camera_driver = {
	.driver		= {
		.name	= AK_CAM_DRV_NAME,
		.of_match_table = of_match_ptr(ak_of_match),
	},
	.probe		= ak_camera_probe,
	.remove		= ak_camera_remove,
};

module_platform_driver(ak_camera_driver);

MODULE_DESCRIPTION("AK SoC Camera Host driver");
MODULE_AUTHOR("anyka");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.1.3");
MODULE_ALIAS("platform:" AK_CAM_DRV_NAME);
