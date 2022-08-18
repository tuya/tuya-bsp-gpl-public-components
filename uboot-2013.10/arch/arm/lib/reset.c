/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
 *
 * (C) Copyright 2004
 * DAVE Srl
 * http://www.dave-tech.it
 * http://www.wawnet.biz
 * mailto:info@wawnet.biz
 *
 * (C) Copyright 2004 Texas Insturments
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#ifdef CONFIG_H3_B_CODE
#include <asm/arch-ak39ev33x/ak_timer_wdt.h>
#else
#ifdef CONFIG_37_E_CODE
#include <asm/arch-ak37d/ak_timer_wdt.h>
#else
#include <asm/arch-ak37d/ak_timer_wdt.h>
#endif
#endif

int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	puts ("resetting ...\n");

	udelay (50000);				/* wait 50 ms */

	disable_interrupts();

#if 0
	reset_cpu(0);
#else
	wdt_enable();
	wdt_keepalive(1);
#endif

	/*NOTREACHED*/
	return 0;
}
