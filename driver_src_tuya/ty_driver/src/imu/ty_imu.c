/*
* ty_imu.c - Tuya
*
* Copyright (C) 2022 Tuya Technology Corp.
*
*/

#include "ty_imu.h"


static int match_ok = 0;
static struct imu_dev *g_imu_dev = NULL;

static int imu_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ty_imu_dev *dev = get_dev_tbs();

	if (match_ok)
		return 0;

	/* find matching drivers */
	while (dev->imu_dev != NULL) {
		match_ok = dev->imu_dev->match(client);
		if (match_ok) {
			g_imu_dev = dev->imu_dev;
			g_imu_dev->client = client;
			break;
		}
		dev++;
	}

	if (match_ok)
		return g_imu_dev->probe(g_imu_dev->client, id);

	return 0;
}

static int imu_remove(struct i2c_client *client)
{
	if (match_ok) {
		g_imu_dev->remove(g_imu_dev->client);
		match_ok = 0;
	}

	return 0;
}

static const struct i2c_device_id imu_id_table[] = {
	{"tuya,imu", 0},
	{ },
};

static const struct of_device_id imu_of_match[] = {
	{ .compatible = "tuya,imu" },
	{ },
};

static struct i2c_driver imu_driver = {
	.driver = {
		.name = "imu",
		.owner = THIS_MODULE,
		.of_match_table = imu_of_match,
	},
	.probe = imu_probe,
	.remove = imu_remove,
	.id_table = imu_id_table,
};

int ty_imu_init(void)
{
	return i2c_add_driver(&imu_driver);	
}

void ty_imu_exit(void)
{
	i2c_del_driver(&imu_driver);
}
