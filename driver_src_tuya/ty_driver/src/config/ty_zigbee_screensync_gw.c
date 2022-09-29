#include <ty_subg.h>
#include <ty_motor.h>
#include <ty_color_band.h>
#include <ty_driver.h>

int g_ty_dmodule[] = {
	TY_COLORBAND_MODULE,
	TY_HDMI_MODULE,
};

int g_ty_dmodule_len = sizeof(g_ty_dmodule)/sizeof(int);

struct ty_colorband_dev g_colorband_init = {
	.tx_nbits	= 0x02,			/*默认是1线。0x01=1线，0x02=2线，0x04=4线*/
	.pins_num	= 1,
};

