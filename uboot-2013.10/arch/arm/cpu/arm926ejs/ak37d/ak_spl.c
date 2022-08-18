/*
 * Copyright (C) 2011
 * Heiko Schocher, DENX Software Engineering, hs@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <malloc.h>
#include <stdio_dev.h>
#include <version.h>
#include <net.h>
#include <serial.h>
#include <nand.h>
#include <onenand_uboot.h>
#include <mmc.h>
#include <libfdt.h>
#include <fdtdec.h>
#include <post.h>
#include <logbuff.h>
#include <asm/sections.h>
#include <asm/arch-ak37d/ak_cpu.h>
#include <spl.h>



DECLARE_GLOBAL_DATA_PTR;

void spl_board_init(void)
{
	preloader_console_init();
}

u32 spl_boot_mode(void)
{
	return MMCSD_MODE_RAW;
}

u32 spl_boot_device(void)
{

#ifdef CONFIG_SPINAND_FLASH
	return BOOT_DEVICE_SPINAND;
#else
	return BOOT_DEVICE_SPI;
#endif

}


#ifdef CONFIG_SPL_BUILD
//for panic() -> do_reset()
void wdt_enable(void)
{
	
}
void wdt_keepalive(unsigned int heartbeat)
{
        while(1);
}
#endif


