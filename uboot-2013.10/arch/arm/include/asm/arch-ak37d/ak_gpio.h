/**
 * @file gpio.h
 * @brief gpio function header file
 *
 * This file define gpio macros and APIs: intial, set gpio, get gpio. etc.
 * Copyright (C) 2006 Anyka (GuangZhou) Software Technology Co., Ltd.
 * @author Miaobaoli
 * @date 2005-07-24
 * @version 1.0
 *
 * @note:
 * 1. 对于mmi系统中已定义了的gpio，不需要删除相关代码，只需将其定义为INVALID_GPIO
 
 * 2. 如果需要用到扩展io，只需要打开GPIO_MULTIPLE_USE宏，并设置对应的gpio
 *    GPIO_EXPAND_OUT1和GPIO_EXPAND_OUT2，如果只有一组扩展io,可以将GPIO_EXPAND_OUT2
 *	  设为INVALID_GPIO即可
 * 
 * 3. 对于不同的硬件板请以宏隔开并配置好相应宏定义
 *
 */

#ifndef __GPIO_H__
#define __GPIO_H__

#include <asm/arch-ak37d/ak_cpu.h>

/**@brief total number
 */
 
/*
* ak3780d include 123 gpio(GPIO[108:0] + GPI[13:0])
* GPI[13:0] Mapping to GPIO[109:122]
*/
#define GPIO_NUMBER                     122

/**@brief invalidate gpio
 */
#define INVALID_GPIO                    0xfe

/** @name gpio output level define
 *  define gpio output level
 */
 /*@{*/
#define GPIO_LEVEL_LOW                  0
#define GPIO_LEVEL_HIGH                 1
/* @} */

/** @name gpio dir define
 *  define gpio dir
 */
 /*@{*/
#define GPIO_DIR_INPUT                  0
#define GPIO_DIR_OUTPUT                 1
/* @} */

/** @name gpio interrupt control define
 *  define gpio interrupt enable/disable
 */
 /*@{*/
#define GPIO_INTERRUPT_DISABLE          0
#define GPIO_INTERRUPT_ENABLE           1
/* @} */

/** @name gpio interrupt active level
 *  define gpio interrupt active level
 */
 /*@{*/
#define GPIO_INTERRUPT_LOW_ACTIVE       0
#define GPIO_INTERRUPT_HIGH_ACTIVE      1   
/* @} */

/** @name gpio interrupt type define
 *  define gpio interrupt type
 */
 /*@{*/
#define GPIO_LEVEL_INTERRUPT            0
#define GPIO_EDGE_INTERRUPT             1
/* @} */


/**************** gpio offsets ************************/
#define AK_GPIO_GROUP1		(32*0)
#define AK_GPIO_GROUP2		(32*1)
#define AK_GPIO_GROUP3		(32*2)
#define AK_GPIO_GROUP4		(32*3)

#define AK_GPIO_GROUP1_NO(offset)		( AK_GPIO_GROUP1 + (offset))
#define AK_GPIO_GROUP2_NO(offset)		( AK_GPIO_GROUP2 + (offset))
#define AK_GPIO_GROUP3_NO(offset)		( AK_GPIO_GROUP3 + (offset))
#define AK_GPIO_GROUP4_NO(offset)		( AK_GPIO_GROUP4 + (offset))
	
#define AK_GPIO_0			AK_GPIO_GROUP1_NO(0)
#define AK_GPIO_1			AK_GPIO_GROUP1_NO(1)
#define AK_GPIO_2			AK_GPIO_GROUP1_NO(2)
#define AK_GPIO_3			AK_GPIO_GROUP1_NO(3)
#define AK_GPIO_4			AK_GPIO_GROUP1_NO(4)
#define AK_GPIO_5			AK_GPIO_GROUP1_NO(5)
#define AK_GPIO_6			AK_GPIO_GROUP1_NO(6)
#define AK_GPIO_7			AK_GPIO_GROUP1_NO(7)
#define AK_GPIO_8			AK_GPIO_GROUP1_NO(8)
#define AK_GPIO_9			AK_GPIO_GROUP1_NO(9)
#define AK_GPIO_10			AK_GPIO_GROUP1_NO(10)
#define AK_GPIO_11			AK_GPIO_GROUP1_NO(11)
#define AK_GPIO_12			AK_GPIO_GROUP1_NO(12)
#define AK_GPIO_13			AK_GPIO_GROUP1_NO(13)
#define AK_GPIO_14			AK_GPIO_GROUP1_NO(14)
#define AK_GPIO_15			AK_GPIO_GROUP1_NO(15)
#define AK_GPIO_16			AK_GPIO_GROUP1_NO(16)
#define AK_GPIO_17			AK_GPIO_GROUP1_NO(17)
#define AK_GPIO_18			AK_GPIO_GROUP1_NO(18)
#define AK_GPIO_19			AK_GPIO_GROUP1_NO(19)
#define AK_GPIO_20			AK_GPIO_GROUP1_NO(20)
#define AK_GPIO_21			AK_GPIO_GROUP1_NO(21)
#define AK_GPIO_22			AK_GPIO_GROUP1_NO(22)
#define AK_GPIO_23			AK_GPIO_GROUP1_NO(23)
#define AK_GPIO_24			AK_GPIO_GROUP1_NO(24)
#define AK_GPIO_25			AK_GPIO_GROUP1_NO(25)
#define AK_GPIO_26			AK_GPIO_GROUP1_NO(26)
#define AK_GPIO_27			AK_GPIO_GROUP1_NO(27)
#define AK_GPIO_28			AK_GPIO_GROUP1_NO(28)
#define AK_GPIO_29			AK_GPIO_GROUP1_NO(29)
#define AK_GPIO_30			AK_GPIO_GROUP1_NO(30)
#define AK_GPIO_31			AK_GPIO_GROUP1_NO(31)

#define AK_GPIO_32			AK_GPIO_GROUP2_NO(0)
#define AK_GPIO_33			AK_GPIO_GROUP2_NO(1)
#define AK_GPIO_34			AK_GPIO_GROUP2_NO(2)
#define AK_GPIO_35			AK_GPIO_GROUP2_NO(3)
#define AK_GPIO_36			AK_GPIO_GROUP2_NO(4)
#define AK_GPIO_37			AK_GPIO_GROUP2_NO(5)
#define AK_GPIO_38			AK_GPIO_GROUP2_NO(6)
#define AK_GPIO_39			AK_GPIO_GROUP2_NO(7)
#define AK_GPIO_40			AK_GPIO_GROUP2_NO(8)
#define AK_GPIO_41			AK_GPIO_GROUP2_NO(9)
#define AK_GPIO_42			AK_GPIO_GROUP2_NO(10)
#define AK_GPIO_43			AK_GPIO_GROUP2_NO(11)
#define AK_GPIO_44			AK_GPIO_GROUP2_NO(12)
#define AK_GPIO_45			AK_GPIO_GROUP2_NO(13)
#define AK_GPIO_46			AK_GPIO_GROUP2_NO(14)
#define AK_GPIO_47			AK_GPIO_GROUP2_NO(15)
#define AK_GPIO_48			AK_GPIO_GROUP2_NO(16)
#define AK_GPIO_49			AK_GPIO_GROUP2_NO(17)
#define AK_GPIO_50			AK_GPIO_GROUP2_NO(18)
#define AK_GPIO_51			AK_GPIO_GROUP2_NO(19)
#define AK_GPIO_52			AK_GPIO_GROUP2_NO(20)
#define AK_GPIO_53			AK_GPIO_GROUP2_NO(21)
#define AK_GPIO_54			AK_GPIO_GROUP2_NO(22)
#define AK_GPIO_55			AK_GPIO_GROUP2_NO(23)
#define AK_GPIO_56			AK_GPIO_GROUP2_NO(24)
#define AK_GPIO_57			AK_GPIO_GROUP2_NO(25)
#define AK_GPIO_58			AK_GPIO_GROUP2_NO(26)
#define AK_GPIO_59			AK_GPIO_GROUP2_NO(27)
#define AK_GPIO_60			AK_GPIO_GROUP2_NO(28)
#define AK_GPIO_61			AK_GPIO_GROUP2_NO(29)
#define AK_GPIO_62			AK_GPIO_GROUP2_NO(30)
#define AK_GPIO_63			AK_GPIO_GROUP2_NO(31)

#define AK_GPIO_64			AK_GPIO_GROUP3_NO(0)
#define AK_GPIO_65			AK_GPIO_GROUP3_NO(1)
#define AK_GPIO_66			AK_GPIO_GROUP3_NO(2)
#define AK_GPIO_67			AK_GPIO_GROUP3_NO(3)
#define AK_GPIO_68			AK_GPIO_GROUP3_NO(4)
#define AK_GPIO_69			AK_GPIO_GROUP3_NO(5)
#define AK_GPIO_70			AK_GPIO_GROUP3_NO(6)
#define AK_GPIO_71			AK_GPIO_GROUP3_NO(7)
#define AK_GPIO_72			AK_GPIO_GROUP3_NO(8)
#define AK_GPIO_73			AK_GPIO_GROUP3_NO(9)
#define AK_GPIO_74			AK_GPIO_GROUP3_NO(10)
#define AK_GPIO_75			AK_GPIO_GROUP3_NO(11)
#define AK_GPIO_76			AK_GPIO_GROUP3_NO(12)
#define AK_GPIO_77			AK_GPIO_GROUP3_NO(13)
#define AK_GPIO_78			AK_GPIO_GROUP3_NO(14)
#define AK_GPIO_79			AK_GPIO_GROUP3_NO(15)
#define AK_GPIO_80			AK_GPIO_GROUP3_NO(16)
#define AK_GPIO_81			AK_GPIO_GROUP3_NO(17)
#define AK_GPIO_82			AK_GPIO_GROUP3_NO(18)
#define AK_GPIO_83			AK_GPIO_GROUP3_NO(19)
#define AK_GPIO_84			AK_GPIO_GROUP2_NO(20)
#define AK_GPIO_85			AK_GPIO_GROUP3_NO(21)
#define AK_GPIO_86			AK_GPIO_GROUP3_NO(22)
#define AK_GPIO_87			AK_GPIO_GROUP3_NO(23)
#define AK_GPIO_88			AK_GPIO_GROUP3_NO(24)
#define AK_GPIO_89			AK_GPIO_GROUP3_NO(25)
#define AK_GPIO_90			AK_GPIO_GROUP3_NO(26)
#define AK_GPIO_91			AK_GPIO_GROUP3_NO(27)
#define AK_GPIO_92			AK_GPIO_GROUP3_NO(28)
#define AK_GPIO_93			AK_GPIO_GROUP2_NO(29)
#define AK_GPIO_94			AK_GPIO_GROUP3_NO(30)
#define AK_GPIO_95			AK_GPIO_GROUP3_NO(31)

#define AK_GPIO_96			AK_GPIO_GROUP3_NO(0)
#define AK_GPIO_97			AK_GPIO_GROUP3_NO(1)
#define AK_GPIO_98			AK_GPIO_GROUP3_NO(2)
#define AK_GPIO_99			AK_GPIO_GROUP3_NO(3)
#define AK_GPIO_100			AK_GPIO_GROUP3_NO(4)
#define AK_GPIO_101			AK_GPIO_GROUP3_NO(5)
#define AK_GPIO_102			AK_GPIO_GROUP3_NO(6)
#define AK_GPIO_103			AK_GPIO_GROUP3_NO(7)
#define AK_GPIO_104			AK_GPIO_GROUP3_NO(8)
#define AK_GPIO_105			AK_GPIO_GROUP3_NO(9)
#define AK_GPIO_106			AK_GPIO_GROUP3_NO(10)
#define AK_GPIO_107			AK_GPIO_GROUP3_NO(11)
#define AK_GPIO_108			AK_GPIO_GROUP3_NO(12)
#define AK_GPIO_109			AK_GPIO_GROUP3_NO(13)
#define AK_GPIO_110			AK_GPIO_GROUP3_NO(14)
#define AK_GPIO_111			AK_GPIO_GROUP3_NO(15)
#define AK_GPIO_112			AK_GPIO_GROUP3_NO(16)
#define AK_GPIO_113			AK_GPIO_GROUP3_NO(17)
#define AK_GPIO_114			AK_GPIO_GROUP3_NO(18)
#define AK_GPIO_115			AK_GPIO_GROUP3_NO(19)
#define AK_GPIO_116			AK_GPIO_GROUP2_NO(20)
#define AK_GPIO_117			AK_GPIO_GROUP3_NO(21)
#define AK_GPIO_118			AK_GPIO_GROUP3_NO(22)
#define AK_GPIO_119			AK_GPIO_GROUP3_NO(23)
#define AK_GPIO_120			AK_GPIO_GROUP3_NO(24)
#define AK_GPIO_121			AK_GPIO_GROUP3_NO(25)
#define AK_GPIO_122			AK_GPIO_GROUP3_NO(26)


typedef enum
{
    PULLUP = 0,
    PULLDOWN,
    PULLUPDOWN,
    PUNDEFINED
}T_GPIO_TYPE;


/**
 * @brief share pins
 * 
 */
typedef enum
{
    ePIN_AS_MMCSD = 0,             ///< share pin as MDAT1, 8 lines
    ePIN_AS_I2S,                ///< share pin as I2S bit[24]:0
    ePIN_AS_PWM0,               ///< share pin as PWM0   
    ePIN_AS_PWM1,               ///< share pin as PWM1
    ePIN_AS_PWM2,               ///< share pin as PWM2
    ePIN_AS_PWM3,               ///< share pin as PWM3
    ePIN_AS_PWM4,               ///< share pin as PWM4
	
    ePIN_AS_SDIO,               ///< share pin as SDIO
    ePIN_AS_UART1,              ///< share pin as UART1
    ePIN_AS_UART2,              ///< share pin as UART2
    ePIN_AS_CAMERA,             ///< share pin as CAMERA
    ePIN_AS_SPI0,               ///< share pin as SPI1 bit[25]:0
    ePIN_AS_SPI1,               ///< share pin as SPI2  bit[26]:1
    ePIN_AS_JTAG,               ///< share pin as JTAG
    ePIN_AS_TWI,                ///< share pin as I2C
    ePIN_AS_MAC,                ///< share pin as Ethernet MAC
    ePIN_AS_OPCLK,

    ePIN_AS_DUMMY

}E_GPIO_PIN_SHARE_CONFIG;



typedef struct
{
    E_GPIO_PIN_SHARE_CONFIG func_module;
    unsigned long reg1_bit_mask;
    unsigned long reg1_bit_value;
    unsigned long reg2_bit_mask;
    unsigned long reg2_bit_value;
    unsigned long reg3_bit_mask;
    unsigned long reg3_bit_value;
    unsigned long reg4_bit_mask;
    unsigned long reg4_bit_value;
}
T_SHARE_CFG_FUNC_MODULE;









#endif

