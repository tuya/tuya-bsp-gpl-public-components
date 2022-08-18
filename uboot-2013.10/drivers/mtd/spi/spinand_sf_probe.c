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

//#include <common.h>
#include <command.h>
#include <s_record.h>
#include <net.h>
#include <exports.h>
#include <xyzModem.h>
//#ifdef CONFIG_PARTITION_TABLE_BY_CUSTOMER_ENV
#include <linux/mtd/mtd.h>
extern struct mtd_info sflash_mtd;
//#endif

#define SPI_FLASH_COM_RDID        0x9f
#define SPI_FLASH_COM_READ        0x03
#define SPI_FLASH_COM_PROGRAM     0x02
#define SPI_FLASH_COM_WR_EN       0x06

#define SPI_NAND_READ_ID            0x9F

#define SPI_NAND_WRITE_ENABLE       0x06
#define SPI_NAND_WRITE_DISABLE      0x04

#define SPI_NAND_SET_FEATURE        0x1F
#define SPI_NAND_GET_FEATURE        0x0F

#define SPI_NAND_PAGE_READ          0x13
#define SPI_NAND_READ_CACHE         0x03
#define SPI_NAND_READ_CACHE_X2      0x3B
#define SPI_NAND_READ_CACHE_X4      0x6B

#define SPI_NAND_PROGRAM_LOAD       0x02
#define SPI_NAND_PROGRAM_LOAD_X4    0x32
#define SPI_NAND_PROGRAM_EXECUTE    0x10

#define SPI_NAND_BLOCK_ERASE        0xD8


#define ADDR_PROTECTION 0xA0
#define ADDR_FEATURE    0xB0
#define ADDR_STATUS     0xC0



#define STATUS_ECCS1    (1<<5)
#define STATUS_ECCS0    (1<<4)
#define STATUS_P_FAIL   (1<<3)
#define STATUS_E_FAIL   (1<<2)
#define STATUS_OIP      (1<<0)

#define MAX_RD_SIZE (32*1024)

static int feature_cmd[3] = {0xC0, 0xB0, 0xA0};

#define FEATURE_OTP_EN  (1<<6)
#define FEATURE_ECC_EN  (1<<4)  //(1<<5)


int  spinand_is_badblock(struct spinand_flash *flash, unsigned long offset);
int  spinand_isbadblock(struct spinand_flash *flash, u32 block);
int  spinand_setbadblock(struct spinand_flash *flash, u32 offset);


extern u32 part_num;



DECLARE_GLOBAL_DATA_PTR;


/*
 * SPI device driver setup and teardown
 */
struct spinand_flash_info {
	char		*name;

	/* JEDEC id zero means "no ID" (most older chips); otherwise it has
	 * a high byte of zero plus three data bytes: the manufacturer id,
	 * then a two byte device id.
	 */
	u32			jedec_id;
	u16			ext_id;
	u16			planecnt;

	u32 		page_size;
	u32 		page_per_block;

	/* The size listed here is what works with OPCODE_SE, which isn't
	 * necessarily called a "sector" by the vendor.
	 */
	unsigned	block_size;
	u16			n_blocks;

	/*|--------------------64bytes------------------------------|*/
	/*|---12---|-4--|---12----|-4--|---12---|-4--|---12----|-4--|*/
	/*|-seglen-|skip|-segllen-|skip|-seglen-|skip|-segllen-|skip|*/
	u32 		oob_size;        
	u16 		oob_up_skiplen;
	u16 		oob_seglen;
	u16 		oob_down_skiplen;	
	u16 		oob_seg_perpage;
	u16			oob_vail_data_offset;

	/**
	 *  chip character bits:
	 *  bit 0: under_protect flag, the serial flash under protection or not when power on
	 *  bit 1: fast read flag, the serial flash support fast read or not(command 0Bh)
	 *  bit 2: AAI flag, the serial flash support auto address increment word programming
	 *  bit 3: support dual write or no
	 *  bit 4: support dual read or no
	 *  bit 5: support quad write or no
	 *  bit 6: support quad read or no
	 *  bit 7: the second status command (35h) flag,if use 4-wire(quad) mode,the bit must be is enable
	 */
	u16			flags;
#if 0
#define	SFLAG_UNDER_PROTECT			(1<<0)
#define SFLAG_FAST_READ           	(1<<1)
#define SFLAG_AAAI                	(1<<2)
#define SFLAG_COM_STATUS2         	(1<<3)

#define SFLAG_DUAL_IO_READ         	(1<<4)
#define SFLAG_DUAL_READ           	(1<<5)
#define SFLAG_QUAD_IO_READ         	(1<<6)
#define SFLAG_QUAD_READ           	(1<<7)

#define SFLAG_DUAL_IO_WRITE        	(1<<8)
#define SFLAG_DUAL_WRITE          	(1<<9)
#define SFLAG_QUAD_IO_WRITE        	(1<<10)
#define SFLAG_QUAD_WRITE          	(1<<11)

#define SFLAG_SECT_4K       		(1<<12)
#endif

};




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
u8 g_spinand_init_flag = 0;


static 	struct spinand_flash_status_reg status_reg_list[] = {
		{
			.jedec_id = 0xc8f4c8f4,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_efail= 2,	.b_pfail= 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,

			.b_qe = 8,	.b_srp1 = 9,.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},
		/*normal status reg define*/
		{
			.jedec_id = 0xc8f2c8f2,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_efail= 2,	.b_pfail= 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,

			.b_qe = 8,	.b_srp1 = 9,.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},
		/*normal status reg define*/
		{
			.jedec_id = 0xc8f1c8f1,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_efail= 2,	.b_pfail= 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,

			.b_qe = 8,	.b_srp1 = 9,.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},
		/*normal status reg define*/
		{
			.jedec_id = 0xc8d1c8d1,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_efail= 2,	.b_pfail= 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,

			.b_qe = 8,	.b_srp1 = 9,.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},	
		/*normal status reg define*/
		{
			.jedec_id = 0xc8d2c8d2,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_efail= 2,	.b_pfail= 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,

			.b_qe = 8,	.b_srp1 = 9,.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},				
		/*normal status reg define*/
		{
			.jedec_id = 0xb148c8b1,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_efail= 2,	.b_pfail= 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,

			.b_qe = 8,	.b_srp1 = 9,.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},
		/*normal status reg define*/
		{
			.jedec_id = 0xc8217f7f,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_efail= 2,	.b_pfail= 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,

			.b_qe = 8,	.b_srp1 = 9,.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},
		/*normal status reg define*/
		{
			.jedec_id = 0xc831C831,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_efail= 2,	.b_pfail= 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,

			.b_qe = 8,	.b_srp1 = 9,.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},	
		/*normal status reg define*/
		{
			.jedec_id = 0xa1e1a1e1,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_efail= 2,	.b_pfail= 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,

			.b_qe = 8,	.b_srp1 = 9,.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},
		{
			.jedec_id = 0xc212c212,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_efail= 2,	.b_pfail= 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,

			.b_qe = 8,	.b_srp1 = 9,.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},		
		/*normal status reg define*/
		{
			.jedec_id = 0,	.ext_id = 0,
			.b_wip = 0,	.b_wel = 1,	.b_bp0 = 2,	.b_bp1 = 3,
			.b_bp2 = 4,	.b_bp3 = 5,	.b_bp4 = 6,	.b_srp0 = 7,
			
			.b_srp1 = 8,.b_qe = 9,	.b_lb = 10,	.b_cmp = 14,
			.b_sus = 15,
		},
};


/* NOTE: double check command sets and memory organization when you add
 * more flash chips.  This current list focusses on newer chips, which
 * have been converging on command sets which including JEDEC ID.
 */
static struct spinand_flash_info ak_spinand_param_supportlist [] = {
	{ "GD5G1GQ4U", 0xf1c8f1c8, 0, 1, 2048, 64, 128*1024, 1024, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	{ "GD5G2GQ4U", 0xf2c8f2c8, 0, 1, 2048, 64, 128*1024, 2048, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	{ "GD5G4GQ4U", 0xf4c8f4c8, 0, 1, 2048, 64, 128*1024, 4096, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},	
	{ "PSU1GQ4U", 0x7f7f21c8, 0, 1, 2048, 64, 128*1024, 1024, 64, 8, 8, 0, 4, 8, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	{ "AFS1GQ4UAAWC2", 0x31c831c8, 0, 1, 2048, 128, 256*1024, 512, 64, 0, 6, 10, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},	
	{ "PN26G01AWSIUG", 0xe1a1e1a1, 0, 1, 2048, 64, 128*1024, 1024, 64, 0, 8, 0, 4, 6, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	{ "MX35LF1GE4AB", 0x12c212c2, 0, 1, 2048, 64, 128*1024, 1024, 64, 0, 16, 0, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},

	{ "GD5F2GQ4UB", 0xd2c8d2c8, 0, 1, 2048, 64, 128*1024, 2048, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},//OK
	{ "GD5F1GQ4UB", 0xd1c8d1c8, 0, 1, 2048, 64, 128*1024, 1024, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},  //OK
	{ "FM25S01A", 0x7f7fe4a1, 0, 1, 2048, 64, 128*1024, 1024, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},//OK
	{ "FM25S02A", 0xe5a1e5a1, 0, 1, 2048, 64, 128*1024, 2048, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},//ok
	{ "GD5F1GQ5UEYIG", 0x51c851c8, 0, 1, 2048, 64, 128*1024, 1024, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},//ok
	{ "GD5F2GQ5UEYIG", 0x52c852c8, 0, 1, 2048, 64, 128*1024, 2048, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},//OK
	{ "XT26G10B", 0xf10bf10b, 0, 1, 2048, 64, 128*1024, 2048, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},//ok
	
	{ "W25N01GV", 0x21aaef, 0, 1, 2048, 64, 128*1024, 2048, 64, 0, 4, 12, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	//{ "DS35Q2GB", 0x72e572e5, 0, 2048, 64, 128*1024, 2048, 64, 0, 16, 0, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	{ "W25N02KV", 0x22aaef, 0, 1, 2048, 64, 128*1024, 2048, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	{ "XT26G01A", 0xe10be10b, 0, 1, 2048, 64, 128*1024, 1024, 64, 0, 10, 0, 4, 8, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	{ "XT26G02A", 0xe20be20b, 0, 1, 2048, 64, 128*1024, 2048, 64, 0, 10, 0, 4, 8, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	{ "FS35ND01G", 0xcd11eacd, 0, 1, 2048, 64, 128*1024, 1024, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	{ "FS35ND02G", 0xcd11ebcd, 0,1, 2048, 64, 128*1024, 2048, 64, 0, 12, 4, 4, 4, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
	{ "XT26G02E", 0x242c242c, 0, 2, 2048, 64, 128*1024, 2048, 64, 0, 16, 0, 4, 32, SFLAG_DUAL_READ|SFLAG_QUAD_READ|SFLAG_QUAD_WRITE,},
};


/* cdh:check ok 2016.10.17 */
static int init_spinand_flash_rw_info(struct spinand_flash *flash)
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
		if(flash->flags & SFLAG_DUAL_READ) {
			xfer_ctl->rx_opcode = CMD_READ_ARRAY_DUAL;
			xfer_ctl->rxd_bus_width = SPI_XFER_2DATAWIRE;
			xfer_ctl->rxa_bus_width = SPI_XFER_1DATAWIRE;
			xfer_ctl->dummy_len = 1; // cdh:check ok
		} else if (flash->flags & SFLAG_DUAL_IO_READ) {
			xfer_ctl->rx_opcode = CMD_READ_ARRAY_DUAL_IO;
			xfer_ctl->rxd_bus_width = SPI_XFER_2DATAWIRE;
			xfer_ctl->rxa_bus_width = SPI_XFER_2DATAWIRE;
			xfer_ctl->dummy_len = 1; // cdh:check ok
		}

		if(flash->flags & SFLAG_DUAL_WRITE) {
			xfer_ctl->tx_opcode = CMD_PAGE_PROGRAM_DUAL; // cdh:not support dual write
			xfer_ctl->txd_bus_width = SPI_XFER_2DATAWIRE;
			xfer_ctl->txa_bus_width = SPI_XFER_1DATAWIRE;
		} else if(flash->flags & SFLAG_DUAL_IO_WRITE) {
			xfer_ctl->tx_opcode = CMD_PAGE_PROGRAM_2IO;
			xfer_ctl->txd_bus_width = SPI_XFER_2DATAWIRE;
			xfer_ctl->txa_bus_width = SPI_XFER_2DATAWIRE;
		}	
	}

	if(flash->bus_width & FLASH_BUS_WIDTH_4WIRE){
		if(flash->flags & SFLAG_QUAD_READ) {
			xfer_ctl->rx_opcode = CMD_READ_ARRAY_QUAD;
			xfer_ctl->rxd_bus_width = SPI_XFER_4DATAWIRE;
			xfer_ctl->rxa_bus_width = SPI_XFER_1DATAWIRE;
			xfer_ctl->dummy_len = 1; // cdh:check ok
		}else if(flash->flags & SFLAG_QUAD_IO_READ){
			xfer_ctl->rx_opcode = CMD_READ_ARRAY_QUAD_IO;
			xfer_ctl->rxd_bus_width = SPI_XFER_4DATAWIRE;
			xfer_ctl->rxa_bus_width = SPI_XFER_4DATAWIRE;
			xfer_ctl->dummy_len = 3; // cdh:modify from winbond 32MB spi norflash
		}

		if(flash->flags & SFLAG_QUAD_WRITE) {
			xfer_ctl->tx_opcode = CMD_PAGE_PROGRAM_QUAD;  // cdh:check ok
			xfer_ctl->txd_bus_width = SPI_XFER_4DATAWIRE;			
			xfer_ctl->txa_bus_width = SPI_XFER_1DATAWIRE;
		}else if(flash->flags & SFLAG_QUAD_IO_WRITE) {  
			xfer_ctl->tx_opcode = CMD_PAGE_PROGRAM_4IO;  // cdh:not support dual write
			xfer_ctl->txd_bus_width = SPI_XFER_4DATAWIRE;
			xfer_ctl->txa_bus_width = SPI_XFER_4DATAWIRE;
		}
	}
	return 0;
}

u8 spinand_IsInit(void)
{
	return g_spinand_init_flag;
}







int write_enable_spinand(struct spi_slave *spi)
{
    u8 buf1[1];
    
    //write enable
    buf1[0] = SPI_NAND_WRITE_ENABLE;
	return spi_flash_cmd_write(spi, buf1,1, NULL, 0);
}

int write_disable_spinand(struct spi_slave *spi)
{
    u8 buf1[1];
    
    //write enable
    buf1[0] = SPI_NAND_WRITE_DISABLE;
	return spi_flash_cmd_write(spi, buf1,1, NULL, 0);
}


static int get_feature(struct spi_slave *spi, u8 addr, u8 *val)
{
	u8 buf[2];

	buf[0] = SPI_NAND_GET_FEATURE;
	buf[1] = addr;

	if (spi_flash_cmd_read(spi, buf,2, val, 1) == -1)
    {
        printf("erase: spi_flash_cmd_write fail\r\n");
        return -1;
    }
	
	return 0;
}

static int set_feature(struct spi_slave *spi, unsigned char addr, unsigned char val)
{
    unsigned char buf[3] = {0};
	unsigned char data[3] = {0};

    buf[0] = SPI_NAND_SET_FEATURE;
    buf[1] = addr;

	data[0] = val;

    if (spi_flash_cmd_write(spi, buf,2, data, 1) == -1)
    {
        printf("erase: spi_flash_cmd_write fail\r\n");
        return -1;
    }
       
    return 0;
}


static int wait_cmd_finish(struct spi_slave *spi, u8 *status)
{
	u32 timeout = 0;

	while(timeout++ < 1000000)
	{
		
		if(get_feature(spi, ADDR_STATUS, status) == 0)
		{
			//printf("get_feature status:%x, STATUS_OIP:%x\n", *status, STATUS_OIP);
			if(!(*status&STATUS_OIP))
				return 0;
		}
	}

	printf("get_feature timeout status:%x\n", *status);
	return -1;
}


static int check_ECC_status(struct spi_slave *spi)
{
	//u32 timeout = 0;
	u8 ecc = 0;
	u8 status = 0;


	if(get_feature(spi, ADDR_STATUS, &status) == -1)
	{
		printf("r ecc get_feature fail \r\n");
        return -1;
	}
	
	ecc = (status >> 4) & 0x3;
	//printf("status:%x, ecc:%d\n", status, ecc);
    if(2 == ecc)    //bit error and not corrected
    {
    	printf("r ecc error sr:%x\r\n", status);
        return -1;
    }
	else
	{
		return 0;
	}

	return -1;
}

int spi_nand_erase(struct spinand_flash *flash, u32 page)
{
    unsigned char cmdbuf[4];
    unsigned char status;
	//unsigned long timeout = SPI_FLASH_PAGE_ERASE_TIMEOUT;
    
    if (!spinand_IsInit())
    {
        printf("spi_nand_erase spinand not init\r\n");
        return -1;
    }

    //write enable
    write_enable_spinand(flash->spi);

    //erase
    cmdbuf[0] = SPI_NAND_BLOCK_ERASE;
    cmdbuf[1] = (page >> 16) & 0xFF;
    cmdbuf[2] = (page >> 8) & 0xFF;
    cmdbuf[3] = (page >> 0) & 0xFF;

    //erase
    if (spi_flash_cmd_write(flash->spi, cmdbuf,4, NULL, 0) == -1)
    {
        printf("erase: spi_flash_cmd_write fail\r\n");
        goto ERASE_EXIT;
    }
    //us_delay(10);
    //check status
   if (wait_cmd_finish(flash->spi, &status) == 0)
    {
    	//printf("status:%x\r\n", status);
        if(!(status & STATUS_E_FAIL))
        {
             return 0;
        }
        else
        {
            printf("wait_cmd_finish fail status == STATUS_E_FAIL\r\n");
        }
    }

ERASE_EXIT:    
    
    return -1;
}

int ecc_ctl(struct spinand_flash *flash, bool bEnable)
{
    unsigned char feature;

    if(-1 == get_feature(flash->spi, ADDR_FEATURE, &feature))
    {
        return -1;
    }

    if(bEnable)
        feature |= FEATURE_ECC_EN;
    else
        feature &= ~FEATURE_ECC_EN;

    if(-1 == set_feature(flash->spi,ADDR_FEATURE, feature))
    {
        return -1;
    }

    return 0;
}


bool spi_nand_read_noecc(struct spinand_flash *flash, u32 page, u32 column, u8 *buf, u32 len)
{
    u8 cmdbuf[4];
	u8 status;
	unsigned long pageperblock = flash->erase_size/flash->page_size;
	unsigned long block = page/pageperblock;

	if (!spinand_IsInit())
	{
		printf("spi_nand_erase spinand not init\r\n");
		return -1;
	}

    //set to no ecc
    if (-1 == ecc_ctl(flash, false))
    {
        printf("set no ecc fail\r\n");
        return -1;
    }

    cmdbuf[0] = SPI_NAND_PAGE_READ;
    cmdbuf[1] = (page >> 16) & 0xFF;
    cmdbuf[2] = (page >> 8) & 0xFF;
    cmdbuf[3] = (page >> 0) & 0xFF;

	if (spi_flash_cmd_write(flash->spi, cmdbuf,4, NULL, 0) == -1)
    {
        printf("erase: read spi_flash_cmd_write fail\r\n");
        return -1;
    }

	//printf("read page:%d.\r\n", page);
	if(wait_cmd_finish(flash->spi, &status) == -1)
	{
		printf("wait_cmd_finish error, page:%d\n", page);
		return -1;
	}

	cmdbuf[0] = SPI_NAND_READ_CACHE;
	cmdbuf[1] = (column >> 8) & 0x0F;
	cmdbuf[2] = column & 0xFF;
	cmdbuf[3] = 0x0;

	if(flash->planecnt == 2 && block%2 != 0)
	{
		cmdbuf[1] |= 0x10;
	}


	if (spi_flash_cmd_read(flash->spi, cmdbuf,4, buf, len) == -1)
    {
        printf("erase: spi_flash_cmd_read fail\r\n");
        return -1;
    }

    if (-1 == ecc_ctl(flash, true))
    {
        printf("spi_nand_read_noecc set no ecc fail\r\n");
        return -1;
    }

    return 0;
}



int spi_nand_read(struct spinand_flash *flash, u32 page, u32 column, u8 *buf, u32 len)
{
	u8 cmdbuf[4];
	u8 status;
	unsigned long pageperblock = flash->erase_size/flash->page_size;
	unsigned long block = page/pageperblock;
	//u32 count = 0;
	//u32 readlen=  0;

	if (!spinand_IsInit())
    {
        printf("spi_nand_erase spinand not init\r\n");
        return -1;
    }

	cmdbuf[0] = SPI_NAND_PAGE_READ;
    cmdbuf[1] = (page >> 16) & 0xFF;
    cmdbuf[2] = (page >> 8) & 0xFF;
    cmdbuf[3] = (page >> 0) & 0xFF;
	
	if (spi_flash_cmd_write(flash->spi, cmdbuf,4, NULL, 0) == -1)
    {
        printf("erase: read spi_flash_cmd_write fail\r\n");
        return -1;
    }

	//printf("read page:%d.\r\n", page);
	if(wait_cmd_finish(flash->spi, &status) == -1)
	{
		printf("wait_cmd_finish error, page:%d\n", page);
		return -1;
	}

	cmdbuf[0] = SPI_NAND_READ_CACHE;   
	cmdbuf[1] = (column >> 8) & 0x0F;
	cmdbuf[2] = column & 0xFF;
	cmdbuf[3] = 0x0;

	if(flash->planecnt == 2 && block%2 != 0)
	{
		cmdbuf[1] |= 0x10;
	}


	if (spi_flash_cmd_read(flash->spi, cmdbuf,4, buf, len) == -1)
    {
        printf("erase: spi_flash_cmd_write fail\r\n");
        return -1;
    }

	if(check_ECC_status(flash->spi) == -1)
	{
		printf("check_ECC_status error, page:%d\n", page);
		return -1;
	}

	return 0;
}



int spi_nand_write(struct spinand_flash *flash, u32 page, u32 column, const void *buf, u32 len)
{
	unsigned char cmdbuf[4];
    //unsigned long count = len;
    unsigned char status;
	unsigned long pageperblock = flash->erase_size/flash->page_size;
	unsigned long block = page/pageperblock;


	if (!spinand_IsInit())
    {
        printf("spi_nand_write spinand not init\r\n");
        return -1;
    }

    //write enable
    write_enable_spinand(flash->spi);

    //program load
    cmdbuf[0] = SPI_NAND_PROGRAM_LOAD;    
    cmdbuf[1] = (column >> 8) & 0x0F;
    cmdbuf[2] = column & 0xFF;
    //buf[3] = data[0];

	if(flash->planecnt == 2 && block%2 != 0)
	{
		cmdbuf[1] |= 0x10;
	}

    if (spi_flash_cmd_write(flash->spi, cmdbuf,3, buf, len) == -1)
    {
        printf("write: write spi_flash_cmd_write fail\r\n");
        return -1;
    }

    //program execute
    cmdbuf[0] = SPI_NAND_PROGRAM_EXECUTE;
    cmdbuf[1] = (page >> 16) & 0xFF;
    cmdbuf[2] = (page >> 8) & 0xFF;
    cmdbuf[3] = (page >> 0) & 0xFF;

	if (spi_flash_cmd_write(flash->spi, cmdbuf,4, NULL, 0) == -1)
    {
        printf("write: write spi_flash_cmd_write fail\r\n");
        return -1;
    }


    //check status
    if (wait_cmd_finish(flash->spi, &status) == 0)
    {
        if(!(status & STATUS_P_FAIL))
        {
        	//printf("status:%x\r\n", status);
            return 0;
        }
        else
        {
        	printf("fail status == STATUS_P_FAIL\r\n");
        }
    }

    return -1;
}





int spinand_flash_erase_inter(struct spinand_flash *flash, u32 offset, size_t len)
{
	u32 addr = offset;
	u32 page = addr/flash->page_size;

	if(flash == NULL)
	{
		printf("error:flash == NULL\r\n");
		return -1;
	}


	if(offset % flash->erase_size != 0)
	{
		printf("error: erase offset mod erase_size != 0\r\n");
		return -1;
	}


	if(len % flash->erase_size != 0)
	{
		printf("error: erase len mod erase_size != 0\r\n");
		return -1;
	}

	while (len) {
		if (spi_nand_erase(flash, page) == -1) {
			printf("error: spi_nand_erase fail, addr:%d\r\n", addr);
			return -1;
		}

		len -= flash->erase_size;
	}

	return 0;
}

int spinand_flash_write_inter(struct spinand_flash *flash, u32 offset, size_t len, const void *data,  const void *oob, size_t oob_len)
{
	u32 page = 0;
	//u32 column = 0;
	//u32 readlen = oob_len;
	//u32 xfer_len = 0;
	//u32 idex = 0;
	//u32 oob_idex = 0;
	u8 *p_buftmp_oob = NULL;
	u32 buftmp_len = 0;
	u8  oob_buf[64] = {0};
	u8  buf[2112] = {0};
	u32 read_len_oob = 0;
	u32 oob_seglen_len = 0;
	u32 oob_xfer_len = 0;
	u32 spare_offset = 0;
	u32 oob_offset = 0;
	u32 write_len = 0;
	u32 oob_add_len = 0;
	int ret = -1;

	if(flash == NULL)
	{
		printf("error:flash == NULL\r\n");
		return -1;
	}

	page = offset/flash->page_size;

	if(len > flash->page_size )
	{
		printf("error:spinand_flash_read_page more than page size\r\n");
		return -1;
	}

	memset(buf, 0xFF, 2112);

	//fill buf
	memcpy(buf, data, len);
	write_len = flash->page_size;
	if(oob != NULL && oob_len != 0)
	{
		//read page and oob
		if(spi_nand_read(flash, page, flash->page_size, oob_buf, 64) == -1)
		{
			printf("error:spi_nand_read fail, page:%d\r\n", page);
			return -1;
		}


		read_len_oob = oob_len;
	    oob_seglen_len = flash->oob_up_skiplen + flash->oob_seglen + flash->oob_down_skiplen;
	    oob_xfer_len = flash->oob_seglen;
	    spare_offset = flash->badblock_offset;


		p_buftmp_oob = oob_buf + spare_offset; //offset to spare data

	    while(read_len_oob> 0){
			buftmp_len = (read_len_oob > oob_xfer_len) ? oob_xfer_len : read_len_oob;	   	
			memcpy(p_buftmp_oob + oob_add_len, oob+oob_offset, buftmp_len);			
			read_len_oob -= buftmp_len;
			oob_offset += buftmp_len;
			oob_add_len +=oob_seglen_len;
			//pp_printf("read_len_oob=%d, oob_offset=%d, oob_add_len=%d\n", read_len_oob, oob_offset, oob_add_len);
		}
		memcpy(&buf[flash->page_size], oob_buf, flash->oob_size);
		write_len = flash->page_size + flash->oob_size;
	}

	
	//write
	ret = spi_nand_write(flash, page, 0,  buf, write_len);
	if(ret == -1)
	{
		printf("error:spi_nand_write fail, page:%d\r\n", page);
	}
	
    return ret; 


}

int spinand_flash_read_inter(struct spinand_flash *flash, u32 offset, size_t len, void *data,  void *oob, size_t oob_len)
{
	u32 page = 0;//, i = 0;
	u32 column = 0;
	u32 readlen = oob_len;
	u32 xfer_len = 0;
	u32 idex = flash->badblock_offset;
	u32 oob_idex = 0;
	u8  buf[2112] = {0};
	u32 will_read_len = 0;

	//printf("offset:%d, len:%d\n", offset, len);
	
	if(flash == NULL)
	{
		printf("error:flash == NULL\r\n");
		return -1;
	}


	memset(buf, 0xFF, 2112);

	page = offset/flash->page_size;
	will_read_len = flash->page_size + flash->oob_size;	

	if(len > flash->page_size )
	{
		printf("error:spinand_flash_read_page more than page size\r\n");
		return -1;
	}
	
	//printf("will_read_len:%d, flash->oob_size:%d\r\n", will_read_len, flash->oob_size);
	if(spi_nand_read(flash, page, column, buf, will_read_len) == -1)
	{
		printf("error:spi_nand_read fail, page:%d\r\n", page);
		return -1;
	}

	
	memcpy(data, buf, len);

	if(oob != NULL && oob_len != 0)
	{
		while(readlen > 0) 
		{
			xfer_len = (readlen > flash->oob_seglen) ? flash->oob_seglen : readlen;
			memcpy(oob + oob_idex, buf + flash->page_size +idex, xfer_len);
			
			idex += (flash->oob_up_skiplen + flash->oob_seglen + flash->oob_down_skiplen);

			readlen -= xfer_len;		
			oob_idex += xfer_len;
		}
	}

	return 0;
}

int spinand_flash_read_oob(struct spinand_flash *flash, u32 offset, void *oob, size_t oob_len)
{
	u32 colnum = flash->page_size + flash->oob_size;
	u32 page = offset /flash->page_size;
	u8 buf[64] = {0};
	u32 readlen = oob_len;
	u32 xfer_len = 0;
	u32 idex = flash->badblock_offset;
	u32 oob_idex = 0;

	if(flash == NULL)
	{
		printf("error:flash == NULL\r\n");
		return -1;
	}
	
	if(spi_nand_read(flash, page, colnum, buf, 64) == -1)
	{
		return -1;
	}

	if(oob != NULL && oob_len != 0)
	{
		while(readlen > 0) 
		{
			xfer_len = (readlen > flash->oob_seglen) ? flash->oob_seglen : readlen;
			memcpy(oob + oob_idex, buf + idex, xfer_len);
			
			idex += (flash->oob_up_skiplen + flash->oob_seglen + flash->oob_down_skiplen);

			readlen -= xfer_len;		
			oob_idex += xfer_len;
		}
	}

	return 0;
}


/**
* @brief: enable 4 wire transfer mode.
* 
* @author lixinhai
* @date 2014-04-10
* @param[in] flash  spiflash handle.
*/
static int quad_mode_enable(struct spinand_flash *flash)
{
	int shift;
	//int idx;
	u32 regval;
	u8 addr;
	u8 status = 0;
	struct flash_status_reg *fsr = (struct flash_status_reg *)&flash->spinand_stat_reg;

	shift = fsr->b_qe / 8;
	//idx = fsr->b_qe % 8;
	
	addr = feature_cmd[shift];
	if(-1 == wait_cmd_finish(flash->spi, &status))
	{
		return -1;
	}

	write_enable_spinand(flash->spi);

	if(get_feature(flash->spi, addr, &status) == -1)
	{
		write_disable_spinand(flash->spi);
		printf("quad get_feature fail \r\n");
        return -1;
	}
	
	regval = (u32)status;
	regval |= 1<<(fsr->b_qe % 8);
	
	if(set_feature(flash->spi, addr, (u8)regval) == -1)
	{
		write_disable_spinand(flash->spi);
		printf("quad set_feature fail \r\n");
        return -1;
	}

	write_disable_spinand(flash->spi);
	
	return 0;
}

/**
* @brief: disable 4 wire transfer mode.
* 
* @author lixinhai
* @date 2014-04-10
* @param[in] flash  spiflash handle.
*/
static int quad_mode_disable(struct spinand_flash *flash)
{
	//int idx; 
	int	shift;
	u32 regval;
	u8 addr;
	u8 status = 0;
	struct flash_status_reg *fsr = (struct flash_status_reg *)&flash->spinand_stat_reg;

	shift = fsr->b_qe / 8;
	//idx = fsr->b_qe % 8; 
	addr = feature_cmd[shift];

	if(!wait_cmd_finish(flash->spi, &status))
	{
		return -1;
	}
	
	write_enable_spinand(flash->spi);
	
	if(get_feature(flash->spi, addr, &status) == -1)
	{
		write_disable_spinand(flash->spi);
		printf("quad get_feature fail \r\n");
        return -1;
	}
	
	regval = (u32)status;
	regval &= ~(1<<(fsr->b_qe%8));
	if(set_feature(flash->spi, addr, (u8)regval) == -1)
	{
		write_disable_spinand(flash->spi);
		printf("quad set_feature fail \r\n");
        return -1;
	}
	
	write_disable_spinand(flash->spi);
	return 0;
}



static int spinand_cfg_quad_mode(struct spinand_flash *flash)
{
	int ret = 0;
	
	if((flash->bus_width & FLASH_BUS_WIDTH_4WIRE) && 
		(flash->flags & (SFLAG_QUAD_WRITE|SFLAG_QUAD_IO_WRITE|
			SFLAG_DUAL_READ|SFLAG_DUAL_IO_READ))) {	
		ret = quad_mode_enable(flash);
		if(ret == -1) {
			flash->bus_width &= ~FLASH_BUS_WIDTH_4WIRE;
			printf("config the spiflash quad enable fail. transfer use 1 wire.\n");
		}
	}
	else
		quad_mode_disable(flash);

	return ret;
}





static struct spinand_flash *spinand_flash_validate_params(struct spi_slave *spi, u8 *idcode)
{
	struct spinand_flash_info	*info;
	struct spinand_flash_status_reg *sr;
	struct spinand_flash *flash;
    T_SPI_NAND_PARAM  spinand_flash_param;
	int i;
	u32 jedec = 0;

	jedec = idcode[3];
	jedec = jedec << 8;
	jedec |= idcode[2];
	jedec = jedec << 8;
	jedec |= idcode[1];
	jedec = jedec << 8;
	jedec |= idcode[0];

	debug("akspi nand ID: 0x%08x\n", jedec);
	for (i = 0, info = ak_spinand_param_supportlist;i < ARRAY_SIZE(ak_spinand_param_supportlist);i++, info++) {
		if (info->jedec_id == jedec) {
			break;
		}
	}

	if(i == ARRAY_SIZE(ak_spinand_param_supportlist))
	{
		printf("error: not find match spinandflash spinandflashid:%x\n", jedec);
		return NULL;
	}
	else
	{
		spinand_flash_param.chip_id = jedec; //0xc1d8c1d8;//chip id
	    spinand_flash_param.page_size = info->page_size; //page size
	    spinand_flash_param.page_per_blk = info->page_per_block; //page of one blok
	    spinand_flash_param.blk_num = info->n_blocks;//total block number
	    spinand_flash_param.group_blk_num = info->n_blocks;//the same concept as die, according to nand's struture
	   	spinand_flash_param.plane_blk_num = info->n_blocks/info->planecnt;   
	    spinand_flash_param.spare_size = info->oob_size;//spare
	    spinand_flash_param.flag = info->flags;//0x104;//
	    memcpy(spinand_flash_param.des_str, info->name, strlen(info->name));//descriptor string
	}


	flash = malloc(sizeof(*flash));
	if (!flash) {
		debug("SF: Failed to allocate spi_flash\n");
		return NULL;
	}
	memset(flash, '\0', sizeof(*flash));

	/* Assign spi data */
	flash->spi = spi;
	flash->name = "SPINAND";//spinand_flash_param.des_str; //params->name;
	flash->memory_map = spi->memory_map;
	flash->bus_width = spi->bus_width;
	flash->flags = spinand_flash_param.flag ;//params->flags;
	flash->flashid = spinand_flash_param.chip_id; //params->jedec;
	flash->oob_size = spinand_flash_param.spare_size;

	flash->badblock_offset = info->oob_vail_data_offset;//spinand_flash_param.flag & 0xFF;
	//flash->oob_idex = (spinand_flash_param.flag>>10)&0x01;

	flash->oob_up_skiplen = info->oob_up_skiplen;
	flash->oob_seglen = info->oob_seglen;
	flash->oob_down_skiplen = info->oob_down_skiplen;
	flash->oob_seg_perpage = info->oob_seg_perpage;
	
	//g_spinand_info = (T_SPINAND_INFO *)ak_spinand_supportlist + flash->oob_idex;
	for (i=0; i<ARRAY_SIZE(status_reg_list); i++) {
		sr = &status_reg_list[i];
		if (sr->jedec_id == flash->flashid) {
			//printf("SF: status_reg_list supported flash IDs:0x%x\n", params->jedec);
			break;
		}
	}

	flash->spinand_stat_reg = *sr;

	flash->bus_width = FLASH_BUS_WIDTH_1WIRE;
	debug("anyka spi flash. bus_width:1\n");

	/* Assign spi_flash ops */
	flash->spinand_write = spinand_flash_write_inter;
	flash->spinand_erase = spinand_flash_erase_inter;
	flash->spinand_read = spinand_flash_read_inter;
	flash->spinand_isbadblock = spinand_isbadblock;
	flash->spinand_setbadblock = spinand_setbadblock;

	/* Compute the flash size */
	flash->page_size = spinand_flash_param.page_size;
	flash->sector_size = spinand_flash_param.page_per_blk * spinand_flash_param.page_size;
	flash->erase_size = flash->sector_size;
	flash->size = flash->sector_size * spinand_flash_param.blk_num;
	flash->planecnt = info->planecnt;

	//printf("flash->planecnt:%d\n", flash->planecnt);
	
	/* Poll cmd seclection */
	//flash->poll_cmd = CMD_READ_STATUS;

	// cdh:intial spi operation data wire and cmd
	init_spinand_flash_rw_info(flash);
    spinand_cfg_quad_mode(flash);
	g_spinand_init_flag = 1;


	sflash_mtd.name = "nand0";
	sflash_mtd.type = MTD_DATAFLASH;
	sflash_mtd.writesize = flash->page_size;
	sflash_mtd.size = flash->size;
	sflash_mtd.erasesize = flash->erase_size;
	sflash_mtd.numeraseregions = 0;
	add_mtd_device(&sflash_mtd);
	
	debug("spi_flash_validate_params finish.\n");
	return flash;
}

struct spinand_flash *spinand_flash_probe(unsigned int bus, unsigned int cs,
		unsigned int max_hz, unsigned int spi_mode)
{
	struct spi_slave *spi;
	struct spinand_flash *flash = NULL;
	u8 idcode[5];
	u8 cmd[2] = {0};
	int ret;
	
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
	cmd[0]=CMD_READ_ID;
	cmd[1]=0x0;
	ret = spi_flash_cmd_id(spi, cmd, 2, idcode, sizeof(idcode));
	if (ret) {
		printf("SF: Failed to get idcodes\n");
		goto err_read_id;
	}
	
	/* Validate params from spi_flash_params table */
	flash = spinand_flash_validate_params(spi, idcode);
	if (!flash)
		goto err_read_id;


	/* Release spi bus */
	spi_release_bus(spi);

	return flash;

err_read_id:
	spi_release_bus(spi);
err_claim_bus:
	spi_free_slave(spi);
	return NULL;
}

void spinand_set_no_protection(struct spinand_flash *flash)
{
	if (-1 == set_feature(flash->spi, ADDR_PROTECTION, 0))
    {
        printf("set_feature fail, and no write \r\n");
    }
}

void spinand_flash_free(struct spinand_flash *flash)
{
	spi_free_slave(flash->spi);
	free(flash);
}

int  spinand_isbadblock(struct spinand_flash *flash, u32 offset)
{
	unsigned long block = 0;
	unsigned long pageperblock = flash->erase_size/flash->page_size;
	unsigned long page, page_cnt = 2, i = 0;
	unsigned long column = flash->page_size;
	unsigned char buf[2] = {0};

	block = (unsigned long)offset/flash->erase_size;
	page = block*pageperblock;
	for(i = 0; i < page_cnt; i++)
	{
		memset(buf, 0, 2);
		if (-1 == spi_nand_read_noecc(flash, page, column, buf, 1))
	    {
	        printf("Nand_IsBadBlock read oob fail nAbsPage:%ld\n", page);
	        return -1;
	    }

		if(buf[0] != 0xFF)
		{
			return 0;
		}
		page += pageperblock - 1;
	}

    return -1;
}

int  spinand_setbadblock(struct spinand_flash *flash, u32 offset)
{
	unsigned long block = 0;
	unsigned long pageperblock = flash->erase_size/flash->page_size;
	unsigned long page, page_cnt = 2, i = 0;
	unsigned long column = flash->page_size;
	unsigned char buf[2] = {0};

	//debug("Nand_SetBadBlock block:%d\n", block);

	block = (unsigned long)offset/flash->erase_size;
	page = block*pageperblock;

	for(i = 0; i < page_cnt; i++)
	{
		memset(buf, 0, 2);
		if (-1 == spi_nand_read_noecc(flash, page, column, buf, 1))
	    {
	        printf("Nand_IsBadBlock read oob fail nAbsPage:%ld\n", page);
	        return -1;
	    }

		if(buf[0] == 0xFF)
		{
			if (-1 == spi_nand_write(flash, page, column, buf, 1))
		    {
		        printf("Nand_IsBadBlock write oob fail nAbsPage:%ld\n", page);
		        return -1;
		    }
		}
		else
		{
			printf("the block %ld is bad block, no need set\n", block);
			return 0;
		}
	}

    return 0;
}



int  spinand_erase_partition(struct spinand_flash *flash,        u32 start_pos,  u32 partition_size)
{	
	u32 blockcnt = 0;
	u32 b_idex = 0, good_block = 0;
	int ret = 0;
	

	blockcnt = (partition_size + flash->erase_size - 1)/flash->erase_size;
	while(1)
	{
		if(b_idex == blockcnt)
		{
			break;
		}

		if(spinand_flash_isbadblock(flash, start_pos + b_idex*flash->erase_size) == 0)
		{
			printf("the block is bad block = %d\n", (start_pos + b_idex*flash->erase_size)/flash->erase_size);
			b_idex++;
			continue;
		}
	
		ret = spinand_flash_erase(flash, start_pos + b_idex*flash->erase_size, flash->erase_size);
		if (ret == -1)
		{
			b_idex++;
			spinand_flash_setbadblock(flash, start_pos + b_idex*flash->erase_size);
			printf("error: erase fail, set badblcok: start_pos = %d\n", start_pos + b_idex*flash->erase_size);
			continue;
		}

		b_idex++;
		good_block++;
	}

	if(good_block == 0)
	{
		printf("error: all block is bad, pls chk\n");
		return -1;
	}
	

	return 0;
}

int  spinand_read_partition(struct spinand_flash *flash, u32 start_pos, u32 partition_size, u8 *buf, u32 buf_len)
{
	u32 read_len = buf_len;
	u32 write_idex = 0;
	u32 page_idex = 0;
	u8 *tmp_buf = NULL;
	u32 will_read_len = 0;
	u32 buf_idex = 0, b_idex = 0;;
	int ret = -1;


	//printf("start_pos:%d, partition_size:%d, buf_len:%d\n", start_pos, partition_size, buf_len);
	while(1)
	{

		if(read_len == 0)
		{
			break;
		}
		if(page_idex*flash->page_size > partition_size)
		{
			printf("error: more than partition size, saved_size = %d, write_size:%d\n", partition_size, page_idex*flash->page_size);
			return -1;
		}
		
		if(spinand_flash_isbadblock(flash, start_pos + b_idex*flash->erase_size) == 0)
		{
			printf("the block is bad block = %d\n", (start_pos + b_idex*flash->erase_size)/flash->erase_size);
			b_idex++;
			page_idex += (flash->erase_size/flash->page_size);
			continue;
		}

		tmp_buf = buf + write_idex*flash->page_size;
		buf_idex = 0;
		while(1)
		{
			
			if(read_len > flash->page_size)
			{
				will_read_len = flash->page_size;
			}
			else
			{
				will_read_len = read_len;
			}

			//printf("will_read_len:%d, page_pos:%d\r\n", will_read_len, start_pos+page_idex*flash->page_size);
			ret = spinand_flash_read(flash, start_pos+page_idex*flash->page_size, will_read_len, &tmp_buf[buf_idex], NULL, 0);
			if (ret == -1)
			{
				printf("error: spinand_flash_read, pos:%d\n", start_pos+page_idex*flash->page_size);
				return -1;
			}
			page_idex++;
			write_idex++;
			read_len -= will_read_len;
			buf_idex += will_read_len;

			//printf("buf_idex:%d, read_len:%d, write_idex:%d, page_idex:%d, %d, %d\r\n", buf_idex, read_len, write_idex, page_idex, flash->erase_size, flash->page_size);
			
			if(read_len == 0)
			{
				break;
			}

			if((start_pos+page_idex*flash->page_size)%flash->erase_size == 0)
			{
				//printf("write next block\r\n");
				break;
			}
		}
		b_idex++;
	}
	return 0;	
}

int  spinand_read_partition_with_oob(struct spinand_flash *flash, u32 start_pos, u32 partition_size, u8 *buf, u32 buf_len)
{
	u32 read_len = buf_len;
	u32 read_idex = 0;
	u32 page_idex = 0;
	u8 *tmp_buf = NULL;
	//u32 will_read_len = 0;
	u32 buf_idex = 0, b_idex = 0, idex = 0;
	int ret = -1;


	if(buf_len % (flash->page_size + flash->oob_size) != 0)
	{
		printf("error: buf_len mod (flash->page_size + flash->oob_size) != 0:%d, %d, %d\n", buf_len, flash->page_size, flash->oob_size);
		return -1;
	}

	
	while(1)
	{
		if(page_idex*flash->page_size > partition_size)
		{
			printf("error: more than partition size, saved_size = %d, write_size:%d\n", partition_size, page_idex*flash->page_size);
			return -1;
		}

	
		if(read_len == 0)
		{
			break;
		}
		
		if(spinand_flash_isbadblock(flash, start_pos + b_idex*flash->erase_size) == 0)
		{
			printf("the block is bad block = %d\n", (start_pos + b_idex*flash->erase_size)/flash->erase_size);
			b_idex++;
			page_idex += (flash->erase_size/flash->page_size);
			continue;
		}

		idex = 0;
		tmp_buf = buf + read_idex*(flash->page_size + flash->oob_size);
		buf_idex = 0;
		while(1)
		{
			//printf("will_write_len:%d, page_pos:%d\r\n", will_read_len, start_pos+page_idex*flash->page_size);
			ret = spinand_flash_read(flash, start_pos+page_idex*flash->page_size, flash->page_size, &tmp_buf[buf_idex], &tmp_buf[buf_idex + flash->page_size], 28);
			if (ret == -1)
			{
				printf("error: spinand_flash_read, pos:%d\n", start_pos+page_idex*flash->page_size);
				return -1;
			}
			idex++;
			page_idex++;
			read_idex++;
			read_len-= flash->page_size + flash->oob_size;
			buf_idex += flash->page_size + flash->oob_size;
			
			if(read_len == 0)
			{
				break;
			}

			if((start_pos+page_idex*flash->page_size)%flash->erase_size == 0)
			{
				//printf("write next block\r\n");
				break;
			}
		}
		b_idex++;
	}
	return 0;	
}



int  spinand_write_partition(struct spinand_flash *flash, u32 start_pos, u32 partition_size, u8 *buf, u32 buf_len)
{
	u32 write_len = buf_len;
	u32 write_idex = 0;
	u32 page_idex = 0;
	u8 *tmp_buf = NULL;
	u32 will_write_len = 0;
	u32 buf_idex = 0, b_idex = 0, idex = 0;
	int ret = -1;

	
	while(1)
	{
		if(page_idex*flash->page_size > partition_size)
		{
			printf("error: more than partition size, saved_size = %d, write_size:%d\n", partition_size, page_idex*flash->page_size);
			return -1;
		}

	
		if(write_len == 0)
		{
			break;
		}
		
		if(spinand_flash_isbadblock(flash, start_pos + b_idex*flash->erase_size) == 0)
		{
			printf("the block is bad block = %d\n", (start_pos + b_idex*flash->erase_size)/flash->erase_size);
			b_idex++;
			page_idex += (flash->erase_size/flash->page_size);
			continue;
		}

		idex = 0;
		tmp_buf = buf + write_idex*flash->page_size;
		buf_idex = 0;
		while(1)
		{
			
			if(write_len > flash->page_size)
			{
				will_write_len = flash->page_size;
			}
			else
			{
				will_write_len = write_len;
			}

			//printf("will_write_len:%d, page_pos:%d\r\n", will_write_len, start_pos+page_idex*flash->page_size);
			ret = spinand_flash_write(flash, start_pos+page_idex*flash->page_size, will_write_len, &tmp_buf[buf_idex], NULL, 0);
			if (ret == -1)
			{
				printf("error: spinand write fail, and set badblock, write next block, pos:%d\n", start_pos+page_idex*flash->page_size);
				spinand_flash_setbadblock(flash, start_pos + b_idex*flash->erase_size);
				write_idex = write_idex - idex;
				page_idex += (flash->erase_size/flash->page_size);
				break;
			}
			idex++;
			page_idex++;
			write_idex++;
			write_len-= will_write_len;
			buf_idex += will_write_len;
			
			if(write_len == 0)
			{
				break;
			}

			if((start_pos+page_idex*flash->page_size)%flash->erase_size == 0)
			{
				//printf("write next block\r\n");
				break;
			}
		}
		b_idex++;
	}


	return 0;
}


int  spinand_write_partition_with_oob(struct spinand_flash *flash, u32 start_pos, u32 partition_size, u8 *buf, u32 buf_len)
{
	u32 write_len = buf_len;
	u32 write_idex = 0;
	u32 page_idex = 0;
	u8 *tmp_buf = NULL;
	u32 buf_idex = 0, b_idex = 0, idex = 0;
	int ret = -1;


	if(buf_len % (flash->page_size + flash->oob_size) != 0)
	{
		printf("error: buf_len:%d,  (flash->page_size + flash->oob_size) != 0:%d, %d\n", buf_len, flash->page_size, flash->oob_size);
		return -1;
	}

	
	while(1)
	{
		if(page_idex*flash->page_size > partition_size)
		{
			printf("error: more than partition size, saved_size = %d, write_size:%d\n", partition_size, page_idex*flash->page_size);
			return -1;
		}

	
		if(write_len == 0)
		{
			break;
		}
		
		if(spinand_flash_isbadblock(flash, start_pos + b_idex*flash->erase_size) == 0)
		{
			printf("the block is bad block = %d\n", (start_pos + b_idex*flash->erase_size)/flash->erase_size);
			b_idex++;
			page_idex += (flash->erase_size/flash->page_size);
			continue;
		}

		idex = 0;
		tmp_buf = buf + write_idex*(flash->page_size + flash->oob_size);
		buf_idex = 0;
		while(1)
		{
			//printf("will_write_len:%d, page_pos:%d\r\n", will_write_len, start_pos+page_idex*flash->page_size);
			ret = spinand_flash_write(flash, start_pos+page_idex*flash->page_size, flash->page_size, &tmp_buf[buf_idex], &tmp_buf[buf_idex + flash->page_size], 28);
			if (ret == -1)
			{
				printf("error: spinand write fail, and set badblock, write next block, pos:%d\n", start_pos+page_idex*flash->page_size);
				spinand_flash_setbadblock(flash, start_pos + b_idex*flash->erase_size);
				write_idex = write_idex - idex;
				page_idex += (flash->erase_size/flash->page_size);
				break;
			}
			idex++;
			page_idex++;
			write_idex++;
			write_len-= flash->page_size + flash->oob_size;
			buf_idex += flash->page_size + flash->oob_size;
			
			if(write_len == 0)
			{
				break;
			}

			if((start_pos+page_idex*flash->page_size)%flash->erase_size == 0)
			{
				//printf("write next block\r\n");
				break;
			}
		}
		b_idex++;
	}


	return 0;
}

#ifdef CONFIG_UPDATE_FLAG
#define UPDATE_NULL  ""       // NO UPDATA
#define UPDATE_ING   "U_ING"  //UPDATE ING
#define UPDATE_END   "U_END"  //UPDATE FINISH
#define UPDATE_ERR   "U_ERR"  //UPDATE ERROR, is not to update

#define PART_DATA_TYPE		0
#define PART_BIN_TYPE		1
#define PART_FS_TYPE		2


typedef enum
{
	U_UPDATE_NULL = 0,
	U_UPDATE_ING,
	U_UPDATE_ENG,
	U_UPDATE_ERR
}E_UPDATE_FLAG;



typedef struct tag_update_info{
	unsigned long  start_page;
	unsigned long  data_len;
	unsigned long  partitin_len;
	unsigned char	partition_name[8];

	unsigned long  start_page_bak;
	unsigned long  data_len_bak;
	unsigned long  partitin_len_bak;
	unsigned char	partition_name_bak[8];

	unsigned char	update_flag; //0->no update 1 ->updater ing  2->update OK  3->can not updater
	unsigned char	partition_type;
}T_UPDATE_INFO;


unsigned char m_aBuffer[4096] = {0};
unsigned long g_continu_badBlocknum = 0;
#define CONTINU_MAX_BD_NUM  50
#define PARTITION_CNT  				3


extern int spi_nand_write(struct spinand_flash *flash, u32 page, u32 column, const void *buf, u32 len);
extern int spi_nand_read(struct spinand_flash *flash, u32 page, u32 column, u8 *buf, u32 len);



int spinand_write_partition_data(struct spinand_flash *flash, unsigned long start_page, unsigned long date_len, unsigned long partition_len, unsigned char *pBuf, unsigned char type)
{
	unsigned long nPageCnt = 0, page = 0, i =0, buf_idex = 0, j = 0, block_cnt = 0;
	unsigned long page_idex = 0, partition_pagenum = 0, block_idex = 0;
	unsigned char *pPage_buf;
	unsigned char *pcompare_buf;
	unsigned char check = 1;
	unsigned long PagePerBlock = flash->erase_size/flash->page_size;
	unsigned long will_write_len = 0;


	if(type == PART_FS_TYPE)
	{
		nPageCnt = 	(date_len + flash->page_size + flash->oob_size - 1)/(flash->page_size + flash->oob_size);
	}
	else
	{
		nPageCnt = 	(date_len + flash->page_size - 1)/flash->page_size;
	}

	printf("partition_len:%ld, nPageCnt:%ld, type:%d \r\n",date_len, nPageCnt, type);
	page = 0;
	page_idex = 0;
	partition_pagenum = partition_len/flash->page_size;
	g_continu_badBlocknum = 0;

	will_write_len = flash->page_size + flash->oob_size;
	printf("partition_len:%ld, partition_pagenum:%ld\r\n", partition_len, partition_pagenum);
	//擦完分区的所有块
	while(1)
	{
		//if read page cnt more than partition page num error
		if(page == partition_pagenum)
		{
			break;
		}

		block_idex = (start_page+page)/PagePerBlock;
		//printf("block:%d, %d, %d\r\n", block_idex, start_page, page);
		if(spinand_flash_isbadblock(flash, block_idex*flash->erase_size) == 0){
			printf("the block %ld is bad block\r\n", block_idex);
		}
		else
		{
			printf("e.");
			//擦块
			if(spinand_flash_erase(flash, block_idex*flash->erase_size, flash->erase_size) == - 1){
				printf("Prod_NandEraseBlock fail block,and set badblock:%ld \r\n", block_idex);
				//标志此块为坏块
				spinand_flash_setbadblock(flash, block_idex*flash->erase_size);
			}
			else{
				block_cnt++;
			}
		}
		page += PagePerBlock; //指向下一个块
	}

	if(block_cnt == 0)
	{
		printf("erro: no any valib blcok\r\n");
		return -1;
	}


	page = 0;
	page_idex = 0;
	//printf("partition_pagenum:%d\r\n", partition_pagenum);
	//for(page = 0; page < nPageCnt; page++)
	while(1)
	{
		//read data len page num
		if(page_idex == nPageCnt)
		{
			break;
		}

		//if read page cnt more than partition page num error
		if(page > partition_pagenum)
		{
			printf("error: read page more than partition page num\r\n");
			return -1;
		}

		//printf("page:%d\r\n", page);

		//判断此块是否坏块

		block_idex = (start_page+page)/PagePerBlock;
		//printf("block:%d, %d, %d\r\n", block_idex, bios_start_page, page);
		if(spinand_flash_isbadblock(flash, block_idex*flash->erase_size) == 0)
		{
			printf("the block %ld is bad block\r\n", block_idex);
			page += PagePerBlock; //指向下一个块
		}
		else
		{
			printf("w.");
			//写块
			buf_idex = page_idex*(flash->page_size+ flash->oob_size);
			pPage_buf = pBuf+buf_idex;
			for(i = 0; i < PagePerBlock; i++)
			{
				if(page_idex == nPageCnt)
				{
					break;
				}

				if(spi_nand_write(flash, block_idex*PagePerBlock+i, 0,  pPage_buf, will_write_len) == -1)
				{
					//标志此块为坏块
					spinand_flash_setbadblock(flash, block_idex*flash->erase_size);

					//
					page_idex -= i;
					page += PagePerBlock; //指向下一个块
					printf("FHA_Nand_WritePage_burn fail block:%ld, page_idex:%ld, i:%ld\r\n", block_idex, page_idex, i);
					break;
				}
				if(check)
				{
					//比较读出来与写下去的数据
					pcompare_buf = (unsigned char *)m_aBuffer;
					if(spi_nand_read(flash, block_idex*PagePerBlock+i, 0, pcompare_buf, will_write_len) == -1)
					{
						//标志此块为坏块
						spinand_flash_setbadblock(flash, block_idex*flash->erase_size);

						//
						page_idex -= i;
						page += PagePerBlock; //指向下一个块
						printf("FHA_Nand_ReadPage_burn fail block:%ld, page_idex:%ld, i:%ld\r\n", block_idex, page_idex, i);
						break;
					}

					for(j = 0; j < will_write_len; j++)
					{
						if(pcompare_buf[j] != pPage_buf[j])
						{
							printf("update date compare fail, page:%ld, idex:%ld, dst:%02x, src:%02x\r\n", i, j, pcompare_buf[j], pPage_buf[j]);
							break;
						}
					}

					if(j !=  flash->page_size+ flash->oob_size)
					{
						//标志此块为坏块
						spinand_flash_setbadblock(flash, block_idex*flash->erase_size);

						g_continu_badBlocknum++;
						page_idex -= i;
						page += 	PagePerBlock; //指向下一个块
						printf("compare data fail  j:%ld, g_nPageSize+ g_oob_size:%d\r\n", j, flash->page_size+ flash->oob_size);

						if(CONTINU_MAX_BD_NUM <= g_continu_badBlocknum)
						{
							printf("error: continue read data compare fail, set bad block %ld more than %d\r\n", g_continu_badBlocknum, CONTINU_MAX_BD_NUM);
							return -1;
						}

						break;
					}
				}

				pPage_buf += flash->page_size+ flash->oob_size;
				page++;
				page_idex++;
			}
		}
	}


	printf("write data finish\r\n");

	return 0;
}



int spinand_read_partition_data(struct spinand_flash *flash, unsigned long start_page, unsigned long date_len, unsigned long partition_len, unsigned char *pBuf, unsigned char type)
{
	unsigned long nPageCnt = 0, page = 0, i = 0;
	unsigned long page_idex = 0, partition_pagenum = 0, block_idex = 0;
	unsigned long PagePerBlock = flash->erase_size/flash->page_size;
	unsigned long will_read_len = flash->page_size + flash->oob_size;

	if(type == PART_FS_TYPE)
	{
		nPageCnt = 	(date_len + flash->page_size + flash->oob_size - 1)/(flash->page_size + flash->oob_size);
	}
	else
	{
		nPageCnt = 	(date_len + flash->page_size - 1)/flash->page_size;
	}

	printf("start_page:%ld, nPageCnt:%ld, type:%d\r\n", start_page, nPageCnt, type);
	page = 0;
	page_idex = 0;
	partition_pagenum = partition_len/flash->page_size;
	printf("partition_pagenum:%ld\r\n", partition_pagenum);
	//for(page = 0; page < nPageCnt; page++)
	while(1)
	{
		//read data len page num
		if(page_idex == nPageCnt)
		{
			break;
		}

		//printf("page:%d\r\n", page);
		//if read page cnt more than partition page num error
		if(page > partition_pagenum)
		{
			printf("error: read page more than partition page num\r\n");
			return -1;
		}

		//判断此块是否坏块

		block_idex = (start_page+page)/PagePerBlock;
		//printf("block:%d, %d, %d\r\n", block_idex, start_page, page);
		if(spinand_flash_isbadblock(flash, block_idex*flash->erase_size) == 0)
		{
			printf("the block %ld is bad block\r\n", block_idex);
			page += PagePerBlock; //指向下一个块
		}
		else
		{
			printf("r.");
			for(i = 0; i < PagePerBlock; i++)
			{
				if (spi_nand_read(flash, block_idex*PagePerBlock + i, 0, pBuf, will_read_len) == -1)
				{
					printf("read partitin data fail\r\n");
					return -1;
				}
				pBuf += flash->page_size + flash->oob_size;
				page ++;
				page_idex++;

				//read data len page num
				if(page_idex == nPageCnt)
				{
					break;
				}
			}
		}
	}

	return 0;
}


//updater
static int spinand_reconvert(struct spinand_flash *flash, T_UPDATE_INFO *part_info, unsigned long* p_nBiosAddress, unsigned char *read_import_part_falg)
{
	unsigned char *pBuf;
	unsigned long r_start_page = 0;
	unsigned long r_date_len = 0;
	unsigned long r_partition_len = 0;

	unsigned long w_start_page = 0;
	unsigned long w_date_len = 0;
	unsigned long w_partition_len = 0;
	int ret = -1;


	if(part_info->update_flag == U_UPDATE_ING)//升级标志为正在升级,需要把主分区数据恢复到备份分区上
	{
		r_start_page = part_info->start_page;
		r_date_len = part_info->data_len;
		r_partition_len = part_info->partitin_len;

		part_info->data_len_bak = part_info->data_len;  //有效的数据长度应是主分区的

		w_start_page = part_info->start_page_bak;
		w_date_len = part_info->data_len_bak;
		w_partition_len = part_info->partitin_len_bak;
	}
	else//升级标志为已升级,需要把备份分区数据更新到主分区上
	{

		part_info->data_len = part_info->data_len_bak;//有效的数据长度应是备份分区的

		w_start_page = part_info->start_page;
		w_date_len = part_info->data_len;
		w_partition_len = part_info->partitin_len;

		r_start_page = part_info->start_page_bak;
		r_date_len = part_info->data_len_bak;
		r_partition_len = part_info->partitin_len_bak;
	}

	*read_import_part_falg = 0;
	pBuf = (unsigned char*)(*p_nBiosAddress);
	//读取分区数据
	ret = spinand_read_partition_data(flash, r_start_page, r_date_len, r_partition_len, pBuf, part_info->partition_type);
	if(ret == -1)
	{
		if(part_info->update_flag == U_UPDATE_ING)
		{
			*read_import_part_falg = 1;
		}
		return -1;
	}

	pBuf = (unsigned char*)(*p_nBiosAddress);
	//写入分区数据
	ret = spinand_write_partition_data(flash, w_start_page, w_date_len, w_partition_len, pBuf, part_info->partition_type);
	if(ret == -1)
	{
		if(part_info->update_flag == U_UPDATE_ENG)
		{
			*read_import_part_falg = 1;
		}

		return -1;
	}

	printf("spinand_reconvert finish\r\n");

	return 0;
}


int spinand_get_update_flag(struct spinand_flash *flash, T_UPDATE_INFO update_info[])
{
	ulong kernel_update_flag = 0;
	ulong rootfs_update_flag = 0;
	ulong app_update_flag = 0;

	ulong img_len = 0;

	ulong partition_offsset = 0;
	ulong partition_size = 0;

	kernel_update_flag = getenv_ulong("update_flag_kernel", 16, rootfs_update_flag);
	if(kernel_update_flag == 1){
		update_info[0].update_flag = U_UPDATE_ENG;//表示平台升级完成
		if(get_mtdparts_by_partition_name (CONFIG_BIOS_NAME, part_num, &partition_offsset, &partition_size) == -1){
			printf("update error: no find KERNEL partition.\n");
			return -1;
		}

		update_info[0].start_page = partition_offsset/flash->page_size;
		update_info[0].data_len = partition_size;
		update_info[0].partitin_len = partition_size;
		memset(update_info[0].partition_name,0, 8);
		memcpy(update_info[0].partition_name, CONFIG_BIOS_NAME, strlen(CONFIG_BIOS_NAME));

		if(get_mtdparts_by_partition_name (CONFIG_BIOSBK_NAME, part_num, &partition_offsset, &partition_size) == -1){
			printf("update error: no find KERBK partition.\n");
			return -1;
		}

		update_info[0].start_page_bak = partition_offsset/flash->page_size;
		update_info[0].data_len_bak = partition_size;
		update_info[0].partitin_len_bak = partition_size;
		memset(update_info[0].partition_name_bak,0, 8);
		memcpy(update_info[0].partition_name_bak, CONFIG_BIOSBK_NAME, strlen(CONFIG_BIOSBK_NAME));


		img_len = getenv_hex("img_len_kernel", img_len);
		if(img_len == 0 && img_len > update_info[0].partitin_len)
		{
			printf("update error: kernel_img_len is error :%ld\n", img_len);
			return -1;
		}

		update_info[0].data_len = img_len;
		update_info[0].data_len_bak = img_len;
		update_info[0].partition_type = PART_BIN_TYPE;
	}
	/*//与建浩讨论,暂时不考虑这个备份分区升级出错的情况
	else if(kernel_update_flag == 2){
		update_info[0].update_flag = UPDATE_ING;//表示平台升级失败
	}*/
	else{
		update_info[0].update_flag = U_UPDATE_NULL;//表示没有升级
	}

	rootfs_update_flag = getenv_ulong("update_flag_rootfs", 16, rootfs_update_flag);
	if(rootfs_update_flag == 1){
		update_info[1].update_flag = U_UPDATE_ENG;
		if(get_mtdparts_by_partition_name (CONFIG_ROOT_NAME, part_num, &partition_offsset, &partition_size) == -1){
			printf("update error: no find ROOTFS partition.\n");
			return -1;
		}

		update_info[1].start_page = partition_offsset/flash->page_size;
		update_info[1].data_len = partition_size;
		update_info[1].partitin_len = partition_size;
		memset(update_info[1].partition_name,0, 8);
		memcpy(update_info[1].partition_name, CONFIG_ROOT_NAME, strlen(CONFIG_ROOT_NAME));

		if(get_mtdparts_by_partition_name (CONFIG_ROOTBK_NAME, part_num, &partition_offsset, &partition_size) == -1){
			printf("update error: no find ROOTBK partition.\n");
			return -1;
		}

		update_info[1].start_page_bak = partition_offsset/flash->page_size;
		update_info[1].data_len_bak = partition_size;
		update_info[1].partitin_len_bak = partition_size;
		memset(update_info[1].partition_name_bak,0, 8);
		memcpy(update_info[1].partition_name_bak, CONFIG_ROOTBK_NAME, strlen(CONFIG_ROOTBK_NAME));

		img_len = getenv_hex("img_len_rootfs", img_len);
		if(img_len == 0 && img_len > update_info[1].partitin_len)
		{
			printf("update error: rootfs_img_len is error :%ld\n", img_len);
			return -1;
		}

		update_info[1].data_len = img_len;
		update_info[1].data_len_bak = img_len;
		update_info[1].partition_type = PART_FS_TYPE;

	}
	/*//与建浩讨论,暂时不考虑这个备份分区升级出错的情况
	else if(rootfs_update_flag == 2){
		update_info[0].update_flag = UPDATE_ING;//表示平台升级失败
	}*/
	else{
		update_info[1].update_flag = U_UPDATE_NULL;
	}

	app_update_flag = getenv_ulong("update_flag_app", 16, app_update_flag);
	if(app_update_flag == 1){
		update_info[2].update_flag = U_UPDATE_ENG;
		if(get_mtdparts_by_partition_name (CONFIG_APP_NAME, part_num, &partition_offsset, &partition_size) == -1){
			printf("update error: no find APP partition.\n");
			return -1;
		}

		update_info[2].start_page = partition_offsset/flash->page_size;
		update_info[2].data_len = partition_size;
		update_info[2].partitin_len = partition_size;
		memset(update_info[2].partition_name,0, 8);
		memcpy(update_info[2].partition_name, CONFIG_APP_NAME, strlen(CONFIG_APP_NAME));

		if(get_mtdparts_by_partition_name (CONFIG_APPBK_NAME, part_num, &partition_offsset, &partition_size) == -1){
			printf("update error: no find APPBK partition.\n");
			return -1;
		}

		update_info[2].start_page_bak = partition_offsset/flash->page_size;
		update_info[2].data_len_bak = partition_size;
		update_info[2].partitin_len_bak = partition_size;
		memset(update_info[2].partition_name_bak,0, 8);
		memcpy(update_info[2].partition_name_bak, CONFIG_APPBK_NAME, strlen(CONFIG_APPBK_NAME));

		img_len = getenv_hex("img_len_app", img_len);
		if(img_len == 0 && img_len > update_info[2].partitin_len)
		{
			printf("update error: app_img_len is error :%ld\n", img_len);
			return -1;
		}

		update_info[2].data_len = img_len;
		update_info[2].data_len_bak = img_len;
		update_info[2].partition_type = PART_FS_TYPE;
	}
	/*//与建浩讨论,暂时不考虑这个备份分区升级出错的情况
	else if(app_update_flag == 2){
		update_info[0].update_flag = UPDATE_ING;//表示平台升级失败
	}*/
	else{
		update_info[2].update_flag = U_UPDATE_NULL;
	}


	return 0;
}


int spinand_set_update_flag(ulong index ,unsigned char update_flag)
{
	if(index == 0)
		setenv_hex("update_flag_kernel", update_flag);
	if(index == 1)
		setenv_hex("update_flag_rootfs", update_flag);
	if(index == 2)
		setenv_hex("update_flag_app", update_flag);

	return 0;
}




int spinand_update(struct spinand_flash *flash)
{
	unsigned long partiton_cnt = PARTITION_CNT;
	unsigned char update_asa_flag = 0;
	unsigned char read_import_part_falg = 0;
	//T_PARTITION_NAME_INFO *partition_info = NULL;
	unsigned char asa_name_buf[64] = {0};
	T_UPDATE_INFO update_info[PARTITION_CNT] = {{0}};
	unsigned long p_nBiosAddress;
	unsigned long i = 0;


	if(spinand_get_update_flag(flash, update_info) == -1){
		printf("error: get update info fail\r\n");
		return -1;
	}

	for(i = 0; i < PARTITION_CNT; i++){
		if(update_info[i].update_flag != U_UPDATE_NULL){
			break;
		}
	}

	if(i == PARTITION_CNT){
		printf("no need update\r\n");
		return 0;
	}

	p_nBiosAddress = (unsigned long)0x82000000;

	memcpy(asa_name_buf, &partiton_cnt, sizeof(unsigned long));
	//partition_info = (T_PARTITION_NAME_INFO *)(asa_name_buf + 4);

	//根据升级标志进行恢复主分区或者备份分区
	for(i = 0; i < PARTITION_CNT; i++){
		//memset(partition_info[i].update_flag, 0, 6);
		//memcpy(partition_info[i].update_flag, UPDATE_NULL, 1);
		if(update_info[i].update_flag == U_UPDATE_ING)//目前不考虑平台升级出错后需要恢复备份分区的情况
		{
			printf("update %s data to %s ,pls wait...\r\n", update_info[i].partition_name, update_info[i].partition_name_bak);
			if(-1 == spinand_reconvert(flash, &update_info[i], &p_nBiosAddress, &read_import_part_falg))
			{
				printf("error: spinand_reconvert update_flag =1 fail\r\n");
				//memcpy(partition_info[i].update_flag, UPDATE_ERR, strlen(UPDATE_ERR));//
				return -1;
			}
			update_asa_flag = 1;
		}
		else if(update_info[i].update_flag == U_UPDATE_ENG)//表示已升级
		{
			printf("update %s data form %s ,pls wait...\r\n", update_info[i].partition_name, update_info[i].partition_name_bak);
			if(-1 == spinand_reconvert(flash, &update_info[i], &p_nBiosAddress, &read_import_part_falg))
			{
				//由于是写主份区,
				if(read_import_part_falg == 1)
				{
					printf("error: write main partition data fail, partition:%ld\r\n", i);
					while(1);
				}

				//读备份分区失败
				printf("error: spinand_reconvert update_flag =2 fail\r\n");
				//memcpy(partition_info[i].update_flag, UPDATE_ERR, strlen(UPDATE_ERR));//
			}
			update_asa_flag = 1;
		}
		else
		{
			continue;//3表示无法再升级 和 0表示没有升级不需侨何处理直接从主分区启动
		}
	}


	if(update_asa_flag == 1)
	{
		for(i = 0; i < PARTITION_CNT; i++){
			if(update_info[i].update_flag != U_UPDATE_NULL){
				spinand_set_update_flag(i ,U_UPDATE_NULL);
				update_info[i].update_flag = U_UPDATE_NULL;
			}
		}
		saveenv();
	}
	return 0;

}

int spinand_update_main(void)
{
	struct spinand_flash *flash = NULL;
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;
	
	flash = spinand_flash_probe(bus, cs, speed, mode);
	if (!flash) {
		puts("update spinand probe failed.\n");
		return -1;
	}


	if(spinand_update(flash) == -1){
		spinand_flash_free(flash);
		puts("update spinand_update_main failed.\n");
		return -1;
	}

	spinand_flash_free(flash);
	flash = NULL;

	return 0;

}
#endif




