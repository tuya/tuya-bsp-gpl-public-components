/*
* ty_driver.c - Tuya
*
* Copyright (C) 2020 Tuya Technology Corp.
*
* Author: dyh
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <ty_driver.h>

extern int g_ty_dmodule[];
extern int g_ty_dmodule_len;

static int __init ty_driver_init(void)
{
	WORD	i 	= 0;
	BYTE	ret = 0;
	
	MS_INFO("driver built in: %s %s\n",__TIME__,__DATE__);
	
	if (g_ty_dmodule_len > TY_MODULE_NR) {
		MS_ERROR("config error.need less than total drivers\n");
		goto failed;
	}
	for (i = 0; i < g_ty_dmodule_len; i++) {
		switch(g_ty_dmodule[i]) {
			case TY_MOTOR_MODULE:
				ret = ty_motor_init();
				if (ret)
					MS_ERROR("module %d failed.\n", g_ty_dmodule[i]);
				break;
			case TY_SUBG_MODULE:
				ret = ty_subg_init();
				if (ret)
					MS_ERROR("module %d failed.\n", g_ty_dmodule[i]);				
				break;
			case TY_COLORBAND_MODULE:
				ret = ty_colorband_init();
				if (ret)
					MS_ERROR("module %d failed.\n", g_ty_dmodule[i]);				
				break;
			case TY_HDMI_MODULE:
				ret = ty_hdmi_init();
				if (ret)
					MS_ERROR("module %d failed.\n", g_ty_dmodule[i]);
				break;
			case TY_IMU_MODULE:
				ret = ty_imu_init();
				if (ret)
					MS_ERROR("module %d failed.\n", g_ty_dmodule[i]);
				break;
			default:
				
				break;
		}
	}
	
	goto end;
	
failed:
	return -1;
end:
    return ret;
}

static void __exit ty_driver_exit(void)
{
	WORD	i = 0;

	if (g_ty_dmodule_len > TY_MODULE_NR) {
		MS_ERROR("config error.need less than total drivers\n");
		goto failed;
	}
	
	for (i = 0; i < g_ty_dmodule_len; i++) {
		switch(g_ty_dmodule[i]) {
			case TY_MOTOR_MODULE:
				ty_motor_exit();
				break;
			case TY_SUBG_MODULE:
				ty_subg_exit();
				break;
			case TY_COLORBAND_MODULE:
				ty_colorband_exit();			
				break;	
			case TY_HDMI_MODULE:
				ty_hdmi_exit();
				break;
			case TY_IMU_MODULE:
				ty_imu_exit();
				break;
			default:
				
				break;
		}		
	}

failed:
    return;
}


module_init(ty_driver_init);
module_exit(ty_driver_exit);

MODULE_AUTHOR("TUYA");
MODULE_DESCRIPTION("TUYA Driver");
MODULE_LICENSE("GPL");



