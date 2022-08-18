/*
 * Common SPI flash Interface
 *
 * Copyright (C) 2008 Atmel Corporation
 * Copyright (C) 2013 Jagannadha Sutradharudu Teki, Xilinx Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef _SPI_FLASH_H_
#define _SPI_FLASH_H_

#include <spi.h>
#include <linux/types.h>
#include <linux/compiler.h>


#define PARTITION_MAX_NUM		20

#define OFFSET_NOT_SPECIFIED	(~0llu)

/* special size referring to all the remaining space in a partition */
#define SIZE_REMAINING		(~0llu)

/* minimum partition size */
#define MIN_PART_SIZE		4096

/* this flag needs to be set in part_info struct mask_flags
 * field for read-only partitions */
#define MTD_WRITEABLE_CMD		1


struct spl_part_info {
	char *name;			/* partition name */
	u8 auto_name;			/* set to 1 for generated name */
	u64 size;			/* total size of the partition */
	u64 offset;			/* offset within device */
	u32 mask_flags;			/* kernel MTD mask flags */
	u32 sector_size;		/* size of sector */
};


struct spi_flash;
/**
  *because of some spi flash is difference of status register difinition.
  *this structure use mapping the status reg function and corresponding.
*/
struct flash_status_reg
{
	u32		jedec_id;	
	u16		ext_id;
	unsigned b_wip:4;		/*write in progress*/
	unsigned b_wel:4;		/*wrute ebabke latch*/
	unsigned b_bp0:4;		/*block protected 0*/
	unsigned b_bp1:4;		/*block protected 1*/
	unsigned b_bp2:4;		/*block protected 2*/
	unsigned b_bp3:4;		/*block protected 3*/
	unsigned b_bp4:4;		/*block protected 4*/
	unsigned b_srp0:4;		/*status register protect 0*/
	
	unsigned b_srp1:4;		/*status register protect 1*/
	unsigned b_qe:4;		/*quad enable*/
	unsigned b_lb:4;		/*write protect control and status to the security reg.*/
/*
	unsigned b_reserved0:4;
	unsigned b_reserved1:4;
	unsigned b_reserved2:4;
*/
	unsigned b_cmp:4;		/*conjunction bp0-bp4 bit*/
	unsigned b_sus:4;		/*exec an erase/program suspend command*/
	
	/*
	* ADS:The Current Address Mode bit is a read only bit in the status Register-3 that
	* indicates which address mode the device is currently operating in.
	* 0:3-Byte Address Mode. 1:4-Byte Address Mode
	*/
	unsigned b_as:8;		/*0:3-Byte Address Mode. 1:4-Byte Address Mode*/

	/*
	* ADP:The ADP bit is a non-volatile bit that determines the initial address mode when
	* the device is powered on or reset. This bit is only used during the power on or device 
	* reset initialization period, and it is only writable. When ADP=0 (Factory default),the device
	* will power up into 3-Byte Address Mode, the Extended Address Register must be used
	* to access memory regions beyond 128Mb.
	* When ADP=1,the device will power up into 4-Byte address Mode directly
	* 0:3-Byte Address Mode. 1:4-Byte Address Mode
	*/
	unsigned b_ap:8;		/*0:3-Byte Address Mode. 1:4-Byte Address Mode*/
	
	int (*read_sr)(struct spi_flash *, u32*);
	int (*write_sr)(struct spi_flash *, u32);
};

struct spi_xfer_ctl
{
	u8		dummy_len;
	u8		erase_opcode;

	u8		tx_opcode;
	u8		rx_opcode;

	u8		txd_bus_width;
	u8		rxd_bus_width;
	
	u8		txa_bus_width;
	u8		rxa_bus_width;	
};


/**
 * struct spi_flash - SPI flash structure
 *
 * @spi:		SPI slave
 * @name:		Name of SPI flash
 * @size:		Total flash size
 * @page_size:		Write (page) size
 * @sector_size:	Sector size
 * @erase_size:		Erase size
 * @bank_read_cmd:	Bank read cmd
 * @bank_write_cmd:	Bank write cmd
 * @bank_curr:		Current flash bank
 * @poll_cmd:		Poll cmd - for flash erase/program
 * @erase_cmd:		Erase cmd 4K, 32K, 64K
 * @memory_map:		Address of read-only SPI flash access
 * @read:		Flash read ops: Read len bytes at offset into buf
 *			Supported cmds: Fast Array Read
 * @write:		Flash write ops: Write len bytes from buf into offeset
 *			Supported cmds: Page Program
 * @erase:		Flash erase ops: Erase len bytes from offset
 *			Supported cmds: Sector erase 4K, 32K, 64K
 * return 0 - Sucess, 1 - Failure
 */
struct spi_flash {
	struct spi_slave *spi;
	const char *name;
	u32 flashid; // cdh:add
	u32 size;
	u32 page_size;
	u32 sector_size;
	u32 erase_size;
#ifdef CONFIG_SPI_FLASH_BAR
	u8 bank_read_cmd;
	u8 bank_write_cmd;
	u8 bank_curr;
#endif
	u8 poll_cmd;
	u8 erase_cmd;

	struct flash_status_reg stat_reg;
	struct spi_xfer_ctl xfer_ctl;
	u8 		bus_width;
	u32 	flags;

	void *memory_map;
	int (*read)(struct spi_flash *flash, u32 offset, size_t len, void *buf);
	int (*write)(struct spi_flash *flash, u32 offset, size_t len,
			const void *buf);
	int (*erase)(struct spi_flash *flash, u32 offset, size_t len);
};

typedef struct
{
    u32 chip_id;
    u32 total_size;             ///< flash total size in bytes
    u32	page_size;       ///< total bytes per page
    u32	program_size;    ///< program size at 02h command
    u32	erase_size;      ///< erase size at d8h command 
    u32	clock;           ///< spi clock, 0 means use default clock 
    
    //chip character bits:
    //bit 0: under_protect flag, the serial flash under protection or not when power on
    //bit 1: fast read flag    
    u8  flag;            ///< chip character bits
    u8	protect_mask;
	u8  reserved1;
    u8  reserved2;
    u8  des_str[32];		   //ÃèÊö·û                                    
}T_SFLASH_PHY_INFO;

/**
 * @brief  serial flash param define
 */
typedef struct tagSFLASH_PARAM
{
    u32   id;                     ///< flash id
    u32   total_size;             ///< flash total size in bytes
    u32   page_size;              ///< bytes per page
    u32   program_size;           ///< program size at 02h command
    u32   erase_size;             ///< erase size at d8h command 
    u32   clock;                  ///< spi clock, 0 means use default clock 

    //chip character bits:
    //bit 0: under_protect flag, the serial flash under protection or not when power on
    //bit 1: fast read flag, the serial flash support fast read or not(command 0Bh)
    //bit 2: AAI flag, the serial flash support auto address increment word programming
    //bit 3: support dual write or no
    //bit 4: support dual read or no
    //bit 5: support quad write or no
    //bit 6: support quad read or no
    //bit 7: the second status command (35h) flag,if use 4-wire(quad) mode,the bit must be is enable
    u8    flag;                   ///< chip character bits
    u8    protect_mask;           ///< protect mask bits in status register:BIT2:BP0, BIT3:BP1, BIT4:BP2, BIT5:BP3, BIT7:BPL
#ifdef USE_FOR_BURNTOOL_PARAM
    u8    reserved1;
    u8    reserved2;
#else
	u8 	*desc;
#endif
}T_SFLASH_PARAM;

struct spi_flash *spi_flash_probe(unsigned int bus, unsigned int cs,
		unsigned int max_hz, unsigned int spi_mode);
		
void spi_flash_free(struct spi_flash *flash);


static inline int spi_flash_read(struct spi_flash *flash, u32 offset,
		size_t len, void *buf)
{
	return flash->read(flash, offset, len, buf);
}

static inline int spi_flash_write(struct spi_flash *flash, u32 offset,
		size_t len, const void *buf)
{
	return flash->write(flash, offset, len, buf);
}

static inline int spi_flash_erase(struct spi_flash *flash, u32 offset,
		size_t len)
{
	//printf("cdh:offset:0x%x, len:0x%x\n", offset, len);
	return flash->erase(flash, offset, len);
}


////////////spinand
struct spinand_flash;
/**
*because of some spi flash is difference of status register difinition.
*this structure use mapping the status reg function and corresponding.
*/
struct spinand_flash_status_reg
{
	u32 	jedec_id;	
	u16 	ext_id;
	unsigned b_wip:4;		/*write in progress*/
	unsigned b_wel:4;		/*wrute ebabke latch*/
	unsigned b_bp0:4;		/*block protected 0*/
	unsigned b_bp1:4;		/*block protected 1*/
	unsigned b_bp2:4;		/*block protected 2*/
	unsigned b_bp3:4;		/*block protected 3*/
	unsigned b_bp4:4;		/*block protected 4*/
	unsigned b_srp0:4;		/*status register protect 0*/

	unsigned b_srp1:4;		/*status register protect 1*/
	unsigned b_qe:4;		/*quad enable*/
	unsigned b_lb:4;		/*write protect control and status to the security reg.*/
	/*
	unsigned b_reserved0:4;
	unsigned b_reserved1:4;
	unsigned b_reserved2:4;
	*/
	unsigned b_cmp:4;		/*conjunction bp0-bp4 bit*/
	unsigned b_sus:4;		/*exec an erase/program suspend cmd_buf*/
	unsigned b_efail:4; 	/**/
	unsigned b_pfail:4; 	/**/
};



/**
* struct spi_flash - SPI flash structure
*
* @spi: 	SPI slave
* @name:		Name of SPI flash
* @size:		Total flash size
* @page_size:		Write (page) size
* @sector_size: Sector size
* @erase_size:	Erase size
* @bank_read_cmd:	Bank read cmd
* @bank_write_cmd: Bank write cmd
* @bank_curr:		Current flash bank
* @poll_cmd:		Poll cmd - for flash erase/program
* @erase_cmd:		Erase cmd 4K, 32K, 64K
* @memory_map:	Address of read-only SPI flash access
* @read:		Flash read ops: Read len bytes at offset into buf
*			Supported cmds: Fast Array Read
* @write:		Flash write ops: Write len bytes from buf into offeset
*			Supported cmds: Page Program
* @erase:		Flash erase ops: Erase len bytes from offset
*			Supported cmds: Sector erase 4K, 32K, 64K
* return 0 - Sucess, 1 - Failure
*/
struct spinand_flash {
	struct spi_slave *spi;
	const char *name;
	u32 flashid; // cdh:add
	u32 size;
	u32 page_size;
	u32 sector_size;
	u32 erase_size;
	u16	planecnt;
#ifdef CONFIG_SPI_FLASH_BAR
	u8 bank_read_cmd;
	u8 bank_write_cmd;
	u8 bank_curr;
#endif
	u8 poll_cmd;
	u8 erase_cmd;
	

	struct spinand_flash_status_reg spinand_stat_reg;
	struct spi_xfer_ctl xfer_ctl;
	u8		bus_width;
	u32 	flags;

	void *memory_map;

	u32 oob_size;
	u32 badblock_offset;
	u32 oob_idex;

	/*|--------------------64bytes------------------------------|*/
	/*|---12---|-4--|---12----|-4--|---12---|-4--|---12----|-4--|*/
	/*|-seglen-|skip|-segllen-|skip|-seglen-|skip|-segllen-|skip|*/
	unsigned long 	oob_up_skiplen;
	unsigned long 	oob_seglen;
	unsigned long 	oob_down_skiplen;	
	unsigned long 	oob_seg_perpage;



	int (*spinand_read)(struct spinand_flash *flash, u32 offset, size_t len, void *buf,  void *oob, size_t oob_len);
	int (*spinand_write)(struct spinand_flash *flash, u32 offset, size_t len,const void *buf,  const void *oob, size_t oob_len);
	int (*spinand_erase)(struct spinand_flash *flash, u32 offset, size_t len);
	int (*spinand_isbadblock)(struct spinand_flash *flash, u32 offset);
	int (*spinand_setbadblock)(struct spinand_flash *flash, u32 offset);
};



typedef struct Nand_phy_info{
	u32  chip_id;//chip id
	u16  page_size; //page size
	u16  page_per_blk; //page of one blok
	u16  blk_num;//total block number
	u16  group_blk_num;//the same concept as die, according to nand's struture
	u16  plane_blk_num;   
	u8	 spare_size;//spare
	u8	 col_cycle;//column address cycle

	u8	 lst_col_mask;//
	u8	 row_cycle;//
	u8	 delay_cnt;

	u8	 custom_nd;//nand type flag, used to detect the original invilid block
	u32  flag;//
	u32  cmd_len;//nandflash command length
	u32  data_len;//nandflash data length
	u8	 des_str[32];//descriptor string
}T_SPI_NAND_PARAM;


#define		SPINAND_PARAM_OFFSET	            0x200 


unsigned long efuse_read(void);

int spl_device_parse(const char *const mtd_dev, ulong flash_size);
int get_mtdparts_by_partition_name (char *partition_name, unsigned long partition_num, unsigned long *run_add, unsigned long *run_len);
int get_mtdparts_by_partition_flash_offset (unsigned long flash_offset, unsigned long partition_num, unsigned long *run_add, unsigned long *run_len);

void spl_fdt_set_totalsize(void *fdt, uint32_t val);
int spl_fdt_chosen(void *fdt, int force);

int read_logo_data(unsigned long base_addr);



struct spinand_flash *spinand_flash_probe(unsigned int bus, unsigned int cs,unsigned int max_hz, unsigned int spi_mode);
void spinand_flash_free(struct spinand_flash *flash);
void spinand_set_no_protection(struct spinand_flash *flash);
int  spinand_erase_partition(struct spinand_flash *flash,		 u32 start_pos,  u32 partition_size);
int  spinand_write_partition_with_oob(struct spinand_flash *flash, u32 start_pos, u32 partition_size, u8 *buf, u32 buf_len);
int  spinand_write_partition(struct spinand_flash *flash, u32 start_pos, u32 partition_size, u8 *buf, u32 buf_len);
int  spinand_read_partition_with_oob(struct spinand_flash *flash, u32 start_pos, u32 partition_size, u8 *buf, u32 buf_len);
int  spinand_read_partition(struct spinand_flash *flash, u32 start_pos, u32 partition_size, u8 *buf, u32 buf_len);


static inline int spinand_flash_read(struct spinand_flash *flash, u32 offset,
		size_t len, void *buf, void *oob, size_t oob_len)
{
	return flash->spinand_read(flash, offset, len, buf, oob, oob_len);
}

static inline int spinand_flash_write(struct spinand_flash *flash, u32 offset,
		size_t len, const void *buf, const void *oob, size_t oob_len)
{
	return flash->spinand_write(flash, offset, len, buf, oob, oob_len);
}

static inline int spinand_flash_erase(struct spinand_flash *flash, u32 offset,
		size_t len)
{
	//printf("cdh:offset:0x%x, len:0x%x\n", offset, len);
	return flash->spinand_erase(flash, offset, len);
}
	
static inline int spinand_flash_isbadblock(struct spinand_flash *flash, u32 offset)
{
	//printf("cdh:offset:0x%x, len:0x%x\n", offset, len);
	return flash->spinand_isbadblock(flash, offset);
}

static inline int spinand_flash_setbadblock(struct spinand_flash *flash, u32 offset)
{
	//printf("cdh:offset:0x%x, len:0x%x\n", offset, len);
	return flash->spinand_setbadblock(flash, offset);
}

void spi_boot(void) __noreturn;



#endif /* _SPI_FLASH_H_ */
