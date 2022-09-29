#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>

#include "sensor_sys.h"

#define MAX_SENSOR_NUM 2

static struct kobject *sensor_sys_obj = NULL;

/*store sensors ID & interface*/
static sensor_id_cb g_id_cb[MAX_SENSOR_NUM];
static sensor_if_cb g_if_cb[MAX_SENSOR_NUM];

/*
 * sensor_id_read -
 * read sensor ID
 *
 * @kobj:			kernel object
 * @attr:			attr of this node
 * @buf:			the buffer read to
 */
static ssize_t sensor_id_read(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	if (!g_id_cb[0])
		return 0;

	return sprintf(buf, "0x%x\n", g_id_cb[0]());
}

/*
 * sensor_if_read -
 * read sensor interface
 *
 * @kobj:			kernel object
 * @attr:			attr of this node
 * @buf:			the buffer read to
 */
static ssize_t sensor_if_read(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	if (!g_if_cb[0])
		return 0;

	return sprintf(buf, "%s\n", g_if_cb[0]());
}

/*define nodes kobj attribute*/
static struct kobj_attribute sensor_id_obj =__ATTR(sensor_id, S_IRUGO, sensor_id_read, NULL);
static struct kobj_attribute sensor_if_obj =__ATTR(sensor_if, S_IRUGO, sensor_if_read, NULL);

/*nodes attribute*/
static struct attribute *sensor_sys_attrs[] = {
	&sensor_id_obj.attr,
	&sensor_if_obj.attr,
	NULL,
};

/*sys nodes group attribute*/
static struct attribute_group sensor_sys_group = {
	.attrs = sensor_sys_attrs,
};

/*
 * sensor_sys_init -
 * sensor sys init entry
 *
 * @id_cb:			callback of read sensor id
 * @if_cb:			callback of get sensor interface
 */
int sensor_sys_init(sensor_id_cb id_cb, sensor_if_cb if_cb)
{
	int ret = 0;
	printk("%s\n", __func__);

	g_id_cb[0] = id_cb;
	g_if_cb[0] = if_cb;

	sensor_sys_obj = kobject_create_and_add("sensor_sys", NULL);
	if (!sensor_sys_obj)
		goto err_board_obj;

	ret = sysfs_create_group(sensor_sys_obj, &sensor_sys_group);
	if (ret)
		goto err_sysfs_create;

	return 0;

err_sysfs_create:
	//sysfs_remove_group(sensor_sys_obj, &sensor_sys_group);
	kobject_put(sensor_sys_obj);
	printk("\nsysfs_create_group ERROR : %s\n",__func__);
	return 0;
err_board_obj:
	printk("\nobject_create_and_add ERROR : %s\n",__func__);
	return 0;
}

/*
 * sensor_sys_exit -
 * sensor sys exit entry
 */
void sensor_sys_exit(void)
{
	printk("%s\n", __func__);
	sysfs_remove_group(sensor_sys_obj, &sensor_sys_group);
	kobject_put(sensor_sys_obj);
}
