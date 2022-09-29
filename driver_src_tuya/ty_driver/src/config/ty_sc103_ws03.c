

#include <ty_subg.h>
#include <ty_motor.h>
#include <ty_driver.h>

#include "ty_mstar_infinity6.h"

int g_ty_dmodule[] = {
	TY_SUBG_MODULE,
};

int g_ty_dmodule_len = sizeof(g_ty_dmodule)/sizeof(int);

struct ty_subg_dev g_subg_init = {
	.used_pins[0] = {
		.gpio 		= PAD_SPI0_CZ,
		.pin_name 	= "SPI0_CSB",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,
	},

	.used_pins[1] = {
		.gpio		= PAD_FUART_CTS,
		.pin_name	= "SPI0_FIFO_CS",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,
	},
	
	.used_pins[2] = {
		.gpio 		= PAD_SPI0_CK,
		.pin_name 	= "SPI0_CLK",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,		
	},

	.used_pins[3] = {
		.gpio		= PAD_SPI0_DI,
		.pin_name	= "SPI0_IO",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,		
	},

	.used_pins[4] = {
		.gpio		= PAD_SAR_GPIO2,
		.pin_name	= "GPIO2",
		.direction	= INPUT_PIN,			/*0:output 1:input 2:interrupt*/
	},

	.irq_pins[0] = {
		.gpio 		= PAD_SAR_GPIO3,
		.pin_name 	= "SAR_GPIO3",
	},
	
	.wait_event	= 0,
	.irq[0]		= 0,
};

