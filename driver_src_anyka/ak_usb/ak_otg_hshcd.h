/*
 * AKOTG HS register declarations and HCD data structures
 */

#ifndef __ANYKA_OTG_HS_H_
#define __ANYKA_OTG_HS_H_


#include <linux/delay.h>
#include <asm/gpio.h>

#define AKOTG_HC_HCD

#define USB_OP_MOD_REG			(AK_VA_SYSCTRL + 0x58)
#define USB_MODULE_RESET_REG	(AK_VA_SYSCTRL + 0x20)

#ifdef CONFIG_MACH_AK39EV330
/*
 * 0x0800,0034:
 * Other Source Wake-up Control & USB Rref Control Register
 * Controlling the wakeup function of other wake -up sources,
 * setting the on-chip reference resistor for USB PHY.
 */
#define USB_RREF_CON_REG		(AK_VA_SYSCTRL + 0x34)
#endif

#define USB_HC_BASE_ADDR	(AK_VA_USB)
#define H_MAXPACKET			512	   /* bytes in fifo */

#endif /* __ANYKA_OTG_HS_H_ */
