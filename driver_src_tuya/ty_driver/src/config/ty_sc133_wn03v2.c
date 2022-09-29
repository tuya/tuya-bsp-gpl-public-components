

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
		.pin_name 	= "MOTOR_IN1",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,
	},

	.used_pins[1] = {
		.gpio		= PAD_SPI0_CK,
		.pin_name	= "MOTOR_IN2",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	
	.used_pins[2] = {
		.gpio 		= PAD_SPI0_DI,
		.pin_name 	= "MOTOR_IN3",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.used_pins[3] = {
		.gpio		= PAD_SPI0_DO,
		.pin_name	= "MOTOR_IN4",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	
	.used_pins[4] = {
		.gpio 		= PAD_FUART_RX,
		.pin_name 	= "MOTOR_IN5",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.used_pins[5] = {
		.gpio		= PAD_FUART_TX,
		.pin_name	= "MOTOR_IN6",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,
	},

	.used_pins[6] = {
		.gpio		= PAD_SAR_GPIO3,
		.pin_name	= "MOTOR_IN7",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	
	.used_pins[7] = {
		.gpio 		= PAD_PM_IRIN,
		.pin_name 	= "MOTOR_IN8",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.pan_step_max 	= 10000,
	.tilt_step_max	= 2000,
	.pan_speed		= 5,
	.tilt_speed		= 4,
	.base_period	= 1000,
	
	.chip			= CHIP_DARLINGTON,
	.chip_pins_num	= MOTOR_DARLINGTON_NR,
};


