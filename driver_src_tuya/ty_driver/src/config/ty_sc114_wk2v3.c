

#include <ty_subg.h>
#include <ty_motor.h>
#include <ty_driver.h>

#include "ty_mstar_infinity6.h"

int g_ty_dmodule[] = {
	TY_MOTOR_MODULE,
};

int g_ty_dmodule_len = sizeof(g_ty_dmodule)/sizeof(int);

struct ty_motor_dev g_motor_init = {
	.used_pins[0] = {
		.gpio 		= PAD_SPI0_CZ,
		.pin_name 	= "MOTO_SPI_CS",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,
	},

	.used_pins[1] = {
		.gpio		= PAD_SPI0_CK,
		.pin_name	= "MOTO_SPI_CLK",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,		
	},
	
	.used_pins[2] = {
		.gpio 		= PAD_SPI0_DI,
		.pin_name 	= "MOTO_SPI_MOSI",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.used_pins[3] = {
		.gpio		= PAD_SPI0_DO,
		.pin_name	= "MOTO_SPI_MISO",
		.direction	= INPUT_PIN,
	},
	
	.used_pins[4] = {
		.gpio 		= PAD_FUART_CTS,
		.pin_name 	= "MOTO_VD_FZ",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.used_pins[5] = {
		.gpio		= PAD_PWM0,
		.pin_name	= "MOTOR_RST",
		.direction	= OUTPUT_PIN,			/*1:input 0:output*/
		.init_val 	= LOW_LEVEL,
	},

	
	.pan_step_max 	= 5370,
	.tilt_step_max	= 950,
	.pan_speed		= 5,
	.tilt_speed		= 4,
	.base_period	= 1000,

	.motor_select	= 0,
	.pan_dir		= 0,
	.tilt_dir		= 1,

	.chip			= CHIP_MS41929,
	.chip_pins_num	= MOTOR_MS41929_NR,
};

