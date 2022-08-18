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
#include <asm/arch-ak37d/ak_cpu.h>
#include <asm/arch-ak37d/ak_gpio.h>



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
    
    // drv
    unsigned char gpio_drv_bitstart;
    unsigned char gpio_drv_bitmask;
    T_ATTR_CFG_VALID_TYPE gpio_drv_sel_valid;
    unsigned long gpio_drv_reg;
	T_ATTR_CFG_STR_TYPE gpio_drv_strenth;
	
    // ie
    unsigned char gpio_ie_bitstart;
    unsigned char gpio_ie_bitmask;
    T_ATTR_CFG_VALID_TYPE gpio_ie_sel_valid;
    unsigned long gpio_ie_reg;
	unsigned char gpio_ie_enable_set;
	
    // pupd_en
    unsigned char gpio_pupd_en_bitstart;
    unsigned char gpio_pupd_en_bitmask;
    T_ATTR_CFG_VALID_TYPE gpio_pupd_en_sel_valid;
    unsigned long gpio_pupd_en_reg;
	unsigned char gpio_pupd_en_enable_set;
	
    // pupd_sel
    unsigned char gpio_pupd_sel_bitstart;
    unsigned char gpio_pupd_sel_bitmask;
    T_ATTR_CFG_VALID_TYPE gpio_pupd_sel_sel_valid;
    unsigned long gpio_pupd_sel_reg; 
	unsigned char gpio_pupd_sel_pu_set;
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


static unsigned long gpio_pin_dir_reg[] = {GPIO_DIR_REG1, GPIO_DIR_REG2, GPIO_DIR_REG3, GPIO_DIR_REG4};
static unsigned long gpio_pin_in_reg[]  = {GPIO_IN_REG1, GPIO_IN_REG2, GPIO_IN_REG3, GPIO_IN_REG4};
static unsigned long gpio_pin_out_reg[] = {GPIO_OUT_REG1, GPIO_OUT_REG2, GPIO_OUT_REG3, GPIO_OUT_REG4};
static unsigned long gpio_pin_inte_reg[]= {GPIO_INT_EN1, GPIO_INT_EN2, GPIO_INT_EN3, GPIO_INT_EN4};

static volatile unsigned long gpio_pin_inte[4] = {0};
static volatile unsigned long gpio_pin_intp[4] = {0};


T_GPIO_ATTR_CFG_FUNC_PULL ak_gpio_attr[GPIO_NUMBER+1] = {
	{AK_GPIO_0,  
			 8, 0x3, CFG_VALID, PAD_DRV_CFG3_REG,CFG_STR_00,
			 20, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1, 
			 20, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 20, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_1,  
			 0, 0x3, CFG_VALID, PAD_DRV_CFG0_REG,CFG_STR_00,  
			 0, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 0, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 0, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_2,  
			 2, 0x3, CFG_VALID, PAD_DRV_CFG0_REG, CFG_STR_00, 
			 1, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 1, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 1, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_3,  
			 4, 0x3, CFG_INVALID, PAD_DRV_CFG0_REG,CFG_STR_00,
			 2, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1, 
			 2, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 2, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_4,  
			 6, 0x3, CFG_INVALID, PAD_DRV_CFG0_REG, CFG_STR_00,
			 3, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 3, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 3, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_5,  
			 8, 0x3, CFG_INVALID, PAD_DRV_CFG0_REG,CFG_STR_00,
			 4, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1, 
			 4, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 4, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_6,  
			 10, 0x3, CFG_VALID, PAD_DRV_CFG0_REG,CFG_STR_00,  
	     	 5, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 5, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 5, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_7,  
			 12, 0x3, CFG_VALID, PAD_DRV_CFG0_REG,CFG_STR_00,  
	     	 6, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 6, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 6, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_8,  
			 14, 0x3, CFG_VALID, PAD_DRV_CFG0_REG, CFG_STR_00,
	         7, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 7, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 7, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_9,  
			 16, 0x3, CFG_VALID, PAD_DRV_CFG0_REG, CFG_STR_00, 
	     	 8, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 8, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 8, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_10, 
			 18, 0x3, CFG_VALID, PAD_DRV_CFG0_REG, CFG_STR_00,
			 9, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 9, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 9, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_11,  
			 20, 0x3, CFG_VALID, PAD_DRV_CFG0_REG, CFG_STR_00, 
	     	 10, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 10, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 10, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_12,  
			 28, 0x3, CFG_INVALID, PAD_DRV_CFG3_REG, CFG_STR_00,
			 30, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 30, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 30, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_13,  
			 22, 0x3, CFG_VALID, PAD_DRV_CFG0_REG, CFG_STR_00,
	     	 11, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 11, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 11, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_14,  
			 24, 0x3, CFG_VALID, PAD_DRV_CFG0_REG, CFG_STR_00, 
	     	 12, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 12, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 12, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_15,  
			 26, 0x3, CFG_VALID, PAD_DRV_CFG0_REG,CFG_STR_00,  
			 13, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 13, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 13, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_16,  
			 0, 0x3, CFG_INVALID, 0, CFG_STR_00,   // invalid drv
			 0, 0x0, CFG_INVALID, 0, 0,	// invalid ie
			 0, 0x1, CFG_INVALID, 0, 0,		//  ao_pad_ren_cfg???
			 0, 0x1, CFG_INVALID, 0, 0     // invalid sel pupd
			},
	{AK_GPIO_17,  
			 0, 0x3, CFG_INVALID, 0,CFG_STR_00,  		// invalid drv
			 0, 0x0, CFG_INVALID, 0, 0,			// invalid ie
			 1, 0x1, CFG_INVALID, 0, 0,
			 0, 0x1, CFG_INVALID, 0, 0				// invalid sel pupd
			},
	{AK_GPIO_18,  
	         6, 0x3, CFG_VALID, PAD_DRV_CFG3_REG, CFG_STR_00, 
			 19, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 19, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0,
			 19, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_19,  
			 28, 0x3, CFG_VALID, PAD_DRV_CFG0_REG, CFG_STR_00, 
			 14, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 14, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 14, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_20,  
	         30, 0x3, CFG_VALID, PAD_DRV_CFG0_REG, CFG_STR_00, 
			 15, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 15, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 15, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_21,  
	         0, 0x3, CFG_INVALID, 0, CFG_STR_00,    // invalid drv
			 0, 0x0, CFG_INVALID, 0, 0,			// invalid ie
			 2, 0x1, CFG_INVALID, 0, 0,     // ao_pad_ren_cfg
			 0, 0x1, CFG_INVALID, 0, 0				// invalid sel pupd
			},
	{AK_GPIO_22,  
			 0, 0x3, CFG_INVALID, 0, CFG_STR_00,    // invalid drv
			 0, 0x0, CFG_INVALID, 0, 0,			// invalid ie
			 3, 0x1, CFG_INVALID, 0, 0,     // ao_pad_ren_cfg
			 0, 0x1, CFG_INVALID, 0, 0				// invalid sel pupd
			},
	{AK_GPIO_23,  
	         0, 0x3, CFG_INVALID, PAD_DRV_CFG1_REG,  CFG_STR_00,
			 16, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 16, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 16, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_24,  
			 2, 0x3, CFG_VALID, PAD_DRV_CFG1_REG,  CFG_STR_00,
			 17, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 17, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 17, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_25,  
	         6, 0x3, CFG_VALID, PAD_DRV_CFG1_REG,  CFG_STR_00,
			 19, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 19, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 19, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_26,  
	         8, 0x3, CFG_VALID, PAD_DRV_CFG1_REG,  CFG_STR_00,
			 20, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 20, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0,
			 20, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG, 0x0
			},
	{AK_GPIO_27,  
			 10, 0x3, CFG_VALID, PAD_DRV_CFG1_REG,  CFG_STR_00,
			 21, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 21, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0,
			 21, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG, 0x0
			},
	{AK_GPIO_28,  
			 12, 0x3, CFG_VALID, PAD_DRV_CFG1_REG,  CFG_STR_00,
			 22, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 22, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0,
			 22, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG, 0x0
			},
	{AK_GPIO_29,  
	         10, 0x3, CFG_VALID, PAD_DRV_CFG3_REG,  CFG_STR_00,
			 21, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 21, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 21, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_30,  
	         12, 0x3, CFG_VALID, PAD_DRV_CFG3_REG,  CFG_STR_00,
			 22, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 22, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0,
			 22, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_31,  
	         14, 0x3, CFG_INVALID, PAD_DRV_CFG1_REG, CFG_STR_00, 
			 23, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 23, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x1, 
			 23, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG, 0x0
			},
	{AK_GPIO_32,  
	         16, 0x3, CFG_INVALID, PAD_DRV_CFG1_REG,  CFG_STR_00,
			 24, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 24, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x1,
			 24, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG, 0x0
			},
	{AK_GPIO_33,  
	         18, 0x3, CFG_VALID, PAD_DRV_CFG1_REG,CFG_STR_00,  
			 25, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 25, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 25, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_34,  
	         20, 0x3, CFG_VALID, PAD_DRV_CFG1_REG, CFG_STR_00, 
			 26, 0x1, CFG_VALID, PAD_IE_CFG0_REG,0x1,
			 26, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 26, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_35,  
			 22, 0x3, CFG_VALID, PAD_DRV_CFG1_REG,  CFG_STR_00,
			 27, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 27, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 27, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_36,  
			 24, 0x3, CFG_VALID, PAD_DRV_CFG1_REG, CFG_STR_00, 
			 28, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 28, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG, 0x0,
			 28, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG, 0x0
			},
	{AK_GPIO_37,  
			 26, 0x3, CFG_VALID, PAD_DRV_CFG1_REG, CFG_STR_00, 
			 29, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 29, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 29, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_38,  
			 28, 0x3, CFG_VALID, PAD_DRV_CFG1_REG,  CFG_STR_00,
			 30, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 30, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 30, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_39,  
			 30, 0x3, CFG_VALID, PAD_DRV_CFG1_REG, CFG_STR_00, 
			 31, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 31, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 31, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG,0x0
			},
	{AK_GPIO_40,  
	         0, 0x3, CFG_VALID, PAD_DRV_CFG2_REG,  CFG_STR_00,
			 0, 0x1, CFG_VALID, PAD_IE_CFG1_REG,0x1,
			 0, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0,
			 0, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG,0x0
			},
	{AK_GPIO_41,  
	         2, 0x3, CFG_VALID, PAD_DRV_CFG2_REG, CFG_STR_00, 
			 1, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 1, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 1, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_42,  
	         4, 0x3, CFG_VALID, PAD_DRV_CFG2_REG, CFG_STR_00,  
			 2, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 2, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 2, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_43,  
	         6, 0x3, CFG_INVALID, PAD_DRV_CFG2_REG,  CFG_STR_00,
			 3, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 3, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0, 
			 3, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_44,  
			 8, 0x3, CFG_INVALID, PAD_DRV_CFG2_REG, CFG_STR_00, 
			 4, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 4, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0, 
			 4, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_45,  
			 10, 0x3, CFG_INVALID, PAD_DRV_CFG2_REG, CFG_STR_00,  
			 5, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 5, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 5, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_46,  
	         12, 0x3, CFG_INVALID, PAD_DRV_CFG2_REG, CFG_STR_00,  
			 6, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 6, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 6, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_47,  
			 14, 0x3, CFG_INVALID, PAD_DRV_CFG2_REG, CFG_STR_00, 
			 7, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 7, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 7, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_48,  
			 16, 0x3, CFG_INVALID, PAD_DRV_CFG2_REG, CFG_STR_00, 
			 18, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 18, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0,
			 18, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_49,  
			 4, 0x3, CFG_VALID, PAD_DRV_CFG1_REG, CFG_STR_01, 
			 18, 0x1, CFG_VALID, PAD_IE_CFG0_REG, 0x1,
			 18, 0x1, CFG_VALID, PU_PD_ENABLE_CFG0_REG,0x0,
			 18, 0x1, CFG_VALID, PU_PD_SEL_CFG0_REG, 0x0
			},
	{AK_GPIO_50,  
			 18, 0x3, CFG_VALID, PAD_DRV_CFG2_REG, CFG_STR_00, 
			 9, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 9, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0,
			 9, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_51,  
			 20, 0x3, CFG_VALID, PAD_DRV_CFG2_REG, CFG_STR_00, 
			 10, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 10, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 10, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_52,  
	         22, 0x3, CFG_VALID, PAD_DRV_CFG2_REG,  CFG_STR_00,
			 11, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 11, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 11, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_53,  
			 24, 0x3, CFG_VALID, PAD_DRV_CFG2_REG,  CFG_STR_00,
			 12, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 12, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 12, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_54,  
			 26, 0x3, CFG_VALID, PAD_DRV_CFG2_REG, CFG_STR_00, 
			 13, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 13, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0,
			 13, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_55,  
	         28, 0x3, CFG_VALID, PAD_DRV_CFG2_REG,  CFG_STR_00,
			 14, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 14, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 14, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_56,  
	         30, 0x3, CFG_VALID, PAD_DRV_CFG2_REG, CFG_STR_00, 
			 15, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 15, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0,
			 15, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_57,  
			 0, 0x3, CFG_VALID, PAD_DRV_CFG3_REG,  CFG_STR_00,
			 16, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 16, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 16, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_58,  
	         2, 0x3, CFG_VALID, PAD_DRV_CFG3_REG, CFG_STR_00, 
			 17, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 17, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0, 
			 17, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_59,  
	         4, 0x3, CFG_VALID, PAD_DRV_CFG3_REG, CFG_STR_00, 
			 18, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 18, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 18, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_60,  
	         14, 0x3, CFG_INVALID, PAD_DRV_CFG3_REG, CFG_STR_00, 
			 23, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 23, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0,
			 23, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_61,  
	         16, 0x3, CFG_INVALID, PAD_DRV_CFG3_REG, CFG_STR_00, 
			 24, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 24, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0,
			 24, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_62,  
	         18, 0x3, CFG_VALID, PAD_DRV_CFG3_REG,  CFG_STR_00,
			 25, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 25, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 25, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_63,  
			 20, 0x3, CFG_VALID, PAD_DRV_CFG3_REG, CFG_STR_00, 
			 26, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 26, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 26, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_64,  
			 22, 0x3, CFG_VALID, PAD_DRV_CFG3_REG,  CFG_STR_00,
			 27, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1, 
			 27, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0, 
			 27, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_65,  
	         24, 0x3, CFG_VALID, PAD_DRV_CFG3_REG, CFG_STR_00, 
			 28, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 28, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 28, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_66,  
			 26, 0x3, CFG_VALID, PAD_DRV_CFG3_REG,  CFG_STR_00,
			 29, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 29, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG,0x0,
			 29, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_67,  
	         30, 0x3, CFG_INVALID, PAD_DRV_CFG3_REG, CFG_STR_00, 
			 31, 0x1, CFG_VALID, PAD_IE_CFG1_REG, 0x1,
			 31, 0x1, CFG_VALID, PU_PD_ENABLE_CFG1_REG, 0x0,
			 31, 0x1, CFG_VALID, PU_PD_SEL_CFG1_REG, 0x0
			},
	{AK_GPIO_68,  
	         0, 0x3, CFG_INVALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 0, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 0, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 0, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_69,  
	         2, 0x3, CFG_INVALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 1, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 1, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 1, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_70,  
			 4, 0x3, CFG_INVALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 2, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 2, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 2, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_71,  
			 6, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 3, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 3, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 3, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_72,  
			 8, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 4, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 4, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 4, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_73,  
			 10, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 5, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 5, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 5, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_74,  
			 12, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 6, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 6, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 6, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_75,  
			 14, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 7, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 7, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 7, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_76,  
			 /* drv str config bit[17:18] = 00 for B version chip */
			 17, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 8, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 8, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 8, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_77,  
			 18, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 9, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 9, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 9, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_78,  
			 20, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 10, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 10, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0,
			 10, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_79,  
			 22, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 11, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 11, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0,
			 11, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG,0x0
			},
	{AK_GPIO_80,  
			 24, 0x3, CFG_VALID, PAD_DRV_CFG4_REG, CFG_STR_00,  
			 12, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 12, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 12, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_81,  
			 26, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 13, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 13, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 13, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_82,  
			 28, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 14, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 14, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 14, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_83,  
			 30, 0x3, CFG_VALID, PAD_DRV_CFG4_REG,  CFG_STR_00,
			 15, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 15, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 15, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_84,  
			 0, 0x3, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 16, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 16, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 16, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_85,  
			 2, 0x3, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 17, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 17, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 17, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_86,  
			 4, 0x3, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 18, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 18, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 18, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_87,  
			 6, 0x3, CFG_VALID, PAD_DRV_CFG5_REG, CFG_STR_00, 
			 19, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 19, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 19, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_88,  
			 8, 0x3, CFG_VALID, PAD_DRV_CFG5_REG, CFG_STR_00, 
			 20, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 20, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 20, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_89,  
			 10, 0x3, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 21, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 21, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 21, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_90,  
			 12, 0x3, CFG_VALID, PAD_DRV_CFG5_REG, CFG_STR_00, 
			 22, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 22, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 22, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_91,  
			 14, 0x3, CFG_VALID, PAD_DRV_CFG5_REG, CFG_STR_00, 
			 23, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 23, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 23, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_92,  
			 /* drv streng bit[17]&bit[6] = 00, must special proceed */
			 17, 0x1, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 24, 0x1, CFG_VALID, PAD_IE_CFG2_REG,0x1,
			 24, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 24, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_93,  
			 18, 0x3, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 25, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 25, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 25, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_94,  
			 20, 0x3, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 26, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 26, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 26, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_95,  
			 22, 0x3, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 27, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 27, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0, 
			 27, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_96,  
			 24, 0x3, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 28, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 28, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 28, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_97,  
	         26, 0x3, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 29, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1, 
			 29, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 29, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_98,  
			 28, 0x3, CFG_VALID, PAD_DRV_CFG5_REG, CFG_STR_00, 
			 30, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 30, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG,0x0,
			 30, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_99,  
			 30, 0x3, CFG_VALID, PAD_DRV_CFG5_REG,  CFG_STR_00,
			 31, 0x1, CFG_VALID, PAD_IE_CFG2_REG, 0x1,
			 31, 0x1, CFG_VALID, PU_PD_ENABLE_CFG2_REG, 0x0,
			 31, 0x1, CFG_VALID, PU_PD_SEL_CFG2_REG, 0x0
			},
	{AK_GPIO_100,  
			 0, 0x3, CFG_VALID, PAD_DRV_CFG6_REG,  CFG_STR_00,
			 15, 0x1, CFG_VALID, PAD_IE_CFG3_REG, 0x1,
			 10, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0,
			 20, 0x1, CFG_VALID, PU_PD_SEL_CFG3_REG, 0x0
			},
	{AK_GPIO_101,  
	         2, 0x3, CFG_VALID, PAD_DRV_CFG6_REG, CFG_STR_00, 
			 16, 0x1, CFG_VALID, PAD_IE_CFG3_REG, 0x1,
			 11, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG,0x0,
			 21, 0x1, CFG_VALID, PU_PD_SEL_CFG3_REG, 0x0
			},
	{AK_GPIO_102,  
			 4, 0x3, CFG_VALID, PAD_DRV_CFG6_REG, CFG_STR_00, 
			 17, 0x1, CFG_VALID, PAD_IE_CFG3_REG, 0x1,
			 12, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0,
			 22, 0x1, CFG_VALID, PU_PD_SEL_CFG3_REG, 0x0
			},
	{AK_GPIO_103,  
			 6, 0x3, CFG_VALID, PAD_DRV_CFG6_REG,  CFG_STR_00,
			 18, 0x1, CFG_VALID, PAD_IE_CFG3_REG, 0x1,
			 13, 0x1, CFG_VALID, PU_PD_ENABLE_CFG3_REG, 0x0,
			 23, 0x1, CFG_VALID, PU_PD_SEL_CFG3_REG, 0x0
			},
	{AK_GPIO_104,  
			 0, 0x3, CFG_INVALID, 0, CFG_STR_00,   // invalid drv
			 0, 0x0, CFG_INVALID, 0,0,			// invalid ie
			 4, 0x1, CFG_INVALID, 0,0,				// ao_pad_ren_cfg:enable
			 0, 0x0, CFG_INVALID, 0,0     // invalid sel pupd
			},
	{AK_GPIO_105,  
			 0, 0x3, CFG_INVALID, 0, CFG_STR_00,  // invalid drv
			 0, 0x0, CFG_INVALID, 0,0,			// invalid ie
			 5, 0x1, CFG_INVALID, 0,0,				// ao_pad_ren_cfg:enable
			 0, 0x0, CFG_INVALID, 0,0      // invalid sel pupd
			},
	{AK_GPIO_106,  
			 0, 0x3, CFG_INVALID, 0,CFG_STR_00,    // invalid drv
			 0, 0x0, CFG_INVALID, 0,0,			// invalid ie
			 6, 0x1, CFG_INVALID, 0,0,				// ao_pad_ren_cfg:enable
			 0, 0x0, CFG_INVALID, 0,0      // invalid sel pupd
			},
	{AK_GPIO_107,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,    // invalid drv 
			 0, 0x0, CFG_INVALID, 0,0,			// invalid ie
			 7, 0x1, CFG_INVALID, 0,0,				// ao_pad_ren_cfg:enable
			 0, 0x0, CFG_INVALID, 0,0      // invalid sel pupd
			},
	{AK_GPIO_108,  
			 0, 0x3, CFG_INVALID, 0,CFG_STR_00,    // invalid drv
			 0, 0x0, CFG_INVALID, 0,0,			// invalid ie
			 8, 0x1, CFG_INVALID, 0,0,			// ao_pad_ren_cfg:enable
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_109,  
			 0, 0x0, CFG_INVALID, 0,  CFG_STR_00,// GPI13
			 0, 0x0, CFG_INVALID, 0, 0,
			 0, 0x0, CFG_INVALID, 0, 0,
			 0, 0x0, CFG_INVALID, 0, 0
			},
	{AK_GPIO_110,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI12
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_111,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI11
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_112,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI10
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_113,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI9
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_114,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI8
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_115,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI7
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_116,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI6
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_117,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI5
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_118,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI4
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_119,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI3
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_120,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI2
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_121,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI1
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			},
	{AK_GPIO_122,  
			 0, 0x0, CFG_INVALID, 0,CFG_STR_00,  // GPI0
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0,
			 0, 0x0, CFG_INVALID, 0,0
			}
};




T_GPIO_SHAREPIN_CFG_AS_GPIO ak_gpio_sharepin_as_gpio[GPIO_NUMBER+1] = {
	{AK_GPIO_0,  
			 8, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG,0x0
			},
	{AK_GPIO_1,  
			 0, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_2,  
			 2, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_3,  
			 4, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_4,  
			 6, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_5,  
			 8, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_6,  
			 10, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_7,  
			 12, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_8,  
			 14, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_9,  
			 16, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_10, 
			 18, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_11,  
			 20, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_12,  
			 28, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_13,  
			 22, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_14,  
			 24, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_15,  
			 26, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_18,  
	         6, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_19,  
			 28, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_20,  
			 30, 0x3, CFG_VALID, SHARE_PIN_CFG0_REG,0x0
			},
	{AK_GPIO_23,  
	         0, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,  0x0
			},
	{AK_GPIO_24,  
			 2, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,0x0
			},
	{AK_GPIO_25,  
			 6, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,0x0
			},
	{AK_GPIO_26,  
			 8, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x0
			},
	{AK_GPIO_27,  
			 10, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x0
			},
	{AK_GPIO_28,  
			 12, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x0
			},
	{AK_GPIO_29,  
	         10, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_30,  
	         12, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_31,  
			 14, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x0
			},
	{AK_GPIO_32,  
			 16, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x0
			},
	{AK_GPIO_33,  
			 18, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,0x0
			},
	{AK_GPIO_34,  
			 20, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,0x0
			},
	{AK_GPIO_35,  
			 22, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,0x0
			},
	{AK_GPIO_36,  
			 24, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x0
			},
	{AK_GPIO_37,  
			 26, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG,0x0
			},
	{AK_GPIO_38,  
			 28, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x0
			},
	{AK_GPIO_39,  
			 30, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x0 
			},
	{AK_GPIO_40,  
	         0, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,  0x0
			},
	{AK_GPIO_41,  
	         2, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0 
			},
	{AK_GPIO_42,  
	         4, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0  
			},
	{AK_GPIO_43,  
	         6, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,  0x0
			},
	{AK_GPIO_44,  
			 8, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0 
			},
	{AK_GPIO_45,  
			 10, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0  
			},
	{AK_GPIO_46,  
	         12, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0  
			},
	{AK_GPIO_47,  
			 14, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0 
			},
	{AK_GPIO_48,  
			 16, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0 
			},
	{AK_GPIO_49,  
			 4, 0x3, CFG_VALID, SHARE_PIN_CFG1_REG, 0x0
			},
	{AK_GPIO_50,  
			 18, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0 
			},
	{AK_GPIO_51,  
			 20, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0 
			},
	{AK_GPIO_52,  
	         22, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0
			},
	{AK_GPIO_53,  
			 24, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,  0x0
			},
	{AK_GPIO_54,  
			 26, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0 
			},
	{AK_GPIO_55,  
	         28, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG,  0x0
			},
	{AK_GPIO_56,  
	         30, 0x3, CFG_VALID, SHARE_PIN_CFG2_REG, 0x0 
			},
	{AK_GPIO_57,  
			 0, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG,  0x0
			},
	{AK_GPIO_58,  
	         2, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_59,  
	         4, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_60,  
	         14, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_61,  
	         16, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_62,  
	         18, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_63,  
			 20, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_64,  
			 22, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_65,  
	         24, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_66,  
			 26, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0
			},
	{AK_GPIO_67,  
	         30, 0x3, CFG_VALID, SHARE_PIN_CFG3_REG, 0x0 
			},
	{AK_GPIO_68,  
	         0, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_69,  
	         2, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_70,  
			 4, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_71,  
			 6, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_72,  
			 8, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_73,  
			 10, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_74,  
			 12, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_75,  
			 14, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_76,  
			 16, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_77,  
			 18, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_78,  
			 20, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_79,  
			 22, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_80,  
			 24, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG, 0x0  
			},
	{AK_GPIO_81,  
			 26, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_82,  
			 28, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_83,  
			 30, 0x3, CFG_VALID, SHARE_PIN_CFG4_REG,  0x0
			},
	{AK_GPIO_84,  
			 0, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x0
			},
	{AK_GPIO_85,  
			 2, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x0
			},
	{AK_GPIO_86,  
			 4, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x0
			},
	{AK_GPIO_87,  
			 6, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG, 0x0 
			},
	{AK_GPIO_88,  
			 8, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG, 0x0 
			},
	{AK_GPIO_89,  
			 10, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x0
			},
	{AK_GPIO_90,  
			 12, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG, 0x0 
			},
	{AK_GPIO_91,  
			 14, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG, 0x0 
			},
	{AK_GPIO_92,  
			 16, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x0
			},
	{AK_GPIO_93,  
			 18, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x0
			},
	{AK_GPIO_94,  
			 20, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x0
			},
	{AK_GPIO_95,  
			 22, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x0
			},
	{AK_GPIO_96,  
			 24, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x0
			},
	{AK_GPIO_97,  
	         26, 0x3, CFG_VALID, SHARE_PIN_CFG5_REG,  0x0
			},
	{AK_GPIO_98,  
			 28, 0x7, CFG_VALID, SHARE_PIN_CFG5_REG, 0x0 
			},
	{AK_GPIO_99,  
			 0, 0x3, CFG_INVALID, 0,  0x0
			},
	{AK_GPIO_100,  
			 0, 0x3, CFG_INVALID, 0,  0x0
			},
	{AK_GPIO_101,  
	         0, 0x3, CFG_INVALID, 0,  0x0
			},
	{AK_GPIO_102,  
			 0, 0x3, CFG_INVALID, 0,  0x0
			},
	{AK_GPIO_103,  
			 0, 0x3, CFG_INVALID, 0,  0x0
			},
	{AK_GPIO_109,  
			 31, 0x1, CFG_VALID, SHARE_PIN_CFG5_REG,0// GPI13
			},
	{AK_GPIO_110,  
			 31, 0x1, CFG_VALID, SHARE_PIN_CFG5_REG,0  // GPI12
			},
	{AK_GPIO_111,  
			 31, 0x1, CFG_VALID, SHARE_PIN_CFG5_REG,0  // GPI11
			},
	{AK_GPIO_112,  
			 31, 0x1, CFG_VALID, SHARE_PIN_CFG5_REG,0  // GPI10
			},
	{AK_GPIO_113,  
			 31, 0x1, CFG_VALID, SHARE_PIN_CFG5_REG,0  // GPI9
			},
	{AK_GPIO_114,  
			 31, 0x1, CFG_VALID, SHARE_PIN_CFG5_REG,0  // GPI8
			},
	{AK_GPIO_115,  
			 031, 0x1, CFG_VALID, SHARE_PIN_CFG6_REG,0  // GPI7
			},
	{AK_GPIO_116,  
			 31, 0x1, CFG_VALID, SHARE_PIN_CFG6_REG,0  // GPI6
			},
	{AK_GPIO_117,  
			 29, 0x1, CFG_VALID, SHARE_PIN_CFG6_REG,0  // GPI5
			},
	{AK_GPIO_118,  
			 29, 0x1, CFG_VALID, SHARE_PIN_CFG6_REG,0 // GPI4
			},
	{AK_GPIO_119,  
			 30, 0x1, CFG_VALID, SHARE_PIN_CFG6_REG,0  // GPI3
			},
	{AK_GPIO_120,  
			 30, 0x1, CFG_VALID, SHARE_PIN_CFG6_REG,0  // GPI2
			}
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

			if(ak_gpio_attr[i].gpio_pupd_sel_sel_valid == CFG_VALID){
				reg_data = ak_gpio_attr[i].gpio_pupd_sel_reg;
				regvalue = __raw_readl(reg_data);
				regvalue &= ~(ak_gpio_attr[i].gpio_pupd_sel_bitmask << ak_gpio_attr[i].gpio_pupd_sel_bitstart);

				/* set gpio pu according to gpio pu set */
				if(ak_gpio_attr[i].gpio_pupd_sel_pu_set){
					regvalue |= (0x1 << ak_gpio_attr[i].gpio_pupd_sel_bitstart);
				}else{
					regvalue &= ~(0x1 << ak_gpio_attr[i].gpio_pupd_sel_bitstart);
				}
				__raw_writel(regvalue, reg_data);
			}else{
				printf("cfg pu_sel invalid!\n");
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

			if(ak_gpio_attr[i].gpio_pupd_sel_sel_valid == CFG_VALID){
				reg_data = ak_gpio_attr[i].gpio_pupd_sel_reg;
				regvalue = __raw_readl(reg_data);
				regvalue &= ~(ak_gpio_attr[i].gpio_pupd_sel_bitmask << ak_gpio_attr[i].gpio_pupd_sel_bitstart);

				/* 
				* set gpio pu according to gpio pu set 
				* pull down opposite pull up set
				*/
				if(ak_gpio_attr[i].gpio_pupd_sel_pu_set){
					regvalue &= ~(0x1 << ak_gpio_attr[i].gpio_pupd_sel_bitstart);
				}else{
					regvalue |= (0x1 << ak_gpio_attr[i].gpio_pupd_sel_bitstart);
				}
				__raw_writel(regvalue, reg_data);
			}else{
				printf("cfg pd_sel invalid!\n");
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

