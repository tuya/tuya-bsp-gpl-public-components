/*
* ty_imu.c - Tuya
*
* Copyright (C) 2022 Tuya Technology Corp.
*
*/

#include "ty_imu.h"

extern struct imu_dev sh3001_dev;
extern struct imu_dev qmi8658_dev;
extern struct imu_dev bmi088_dev;

struct ty_imu_dev g_imu_devs[] = {
	{.imu_dev = &sh3001_dev},
	{.imu_dev = &qmi8658_dev},
	{.imu_dev = &bmi088_dev},
	{.imu_dev = NULL},
};

/* Get imu device table */
struct ty_imu_dev *get_dev_tbs(void)
{
	return g_imu_devs;
}
