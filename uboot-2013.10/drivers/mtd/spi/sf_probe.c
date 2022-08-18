/*
 * SPI flash probing
 *
 * Copyright (C) 2008 Atmel Corporation
 * Copyright (C) 2010 Reinhard Meyer, EMK Elektronik
 * Copyright (C) 2013 Jagannadha Sutradharudu Teki, Xilinx Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <fdtdec.h>
#include <malloc.h>
#include <spi.h>
#include <spi_flash.h>

#include "sf_internal.h"
#ifdef CONFIG_H3_B_CODE
#include <asm/arch-ak39ev33x/ak_cpu.h>
#else
#ifdef CONFIG_37_E_CODE
#include <asm/arch-ak37e/ak_cpu.h>
#else
#include <asm/arch-ak37d/ak_cpu.h>
#endif
#endif


//#include <common.h>
#include <command.h>
#include <s_record.h>
#include <net.h>
#include <exports.h>
#include <xyzModem.h>
#include <linux/mtd/mtd.h>
struct mtd_info sflash_mtd;


T_SFLASH_PHY_INFO g_spiflash_parm = {0};


DECLARE_GLOBAL_DATA_PTR;

/**
 * struct spi_flash_params - SPI/QSPI flash device params structure
 *
 * @name:		Device name ([MANUFLETTER][DEVTYPE][DENSITY][EXTRAINFO])
 * @jedec:		Device jedec ID (0x[1byte_manuf_id][2byte_dev_id])
 * @ext_jedec:		Device ext_jedec ID
 * @sector_size:	Sector size of this device
 * @nr_sectors:		No.of sectors on this device
 * @flags:		Importent param, for flash specific behaviour
 */
struct spi_flash_params {
	const char *name;
	u32 jedec;
	u16 ext_jedec;
	u32 sector_size;
	u32 nr_sectors;
	u32 flags;
	u32 id_version;
};

#define SPIBOOT_PARAM_ADDR_BASE		(0x48000200)
#define SPIBOOT_PARAM_LEN		  	(512)
static int flash_QE_flag = 0;

static 	struct flash_status_reg status_reg_list[] = {
		/*spiflash kh25l128*/
		{
			.jedec_id = 0xc22018,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_qe = 6,	.b_srp0 = 7,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_cmd_write_status,
		},
		/*spiflash kh25l64*/
		{
			.jedec_id = 0xc22017,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_qe = 6,	.b_srp0 = 7,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_cmd_write_status,
		},
		
		/*spiflash xm25qh64a*/
		{
			.jedec_id = 0x207017,	.ext_id = 0,
			.b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
			.b_bp2 = 4, .b_bp3 = 5, .b_qe = 6,	.b_srp0 = 7,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_cmd_write_status,
		},
		
		/*spiflash xm25qh128a*/
		{
			.jedec_id = 0x207018,	.ext_id = 0,
			.b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
			.b_bp2 = 4, .b_bp3 = 5, .b_qe = 6,	.b_srp0 = 7,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_cmd_write_status,
		},
		/*spiflash xm25qh64c*/
		{
			.jedec_id = 0x204017,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,
			.b_srp1 = 8,.b_qe = 9,	.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},
		
		/*spiflash xm25qh128c*/
		{
			.jedec_id = 0x204018,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,
			.b_srp1 = 8,.b_qe = 9,	.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},

		/*spiflash PN26f64B*/
		{
			.jedec_id = 0x1c7017,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_qe = 6,	.b_srp0 = 7,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_cmd_write_status,
		},	
					
		/*spiflash gd25q128c*/
		{
			.jedec_id = 0xc84018,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,
			.b_srp1 = 8,.b_qe = 9,	.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},

		/*spiflash gd25q64 */
		{
			.jedec_id = 0xc84017,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,
			.b_srp1 = 8,.b_qe = 9,	.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},

		/* WB w25q64jv 8MB flash */ 
		{
			.jedec_id = 0xef4016,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,
			.b_srp1 = 8, .b_qe = 9,	.b_lb = 11,	.b_cmp = 14,
			.b_sus = 15, 
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},
		
		/* w25q128 */
		{
			.jedec_id = 0xef4018,	.ext_id = 0,
			.b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
			.b_bp2 = 4, .b_bp3 = 5, .b_bp4 = 6, .b_srp0 = 7,
			.b_srp1 = 8, .b_qe = 9, .b_lb = 11, .b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},

		/* WB w25q256fv 32B flash */ 
		{
			.jedec_id = 0xef4019,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,
			.b_srp1 = 8, .b_qe = 9,	.b_lb = 11,	.b_cmp = 14,
			.b_sus = 15, .b_as =16,  .b_ap = 17,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},

		/*spiflash FM25Q64*/
		{
			.jedec_id = 0xa14017,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_srp0 = 7,
			
			.b_srp1 = 8,.b_qe = 9,	.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},		
		/*spiflash FM25Q128*/
		{
			.jedec_id = 0xa14018,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_srp0 = 7,
			
			.b_srp1 = 8,.b_qe = 9,	.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},
		/* by25q128 */                                                                                 
        {   
            .jedec_id = 0x684018,   .ext_id = 0,
            .b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
            .b_bp2 = 4, .b_bp3 = 5, .b_bp4 = 6, .b_srp0 = 7,
            .b_srp1 = 8,.b_qe = 9,  .b_lb = 10, .b_cmp = 14, 
            .b_sus = 15, 
            .read_sr = spi_flash_cmd_read_status,
            .write_sr = spi_flash_gd25q128c_cmd_write_status,
        },
		/* by25q64 */
        {
            .jedec_id = 0x684017,   .ext_id = 0,
            .b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
            .b_bp2 = 4, .b_bp3 = 5, .b_bp4 = 6, .b_srp0 = 7,
            .b_srp1 = 8,.b_qe = 9,  .b_lb = 10, .b_cmp = 14, 
            .b_sus = 15,
            .read_sr = spi_flash_cmd_read_status,
            .write_sr = spi_flash_gd25q128c_cmd_write_status,
        },
 		/* bh25q128as */
        {
            .jedec_id = 0x684018,   .ext_id = 0,
            .b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
            .b_bp2 = 4, .b_bp3 = 5, .b_bp4 = 6, .b_srp0 = 7,
            .b_srp1 = 8,.b_qe = 9,  .b_lb = 10, .b_cmp = 14, 
            .b_sus = 15,
            .read_sr = spi_flash_cmd_read_status,
            .write_sr = spi_flash_gd25q128c_cmd_write_status,
        },
 		/* bh25q64as */
        {
            .jedec_id = 0x684017,   .ext_id = 0,
            .b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
            .b_bp2 = 4, .b_bp3 = 5, .b_bp4 = 6, .b_srp0 = 7,
            .b_srp1 = 8,.b_qe = 9,  .b_lb = 10, .b_cmp = 14, 
            .b_sus = 15,
            .read_sr = spi_flash_cmd_read_status,
            .write_sr = spi_flash_gd25q128c_cmd_write_status,
        },
        /* ZB25VQ32 */
		{
			.jedec_id = 0x5e4016,	.ext_id = 0,
			.b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
			.b_bp2 = 4, .b_srp0 = 7,
			.b_srp1 = 8,.b_qe = 9, .b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},

		/* ZB25VQ64 */
		{
			.jedec_id = 0x5e4017,	.ext_id = 0,
			.b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
			.b_bp2 = 4, .b_srp0 = 7,
			.b_srp1 = 8,.b_qe = 9, .b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},

		/* ZB25VQ128 */
		{
			.jedec_id = 0x5e4018,	.ext_id = 0,
			.b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
			.b_bp2 = 4, .b_srp0 = 7,
			.b_srp1 = 8,.b_qe = 9, .b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},

		/* XT25F64B */
		{
			.jedec_id = 0x0b4017,	.ext_id = 0,
			.b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
			.b_bp2 = 4, .b_bp3 = 5, .b_bp4 = 6, .b_srp0 = 7,
			.b_srp1 = 8,.b_qe = 9,	.b_lb = 10, .b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_cmd_write_status,
		},
		/* XT25F128B */
		 {
			.jedec_id = 0x0b4018,	.ext_id = 0,
			.b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
			.b_bp2 = 4, .b_bp3 = 5, .b_bp4 = 6, .b_srp0 = 7,
			.b_srp1 = 8,.b_qe = 9,	.b_lb = 10, .b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_cmd_write_status,
		},

		/* NM25Q64EVB */
		{
			.jedec_id = 0x522217,	.ext_id = 0,
			.b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
			.b_bp2 = 4, .b_bp3 = 5, .b_bp4 = 6, 
			.b_cmp = 7, 
			.b_srp0 = 8, .b_srp1 = 9,.b_qe = 10,
			
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},
		/* NM25Q128EVB */
		 {
			.jedec_id = 0x522118,	.ext_id = 0,
			.b_wip = 0, .b_wel = 1, .b_bp0 = 2, .b_bp1 = 3,
			.b_bp2 = 4, .b_bp3 = 5, .b_bp4 = 6, 
			.b_cmp = 7, 
			.b_srp0 = 8, .b_srp1 = 9,.b_qe = 10,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_cmd_write_status,
		},
		/* normal status reg define */
		{
			.jedec_id = 0,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,
			.b_srp1 = 8,.b_qe = 9,	.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
			.read_sr = spi_flash_cmd_read_status,
			.write_sr = spi_flash_gd25q128c_cmd_write_status,
		},
};


static const struct spi_flash_params spi_flash_params_table[] = {
#ifdef CONFIG_SPI_FLASH_ANYKA

	/* Atmel -- some are (confusingly) marketed as "DataFlash" */
	{ "at25fs010",  0x1f6601, 0, 32 * 1024, 4, SECT_4K, 0,},
	{ "at25fs040",  0x1f6604, 0, 64 * 1024, 8, SECT_4K, 0,},

	{ "at25df041a", 0x1f4401, 0, 64 * 1024, 8, SECT_4K, 0,},
	{ "at25df641",  0x1f4800, 0, 64 * 1024, 128, SECT_4K, 0,},

	{ "at26f004",   0x1f0400, 0, 64 * 1024, 8, SECT_4K, 0,},
	{ "at26df081a", 0x1f4501, 0, 64 * 1024, 16, SECT_4K, 0,},
	{ "at26df161a", 0x1f4601, 0, 64 * 1024, 32, SECT_4K, 0,},
	{ "at26df321",  0x1f4701, 0, 64 * 1024, 64, SECT_4K, 0,},

	/* Macronix */
	{ "mx25l3205d", 0xc22016, 0, 64 * 1024, 64, SECT_4K | SFLAG_DUAL_READ, 0,},
	{ "kh25l64", 0xc22017, 0, 64 * 1024, 128, SECT_4K | SFLAG_DUAL_READ, 0,},//check ok
	{ "kh25l128", 0xc22018, 0, 64 * 1024, 256, SECT_4K | SFLAG_DUAL_READ, 0,},//check ok
	{ "mx25l12855e", 0xc22618, 0, 64 * 1024, 256, SECT_4K | SFLAG_DUAL_READ, 0,},

	/* Spansion -- single (large) sector size only, at least
	 * for the chips listed here (without boot sectors).
	 */
	{ "s25sl004a", 0x010212, 0, 64 * 1024, 8, SECT_4K, 0,},
	{ "s25sl008a", 0x010213, 0, 64 * 1024, 16, SECT_4K, 0,},
	{ "s25sl016a", 0x010214, 0, 64 * 1024, 32, SECT_4K, 0,},
	{ "s25sl032a", 0x010215, 0, 64 * 1024, 64, SECT_4K, 0,},
	{ "s25sl064a", 0x010216, 0, 64 * 1024, 128, SECT_4K, 0,},
	{ "s25sl12800", 0x012018, 0x0300, 256 * 1024, 64, SECT_4K, 0,},
	{ "s25sl12801", 0x012018, 0x0301, 64 * 1024, 256, SECT_4K, 0,},
	{ "s25fl129p0", 0x012018, 0x4d00, 256 * 1024, 64, SECT_4K, 0,},
	{ "s25fl129p1", 0x012018, 0x4d01, 64 * 1024, 256, SECT_4K, 0,},

	/* SST -- large erase sizes are "overlays", "sectors" are 4K */
	{ "sst25vf040b", 0xbf258d, 0, 64 * 1024, 8, SECT_4K, 0,},
	{ "sst25vf080b", 0xbf258e, 0, 64 * 1024, 16, SECT_4K, 0,},
	{ "sst25vf016b", 0xbf2541, 0, 64 * 1024, 32, SECT_4K, 0,},
	{ "sst25vf032b", 0xbf254a, 0, 64 * 1024, 64, SECT_4K, 0,},
	{ "sst25wf512",  0xbf2501, 0, 64 * 1024, 1, SECT_4K, 0,},
	{ "sst25wf010",  0xbf2502, 0, 64 * 1024, 2, SECT_4K, 0,},
	{ "sst25wf020",  0xbf2503, 0, 64 * 1024, 4, SECT_4K, 0,},
	{ "sst25wf040",  0xbf2504, 0, 64 * 1024, 8, SECT_4K, 0,},

	/* ST Microelectronics -- newer production may have feature updates */
	{ "m25p05",  0x202010,  0, 32 * 1024, 2, SECT_4K, 0,},
	{ "m25p10",  0x202011,  0, 32 * 1024, 4, SECT_4K, 0,},
	{ "m25p20",  0x202012,  0, 64 * 1024, 4, SECT_4K, 0,},
	{ "m25p40",  0x202013,  0, 64 * 1024, 8, SECT_4K, 0,},
	{ "m25p80",         0,  0, 64 * 1024, 16, SECT_4K, 0,},
	{ "m25p16",  0x202015,  0, 64 * 1024, 32, SECT_4K, 0,},
	{ "m25p32",  0x202016,  0, 64 * 1024, 64, SECT_4K, 0,},
	{ "m25p64",  0x202017,  0, 64 * 1024, 128, SECT_4K, 0,},
	{ "m25p128", 0x202018, 0, 256 * 1024, 64, SECT_4K, 0,},

	{ "m45pe10", 0x204011,  0, 64 * 1024, 2, SECT_4K, 0,},
	{ "m45pe80", 0x204014,  0, 64 * 1024, 16, SECT_4K, 0,},
	{ "m45pe16", 0x204015,  0, 64 * 1024, 32, SECT_4K, 0,},

	{ "m25pe80", 0x208014,  0, 64 * 1024, 16, SECT_4K, 0,},
	{ "m25pe16", 0x208015,  0, 64 * 1024, 32, SECT_4K, 0,},

	/* Winbond -- w25x "blocks" are 64K, "sectors" are 4KiB */
	{ "w25x10", 0xef3011, 0, 64 * 1024, 2, SECT_4K, 0,},
	{ "w25x20", 0xef3012, 0, 64 * 1024, 4, SECT_4K, 0,},
	{ "w25x40", 0xef3013, 0, 64 * 1024, 8, SECT_4K, 0,},
	{ "w25x80", 0xef3014, 0, 64 * 1024, 16, SECT_4K, 0,},
	{ "w25x16", 0xef3015, 0, 64 * 1024, 32, SECT_4K, 0,},
	{ "w25x32", 0xef3016, 0, 64 * 1024, 64, SECT_4K, 0,},
	{ "w25x64", 0xef3017, 0, 64 * 1024, 128, SECT_4K, 0,},
	
	//{ "w25q32", 0xef4016, 0, 32 * 1024, 128, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE, 0,},
	{ "w25q64fv", 0xef4017, 0, 64 * 1024, 128, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE, 0,},
	{ "w25q64jv", 0xef4016, 0, 64 * 1024, 128, SECT_4K|SFLAG_COM_STATUS3|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE, 0,},
	{ "w25q128", 0xef4018, 0, 64 * 1024, 256,  SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE, 0,},

	/* GigaDevice -- w25x "blocks" are 64K, "sectors" are 4KiB */
	{ "gd25q64b", 0xc84017, 0, 64 * 1024, 128, SECT_4K|SFLAG_COM_STATUS2|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE, 0,},
	{ "gd25q64c", 0xc84017, 0, 64 * 1024, 128, SECT_4K|SFLAG_COM_STATUS2|SFLAG_COM_STATUS3|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE, 0x53},
	{ "gd25q128", 0xc84018, 0, 64 * 1024, 256, SECT_4K|SFLAG_COM_STATUS2|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE, 0,},
	{ "wb25q256", 0xef4019, 0, 64 * 1024, 512, SFLAG_ADDR_4B|SECT_4K|SFLAG_COM_STATUS2|SFLAG_COM_STATUS3, 0,},
    {"gd25q256", 0xc84019, 0, 64 * 1024, 512, SFLAG_ADDR_4B|SECT_4K|SFLAG_COM_STATUS2|SFLAG_COM_STATUS3|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE, 0,},
	//{ "s25fl256s", 0x010219, 0, 64 * 1024, 512, SFLAG_COM_STATUS2|SFLAG_COM_STATUS4|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE , 0,},
	
	/* XMC -- xm25qh64a "blocks" are 64K, "sectors" are 4KiB */
	{ "xm25qh64a", 0x207017, 0, 64 * 1024, 128, SFLAG_QUAD_NO_QE|SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE , 0,},
	{ "xm25qh64b", 0x206017, 0, 64 * 1024, 128, SFLAG_QUAD_NO_QE|SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE , 0,},
	{ "xm25qh64c", 0x204017, 0, 64 * 1024, 128, SFLAG_QUAD_NO_QE|SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE , 0,},
	{ "xm25qh128a", 0x207018, 0, 64 * 1024, 256, SFLAG_QUAD_NO_QE|SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE , 0,},
	{ "xm25qh128b", 0x206018, 0, 64 * 1024, 256, SFLAG_QUAD_NO_QE|SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE , 0,},
	{ "xm25qh128c", 0x204018, 0, 64 * 1024, 256, SFLAG_QUAD_NO_QE|SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE , 0,},
	{ "PN26f64B",  0x1c7017, 0, 64 * 1024, 128, SFLAG_QUAD_NO_QE|SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE, 0,},
	{ "xm25qh64c", 0x204017, 0, 64 * 1024, 128, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE , 0,},
	{ "xm25qh128c", 0x204018, 0, 64 * 1024, 256, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE , 0,},

	/* BOYA  BY25Q64ASSIG -- "blocks" are 64K, "sectors" are 4KiB */
   { "BY25Q64ASSIG",  0x684017, 0, 64 * 1024, 128, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE,},
   { "BY25Q128ASSIG",  0x684018, 0, 64 * 1024, 256, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE,},

    /* FM25Q64  FM25Q128  "blocks" are 64K, "sectors" are 4KiB */
    { "FM25Q64",  0xa14017, 0, 64 * 1024, 128, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE,},
    { "FM25Q128",  0xa14018, 0, 64 * 1024, 256, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE,},
   	
   /* ZB25VQ32  "blocks" are 64K, "sectors" are 4KiB */
   { "ZB25VQ32",  0x5e4016, 0, 64 * 1024, 64, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE,},

   /* ZB25VQ64  "blocks" are 64K, "sectors" are 4KiB */
   { "ZB25VQ64",  0x5e4017, 0, 64 * 1024, 128, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE,},

   /* ZB25VQ128  "blocks" are 64K, "sectors" are 4KiB */
   { "ZB25VQ128",  0x5e4018, 0, 64 * 1024, 256, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE,},


   /* XT25F64B  "blocks" are 64K, "sectors" are 4KiB */
   { "XT25F64B",  0x0b4017, 0, 64 * 1024, 128, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE,},

   /* XT25F128B  "blocks" are 64K, "sectors" are 4KiB */
   { "XT25F128B",  0x0b4018, 0, 64 * 1024, 256, SECT_4K|SFLAG_COM_STATUS2|SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_DUAL_IO_READ|SFLAG_QUAD_IO_READ|SFLAG_QUAD_WRITE,},

	
	{ "nm25q64evb", 0x522217, 0, 64 * 1024, 128, SECT_4K|SFLAG_COM_STATUS2|SFLAG_COM_STATUS3|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE, 0},
	{ "nm25q128evb", 0x522118, 0, 64 * 1024, 256, SECT_4K|SFLAG_COM_STATUS2|SFLAG_COM_STATUS3|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE, 0,},
	
	{ "en25", 0x1c7016, 0, 64 * 1024, 64, SECT_4K, 0,},
#endif

};


/* cdh:check ok 2016.10.17 */
static int init_spiflash_rw_info(struct spi_flash *flash,
	   	const struct spi_flash_params *params)
{
	struct spi_xfer_ctl *xfer_ctl = &flash->xfer_ctl;

	/**default param.*/
	xfer_ctl->rx_opcode = OPCODE_READ;
	xfer_ctl->rxd_bus_width = SPI_XFER_1DATAWIRE;
	xfer_ctl->rxa_bus_width = SPI_XFER_1DATAWIRE;
	xfer_ctl->tx_opcode = CMD_PAGE_PROGRAM;
	xfer_ctl->txd_bus_width = SPI_XFER_1DATAWIRE;
	xfer_ctl->txa_bus_width = SPI_XFER_1DATAWIRE;

	/** 
	* normal slow read cmd=0x03, dummy_len=0
	* normal fast read cmd= 0x0B, dummy_len=1
	*/
	xfer_ctl->dummy_len = FAST_READ_DUMMY_BYTE;
	
	if(flash->bus_width & FLASH_BUS_WIDTH_2WIRE){
		if(params->flags & SFLAG_DUAL_READ) {
			xfer_ctl->rx_opcode = CMD_READ_ARRAY_DUAL;
			xfer_ctl->rxd_bus_width = SPI_XFER_2DATAWIRE;
			xfer_ctl->rxa_bus_width = SPI_XFER_1DATAWIRE;
			xfer_ctl->dummy_len = 1; // cdh:check ok
		} else if (params->flags & SFLAG_DUAL_IO_READ) {
			xfer_ctl->rx_opcode = CMD_READ_ARRAY_DUAL_IO;
			xfer_ctl->rxd_bus_width = SPI_XFER_2DATAWIRE;
			xfer_ctl->rxa_bus_width = SPI_XFER_2DATAWIRE;
			xfer_ctl->dummy_len = 1; // cdh:check ok
		}

		if(params->flags & SFLAG_DUAL_WRITE) {
			xfer_ctl->tx_opcode = CMD_PAGE_PROGRAM_DUAL; // cdh:not support dual write
			xfer_ctl->txd_bus_width = SPI_XFER_2DATAWIRE;
			xfer_ctl->txa_bus_width = SPI_XFER_1DATAWIRE;
		} else if(params->flags & SFLAG_DUAL_IO_WRITE) {
			xfer_ctl->tx_opcode = CMD_PAGE_PROGRAM_2IO;
			xfer_ctl->txd_bus_width = SPI_XFER_2DATAWIRE;
			xfer_ctl->txa_bus_width = SPI_XFER_2DATAWIRE;
		}	
	}

	if(flash->bus_width & FLASH_BUS_WIDTH_4WIRE){
		if(params->flags & SFLAG_QUAD_READ) {
			xfer_ctl->rx_opcode = CMD_READ_ARRAY_QUAD;
			xfer_ctl->rxd_bus_width = SPI_XFER_4DATAWIRE;
			xfer_ctl->rxa_bus_width = SPI_XFER_1DATAWIRE;
			xfer_ctl->dummy_len = 1; // cdh:check ok
		}else if(params->flags & SFLAG_QUAD_IO_READ){
			xfer_ctl->rx_opcode = CMD_READ_ARRAY_QUAD_IO;
			xfer_ctl->rxd_bus_width = SPI_XFER_4DATAWIRE;
			xfer_ctl->rxa_bus_width = SPI_XFER_4DATAWIRE;
			xfer_ctl->dummy_len = 3; // cdh:modify from winbond 32MB spi norflash
		}

		if(params->flags & SFLAG_QUAD_WRITE) {
			xfer_ctl->tx_opcode = CMD_PAGE_PROGRAM_QUAD;  // cdh:check ok
			xfer_ctl->txd_bus_width = SPI_XFER_4DATAWIRE;			
			xfer_ctl->txa_bus_width = SPI_XFER_1DATAWIRE;
		}else if(params->flags & SFLAG_QUAD_IO_WRITE) {  
			xfer_ctl->tx_opcode = CMD_PAGE_PROGRAM_4IO;  // cdh:not support dual write
			xfer_ctl->txd_bus_width = SPI_XFER_4DATAWIRE;
			xfer_ctl->txa_bus_width = SPI_XFER_4DATAWIRE;
		}
	}
	return 0;
}

/*
 * memstr - Find the first substring in memory
 * @s1: The string to be searched
 * @s2: The string to search for
 *
 * Similar to and based on strstr(),
 * but strings do not need to be NUL terminated.
 */
 #if 0
static char *memstr(const char *s1, int l1, const char *s2, int l2)
{
	if (!l2)
		return (char *)s1;

	while (l1 >= l2) {
		l1--;
		if (!memcmp(s1, s2, l2))
			return (char *)s1;
		s1++;
	}
	return NULL;
}
#endif
static struct spi_flash *spi_flash_validate_params(struct spi_slave *spi, u8 *idcode)
{
	struct flash_status_reg *sr;
	const struct spi_flash_params *params;
	struct spi_flash *flash;
	int i;
	u16 jedec = idcode[1] << 8 | idcode[2];
	u16 ext_jedec = idcode[3] << 8 | idcode[4];
	int ret;
	u32 id_ver_c = 0;
	u8	code[5] = {0x5a,0x00,0x00,0x00,0x00}; //OPCODE_VER; 
	u8	id[7];
    
	/* Get the flash id (jedec = manuf_id + dev_id, ext_jedec) */
	for (i=0; i<ARRAY_SIZE(spi_flash_params_table); i++) {
		params = &spi_flash_params_table[i];
		if ((params->jedec >> 16) == idcode[0]) {
			if ((params->jedec & 0xFFFF) == jedec) {
				/*
				  ����gd25q64 ��B�汾��C �汾��Ȼ�������汾��״̬�Ĵ�����д��һ����������˸Ķ���
				*/
				if (params->jedec == 0xc84017) {
					ret = spi_flash_cmd_read(spi, code, 5, id, 7);
					if (ret) {
						printf("error reading JEDEC VERSION\n");
						return NULL;
					}
				
					id_ver_c = id[0]; //0x53
					if (id[0] != 0x53) {
						id_ver_c = 0x0;
					}
					
					//printf("akspi flash VERSION: 0x%x\n", id_ver_c);	
				}
				
				if ((params->ext_jedec == 0)&&(params->id_version == id_ver_c)) {
					//printf("SF: supported flash name:%s, IDs:0x%x, id_version=0x%x\n", params->name, params->jedec, params->id_version);
					break;
				}else if ((params->ext_jedec == ext_jedec)&&(params->id_version == id_ver_c)) {
					//printf("SF: supported flash name:%s, extIDs:0x%x, id_version=0x%x\n", params->name, params->ext_jedec, params->id_version);
					break;
				}
			}
		}
	}

	if (i == ARRAY_SIZE(spi_flash_params_table)) {
		printf("SF: Unsupported flash IDs: ");
		printf("manuf %02x, jedec %04x, ext_jedec %04x\n",
			   idcode[0], jedec, ext_jedec);
		return NULL;
	}

	
	flash = malloc(sizeof(*flash));
	if (!flash) {
		debug("SF: Failed to allocate spi_flash\n");
		return NULL;
	}
	memset(flash, '\0', sizeof(*flash));

	/* Assign spi data */
	flash->spi = spi;
	flash->name = params->name;
	flash->memory_map = spi->memory_map;
	flash->bus_width = spi->bus_width;
	flash->flags = params->flags;
	flash->flashid = params->jedec;
	
	for (i=0; i<ARRAY_SIZE(status_reg_list); i++) {
		sr = &status_reg_list[i];
		if (sr->jedec_id == params->jedec) {
			//printf("SF: status_reg_list supported flash IDs:0x%x\n", params->jedec);
			break;
		}
	}

	flash->stat_reg = *sr;

#ifdef CONFIG_SPI_FLASH_BUSWIDTH_4X
	flash->bus_width = FLASH_BUS_WIDTH_4WIRE;
	debug("anyka spi flash. bus_width:4\n");
#else
	flash->bus_width = FLASH_BUS_WIDTH_1WIRE;
	debug("anyka spi flash. bus_width:1\n");
#endif

	/* Assign spi_flash ops */
#ifdef CONFIG_SPI_FLASH_ANYKA
	flash->write = spi_flash_cmd_write_ops_extra;
#else
	flash->write = spi_flash_cmd_write_ops;
#endif

#ifdef CONFIG_SPI_FLASH_SST
	if (params->flags & SST_WP)
		flash->write = sst_write_wp;
#endif
	flash->erase = spi_flash_cmd_erase_ops;

#ifdef CONFIG_SPI_FLASH_ANYKA
	flash->read = spi_flash_cmd_read_ops_extra;
#else
	flash->read = spi_flash_cmd_read_ops;
#endif
	//flash->read = spi_flash_cmd_read_ops_extra;

	/* Compute the flash size */
	flash->page_size = (ext_jedec == 0x4d00) ? 512 : 256;
	flash->sector_size = params->sector_size;
	flash->size = flash->sector_size * params->nr_sectors;

	/* Compute erase sector and command */
	if (params->flags & SECT_4K) {
		flash->erase_cmd = CMD_ERASE_4K;
		flash->erase_size = 4096;
	} else if (params->flags & SECT_32K) {
		flash->erase_cmd = CMD_ERASE_32K;
		flash->erase_size = 32768;
	} else {
		flash->erase_cmd = CMD_ERASE_64K;
		flash->erase_size = flash->sector_size;
	}
	//flash->erasechip_cmd = CMD_ERASE_CHIP;

	/* Poll cmd seclection */
	flash->poll_cmd = CMD_READ_STATUS;
	
#ifdef CONFIG_SPI_FLASH_STMICRO
	if (params->flags & E_FSR)
		flash->poll_cmd = CMD_FLAG_STATUS;
#endif

	/* Configure the BAR - discover bank cmds and read current bank */
#ifdef CONFIG_SPI_FLASH_BAR
	u8 curr_bank = 0;
	if (flash->size > SPI_FLASH_16MB_BOUN) {
		flash->bank_read_cmd  = (idcode[0] == 0x01) ? CMD_BANKADDR_BRRD : CMD_EXTNADDR_RDEAR;
		flash->bank_write_cmd = (idcode[0] == 0x01) ? CMD_BANKADDR_BRWR : CMD_EXTNADDR_WREAR;

		debug("anyka spi flash. CONFIG_SPI_FLASH_BAR:bank_rd_cmd:0x%x\n", flash->bank_read_cmd);
		if (spi_flash_read_common(flash, &flash->bank_read_cmd, 1, &curr_bank, 1)) {
			debug("SF: fail to read bank addr register\n");
			return NULL;
		}
		
		flash->bank_curr = curr_bank;
	} else {
		flash->bank_curr = curr_bank;
	}

	debug("flash->bank_curr:0x%x\n", flash->bank_curr);
#endif

	/* Flash powers up read-only, so clear BP# bits */
#if defined(CONFIG_SPI_FLASH_ATMEL) || \
	defined(CONFIG_SPI_FLASH_MACRONIX) || \
	defined(CONFIG_SPI_FLASH_SST)
		spi_flash_cmd_write_status(flash, 0);
#endif

	// cdh:intial spi operation data wire and cmd
	init_spiflash_rw_info(flash, params);

    if(flash_QE_flag == 0){
        flash_QE_flag = 1;
	if((flash->bus_width & FLASH_BUS_WIDTH_4WIRE) && 
		(params->flags & (SFLAG_QUAD_WRITE|SFLAG_QUAD_IO_WRITE|SFLAG_DUAL_READ|SFLAG_DUAL_IO_READ))) {		
		debug("config spiflash to quad mode.\n");
		int ret = 0;
		
		if((params->flags & SFLAG_QUAD_NO_QE) != SFLAG_QUAD_NO_QE){
			ret = spi_flash_quad_mode_enable(flash);
			if(ret) {
				flash->bus_width &= ~FLASH_BUS_WIDTH_4WIRE;
				printf("config spiflash quad mode fail. transfer use 1 wire.\n");
			}
		}
	}else {
		if(params->flags & (SFLAG_QUAD_WRITE|SFLAG_QUAD_IO_WRITE|SFLAG_DUAL_READ|SFLAG_DUAL_IO_READ)){
		 	if((params->flags & SFLAG_QUAD_NO_QE) != SFLAG_QUAD_NO_QE){
                spi_flash_quad_mode_enable(flash);
            }
		}
	}
    
    }

//#ifdef CONFIG_PARTITION_TABLE_BY_CUSTOMER_ENV
	sflash_mtd.name = "nor0";
	sflash_mtd.type = MTD_DATAFLASH;
	sflash_mtd.writesize = flash->page_size;
	sflash_mtd.size = flash->size;
	sflash_mtd.erasesize = flash->erase_size;
	sflash_mtd.numeraseregions = 0;
	add_mtd_device(&sflash_mtd);
//#endif
	show_spi_flash_reg(flash);
	debug("spi_flash_validate_params finish.\n");
	return flash;
}


#if 0 //def 0 //CONFIG_OF_CONTROL
int spi_flash_decode_fdt(const void *blob, struct spi_flash *flash)
{
	fdt_addr_t addr;
	fdt_size_t size;
	int node;

	/* If there is no node, do nothing */
	node = fdtdec_next_compatible(blob, 0, COMPAT_GENERIC_SPI_FLASH);
	if (node < 0)
		return 0;

	addr = fdtdec_get_addr_size(blob, node, "memory-map", &size);
	if (addr == FDT_ADDR_T_NONE) {
		debug("%s: Cannot decode address\n", __func__);
		return 0;
	}

	if (flash->size != size) {
		debug("%s: Memory map must cover entire device\n", __func__);
		return -1;
	}
	flash->memory_map = (void *)addr;

	return 0;
}
#endif /* CONFIG_OF_CONTROL */

struct spi_flash *spi_flash_probe(unsigned int bus, unsigned int cs,
		unsigned int max_hz, unsigned int spi_mode)
{
	struct spi_slave *spi;
	struct spi_flash *flash = NULL;
	u8 idcode[5];
	u8 cmd;
	int wr_cnt;
	int ret;
	u32 regval;	
	u32 value = 0;
	u32 i = 0;
	
	/* Setup spi_slave */
	spi = spi_setup_slave(bus, cs, max_hz, spi_mode);
	if (!spi) {
		printf("SF: Failed to set up slave\n");
		return NULL;
	}

	/* Claim spi bus */
	ret = spi_claim_bus(spi);
	if (ret) {
		printf("SF: Failed to claim SPI bus: %d\n", ret);
		goto err_claim_bus;
	}

	/* Read the ID codes */
	ret = spi_flash_cmd(spi, CMD_READ_ID, idcode, sizeof(idcode));
	if (ret) {
		printf("SF: Failed to get idcodes\n");
		goto err_read_id;
	}

#ifdef DEBUG
	printf("SF: Got idcodes\n");
	print_buffer(0, idcode, 1, sizeof(idcode), 0);
#endif

	/* Validate params from spi_flash_params table */
	flash = spi_flash_validate_params(spi, idcode);
	if (!flash)
		goto err_read_id;
        
#if 0 //def 0 //CONFIG_OF_CONTROL
	if (spi_flash_decode_fdt(gd->fdt_blob, flash)) {
		debug("SF: FDT decode error\n");
		goto err_read_id;
	}
#endif

//#ifndef CONFIG_SPL_BUILD
#if 0
	printf("SF: Detected %s with page size ", flash->name);
	print_size(flash->page_size, ", erase size ");
	print_size(flash->erase_size, ", total ");
	print_size(flash->size, "");
	if (flash->memory_map)
		printf(", mapped at %p", flash->memory_map);
	puts("\n");
#endif

#ifndef CONFIG_SPI_FLASH_BAR
	if (flash->size > SPI_FLASH_16MB_BOUN) {
		puts("SF: Warning - Only lower 16MiB accessible\n");
		puts(" Full access #define CONFIG_SPI_FLASH_BAR\n");
	}
#else

	if (flash->size > SPI_FLASH_16MB_BOUN) {
#ifdef CMD_ADDR_4BYTE_MODE
		if (flash->flags & SFLAG_ADDR_4B) {
			cmd = CMD_ADDR_4BYTE_MODE;
			ret = spi_flash_write_common(flash, &cmd, 1, NULL, 0);
			if (ret < 0) {
				printf("SF: fail to write 4byte addr register\n");
				goto err_read_id;
			}
			printf("SF: 4byte addr mode\n");
		}
#else
        /* add check ADP addres mode for notes flash size beyond 16MB, must use 3B mode+Extend or 4B address mode */
		puts("SF: Warning - Fash size Beyond 16MiB accessible\n");
		if (flash->flags & SFLAG_ADDR_4B) {
			struct flash_status_reg *fsr = &flash->stat_reg;

			ret = spi_flash_cmd_read_status(flash, &regval);
			if (ret < 0) {
				printf("SF: fail to read status register\n");
				goto err_read_id;
			}

			if (regval&(0x1<<fsr->b_ap)){
				printf("power on spi norflash 4B address mode, must switch to 3B mode!\n");
				/* switch ADP enter 3B address mode */
				regval &= ~(1<<fsr->b_ap);
				cmd = CMD_WRITE_STATUS3;
				wr_cnt = 2;
				value = (regval>>16) &0xff;
				ret = spi_flash_write_common(flash, &cmd, 1, &value, wr_cnt);
				if (ret < 0) {
					printf("SF: fail to write status register\n");
					goto err_read_id;
				}

				/* cdh:software reset spi norflash enter 3B address mode */
				cmd = CMD_ENABLE_RESET;
				ret = spi_flash_write_common(flash, &cmd, 1, NULL, 0);
				if (ret < 0) {
					printf("SF: fail to write status register\n");
					goto err_read_id;
				}

				cmd = CMD_RESET_DEVICE;
				ret = spi_flash_write_common(flash, &cmd, 1, NULL, 0);
				if (ret < 0) {
					printf("SF: fail to write status register\n");
					goto err_read_id;
				}

				/* cdh:for delay 30us for reset device complete */
				for (i=0; i<3000; i++) {
					;
				}
			}else {
				printf("power on spi norflash 3B address mode!\n");
			}

		}
#endif
	}
#endif

	/* Release spi bus */
	spi_release_bus(spi);

	return flash;

err_read_id:
	spi_release_bus(spi);
err_claim_bus:
	spi_free_slave(spi);
	return NULL;
}

void spi_flash_free(struct spi_flash *flash)
{
	spi_free_slave(flash->spi);
	free(flash);
}


