/*
 * (C) Copyright 2003
 * Texas Instruments.
 * Kshitij Gupta <kshitij@ti.com>
 * Configuation settings for the TI OMAP Innovator board.
 *
 * (C) Copyright 2004
 * ARM Ltd.
 * Philippe Robin, <philippe.robin@arm.com>
 * Configuration for Versatile PB.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ANYCLOUD_AK39EV330_H
#define __ANYCLOUD_AK39EV330_H

#include "ak_board.h"

#define CONFIG_USER_LOAD_ADDR 0x80008000
#define CONFIG_MD5
#define CONFIG_H3_B_CODE   1
/*#define CONFIG_UPDATE_FLAG*/



/* 
* select RMII ethernet interface 
*/
#define CONFIG_ETHERNET_MAC_RMII 	1

#define CONFIG_OF_CONTROL


/* 
* select TF plug in or plug off use GPIO or MCK 
*/
#define TF_DET_MCK_MODE		0xFF
#define TF_MCI_IDEX	        0
#define TF_POWER_ON_GPIO	12
#define TF_DET_GPIO		    76
#define NET_RESET_GPIO		82




#define CONFIG_ANYKA_WATCHDOG   1

/*
 * High Level Configuration Options
 * (easy to change)
 */
#define CONFIG_ARM926EJS	1	/* This is an arm926ejs CPU core */
#define CONFIG_PLAT_ANYKA 	1
#define CONFIG_USE_IRQ      0      
#define CONFIG_BOARD_EARLY_INIT_F
/*
#define CONFIG_DELAY_ENVIRONMENT 1  // cdh:add for environment delay 
*/

/*use for arch/arm/cpu/arm926ejs/start.S */
/*read uboot from 512 bytes offset pos in spi flash 0~511 is irom param.*/
#ifdef  CONFIG_SPINAND_FLASH
#define CONFIG_SYS_TEXT_BASE   0x80eff800    
#else
#define CONFIG_SYS_TEXT_BASE   0x80dffe00   
#endif

#define CONFIG_OF_LIBFDT

/*
 * Stack sizes
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKSIZE	(128*1024)	/* regular stack */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(8*1024)	/* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(4*1024)	/* FIQ stack */
#endif

/*
 * Stack should be on the SRAM because
 * DRAM is not init ,(arch/arm/lib/crt0.S)
 */
#define CONFIG_SYS_INIT_SP_ADDR		(0x80ff0000 - GENERATED_GBL_DATA_SIZE)

/*
 * Size of malloc() pool
 */
#define CONFIG_SYS_MALLOC_LEN	(CONFIG_ENV_SIZE + 4096*1024)


/*
 * input clock of PLL
 */
/* ak39 has 12.0000MHz input clock */
#define CONFIG_SYS_CLK_FREQ	12000000
#define CONFIG_SYS_HZ  		(1000)

/*
 * cdh:add print version info
 */
#define CONFIG_VERSION_VARIABLE 1	

/*
* cdh:add for uart download
*/
#define CONFIG_CMD_LOADB


/* max number of memory banks */
#define CONFIG_SYS_MAX_FLASH_BANKS	1
#define CONFIG_SYS_FLASH_BASE		0x0


/* enviroment variable offset and size in spi nor flash. According to customer's requirement,it can be modified.*/
#ifdef  CONFIG_RTT_DTB_ADDR
#define CONFIG_LZMA
#define CONFIG_DTB_LOADADDR			0x83fd3000
#else
#define CONFIG_DTB_LOADADDR			0x81300000
#endif


#ifdef  CONFIG_SPINAND_FLASH
#define CONFIG_SPL_TEXT_BASE   0x80fff800  
#define CONFIG_SPL_MAX_FOOTPRINT	0x20000 /*32768*/

#else
#define CONFIG_SPL_TEXT_BASE   0x80effe00
#define CONFIG_SPL_MAX_FOOTPRINT	0x20000 /*32768*/
#endif

#ifdef  CONFIG_SPINAND_FLASH
#define CONFIG_ENV_OFFSET  			0x20000
#define CONFIG_ENV_SIZE  			(4*1024)
#define CONFIG_BKENV_OFFSET  		(0x40000)	
#define CONFIG_BKENV_SIZE  			(4*1024)
#define CONFIG_ENV_PARTITION_SIZE  	(128*1024)

#define CONFIG_DEFAULT_MTDPARTS \
    "mtdparts=mtdparts=spi0.0:128K@0x0(SPL),128K@0x20000(ENV),128K@0x40000(ENVBK),256K@0x60000(UBOOT),"\
    "128K@0xa0000(DTB),1664K@0xC0000(KERNEL),"\
    "20480K@0x260000(ROOTFS),20480K@0x1660000(CONFIG),40960K@0x2a60000(APP)\0"\

#else
#ifdef CONFIG_SPL_BOOT   /*spl*/
#define CONFIG_ENV_OFFSET  			0x10000
#define CONFIG_ENV_SIZE  			(4*1024)
#define CONFIG_BKENV_OFFSET  		(0x11000)	
#define CONFIG_BKENV_SIZE  			(4*1024)

#define CONFIG_DEFAULT_MTDPARTS \
    "mtdparts=mtdparts=spi0.0:64K@0x0(SPL),4K@0x10000(ENV),4K@0x11000(ENVBK),"\
    "64K@0x12000(DTB),4096K@0x22000(KERNEL),100K@0x422000(ROOTFS),2048K@0x43B000(CONFIG),1812K@0x63B000(APP)\0"\

	
#else  /*uboot*/

#ifdef  CONFIG_RTT_DTB_ADDR
#define CONFIG_ENV_OFFSET  			(0x34000)
#define CONFIG_ENV_SIZE  			(4*1024)
#define CONFIG_BKENV_OFFSET  		(0x35000)	
#define CONFIG_BKENV_SIZE  			(4*1024)
#else
#define CONFIG_ENV_OFFSET  			(0x40000)
#define CONFIG_ENV_SIZE  			(4*1024)
#define CONFIG_BKENV_OFFSET  			(0x41000)	
#define CONFIG_BKENV_SIZE  			(4*1024)
#endif

#define CONFIG_DEFAULT_MTDPARTS \
    "mtdparts=mtdparts=spi0.0:200K@0x0(UBOOT),4K@0x32000(ENV),4K@0x33000(ENVBK),"\
    "48K@0x34000(DTB),1600K@0x40000(KERNEL),1960K@0x1D0000(ROOTFS),300K@0x3BA000(CONFIG),4076K@0x405000(APP)\0"\

#endif
#endif



#define CONFIG_BOOTARGS "console=ttySAK0,115200n8 root=/dev/mtdblock4 "\
                       "rootfstype=yafffs2 init=/sbin/init mem=64M memsize=64M"\

/*
* Flash config
*/
/*#define CONFIG_CMD_FLASH   1  */
#define CONFIG_AK_SPI
/*#define CONFIG_SPI_XFER_CPU*/
#define CONFIG_SPI_XFER_DMA

#define CONFIG_SPI_FLASH
#define CONFIG_CMD_SF
#define CONFIG_SPI_FLASH_ANYKA 		1
#define CONFIG_CMD_SF_TEST 	
#define CONFIG_SYS_MAX_FLASH_SECT 	8192 
#define CONFIG_SYS_MAX_NAND_DEVICE  1  
#define CONFIG_SYS_NAND_BASE      0x20120000   
#define CONFIG_SPIFLASH_USE_FAST_READ 1

#define CONFIG_SPI_FLASH_BAR		1


/* ATAG configuration */
#define CONFIG_CMDLINE_TAG
#define CONFIG_CMDLINE_EDITING
#define CONFIG_AUTO_COMPLETE

/* MMC */
#define CONFIG_CMD_FAT    1
#define CONFIG_PARTITIONS 1
#define CONFIG_DOS_PARTITION		1
#define CONFIG_MMC     1
#define CONFIG_CMD_MMC
#define CONFIG_GENERIC_MMC		1
#define CONFIG_ANYKA_SDHSMMC		1   

/*
 * Miscellaneous configurable options
 */
#define CONFIG_LONGHELP
#define CONFIG_SYS_LONGHELP	1  
#define CONFIG_SYS_PROMPT	"\nanyka$"
#define CONFIG_SYS_CBSIZE	512
#define CONFIG_SYS_PBSIZE	(CONFIG_SYS_CBSIZE+sizeof(CONFIG_SYS_PROMPT)+16)
#define CONFIG_SYS_MAXARGS	32
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE

/* default load address	*/
#define CONFIG_SYS_LOAD_ADDR		0x82008000

/*
 * Check if key already pressed
 * Don't check if bootdelay < 0
 */
#define CONFIG_ZERO_BOOTDELAY_CHECK
#define CONFIG_BOOTDELAY 	1

#define CONFIG_CFG_DEF_ADDR 		0x81000000
#define CONFIG_CFG_FILE_CNT 		0x8  

#define CONFIG_ENV_SPI_BUS	0
#define CONFIG_ENV_SPI_CS	0
#define CONFIG_ENV_SPI_MAX_HZ	50000000
#define CONFIG_ENV_SPI_MODE	SPI_MODE_0

#define CONFIG_SF_DEFAULT_BUS	0
#define CONFIG_SF_DEFAULT_CS	0
#define CONFIG_SF_DEFAULT_SPEED	50000000
#define CONFIG_SF_DEFAULT_MODE	SPI_MODE_0

#define CONFIG_BIOS_NAME 	"KERNEL"
#define CONFIG_BIOSBK_NAME 	"KERBK"
#define CONFIG_ROOT_NAME 	"ROOTFS"
#define CONFIG_ROOTBK_NAME 	"ROOTBK"
#define CONFIG_APP_NAME 	"APP"
#define CONFIG_APPBK_NAME 	"APPBK"


#define CONFIG_ENV_NAME 	"ENV"
#define CONFIG_ENVBK_NAME 	"ENVBK" 


#define CONFIG_DTB_NAME 	"DTB"
#define CONFIG_LOGO_NAME 	"LOGO"
#define CONFIG_UBOOT_NAME 	"UBOOT"
#define CONFIG_UBOOTBK_NAME "UB_BK"
#define CONFIG_UBOOT_ENTER 	"a_uboot_flags"


#define CONFIG_READ_ENV_A_UBOOT_SIZE 256

#define CONFIG_SPINAND_PAGE_SIZE 2048    // cdh:add for erase section size

#define CONFIG_SECTION_SIZE 4096    // cdh:add for erase section size
#define CONFIG_PARATION_TAB_SIZE 512    // cdh:add for paration table section size

#define CONFIG_SPIP_START_PAGE 0    // cdh:add for searching SPIP flag start flash offset address
#define CONFIG_SPIP_PAGE_CNT  3     // cdh:add for searching SPIP flag
#define CONFIG_PARATION_PAGE_CNT  2     // cdh:add for searching  paration table page cnt


#define CONFIG_ENV_IS_IN_SPI_FLASH 	1
#define CONFIG_ENV_SECT_SIZE 	CONFIG_ENV_SIZE

#define CONFIG_CMD_MTDPARTS  /* Enable MTD parts commands    */
#define CONFIG_MTD_PARTITIONS
#define CONFIG_MTD_DEVICE
#define CONFIG_CMD_LOAD

/* customer need detecting gpio to meet their self-defined requirement. */
#define CONFIG_AK_GPIO

/*
#define CONFIG_TF_UPDATE
*/

/*
 * Physical Memory Map and Memory Size 64M or128MB
 */
#define CONFIG_NR_DRAM_BANKS        		1          /* we have 1 bank of DRAM */
#define CONFIG_SYS_SDRAM_BASE       		0x80000000
#define CONFIG_SYS_SDRAM_PROTECT_SIZE       0x01000000 /* 16MB of DRAM */

/* 64MB or 128M of DRAM Config */
#ifdef CONFIG_DRAM_64M
#define PHYS_SDRAM_SIZE             		(64*1024*1024) 
#define  MEM_SET  "mem=mem=64M\0"
#define  MEM_SIZE_SET  "memsize=memsize=64M\0"
#endif

#ifdef CONFIG_DRAM_128M
#define PHYS_SDRAM_SIZE 					(128*1024*1024) 
#define  MEM_SET  "mem=mem=128M\0"
#define  MEM_SIZE_SET  "memsize=memsize=128M\0"
#endif



/* According to customer different requirement,we defined three kind of partition table:
* 1.First one is partition table defined by anyka. Saved in partition env by anyka burntool. 
* 2.Second one is partition table defined by customer. Saved in partition env by nor flash programer,beacause
* customer don't wanna modify uboot code.
* 3.Third one is partition table defined by customer self-defined. Saved in uboot code,so if you wanna change
* partition table, you have to modify uboot code.
* You can only choose one from below three.
*/


/*
#define CONFIG_PARTITION_TABLE_BY_CUSTOMER_ENV 
*/
/*#define CONFIG_PARTITION_TABLE_BY_SELF_DEFINED */

#if 0
#ifdef CONFIG_PARTITION_TABLE_BY_SELF_DEFINED
#define CONFIG_BOOTARGS "console=ttySAK0,115200n8 root=/dev/mtdblock4 "\
			"rootfstype=squashfs init=/sbin/init mem=64M memsize=64M mtdparts=spi0.0:1952K@0x44000(KERNEL)," \
	"1452K@0x289000(ROOTFS),100K@0x3f4000(CONFIG),3852K@0x40d000(APP),"	\
	"4K@0x22c000(MAC),4K@0x22d000(ENV),64K@0x22e000(DTB),300K@0x23e000(LOGO)"
#endif
#endif

#ifdef  CONFIG_SPINAND_FLASH
#define  MTDIDS_DEFAULT  "nand0=spi0.0"
#define  ROOT_FS_TYPE    "rootfstype=yaffs2\0"
#else
#define  MTDIDS_DEFAULT  "nor0=spi0.0"
#define  ROOT_FS_TYPE    "rootfstype=squashfs\0"
#endif


#if 0
#define CONFIG_DEFAULT_MTDPARTS \
    "mtdparts=mtdparts=spi0.0:272K@0x0(uboot),1952K@0x44000(KERNEL)," \
    "1452K@0x289000(ROOTFS),100K@0x3f4000(CONFIG),3852K@0x40d000(APP)," \
    "4K@0x22c000(MAC),4K@0x22d000(ENV),64K@0x22e000(DTB),300K@0x23e000(LOGO)\0"\

#endif
#define  ROOT_MTD_BLOCK_IDEX    "mtd_root=/dev/mtdblock5\0"


#define CONFIG_EXTRA_ENV_SETTINGS \
	"a_uboot_flags=0x0\0" \
	"kernel_offset=0x0"\
	"kernel_addr=0x0\0" \
	"kernel_size=0x0\0" \
	"dtb_offset=0x0\0" \
	"dtb_size=0x0\0" \
	"dtb_addr=0x0\0" \
	"loadaddr=0x80008000\0" \
	"fdt_high=0xFFFFFFFF\0" \
	"console=ttySAK0,115200n8\0" \
	"ethaddr=00:55:7b:b5:7d:f7\0" \
	"init=/sbin/init\0" \
	"bootargs=console=${console} root=${mtd_root} rootfstype=${rootfstype} init=${init} \0" \
	"read_kernel=sf probe 0:0 ${sf_hz} 0; sf read ${loadaddr} ${kernel_offset} ${kernel_size}\0" \
	"read_dtb=sf probe 0:0 ${sf_hz} 0; sf read ${fdtcontroladdr} ${dtb_addr} ${dtb_size};fdt addr ${fdtcontroladdr}\0" \
	"boot_normal=env set bootargs console=${console} root=${mtd_root} rootfstype=${rootfstype} init=${init} ${mtdparts} ${mem} ${memsize}; run read_kernel; bootm ${loadaddr} - ${fdtcontroladdr}\0" /*go ${loadaddr}*/\
	"bootcmd=run boot_normal\0" \
	"vram=12M\0" \


#define CONFIG_CMD_RUN 
#define CONFIG_CMD_MEMORY 
#define CONFIG_CMD_SAVEENV

/*
 * Hardware drivers
 */
#define CONFIG_AK_SERIAL
#define CONFIG_SERIAL1

/* Define default IP addresses */
#define CONFIG_IPADDR		192.168.1.99	/* own ip address */
#define CONFIG_SERVERIP		192.168.1.1	/* used for tftp (not nfs?) */
#define CONFIG_NETMASK      255.255.255.0    

/* valid baudrates */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 }
#define CONFIG_BAUDRATE		115200

/*
 * For NOR boot, we must set this to the start of where NOR is mapped
 * in memory.
 */
#ifdef CONFIG_NOR_BOOT
#define CONFIG_SYS_TEXT_BASE		0x08000000
#endif

#ifdef CONFIG_SPL_BOOT
/* defines for SPL */
#define CONFIG_SPL
#define CONFIG_SPL_FRAMEWORK
#define CONFIG_SPL_BOARD_INIT
#define CONFIG_SYS_SPL_MALLOC_START	(CONFIG_SYS_TEXT_BASE - \
						CONFIG_SYS_MALLOC_LEN)
#define CONFIG_SYS_SPL_MALLOC_SIZE	CONFIG_SYS_MALLOC_LEN
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
/*#define CONFIG_SPL_LDSCRIPT	"board/$(BOARDDIR)/u-boot-spl-ipam390.lds"*/
/*#define CONFIG_SPL_STACK	0x8001ff00*/

#define CONFIG_SPL_MAX_SIZE	0x20000

/* defines for support spi */
#define CONFIG_SPL_SPI_SUPPORT
#define CONFIG_SPL_SPI_FLASH_SUPPORT
#define CONFIG_SPL_SPI_LOAD
#define CONFIG_SPL_SPI_BUS		0
#define CONFIG_SPL_SPI_CS		0
#define CONFIG_SYS_SPI_U_BOOT_OFFS	0x20000

/* add FALCON boot mode */
#define CONFIG_CMD_SPL
#define CONFIG_SPL_OS_BOOT
#define CONFIG_SYS_NAND_SPL_KERNEL_OFFS	0x00200000
#define CONFIG_SYS_SPL_ARGS_ADDR	0x00180000
#define CONFIG_CMD_SPL_NAND_OFS		0x00180000
#define CONFIG_CMD_SPL_WRITE_SIZE	0x400
#endif

#endif	/* __CONFIG_H */
