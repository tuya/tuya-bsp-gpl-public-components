#include <ty_subg.h>
#include <ty_motor.h>
#include <ty_driver.h>

int g_ty_dmodule[] = {
	TY_MOTOR_MODULE,
};

int g_ty_dmodule_len = sizeof(g_ty_dmodule)/sizeof(int);

struct ty_motor_dev g_motor_init = {
	.used_pins[0] = {
		.gpio 		= 15,
		.pin_name 	= "MOTO_IN1",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,
	},

	.used_pins[1] = {
		.gpio		= 20,
		.pin_name	= "MOTO_IN2",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	
	.used_pins[2] = {
		.gpio 		= 23,
		.pin_name 	= "MOTO_IN3",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.used_pins[3] = {
		.gpio		= 24,
		.pin_name	= "MOTO_IN4",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	.ctrl_pin	  = {
		.gpio		= 42,
		.pin_name	= "MOTO_H_SEL",
		.direction	= OUTPUT_PIN,
		.init_val	= LOW_LEVEL,		
	},
	.ctrl_pin2	  = {
		.gpio		= 19,
		.pin_name	= "MOTO_V_SEL",
		.direction	= OUTPUT_PIN,
		.init_val	= LOW_LEVEL,		
	},

	.pan_step_max	= 10000,//5540,
	.tilt_step_max	= 2000,
	.pan_speed		= 5,
	.tilt_speed 	= 4,
	.base_period	= 1000,		//平台时间基数是4us

	.motor_select		= 1,
	.pan_dir		= 0,
	.tilt_dir		= 1,
		
	.chip			= CHIP_DARLINGTON_MUXALL,
	.chip_pins_num	= MOTOR_DARLINGTON_MUX_NR,

};
