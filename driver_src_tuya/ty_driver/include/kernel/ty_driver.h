#ifndef __TY_DRIVER_H__
#define __TY_DRIVER_H__

#include <ty_typedefs.h>

typedef enum{
	TY_MOTOR_MODULE,
	TY_SUBG_MODULE,
	TY_COLORBAND_MODULE,
	TY_HDMI_MODULE,
	TY_IMU_MODULE,
	TY_MODULE_NR,
}dy_driver_module_e;

extern int ty_motor_init(void) __attribute__((weak));
extern void ty_motor_exit(void) __attribute__((weak));

extern int ty_subg_init(void) __attribute__((weak));
extern void ty_subg_exit(void) __attribute__((weak));

extern int ty_colorband_init(void) __attribute__((weak));
extern void ty_colorband_exit(void) __attribute__((weak));

extern int ty_hdmi_init(void) __attribute__((weak));
extern void ty_hdmi_exit(void) __attribute__((weak));

extern int ty_imu_init(void) __attribute__((weak));
extern void ty_imu_exit(void) __attribute__((weak));

int ty_motor_init(void)
{
	MS_DEBUG("ty_motor_init null\n");
	return 0;
}
void ty_motor_exit(void)	{return;}

int ty_subg_init(void)
{
	MS_DEBUG("ty_subg_init null\n");
	return 0;
}
void ty_subg_exit(void)	{return;}

int ty_colorband_init(void)
{
	MS_DEBUG("ty_colorband_init null\n");
	return 0;
}
void ty_colorband_exit(void)	{return;}

int ty_hdmi_init(void)
{
        MS_DEBUG("ty_hdmi_init null\n");
        return 0;
}
void ty_hdmi_exit(void)    {return;}

int ty_imu_init(void)
{
        MS_DEBUG("ty_imu_init null\n");
        return 0;
}
void ty_imu_exit(void)    {return;}

#endif


