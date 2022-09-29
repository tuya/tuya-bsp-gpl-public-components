#ifndef __TY_IMU_H__
#define __TY_IMU_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>



/* ioctl flag */

#define IMUIO			(0x12)

#define IMU_SET_DELAY		_IOW(IMUIO, 0x10, short)
#define IMU_GET_DELAY		_IOR(IMUIO, 0x11, short)
#define IMU_SET_ENABLE		_IOW(IMUIO, 0x20, short)
#define IMU_GET_ENABLE		_IOR(IMUIO, 0x21, short)


struct imu_dev {
	struct i2c_driver *driver;
	struct i2c_client *client;
	int (*match)(struct i2c_client *client);
	int (*probe)(struct i2c_client *client, const struct i2c_device_id *id);
	int (*remove)(struct i2c_client *client);
};

struct ty_imu_dev {
	struct imu_dev *imu_dev;
};

struct ty_imu_dev *get_dev_tbs(void);

#endif
