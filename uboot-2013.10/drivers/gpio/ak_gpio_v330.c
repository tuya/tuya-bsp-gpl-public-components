/**
 * @file gpio.c
 * @brief gpio function file
 * This file provides gpio APIs: initialization, set gpio, get gpio,
 * gpio interrupt handler.
 * Copyright (C) 2004 Anyka (GuangZhou) Software Technology Co., Ltd.
 * @author tangjianlong
 * @date 2008-01-10
 * @version 1.0
 * @ref anyka technical manual.
 */
 
#include <common.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/arch-ak39ev33x/ak_cpu.h>
#include <asm/arch-ak39ev33x/ak_gpio.h>



//#define INVALID_GPIO                    0xfe

typedef enum {
	 CFG_INVALID = 0,
	 CFG_VALID,
	 CFG_UNDEFINED
 } T_ATTR_CFG_VALID_TYPE ;

typedef enum {
	 CFG_STR_00 = 0,
	 CFG_STR_01,
	 CFG_STR_10,
	 CFG_STR_11
 } T_ATTR_CFG_STR_TYPE ;


typedef struct
{
	 unsigned char gpio_index;

	 // pupd_en
	 unsigned char gpio_pupd_en_bitstart;
	 unsigned char gpio_pupd_en_bitmask;
	 T_ATTR_CFG_VALID_TYPE gpio_pupd_en_sel_valid;
	 unsigned long gpio_pupd_en_reg;
	 unsigned char gpio_pupd_en_enable_set;
}
T_GPIO_ATTR_CFG_FUNC_PULL;


typedef struct
{
    unsigned char gpio_index;

	/* gpio share pin as gpio */ 
    unsigned char gpio_sharepin_sel_bitstart;
    unsigned char gpio_sharepin_sel_bitmask;
    T_ATTR_CFG_VALID_TYPE gpio_sharepin_sel_valid;
    unsigned long gpio_sharepin_sel_reg; 
	unsigned char gpio_sharepin_value;
}
T_GPIO_SHAREPIN_CFG_AS_GPIO;



/*
* ak3780d include 123 gpio(GPIO[108:0] + GPI[13:0])
* GPI[13:0] Mapping to GPIO[109:122]
* GPIO_NORMAL_NUMBER is include input and output funciton
*/
#define GPIO_NORMAL_NUMBER                 109


static unsigned long gpio_pin_dir_reg[] = {GPIO_DIR_REG1, GPIO_DIR_REG2, GPIO_DIR_REG3};
static unsigned long gpio_pin_in_reg[]  = {GPIO_IN_REG1, GPIO_IN_REG2, GPIO_IN_REG3};
static unsigned long gpio_pin_out_reg[] = {GPIO_OUT_REG1, GPIO_OUT_REG2, GPIO_OUT_REG3};
static unsigned long gpio_pin_inte_reg[]= {GPIO_INT_EN1, GPIO_INT_EN2, GPIO_INT_EN3};

static volatile unsigned long gpio_pin_inte[4] = {0};
static volatile unsigned long gpio_pin_intp[4] = {0};



T_GPIO_ATTR_CFG_FUNC_PULL ak_gpio_attr[GPIO_NUMBER+1] = {
	{AK_GPIO_0,
			 0, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_1,
			1, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_2,
			2, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_3,
			3, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_4,
			4, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_5,
			5, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_6,
			 4, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0
			},
	{AK_GPIO_7,
			 5, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0
			},
	{AK_GPIO_8,
			 6, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0
			},
	{AK_GPIO_9,
			 7, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0
			},
	{AK_GPIO_10,
			 0, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},
	{AK_GPIO_11,
			 1, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},
	{AK_GPIO_12,
			 2, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},
	{AK_GPIO_13,
			 3, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},
	{AK_GPIO_14,
			 5, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},
	{AK_GPIO_15,
			 6, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},
	{AK_GPIO_16,
			 7, 0x1, CFG_INVALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},
	{AK_GPIO_17,
			 8, 0x1, CFG_INVALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},
	{AK_GPIO_18,
			 9, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},
	{AK_GPIO_19,
			 11, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},
	{AK_GPIO_20,
			 12, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},
	{AK_GPIO_21,
			 13, 0x1, CFG_INVALID, PU_PD_ENABLE_CFG2_REG, 0x0     // ao_pad_ren_cfg
			},
	{AK_GPIO_22,
			 14, 0x1, CFG_INVALID, PU_PD_ENABLE_CFG2_REG, 0x0     // ao_pad_ren_cfg
			},
	{AK_GPIO_23,
			 15, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},
	{AK_GPIO_24,
			 16, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},
	{AK_GPIO_25,
			 0, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_26,
			 1, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_27,
			 6, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_28,
			 7, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_29,
			 2, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_30,
			 3, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG,0x0
			},
	{AK_GPIO_31,
			 4, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_32,
			 5, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_33,
			 6, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG,0x0
			},
	{AK_GPIO_34,
			 7, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG,0x0
			},
	{AK_GPIO_35,
			 8, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG,0x0
			},
	{AK_GPIO_36,
			 9, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_37,
			 10, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG,0x0
			},
	{AK_GPIO_38,
			 11, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG,0x0
			},
	{AK_GPIO_39,
			 12, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG,0x0
			},
	{AK_GPIO_40,
			 13, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG,0x0
			},
	{AK_GPIO_41,
			 14, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_42,
			 15, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_43,
			 16, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_44,
			 17, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_45,
			 18, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_46,
			 19, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0
			},
	{AK_GPIO_47,
			 8, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_48,
			 9, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_49,
			 27, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_50,
			 10, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_51,
			 11, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_52,
			 12, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_53,
			 13, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_54,
			 14, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_55,
			 15, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_56,
			 16, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_57,
			 17, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_58,
			 18, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_59,
			 19, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_60,
			 20, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_61,
			 21, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0
			},
	{AK_GPIO_62,
			 22, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_63,
			 23, 0x1,CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},


	
	{AK_GPIO_64,
			 0, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0 
			},
	{AK_GPIO_65,
			 1, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0
			},
	{AK_GPIO_66,
			 2, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0
			},
	{AK_GPIO_67,
			 3, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0
			},
	{AK_GPIO_68,
			 8, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0
			},
	{AK_GPIO_69,
			 9, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0
			},


	//
	{AK_GPIO_70,
			 2, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},
	{AK_GPIO_71,
			 3, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},
	{AK_GPIO_72,
			 4, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},
	{AK_GPIO_73,
			 5, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},
	{AK_GPIO_74,
			 6, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},
	{AK_GPIO_75,
			 7, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},


	
	{AK_GPIO_76,
			 4, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},
	{AK_GPIO_77,
			 10, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0
			},
	{AK_GPIO_78,
			 17, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0
			},

	
	{AK_GPIO_79,
			 20, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG,0x0
			},


	
	{AK_GPIO_80,
			 24, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_81,
			 25, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_82,
			 26, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
	{AK_GPIO_85,
			 28, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0
			},
};


T_GPIO_SHAREPIN_CFG_AS_GPIO ak_gpio_sharepin_as_gpio[GPIO_NUMBER+1] = {
	{AK_GPIO_0,  
			 0, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_1,  
			 2, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_2,  
			 3, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_3,  
			 4, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_4,  
			 5, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_5,  
			 6, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_6,  
			 7, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,0x1
			},
	{AK_GPIO_7,  
			 9, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,0x1
			},
	{AK_GPIO_8,  
			 11, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,0x1
			},
	{AK_GPIO_9,  
			 13, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,0x1
			},
	{AK_GPIO_10, 
			 0, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,0x0
			},
	{AK_GPIO_11,  
			 2, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,0x0
			},
	{AK_GPIO_12,  
			 4, 0x1, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0
			},
	{AK_GPIO_13,  
			 5, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,0x0
			},
	{AK_GPIO_14,  
			 8, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,0x0
			},
	{AK_GPIO_15,  
			 10, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,0x0
			},
	{AK_GPIO_16,  
			 12, 0x1, CFG_VALID, SHARE_PIN_CFG2_REG,0x0
			},
	{AK_GPIO_17,  
			 13, 0x1, CFG_VALID, SHARE_PIN_CFG2_REG,0x0
			},
	{AK_GPIO_18,  
	         14, 0x1, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0 
			},
	{AK_GPIO_19,  
			 16, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,0x0
			},
	{AK_GPIO_20,  
			 18, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,0x0
			},
	{AK_GPIO_21,  
	         20, 0x1, CFG_VALID, SHARE_PIN_CFG2_REG,  0x0
			},
	{AK_GPIO_22,  
	         21, 0x1, CFG_VALID, SHARE_PIN_CFG2_REG,  0x0
			},
	{AK_GPIO_23,  
	         22, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,  0x0
			},
	{AK_GPIO_24,  
			 24, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,0x0
			},
	{AK_GPIO_25,  
			 0, 0x1, CFG_VALID, SHARE_PIN_CFG3_REG,0x0
			},
	{AK_GPIO_26,  
			 1, 0x1, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_27,  
			 8, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0
			},
	{AK_GPIO_28,  
			 9, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0
			},
	{AK_GPIO_29,  
	         2, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_30,  
	         4, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_31,  
			 6, 0x1, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_32,  
			 7, 0x1, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_33,  
			 8, 0x1, CFG_VALID, SHARE_PIN_CFG3_REG,0x0
			},
	{AK_GPIO_34,  
			 10, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG,0x0
			},
	{AK_GPIO_35,  
			 12, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG,0x0
			},
	{AK_GPIO_36,  
			 12, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_37,  
			 14, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG,0x0
			},
	{AK_GPIO_38,  
			 14, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_39,
			 16, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_40,  
	         18, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG,  0x0
			},
	{AK_GPIO_41,  
	         20, 0x1, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_42,  
	         21, 0x1, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0  
			},
	{AK_GPIO_43,  
	         22, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG,  0x0
			},
	{AK_GPIO_44,  
			 24, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_45,  
			 26, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0  
			},
	{AK_GPIO_46,  
	         26, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0  
			},
	{AK_GPIO_47,  
			 10, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0 
			},
	{AK_GPIO_48,  
			 12, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0 
			},
/*
	//
	{AK_GPIO_49,  
			 4, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x0
			},
*/
	
	{AK_GPIO_50,  
			 13, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0 
			},
	{AK_GPIO_51,  
			 14, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0 
			},
	{AK_GPIO_52,  
	         15, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0
			},
	{AK_GPIO_53,  
			 16, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG,  0x0
			},
	{AK_GPIO_54,  
			 17, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0 
			},
	{AK_GPIO_55,  
	         18, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG,  0x0
			},
	{AK_GPIO_56,  
	         19, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0 
			},
	{AK_GPIO_57,  
			 20, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG,  0x0
			},
	{AK_GPIO_58,  
	         21, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0 
			},

	//0x08000014
	{AK_GPIO_59,  
	         29, 0x1, CFG_VALID, SHARE_PIN_CFG4_REG, 0x0 
			},
	{AK_GPIO_60,  
	         29, 0x1, CFG_VALID, SHARE_PIN_CFG4_REG, 0x0 
			},
	{AK_GPIO_61,  
	         29, 0x1, CFG_VALID, SHARE_PIN_CFG4_REG, 0x0 
			},
	{AK_GPIO_62,  
	         29, 0x1, CFG_VALID, SHARE_PIN_CFG4_REG, 0x0
			},
	{AK_GPIO_63,  
			 29, 0x1, CFG_VALID, SHARE_PIN_CFG4_REG, 0x0 
			},

	
	{AK_GPIO_64,  
			 0, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x1
			},
	{AK_GPIO_65,  
	         2, 0x1, CFG_VALID, SHARE_PIN_CFG1_REG, 0x1 
			},
	{AK_GPIO_66,  
			 3, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x1
			},
	{AK_GPIO_67,  
	         5, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x1 
			},
	{AK_GPIO_68,  
	         15, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,  0x1
			},
	{AK_GPIO_69,  
	         17, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,  0x1
			},

	
	////0x08000014 and 0x08000078
	{AK_GPIO_70,  
			 19, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,  0x1
			},
	{AK_GPIO_71,  
			 21, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,  0x1
			},
	{AK_GPIO_72,  
			 23, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,  0x1
			},
	{AK_GPIO_73,  
			 25, 0x1, CFG_VALID, SHARE_PIN_CFG1_REG,  0x0
			},
	{AK_GPIO_74,  
			 26, 0x1, CFG_VALID, SHARE_PIN_CFG1_REG,  0x0
			},
	{AK_GPIO_75,  
			 27, 0x1, CFG_VALID, SHARE_PIN_CFG1_REG,  0x0
			},
	
	{AK_GPIO_76,  
			 7, 0x1, CFG_VALID, SHARE_PIN_CFG2_REG,  0x0
			},
	{AK_GPIO_77,  
			 15, 0x1, CFG_VALID, SHARE_PIN_CFG2_REG,  0x0
			},
	{AK_GPIO_78,  
			 26, 0x1, CFG_VALID, SHARE_PIN_CFG2_REG,  0x0
			},
	{AK_GPIO_79,  
			 9, 0x1, CFG_VALID, SHARE_PIN_CFG3_REG,  0x0
			},
	{AK_GPIO_80,  
			 22, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG, 0x0  
			},
	{AK_GPIO_81,  
			 23, 0x1, CFG_VALID, SHARE_PIN_CFG0_REG,  0x0
			},


	{AK_GPIO_83,  
			 25, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x3  //GPI
			},
	{AK_GPIO_84,  
			 27, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x3  //GPI
			},
};

void gpio_int_disableall(void);
void gpio_int_restoreall(void);



/*
* @brief: check gpio index whether legitimate or not
* @author cao_donghua
* @date 2019-08-10
* @return int
* @retval 0:sucess, 1:failed
*/
int gpio_assert_legal(unsigned long pin)
{
    if((pin == INVALID_GPIO) || (pin > GPIO_NUMBER)){
        return -1;
    }else{
        return 0;
    }
}

/**
* @brief: Init gpio.
* @author tangjianlong
* @date 2008-01-10
* @return void
* @retval
*/
void gpio_init( void )
{

    /*
    * disable gpio int before enable its interrupt
	*/
    gpio_int_disableall();

}


/**
 * @brief: gpio_request.
 * @author luoyanliang
 * @date 2017-01-09
 * @return void
 * @retval
 */
int gpio_request(unsigned gpio, const char *label)
{
	gpio_init();
	return 0;
}


/**
 * @brief: gpio_free.
 * @author luoyanliang
 * @date 2017-01-09
 * @return void
 * @retval
 */
int gpio_free(unsigned gpio)
{
	return 0;
}



/**
 * @brief: set gpio output level
 * @author: cao_donghua
 * @date 2019-08-10
 * @param: pin: gpio pin ID.
 * @param: level: 0 or 1.
 * @return void
 * @retval
 */
void gpio_set_pin_level(int pin, unsigned char level)
{
    unsigned long index;
	unsigned long residual;
	u32 regval;
    
    if(-1 == gpio_assert_legal(pin)){
    	printf("err, invlaid gpio!\n");
        return;
    }

	if(pin > GPIO_NORMAL_NUMBER){
    	printf("err, invlaid gpio, is gpi, not output!\n");
        return;
    }
	
    index = pin / 32;
    residual = pin % 32;

	/*
	* level mean: 0: low level, 1:high level
	*/
    if(level){ 
		regval = __raw_readl(gpio_pin_out_reg[index]);
		regval |= (1 << residual);
		__raw_writel(regval, gpio_pin_out_reg[index]);
	}else{
        *(volatile unsigned long*)gpio_pin_out_reg[index] &= ~(1 << residual);
		regval = __raw_readl(gpio_pin_out_reg[index]);
		regval &= ~(1 << residual);
		__raw_writel(regval, gpio_pin_out_reg[index]);
    }
}



/**
 * @brief: get gpio input level
 * @author: cao_donghua
 * @param: pin: gpio pin ID.
 * @date 2019-08-10
 * @return unsigned char
 * @retval: pin level; 1: high; 0: low;
 */
unsigned char gpio_get_pin_level(unsigned int pin)
{
    unsigned long index; 
	unsigned long level = 0;
	unsigned long residual;

    if(-1 == gpio_assert_legal(pin)){
    	printf("err, invlaid gpio!\n");
        return 0xff;
    }

	index = pin / 32;
    residual = pin % 32;
    if(__raw_readl(gpio_pin_in_reg[index]) & (1 << residual)){
        level = 1;
    }else{
        level = 0;
    }
	
    return level;
}


/**
 * @brief: set gpio direction
 * @author: cao_donghua
 * @date 2019-08-10
 * @param: pin: gpio pin ID.
 * @param: dir: 0 means input; 1 means output;
 * @return void
 * @retval
 */
void gpio_set_pin_dir( unsigned long pin, unsigned char dir)
{
    unsigned long index;
	unsigned long residual;
//	unsigned long i;
	u32 regval;
    
    if(-1 == gpio_assert_legal(pin)){
		printf("err, invlaid gpio!\n");
        return;
    }

	if(pin > GPIO_NORMAL_NUMBER){
    	printf("err, invlaid gpio, is gpi, not output!\n");
        return;
    }

	index = pin / 32;
    residual = pin % 32;

	/*
	* gpio direction:0:input dir, 1:output dir
	*/
    if(dir == 0x0){
        regval = __raw_readl(gpio_pin_dir_reg[index]);
		regval &= ~(1 << residual);
		__raw_writel(regval, gpio_pin_dir_reg[index]);
    }else{
		regval = __raw_readl(gpio_pin_dir_reg[index]);
        regval |= (1 << residual);
		__raw_writel(regval, gpio_pin_dir_reg[index]);
    }   

}


/**
 * @brief: set gpio direction input.
 * @author cao_donghua
 * @date 2019-08-10
 * @return void
 * @retval
 */
int gpio_direction_input(unsigned int gpio)
{
	gpio_set_pin_dir(gpio, GPIO_DIR_INPUT);	
	return 0;
}


/**
 * @brief: set gpio direction output and output value level.
 * @author cao_donghua
 * @date 2019-08-10
 * @return 0 1
 * @retval
 */
int gpio_direction_output(int gpio, int value)
{
	gpio_set_pin_dir(gpio, GPIO_DIR_OUTPUT);	
	gpio_set_pin_level(gpio, value);
	
	return 0;
}



/**
 * @brief: get gpio input levle value.
 * @author cao_donghua
 * @date 2019-08-10
 * @return 1 0
 * @retval
 */
int gpio_get_value(unsigned int gpio)
{
    unsigned long level = 0;
	
	level = gpio_get_pin_level(gpio);
    return level;
}

/*
* int enable:1:enable, 0:disable
*/
int gpio_set_pull_up_r(const unsigned long pin, int enable)
{
    unsigned long reg_data;
	unsigned long regvalue;
	unsigned char i = 0;

    if(-1 == gpio_assert_legal(pin)){
		printf("err, invlaid gpio!\n");
        return -1;
    }
	
    for(i = 0; i < (GPIO_NUMBER+1); i++){
        if(pin == ak_gpio_attr[i].gpio_index){
			if(ak_gpio_attr[i].gpio_pupd_en_sel_valid == CFG_VALID){
				reg_data = ak_gpio_attr[i].gpio_pupd_en_reg;
				regvalue = __raw_readl(reg_data);
				regvalue &= ~(ak_gpio_attr[i].gpio_pupd_en_bitmask << ak_gpio_attr[i].gpio_pupd_en_bitstart);
				if(enable){
					/* enable gpio pupd enable */
					if(ak_gpio_attr[i].gpio_pupd_en_enable_set){
						regvalue |= (0x1 << ak_gpio_attr[i].gpio_pupd_en_bitstart);
					}else{
						regvalue &= ~(0x1 << ak_gpio_attr[i].gpio_pupd_en_bitstart);
					}
					__raw_writel(regvalue, reg_data);
				}else{
					/* 
					* disable gpio pupd enable 
					* disable pupd enable, then not set pupd select
					* after set disable ,then return
					*/
					if(ak_gpio_attr[i].gpio_pupd_en_enable_set){
						regvalue &= ~(0x1 << ak_gpio_attr[i].gpio_pupd_en_bitstart);
					}else{
						regvalue |= (0x1 << ak_gpio_attr[i].gpio_pupd_en_bitstart);
					}
					__raw_writel(regvalue, reg_data);

					return 0;
				}
			}else{
				printf("cfg pu_enable invalid!\n");
				break;
			}
            return 0;
        }
    }    

    return -1; 
}


/* 1.enable pulldown 0.disable pulldown */
int gpio_set_pull_down_r(const unsigned long pin, int enable)
{
    unsigned long reg_data;
	unsigned long regvalue;
	unsigned char i = 0;

    if(-1 == gpio_assert_legal(pin)){
		printf("err, invlaid gpio!\n");
        return -1;
    }
	
    for(i = 0; i < (GPIO_NUMBER+1); i++){
        if(pin == ak_gpio_attr[i].gpio_index){
			if(ak_gpio_attr[i].gpio_pupd_en_sel_valid == CFG_VALID){
				reg_data = ak_gpio_attr[i].gpio_pupd_en_reg;
				regvalue = __raw_readl(reg_data);
				regvalue &= ~(ak_gpio_attr[i].gpio_pupd_en_bitmask << ak_gpio_attr[i].gpio_pupd_en_bitstart);
				if(enable){
					/* enable gpio pupd enable */
					if(ak_gpio_attr[i].gpio_pupd_en_enable_set){
						regvalue |= (0x1 << ak_gpio_attr[i].gpio_pupd_en_bitstart);
					}else{
						regvalue &= ~(0x1 << ak_gpio_attr[i].gpio_pupd_en_bitstart);
					}
					__raw_writel(regvalue, reg_data);
				}else{
					/* 
					* disable gpio pupd enable 
					* disable pupd enable, then not set pupd select
					* after set disable ,then return
					*/
					if(ak_gpio_attr[i].gpio_pupd_en_enable_set){
						regvalue &= ~(0x1 << ak_gpio_attr[i].gpio_pupd_en_bitstart);
					}else{
						regvalue |= (0x1 << ak_gpio_attr[i].gpio_pupd_en_bitstart);
					}
					__raw_writel(regvalue, reg_data);

					return 0;
				}
			}else{
				printf("cfg pd_enable invalid!\n");
				break;
			}
            return 0;
        }
    }    
    return -1; 
}



/**
 * @brief set gpio share pin as gpio 
 * @author  liao_zhijun
 * @date 2010-07-28
 * @param pin [in]  gpio pin ID
 * @return bool
 * @retval true set successfully
 * @retval false fail to set
 */
int gpio_set_pin_as_gpio (unsigned int pin)
{
    unsigned long i;
	unsigned long reg_data;
    unsigned long regvalue;
    //unsigned long bit = 0;

    //check param
    if(-1 == gpio_assert_legal(pin)){
    	printf("err, invlaid gpio!\n");
        return -1;
    }

	if(pin ==49)
    {
    	printf("err, 49 invlaid gpio!\n");
        return -1;
    }
	
    //loop to find the correct bits to clr in share ping cfg1
    for(i = 0; i < (GPIO_NUMBER+1); i++){
        if(pin == ak_gpio_sharepin_as_gpio[i].gpio_index){
			if(ak_gpio_sharepin_as_gpio[i].gpio_sharepin_sel_valid == CFG_VALID){
				reg_data = ak_gpio_sharepin_as_gpio[i].gpio_sharepin_sel_reg;
				regvalue = __raw_readl(reg_data);
				regvalue &= ~(ak_gpio_sharepin_as_gpio[i].gpio_sharepin_sel_bitmask << ak_gpio_sharepin_as_gpio[i].gpio_sharepin_sel_bitstart);
				regvalue |= (ak_gpio_sharepin_as_gpio[i].gpio_sharepin_value << ak_gpio_sharepin_as_gpio[i].gpio_sharepin_sel_bitstart);
				__raw_writel(regvalue, reg_data);
			}else{
				printf("cfg gpio sharepin as gpio invalid!\n");
				break;
			}
				
            return 0;
        }
    }
    
    return -1;
}


void gpio_int_disableall(void)
{
    REG32(gpio_pin_inte_reg[0]) = 0;
    REG32(gpio_pin_inte_reg[1]) = 0;
    REG32(gpio_pin_inte_reg[2]) = 0;
	REG32(gpio_pin_inte_reg[3]) = 0;
}

void gpio_int_restoreall(void)
{
    REG32(gpio_pin_inte_reg[0]) = gpio_pin_inte[0];
    REG32(gpio_pin_inte_reg[1]) = gpio_pin_inte[1];
    REG32(gpio_pin_inte_reg[2]) = gpio_pin_inte[2];
	REG32(gpio_pin_inte_reg[3]) = gpio_pin_inte[3];
}

