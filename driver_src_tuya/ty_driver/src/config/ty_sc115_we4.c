

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
		.gpio 		= PAD_PM_LED0,
		.pin_name 	= "TPM0596_CLR",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,
	},

	.used_pins[1] = {
		.gpio		= PAD_FUART_CTS,
		.pin_name	= "TPM0596_RCK",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,		
	},
	
	.used_pins[2] = {
		.gpio 		= PAD_SPI0_CZ,
		.pin_name 	= "TPM0596_EN",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},

	.used_pins[3] = {
		.gpio		= PAD_SPI0_CK,
		.pin_name	= "TPM0596_SRCK",
		.direction	= OUTPUT_PIN,
		.init_val 	= LOW_LEVEL,		
	},
	
	.used_pins[4] = {
		.gpio 		= PAD_SPI0_DI,
		.pin_name 	= "TPM0596_DIN",
		.direction	= OUTPUT_PIN,
		.init_val 	= HIGH_LEVEL,		
	},

	.used_pins[5] = {
		.gpio		= PAD_SPI0_DO,
		.pin_name	= "TPM0596_DOUT",
		.direction	= INPUT_PIN,			/*1:input 0:output*/
	},

	.opto_en		= 1,
	.infi_rot_en	= 1,	
	.opto_pins[0] = {
		.gpio		= PAD_SAR_GPIO2,
		.pin_name	= "H_RIGHT",
		.direction	= INPUT_PIN,			/*1:input 0:output*/
	},
	
	.opto_pins[1] = {
		.gpio		= PAD_PM_IRIN,
		.pin_name	= "H_LEFT",
		.direction	= INPUT_PIN,			/*1:input 0:output*/
	},		

	.pan_roll_step	= 6080,
	.pan_step_max 	= 6350,//5540,
	.tilt_step_max	= 1360,
	.pan_speed		= 5,
	.tilt_speed		= 4,
	.base_period	= 1000,

	.motor_select	= 1,
	.pan_dir		= 0,
	.tilt_dir		= 1,

	.chip			= CHIP_TPM0596,
	.chip_pins_num	= MOTOR_TPM0596_NR,
};

