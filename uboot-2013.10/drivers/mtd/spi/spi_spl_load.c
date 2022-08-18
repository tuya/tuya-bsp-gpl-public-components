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

#include <common.h>
#include <spi_flash.h>
#include <spl.h>
#include "exports.h"

#include <libfdt.h>


extern u32 part_num;


#ifndef CONFIG_SPINAND_FLASH


extern T_SFLASH_PHY_INFO g_spiflash_parm;

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
void spl_spi_load_image(void)
{
	struct spi_flash *flash;
	struct image_header *header;
	unsigned long run_add = 0;
	unsigned long run_len = 0;
	unsigned long partition_cnt = part_num;
	u32 flash_offset = 0;
	u32 partition_size = 0;
	ulong image_flag = 0; 
	u8 head_buf[64] = {0};
	ulong fdtcontroladdr = 0;
	char *buf_temp = NULL;
	char *splbuf = (char *)CONFIG_SPL_TEXT_BASE;
	ulong	of_size = 0;
	ulong	of_len = 0;
	char out_buf[64] = {0};

	/*
	 * Load U-Boot image from SPI flash into RAM
	 */

	flash = spi_flash_probe(CONFIG_SPL_SPI_BUS, CONFIG_SPL_SPI_CS,
				CONFIG_SF_DEFAULT_SPEED, SPI_MODE_3);
	if (!flash) {
		puts("SPI probe failed.\n");
		hang();
	}

	//read page  0 for UBOOT checkout DTB
	if(spi_flash_read(flash, 0, 512, splbuf) == -1)
	{
		puts("spi_flash_read page0 failed.\n");
		hang();
	}

	/* judge for entry uboot or kernel */
	if(gd->env_valid == 0)
	{
		image_flag = getenv_ulong("a_uboot_flags", 16, image_flag);
	}
	else
	{
		pare_str_form_env(g_env_buf, "a_uboot_flags=", out_buf);
		image_flag = simple_strtoul(&out_buf[14], NULL, 16); //getenv_ulong("fdtcontroladdr", 16, fdtcontroladdr);
	}

	//printf("image_flag:%x\n",image_flag);
	if(image_flag){
		if(get_mtdparts_by_partition_name (CONFIG_UBOOT_NAME, partition_cnt, &run_add, &run_len) == -1){
			puts("spinor_get_mtdparts UBOOT failed, and read kernel.\n");
			hang();
		}
	}else{
		if(get_mtdparts_by_partition_name (CONFIG_BIOS_NAME, partition_cnt, &run_add, &run_len) == -1){
			puts("spinor_get_mtdparts KERNEL failed.\n");
			hang();
		}
	}

	flash_offset = run_add;
	partition_size = run_len;
	//printf("flash_offset:%d, partition_size:%d\r\n", flash_offset, partition_size);
	/* use CONFIG_SYS_TEXT_BASE as temporary storage area */
	header = (struct image_header *)(head_buf);

	/* Load u-boot, mkimage header is 64 bytes. */
	spi_flash_read(flash, flash_offset, 0x40, (void *)header);

	/* uimage & uboot.img must be here */
	spl_parse_image_header(header);

	if(image_flag && spl_image.os != IH_OS_U_BOOT){
		puts("UBOOT is no img file\n");
		hang();
	}

	//printf("size:0x%x, load_addr:0x%x\r\n", spl_image.size, spl_image.load_addr);
	/* read */
	spi_flash_read(flash, flash_offset, spl_image.size, (void *)spl_image.load_addr);

	if(spl_image.os == IH_OS_LINUX){
		unsigned i = 0, index = 0;
		char *buf = NULL;

		/* for sky save spi parameter */
		for (i=0; i<512; i++){
			if (splbuf[i] == 0x53 && splbuf[i+1] == 0x50 && splbuf[i+2] == 0x49 && splbuf[i+3] == 0x50)// 0x50495053=SPIP
            {
            	index = i;
                break;
            }
        }
	
		buf = (char *)(spl_image.load_addr+sizeof(struct image_header));

		if(index > 0)
		{
			//SKY37D kernel spiflash param
			for (i=0;i<256;i++)
	        {
				if (buf[i] == 0x53 && buf[i+1] == 0x50 && buf[i+2] == 0x49 && buf[i+3] == 0x50)// 0x50495053=SPIP
	            {
	                memcpy(((char*)spl_image.load_addr+ sizeof(struct image_header))+i+4, (splbuf+index+4), sizeof(T_SFLASH_PHY_INFO));
	                break;
	            }
	        }
		}
		
	}

	//read dtb
	if(get_mtdparts_by_partition_name (CONFIG_DTB_NAME, partition_cnt, &run_add, &run_len) == -1){
		puts("spinor_get_mtdparts DTB failed, and read kernel.\n");
		hang();
	}

	flash_offset = run_add;
	partition_size = run_len;

	//printf("flash_offset:%d, partition_size:%d\r\n", flash_offset, partition_size);
	if(gd->env_valid == 0)
	{
		fdtcontroladdr = getenv_ulong("fdtcontroladdr", 16, fdtcontroladdr);
	}
	else
	{
		pare_str_form_env(g_env_buf, "fdtcontroladdr=", out_buf);
		fdtcontroladdr = simple_strtoul(&out_buf[15], NULL, 16); //getenv_ulong("fdtcontroladdr", 16, fdtcontroladdr);
	}

	if(fdtcontroladdr == 0)
	{
		puts("error fdtcontroladdr == 0\n");
		hang();
	}
	buf_temp = (char *)fdtcontroladdr;
	//printf("fdtcontroladdr:%x, partition_size:%d\r\n", fdtcontroladdr, partition_size);
	if(spi_flash_read(flash, flash_offset, partition_size, (void *)fdtcontroladdr) == -1) {
		puts("spi_flash_read failed.\n");
		hang();
	}

	if(!image_flag){
		//puts("##==fdt chosen start==##\n");
		of_size = fdt_totalsize(fdtcontroladdr);
		of_len = of_size + CONFIG_SYS_FDT_PAD;

		//printf("%s, line:%d, fdt:0x%x, of_len:0x%x!\n", __func__, __LINE__, fdtcontroladdr, of_len);
		spl_fdt_set_totalsize(fdtcontroladdr, of_len);

		spl_fdt_chosen((void *)fdtcontroladdr, 1);
		//puts("##==fdt chosen end  ==##\n");
	}
}

#endif
