/*
 * sys_sensor.c
 *
 * for debug sensor
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

#define NO_SENSOR "nosensor"
#define NAME_SIZE 16

struct sys_sensor_info {
	struct list_head list;
	char *name;
	struct sensor_attr *sensor;
	struct attribute_group *sensor_attr_grp;
	int cur_reg;
};

static LIST_HEAD(sys_sensor_list);

static struct sys_sensor_info *sensor_to_inf(struct sensor_attr *sensor)
{
	struct sys_sensor_info *inf;

	list_for_each_entry(inf, &sys_sensor_list, list) {
		if (inf->sensor == sensor) {
			return inf;
		}
	}

	return NULL;
}

static struct sensor_attr *dev_to_sensor(struct device *dev)
{
	struct sys_sensor_info *inf;

	list_for_each_entry(inf, &sys_sensor_list, list) {
		if (inf->sensor->sd->dev == dev) {
			return inf->sensor;
		}
	}

	return NULL;
}

/*
 * resolution_show - show resolutin and offset
 *
 * width
 * height
 * x
 * y
 *
 */
static ssize_t resolution_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sensor_attr *sensor = dev_to_sensor(dev);
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	int width, height, x, y;
	int r;

	r = sensor_cbi->cb->sensor_get_resolution_func(sensor_cbi->arg, &width, &height);
	if (r) {
		/*fail get width & height*/
		r = snprintf(buf, PAGE_SIZE, "%s\n", NO_SENSOR);
		goto end;
	}

	r = sensor_cbi->cb->sensor_get_valid_coordinate_func (sensor_cbi->arg, &x, &y);
	if (r) {
		/*fail get x & y*/
		r = snprintf(buf, PAGE_SIZE, "%s\n", NO_SENSOR);
		goto end;
	}

	r = snprintf(buf, PAGE_SIZE, "width:%d, height:%d, x:%d, y:%d\n",
			width, height, x, y);

end:
	return r;
}

/*
 * sensor_id_show - show sensor id
 *
 * sensor-id
 *
 */
static ssize_t sensor_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sensor_attr *sensor = dev_to_sensor(dev);
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	int id;
	int r;

	id = sensor_cbi->cb->sensor_read_id_func(sensor_cbi->arg);

	r = snprintf(buf, PAGE_SIZE, "sensor-id:0x%x\n", id);

	return r;
}

/*
 * sensor_bus_data_show - show interface and data type
 *
 * interface
 * lanes
 * data
 *
 */
static ssize_t sensor_bus_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sensor_attr *sensor = dev_to_sensor(dev);
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	int r;

	int interface;
	int lanes;
	int bus;
	char str_interface[16] = "\0";
	char str_data[16] = "\0";

	r = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_INTERFACE, &interface);
	if (r) {
		/*fail get interface*/
		r = snprintf(buf, PAGE_SIZE, "%s\n", NO_SENSOR);
		goto end;
	}

	switch (interface) {
		case DVP_INTERFACE:
			strcpy(str_interface, "dvp");
			break;

		case MIPI_INTERFACE:
			r = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
					GET_MIPI_LANES, &lanes);
			if (r) {
				/*fail get lanes*/
				r = snprintf(buf, PAGE_SIZE, "%s\n", NO_SENSOR);
				goto end;
			}
			sprintf(str_interface, "mipi-%dlanes", lanes);
			break;

		default:
			strcpy(str_interface, "do-not-know");
			break;
	}

	bus = sensor_cbi->cb->sensor_get_bus_type_func(sensor_cbi->arg);
	switch (bus) {
		case BUS_TYPE_RAW:
			strcpy(str_data, "raw");
			break;

		case BUS_TYPE_YUV:
			strcpy(str_data, "yuv");
			break;
		default:
			strcpy(str_data, "do-not-know");
			break;
	}

	r = snprintf(buf, PAGE_SIZE, "bus-interface:%s, data-type:%s\n", str_interface, str_data);

end:
	return r;
}

/*
 * reset_pwdn_show - show reset and pwdn gpio informations
 *
 * reset-gpio
 * reset-current-level
 * pwdn-gpio
 * pwdn-current-level
 *
 */
static ssize_t reset_pwdn_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sensor_attr *sensor = dev_to_sensor(dev);
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	int r;

	int reset_gpio = -1, pwdn_gpio = -1;
	int reset_level, pwdn_level;

	r = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_RESET_GPIO, &reset_gpio);

	r = sensor_cbi->cb->sensor_get_parameter_func(sensor_cbi->arg,
			GET_PWDN_GPIO, &reset_gpio);

	if (reset_gpio >= 0 && reset_gpio < 0xffff) {
		reset_level = gpio_get_value(reset_gpio);
		r = snprintf(buf, PAGE_SIZE, "reset-gpio:%d, reset-current-level:%d\n", reset_gpio, reset_level);
	} else {
		r = snprintf(buf, PAGE_SIZE, "reset-gpio:none\n");
	}

	if (pwdn_gpio >= 0 && pwdn_gpio < 0xffff) {
		pwdn_level = gpio_get_value(pwdn_gpio);
		r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "pwdn-gpio:%d, pwdn-current-level:%d\n", pwdn_gpio, pwdn_level);
	} else {
		r = snprintf(buf + strlen(buf), PAGE_SIZE - strlen(buf), "pwdn-gpio:none\n");
	}

	r = strlen(buf);

	return r;
}

static ssize_t reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sensor_attr *sensor = dev_to_sensor(dev);
	struct sys_sensor_info *inf = sensor_to_inf(sensor);
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	int reg = inf->cur_reg;
	int r;

	r = sensor_cbi->cb->sensor_read_reg_func(sensor_cbi->arg, reg);
	r = snprintf(buf, PAGE_SIZE, "0x%04x\n", r);

	return r;
}

static ssize_t reg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sensor_attr *sensor = dev_to_sensor(dev);
	struct sys_sensor_info *inf = sensor_to_inf(sensor);
	struct sensor_cb_info *sensor_cbi = sensor->sensor_cbi;
	char *endp;
	int reg;
	int value;

	reg = simple_strtol(buf, &endp, 0);
	inf->cur_reg = reg;
	pr_debug("%s reg:0x%04x, endp:%s", __func__, reg, endp);
	if ((endp[0] == ',') ||
			(endp[0] == '=')) {
		value = simple_strtol(endp + 1, NULL, 0);
		pr_debug("%s value :0x%04x", __func__, value);
		sensor_cbi->cb->sensor_write_reg_func(sensor_cbi->arg, reg, value);
	}

	return count;
}
/*
 * dev_attr_resolution: resolution and offset
 * dev_attr_senosr_id: sensor id
 * dev_attr_sensor_regs: sensor registers initial
 * dev_attr_sensor_bus_data: sensor output bus type and data type
 * dev_attr_reset_pwdn: reset and pwdn pins gpio number,
 *     definition of those pins level, current output level of them.
 */
static DEVICE_ATTR(resolution, 0644, resolution_show, NULL);
static DEVICE_ATTR(sensor_id, 0644, sensor_id_show, NULL);
static DEVICE_ATTR(sensor_bus_data, 0644, sensor_bus_data_show, NULL);
static DEVICE_ATTR(reset_pwdn, 0644, reset_pwdn_show, NULL);
static DEVICE_ATTR(reg, 0644, reg_show, reg_store);

static struct attribute *sensor_attrs[] = {
	&dev_attr_resolution.attr,
	&dev_attr_sensor_id.attr,
	&dev_attr_sensor_bus_data.attr,
	&dev_attr_reset_pwdn.attr,
	&dev_attr_reg.attr,
	NULL,
};

int sys_sensor_init(struct sensor_attr *sensor)
{
	struct device *dev = sensor->sd->dev;
	struct sys_sensor_info *inf = NULL;
	struct input_video_attr *input_video = sensor_to_input_video(sensor);
	int input_id = input_video->input_id;
	int r;

	pr_debug("%s %d, dev:%p\n",__func__,__LINE__,dev);

	/*alloc memory for inf*/
	inf = vzalloc(sizeof(*inf));
	if (!inf) {
		pr_err("%s vzalloc inf fail\n", __func__);
		goto malloc_fail;
	}

	/*init inf*/
	inf->sensor = sensor;
	inf->name = NULL;
	inf->sensor_attr_grp = NULL;

	/*alloc memory for sensor_attr_grp*/
	inf->sensor_attr_grp = vzalloc(sizeof(*inf->sensor_attr_grp));
	if (!inf->sensor_attr_grp) {
		pr_err("%s vzalloc sensor_attr_grp fail\n", __func__);
		goto malloc_fail;
	}

	/*alloc for name*/
	inf->name = vzalloc(NAME_SIZE);
	if (!inf->name) {
		pr_err("%s vzalloc inf fail\n", __func__);
		goto malloc_fail;
	}

	/*attach id to name*/
	snprintf(inf->name, NAME_SIZE - 1, "sensor%d", input_id);
	inf->sensor_attr_grp->name = inf->name;

	inf->sensor_attr_grp->attrs = sensor_attrs;

	/*create debug interface*/
	r = sysfs_create_group(&dev->kobj, inf->sensor_attr_grp);
	if (r)
		goto fail;

	/*add to global list*/
	list_add_tail(&inf->list, &sys_sensor_list);

	return 0;

malloc_fail:
	if (inf) {
		if (inf->name)
			vfree(inf->name);
		if (inf->sensor_attr_grp)
			vfree(inf->sensor_attr_grp);
		vfree(inf);
	}
	r = -ENOMEM;
fail:
	return r;
}

int sys_sensor_exit(struct sensor_attr *sensor)
{
	struct device *dev = (sensor && sensor->sd) ? sensor->sd->dev:NULL;
	struct sys_sensor_info *inf;

	if (!dev)
		return -EINVAL;

	inf = sensor_to_inf(sensor);
	if (!inf) {
		pr_err("%s can not find sensor:%p to info\n", __func__, sensor);
		return -EINVAL;
	}

	/*delete debug interface*/
	sysfs_remove_group(&dev->kobj, inf->sensor_attr_grp);

	/*delete list*/
	list_del_init(&inf->list);

	/*free memory*/
	vfree(inf->name);
	vfree(inf->sensor_attr_grp);
	vfree(inf);

	return 0;
}
