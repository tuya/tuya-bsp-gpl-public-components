#include <ty_subg.h>
#include <ty_motor.h>
#include <ty_driver.h>

int g_ty_dmodule[] = {
	TY_MOTOR_MODULE,
	TY_SUBG_MODULE,
};

int g_ty_dmodule_len = sizeof(g_ty_dmodule)/sizeof(int);

struct ty_motor_dev g_motor_init = {
	.used_pins[0] = {
		.gpio 		= 6,
		.pin_name 	= "TPM0596_CLR",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,
	},

	.used_pins[1] = {
		.gpio		= 67,
		.pin_name	= "TPM0596_RCK",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,		
	},
	
	.used_pins[2] = {
		.gpio 		= 68,
		.pin_name 	= "TPM0596_EN",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.used_pins[3] = {
		.gpio		= 7,
		.pin_name	= "TPM0596_SRCK",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	
	.used_pins[4] = {
		.gpio 		= 66,
		.pin_name 	= "TPM0596_DIN",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,		
	},

	.used_pins[5] = {
		.gpio		= -1,
		.pin_name	= "TPM0596_DOUT",
		.direction	= INPUT_PIN,			/*1:input 0:output*/
	},

	.pan_step_max 	= 6350,//5540,
	.tilt_step_max	= 1360,
	.pan_speed		= 1,
	.tilt_speed		= 1,
	.base_period	= 250,		//平台时间基数是4us

	.motor_select	= 0,
	.pan_dir		= 0,
	.tilt_dir		= 1,

	.chip			= CHIP_TPM0596,
	.chip_pins_num	= MOTOR_TPM0596_NR,
};

struct ty_subg_dev g_subg_init = {
	.used_pins[0] = {
		.gpio 		= 23,
		.pin_name 	= "SPI0_CSB",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,
	},

	.used_pins[1] = {
		.gpio		= 24,
		.pin_name	= "SPI0_FIFO_CS",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,
	},
	
	.used_pins[2] = {
		.gpio 		= 13,
		.pin_name 	= "SPI0_CLK",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,		
	},

	.used_pins[3] = {
		.gpio		= 14,
		.pin_name	= "SPI0_IO",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,		
	},

	.used_pins[4] = {
		.gpio		= 41,
		.pin_name	= "GPIO2",
		.direction	= INPUT_PIN,			/*0:output 1:input 2:interrupt*/
	},

	.irq_pins[0] = {
		.gpio 		= 42,
		.pin_name 	= "SAR_GPIO3",
	},
	
	.wait_event	= 0,
	.irq[0]		= 0,
	.recycle_en	= 1,
};

