/*
 * Copyright (C) 2011 OMICRON electronics GmbH
 *
 * based on drivers/mtd/nand/nand_spl_load.c
 *
 * Copyright (C) 2011
 * Heiko Schocher, DENX Software Engineering, hs@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <malloc.h>
#include <common.h>
#include <spi_flash.h>
#include <spl.h>
#include <libfdt.h>

extern u32 part_num ;

#ifdef CONFIG_SPINAND_FLASH

#ifndef CONFIG_SYS_FDT_PAD
#define CONFIG_SYS_FDT_PAD 0x3000
#endif

extern char g_env_buf[CONFIG_ENV_SIZE];
DECLARE_GLOBAL_DATA_PTR;


/*
 * The main entry for SPI booting. It's necessary that SDRAM is already
 * configured and available since this code loads the main U-Boot image
 * from SPI into SDRAM and starts it from there.
 */
void spl_spinand_load_image(void)
{
	struct spinand_flash *flash;
	struct image_header *header;
	uint32_t dcrc = 0, scrc = 0;
	u32 flash_offset = 0;
	u32 partition_size = 0;
	u32 load_addr = 0;
	u32 load_len = 0;
	u32 read_bk_last = 0;
	unsigned long run_add = 0;
	unsigned long run_len = 0;
	char page_buf[2048] = {0};
	unsigned long partition_cnt = part_num;
	ulong fdtcontroladdr = 0;
	char *splbuf = NULL;
	ulong image_flag = 0; 
	ulong	of_size = 0;
	ulong	of_len = 0;
	char out_buf[64] = {0};

	/*
	 * Load U-Boot image from SPI flash into RAM
	 */

	flash = spinand_flash_probe(CONFIG_SPL_SPI_BUS, CONFIG_SPL_SPI_CS,
				CONFIG_SF_DEFAULT_SPEED, SPI_MODE_3);
	if (!flash) {
		puts("spinand probe failed.\n");
		hang();
	}

	//bad block
	//printf("spl_spinand_load_image spinand_scan_all_block \n");
	if(0)//scan_all_block_spinand(flash) == -1)
	{
		puts("scan_all_block_spinand failed.\n");
		hang();
	}

	//spinand_set_no_protection(flash);

	//read page  0 for UBOOT checkout DTB
	splbuf = (void *)(CONFIG_SPL_TEXT_BASE);
	if(spinand_read_partition(flash, 0, flash->page_size,  (void *)splbuf, flash->page_size) == -1)
	{
		puts("spinand_read_partition failed.\n");
		hang();
	}

	
	if(gd->env_valid == 0){
		image_flag = getenv_ulong("a_uboot_flags", 16, image_flag);
	}
	else{
		pare_str_form_env(g_env_buf, "a_uboot_flags=", out_buf);
		image_flag = simple_strtoul(&out_buf[14], NULL, 16); //getenv_ulong("fdtcontroladdr", 16, fdtcontroladdr);
	}
	if(image_flag){
		if(get_mtdparts_by_partition_name (CONFIG_UBOOT_NAME, partition_cnt, &run_add, &run_len) == -1){
			puts("spinand_get_mtdparts UBOOT failed, and read kernel.\n");
			hang();
		}
	}else{
		if(get_mtdparts_by_partition_name (CONFIG_BIOS_NAME, partition_cnt, &run_add, &run_len) == -1){
			puts("spinand_get_mtdparts KERNEL failed.\n");
			hang();
		}
	}

	flash_offset = run_add;
	partition_size = run_len;

	//get uboot 
	debug("flash_offset:0x%x and partition_size:0x%x\n", flash_offset, partition_size);

RETRY_READ:
	/* use CONFIG_SYS_TEXT_BASE as temporary storage area */
	//page_buf = (void *)(CONFIG_SYS_TEXT_BASE);

	/* Load u-boot, mkimage header is 64 bytes. */
	if(spinand_read_partition(flash, flash_offset, flash->page_size,  (void *)page_buf, flash->page_size) == -1)
	{
		puts("spinand_read_partition failed.\n");
		hang();
	}

	header = (struct image_header *)page_buf;

	scrc = header->ih_dcrc;
	spl_parse_image_header(header);

	if(image_flag && spl_image.os != IH_OS_U_BOOT){
		puts("UBOOT is no img file\n");
		hang();
	}

	load_addr = spl_image.load_addr;
	load_len = spl_image.size;

	debug("load_addr:0x%x , entry_point:0x%x, size:0x%x\n", spl_image.load_addr, spl_image.entry_point, spl_image.size);

	if(spinand_read_partition(flash, flash_offset, partition_size, (void *)load_addr, load_len) == -1){
		puts("spinand_read_partition failed.\n");
		hang();
	}

	dcrc = crc32 (0, (const unsigned char *)load_addr + sizeof(struct image_header), load_len - sizeof(struct image_header));
	if(dcrc != be32_to_cpu(scrc)){
		printf("dcrc:0x%x and scrc:0x%x is diffrent, and read back partiton\n", dcrc, be32_to_cpu(scrc));
		if(read_bk_last == 1){
			puts("read bk partition crc error.\n");
			hang();
		}
		else if(image_flag && spl_image.os == IH_OS_U_BOOT){//uboot crc error and uboot bk no exist, fail
			if(get_mtdparts_by_partition_name (CONFIG_UBOOTBK_NAME, partition_cnt, &run_add, &run_len) == -1){
				puts("spinand_get_mtdparts UB_BK failed.\n");
				hang();
			}
			
			flash_offset = run_add;
			partition_size = run_len;
			read_bk_last = 1;
			printf("read uboot bk flash_offset:0x%x and partition_size:0x%x\n", flash_offset, partition_size);
			goto RETRY_READ;
		}
		else if(!image_flag && spl_image.os == IH_OS_LINUX){//kernel crc error  and read kernelbk  partition
			if(get_mtdparts_by_partition_name (CONFIG_BIOSBK_NAME, partition_cnt, &run_add, &run_len) == -1){
				puts("spinand_get_mtdparts KERBK failed.\n");
				hang();
			}
			
			flash_offset = run_add;
			partition_size = run_len;
			read_bk_last = 1;
			printf("read kernel bk flash_offset:0x%x and partition_size:0x%x\n", flash_offset, partition_size);
			goto RETRY_READ;
		}
		else {//kernel crc error and read kernelbk  partition
			puts("kernel crc error,and no kernelbk partition to read, failed.\n");
			hang();
		}
	}

	if(spl_image.os == IH_OS_LINUX)
	{
		unsigned i = 0;
		char *buf = NULL;

		/* for sky save spi parameter */

		buf = (char *)(spl_image.load_addr+sizeof(struct image_header));

		//SKY37D kernel spiflash param
		for (i=0;i<256;i++)
        {
			if (buf[i] == 0x53 && buf[i+1] == 0x50 && buf[i+2] == 0x49 && buf[i+3] == 0x50)// 0x50495053=SPIP
            {
				//puts("get spinand param\n");
                memcpy(((char*)spl_image.load_addr+ sizeof(struct image_header))+i+4, (splbuf+0x200), sizeof(T_SPI_NAND_PARAM));
                break;
            }
        }
	

		if(get_mtdparts_by_partition_name (CONFIG_DTB_NAME, partition_cnt, &run_add, &run_len) == -1){
			puts("spinand_get_mtdparts DTB failed.\n");
			hang();
		}

		flash_offset = run_add;
		partition_size = run_len;
		//get DTB 
		if(gd->env_valid == 0){
			fdtcontroladdr = getenv_ulong("fdtcontroladdr", 16, fdtcontroladdr);
		}
		else{
			pare_str_form_env(g_env_buf, "fdtcontroladdr=", out_buf);
			fdtcontroladdr = simple_strtoul(&out_buf[15], NULL, 16); //getenv_ulong("fdtcontroladdr", 16, fdtcontroladdr);
		}

		if(fdtcontroladdr == 0)
		{
			puts("error fdtcontroladdr == 0\n");
			hang();
		}
		
		debug("fdtcontroladdr:0x%x, flash_offset:0x%x and partition_size:0x%x\n", fdtcontroladdr, flash_offset, partition_size);
		/* Load u-boot, mkimage header is 64 bytes. */
		if(spinand_read_partition(flash, flash_offset, partition_size,  (void *)fdtcontroladdr, partition_size) == -1)
		{
			puts("spinand_read_partition failed.\n");
			hang();
		}

		if(!image_flag)
		{
			//puts("##==fdt chosen start==##\n");
			of_size = fdt_totalsize(fdtcontroladdr);
			of_len = of_size + CONFIG_SYS_FDT_PAD;

			//printf("%s, line:%d, fdt:0x%x, of_len:0x%x!\n", __func__, __LINE__, fdtcontroladdr, of_len);
			spl_fdt_set_totalsize(fdtcontroladdr, of_len);

			spl_fdt_chosen((void *)fdtcontroladdr, 1);
			//puts("##==fdt chosen end  ==##\n");
		}
	}
}

#endif
