#include <ty_subg.h>
#include <ty_motor.h>
#include <ty_driver.h>

int g_ty_dmodule[] = {
	TY_MOTOR_MODULE,
};

int g_ty_dmodule_len = sizeof(g_ty_dmodule)/sizeof(int);

struct ty_motor_dev g_motor_init = {
	.used_pins[0] = {
		.gpio 		= 40,
		.pin_name 	= "MT_A1_IN",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,
	},

	.used_pins[1] = {
		.gpio		= 46,
		.pin_name	= "MT_A2_IN",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	
	.used_pins[2] = {
		.gpio 		= 45,
		.pin_name 	= "MT_B1_IN",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.used_pins[3] = {
		.gpio		= 39,
		.pin_name	= "MT_B2_IN",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	
	.used_pins[4] = {
		.gpio 		= 47,
		.pin_name 	= "MT_C1_IN",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.used_pins[5] = {
		.gpio		= 41,
		.pin_name	= "MT_C2_IN",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,
	},

	.used_pins[6] = {
		.gpio		= 42,
		.pin_name	= "MT_D1_IN",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	
	.used_pins[7] = {
		.gpio 		= 43,
		.pin_name 	= "MT_D2_IN",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.pan_step_max 	= 10000,
	.tilt_step_max	= 2000,
	.pan_speed		= 5,
	.tilt_speed		= 4,
	.base_period	= 1000,
	
	.motor_select	= 0,
	.pan_dir		= 0,
	.tilt_dir		= 0,
	
	.chip			= CHIP_DARLINGTON,
	.chip_pins_num	= MOTOR_DARLINGTON_NR,
};


