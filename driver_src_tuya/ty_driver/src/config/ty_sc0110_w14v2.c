#include <ty_subg.h>
#include <ty_motor.h>
#include <ty_driver.h>

int g_ty_dmodule[] = {
	TY_MOTOR_MODULE,
};

int g_ty_dmodule_len = sizeof(g_ty_dmodule)/sizeof(int);

struct ty_motor_dev g_motor_init = {
	.used_pins[0] = {
		.gpio 		= 2,
		.pin_name 	= "MOTO_IN0",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,
	},

	.used_pins[1] = {
		.gpio		= 1,
		.pin_name	= "MOTO_IN1",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,		
	},
	
	.used_pins[2] = {
		.gpio 		= 0,
		.pin_name 	= "MOTO_IN2",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.used_pins[3] = {
		.gpio		= 3,
		.pin_name	= "MOTO_IN3",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	
	.ctrl_pin     = {
		.gpio 		= 30,
		.pin_name 	= "MOTO_SEL",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.pan_step_max 	= 6350,//5540,
	.tilt_step_max	= 1360,
	.pan_speed		= 1,
	.tilt_speed		= 1,
	.base_period	= 250,		//平台时间基数是4us

	.motor_select	= 0,
	.pan_dir		= 0,
	.tilt_dir		= 1,

	.chip			= CHIP_DARLINGTON_MUX,
	.chip_pins_num	= MOTOR_DARLINGTON_MUX_NR,
};

