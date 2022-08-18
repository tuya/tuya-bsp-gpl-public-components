/* 
 *	mach/reset.h
 */
#ifndef _AK39_TIMER_WDT_H_
#define _AK39_TIMER_WDT_H_

#include <config.h>
#include <common.h>
#include <asm/io.h>
#include <asm/arch-ak37e/ak_cpu.h>
#include <asm/arch-ak37e/ak_types.h> 


void wdt_enable(void);
void wdt_keepalive(unsigned int heartbeat);


#endif
