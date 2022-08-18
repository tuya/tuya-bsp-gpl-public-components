/*
 * (C) Copyright 2000-2004
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
//#include <nios.h>
#include <asm/io.h>
#include <spi_flash.h>
#include <spi.h>
#include <malloc.h>

#include <libfdt_env.h>
#include <libfdt.h>



#ifndef CONFIG_SF_DEFAULT_SPEED
# define CONFIG_SF_DEFAULT_SPEED	1000000
#endif

#ifndef CONFIG_SF_DEFAULT_MODE
# define CONFIG_SF_DEFAULT_MODE		SPI_MODE_3
#endif

#ifndef CONFIG_SF_DEFAULT_CS
# define CONFIG_SF_DEFAULT_CS		0
#endif

#ifndef CONFIG_SF_DEFAULT_BUS
# define CONFIG_SF_DEFAULT_BUS		0
#endif

#define FDT_ALIGN(x, a)		(((x) + (a) - 1) & ~((a) - 1))
#define FDT_TAGALIGN(x)		(FDT_ALIGN((x), FDT_TAGSIZE))
#define FDT_FIRST_SUPPORTED_VERSION	0x10
#define FDT_LAST_SUPPORTED_VERSION	0x11
#define FDT_SW_MAGIC				(~FDT_MAGIC)
#define FDT_ERR_BADVERSION			10
#define FDT_ERR_BADSTATE			7
#define FDT_ERR_BADMAGIC			9
#define FDT_ERR_TRUNCATED			8
#define FDT_ERR_BADSTRUCTURE		11
#define FDT_ERR_BADOFFSET			4
#define FDT_ERR_NOTFOUND			1

flash_info_t flash_info[CONFIG_SYS_MAX_FLASH_BANKS];
struct spl_part_info *spl_mtdparts[PARTITION_MAX_NUM];

extern u32 part_num;
extern char g_env_buf[CONFIG_ENV_SIZE];
DECLARE_GLOBAL_DATA_PTR;


int spl_fdt_path_offset(const void *fdt, const char *path);



/*----------------------------------------------------------------------*/
unsigned long flash_init (void)
{
	int i;
	unsigned long addr;
	flash_info_t *fli = &flash_info[0];
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;
	struct spi_flash *flash = NULL;
	
	flash = spi_flash_probe(bus, cs, speed, mode);
	if (!flash) {
		printf("Failed to initialize SPI flash at %u:%u\n", bus, cs);
		return 1;
	}

	fli->size = flash->size;
	fli->sector_count = flash->size/flash->erase_size; // cdh notes:use erase size , not sector size
	fli->flash_id = flash->flashid;
	//printf("cdh:%s, flash->size:0x%x\n", __func__, flash->size);
	//printf("cdh:%s, flash->erase_size:0x%x\n", __func__, flash->erase_size);
	//printf("cdh:%s, fli->sector_count:0x%x\n", __func__, fli->sector_count);
	//printf("cdh:%s, fli->flash_id:0x%x\n", __func__, fli->flash_id);
	
	addr = CONFIG_SYS_FLASH_BASE;
	for (i=0; i<fli->sector_count; ++i) {
		fli->start[i] = addr;
		addr += flash->erase_size;	// cdh notes:use erase size , not sector size
		fli->protect[i] = 0;
	}

	/* Assign spi_flash ops */
	fli->write = flash->write;
	fli->erase = flash->erase;
	fli->read  = flash->read;
	fli->flash = flash;
	//printf("cdh:flash_size:0x%x\n", fli->size);

	return fli->size;
}

/*--------------------------------------------------------------------*/
void flash_print_info (flash_info_t * info)
{
	int i, k;
	unsigned long size;
	int erased;
	volatile unsigned char *flash;

	printf ("  Size: %ld KB in %d Sectors\n", info->size >> 10, info->sector_count);
	printf ("  Sector Start Addresses:");
	for (i=0; i<info->sector_count; ++i) {

		/* Check if whole sector is erased */
		if (i != (info->sector_count - 1))
			size = info->start[i + 1] - info->start[i];
		else
			size = info->start[0] + info->size - info->start[i];
		erased = 1;
		flash = (volatile unsigned char *) info->start[i];
		for (k = 0; k < size; k++) {
			if (*flash++ != 0xff) {
				erased = 0;
				break;
			}
		}

		/* Print the info */
		if ((i % 5) == 0)
			printf ("\n   ");
		printf (" %08lX%s%s", info->start[i], erased ? " E" : "  ",
			info->protect[i] ? "RO " : "   ");
	}
	printf ("\n");
}

/*-------------------------------------------------------------------*/


int flash_erase (flash_info_t * info, int s_first, int s_last)
{
	int prot;
	int sect;
	int ret = -1;

	/* Some sanity checking */
	if ((s_first < 0) || (s_first > s_last)) {
		printf ("- no sectors to erase\n");
		return 1;
	}
	prot = 0;
	for (sect=s_first; sect<=s_last; ++sect) {
		if (info->protect[sect]) {
			prot++;
		}
	}
	
	if (prot) {
		printf ("- Warning: %d protected sectors will not be erased!\n", prot);
	} else {
		printf ("\n");
	}

#ifdef DEBUG
	for (sect=s_first; sect<=s_last; sect++) {
		printf("- Erase: Sect: %i @ 0x%08x\n", sect,  info->start[sect]);
	}
#endif

	for (sect=s_first; sect<=s_last; sect++) {
		if (info->protect[sect] == 0) {	/* not protected */
			#ifdef CONFIG_SPINAND_FLASH
			//printf ("info->start[%d]:%d, erase_size:%d\n", sect, info->start[sect], info->spi_nand_flash->erase_size);
			if(info->spi_nand_flash->spinand_isbadblock(info->spi_nand_flash, info->start[sect]) == 0)
			{
				printf ("the block is badblock, info->start[%d]:%d\n", sect, (u32)info->start[sect]);
				continue;
			}

			ret = info->spinand_erase(info->spi_nand_flash, info->start[sect], info->spi_nand_flash->erase_size);
			#else
			ret = info->erase(info->flash, info->start[sect], info->flash->erase_size);
			#endif
			if (ret < 0) {
				#ifdef CONFIG_SPINAND_FLASH
				info->spi_nand_flash->spinand_setbadblock(info->spi_nand_flash, info->start[sect]);
				printf("erase fail, set bad block, info->start[%d]:%d\n", sect, (u32)info->start[sect]);
				continue;
				#else
				printf("SF: erase failed\n");
				return 1;
				#endif
			}else{
				puts(".");
			}
		}
	}

	printf ("\n");

	return 0;
}


/*-----------------------------------------------------------------------
 * Copy memory to flash, returns:
 * 0 - OK
 * 1 - write timeout
 * 2 - Flash not erased
 */
int write_buff (flash_info_t * info, uchar * src, ulong addr, ulong cnt)
{
	int ret = -1;

	#ifdef CONFIG_SPINAND_FLASH
	int i= 0;
	int page_num = cnt/info->spi_nand_flash->page_size;
	//printf("page_num:%d, %d\n", page_num, addr );
	if(page_num > 0)
	{
		for(i = 0; i < page_num; i++)
		{
			//printf("i:%d, %d\n", i, addr );
			ret = info->spinand_write(info->spi_nand_flash, (u32)addr, info->spi_nand_flash->page_size, (const void *)src, NULL, 0);
			if(ret != 0)
			{
				printf("spinand SF: write failed\n");
				return 1;
			}
			addr += info->spi_nand_flash->page_size;
			src += info->spi_nand_flash->page_size;
		}
		
	}

	//printf("cnt mod info->spi_nand_flash->page_size:%d\n", cnt%info->spi_nand_flash->page_size);
	if(cnt%info->spi_nand_flash->page_size != 0)
	{
		ret = info->spinand_write(info->spi_nand_flash, (u32)addr, cnt%info->spi_nand_flash->page_size, (const void *)src, NULL, 0);
		if(ret != 0)
		{
			printf("spinand SF: write failed\n");
			return 1;
		}
	}
	#else
	ret = info->write(info->flash, (u32)addr, cnt, (const void *)src);
	#endif
	
	if (ret != 0) {
		printf("SF: write failed\n");
		return 1;
	}

	return (0);
}


///spinand
/*----------------------------------------------------------------------*/
unsigned long spinand_flash_init (void)
{
	int i;
	unsigned long addr;
	flash_info_t *fli = &flash_info[0];
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;
	struct spinand_flash *flash = NULL;


	
	flash = spinand_flash_probe(bus, cs, speed, mode);
	if (!flash) {
		printf("Failed to initialize SPI flash at %u:%u\n", bus, cs);
		return 0;
	}
	//printf("spinand_flash_probe flash_size:%d\n", flash->size);
	fli->size = flash->size;
	fli->sector_count = flash->size/flash->erase_size; // cdh notes:use erase size , not sector size
	fli->flash_id = flash->flashid;
	//printf("cdh:%s, flash->size:0x%x\n", __func__, flash->size);
	//printf("cdh:%s, flash->erase_size:0x%x\n", __func__, flash->erase_size);
	//printf("cdh:%s, fli->sector_count:0x%x\n", __func__, fli->sector_count);
	//printf("cdh:%s, fli->flash_id:0x%x\n", __func__, fli->flash_id);
	
	addr = CONFIG_SYS_FLASH_BASE;
	for (i=0; i<fli->sector_count; ++i) {
		fli->start[i] = addr;
		addr += flash->erase_size;	// cdh notes:use erase size , not sector size
		fli->protect[i] = 0;
	}

	/* Assign spi_flash ops */
	fli->spinand_write = flash->spinand_write;
	fli->spinand_erase = flash->spinand_erase;
	fli->spinand_read  = flash->spinand_read;
	fli->spi_nand_flash = flash;
	//printf("cdh:flash_size:0x%x\n", fli->size);

	spinand_set_no_protection(flash);

	return fli->size;
}



/////////////////////////////////efuse


#define	 AO_GPIO_PAD_CONFIG	       0x0
#define  AO_PWM_CONFIG             0x2
#define  AO_GPI_WAKEUP_CONFIG      0x3
#define  AO_PMU_MUX_CONFIG         0x4
#define  AO_ANALOG_CONFIG          0x5
#define  AO_PMU_FUNC_CONFIG        0x6
#define  AO_PMU_STATUS             0x7


#define RTC_SIGNAL_TO_ANALOG_REG     (11)




#define CHIP_CONF_BASE_ADDR		 0x08000000      // chip configurations
#define EFUSE_CFG_REG			(CHIP_CONF_BASE_ADDR + 0x0000008C)     // EFUSE CFG
#define EFUSE_RDATA1_REG		(CHIP_CONF_BASE_ADDR + 0x00000090)     // EFUSE READ DATA 1
#define EFUSE_RDATA2_REG		(CHIP_CONF_BASE_ADDR + 0x00000094)     // EFUSE READ DATA 2
//#define AUDIO_CODEC_CFG1_REG    (CHIP_CONF_BASE_ADDR + 0x009C) //
#define SYSINT_STATUS_REG				(CHIP_CONF_BASE_ADDR + 0x00000030)      // module SYS interrupt status
#define ALWAYS_ON_PMU_CTRL0_REG			(CHIP_CONF_BASE_ADDR + 0x000000DC)     // Always On PMU Control 0
#define ALWAYS_ON_PMU_CTRL1_REG			(CHIP_CONF_BASE_ADDR + 0x000000E0)     // Always On PMU Control 1
#define RTC_CFG_REG						(CHIP_CONF_BASE_ADDR + 0x00000050)      // RTC CFG register
#define RTC1_CFG_REG					(CHIP_CONF_BASE_ADDR + 0x00000054)      // RTC CFG register


#define AUDIO_CODEC_CFG1_REG			(CHIP_CONF_BASE_ADDR + 0x0000009C)     // AUDIO CODEC CFG1
#define AUDIO_CODEC_CFG2_REG			(CHIP_CONF_BASE_ADDR + 0x000000A0)     // AUDIO CODEC CFG2
#define AUDIO_CODEC_CFG3_REG			(CHIP_CONF_BASE_ADDR + 0x000000A4)     // AUDIO CODEC CFG3



#define REG32_EFUSE(_register_)                               (*(volatile unsigned long *)(_register_))


void enable_ao_module_wr_rd(void)
{
	REG32_EFUSE(RTC_CFG_REG) |= (0x1<<25);

}


void disable_ao_module_wr_rd(void)
{
	REG32_EFUSE(RTC_CFG_REG) &= ~(0x1<<25);

}


void wait_ao_module_rdy(void)
{

	while(!(REG32_EFUSE(SYSINT_STATUS_REG)& 0x80));
}

void write_ao_module_reg(unsigned         long addr,unsigned long data)
{
	unsigned long reg_value = 0;

	wait_ao_module_rdy();

	//note:Core有电的情况下，需要RTC和PMU工作时，应配置Top Config模块0x0800_0050(reg19)寄存器, bit[24] -rtc_pmu_enable
	//和bit[25] - rtc_pmu_wr_enable都为1.否则Always on与Core交互的信号值都固定为0.
	enable_ao_module_wr_rd();

	reg_value = REG32_EFUSE(RTC_CFG_REG);
	reg_value &= ~0xffffff;
	reg_value |=  ((1 << 25)|(1 << 24)|(1<<22)|(0x2<<19)|((addr&0xf)<<14)|((data&0x3fff)<<0));
	reg_value &= ~(1<<18) ;
	REG32_EFUSE(RTC_CFG_REG) = reg_value;

	//udelay(100);

	wait_ao_module_rdy();

	disable_ao_module_wr_rd();


}

unsigned long read_ao_module_reg(unsigned long addr)
{
	unsigned long reg_value1 = 0;
	unsigned long reg_value2 = 0;

	wait_ao_module_rdy();

	enable_ao_module_wr_rd();

	reg_value1 = REG32_EFUSE(RTC_CFG_REG);
	reg_value1 &= ~0xffffff;
	reg_value1 |= ((1<<22) | (0x2<<19) | (1<<18) | ((addr&0xf)<<14));
	REG32_EFUSE(RTC_CFG_REG) = reg_value1;
	//udelay(100);
	
	wait_ao_module_rdy();


	reg_value2 = REG32_EFUSE(RTC1_CFG_REG);

	disable_ao_module_wr_rd();
	//printf("reg_value2:%x\r\n", reg_value2);
	return reg_value2;

}




/**
* @BRIEF  efuse_from_always_on_trimming_for_voltage_test
* @AUTHOR Mo_Shizhong
* @DATE   2019-4 -15
* @PARAM
* @PARAM
* @RETURN
* @NOTE:  将trimming值写入always on寄存器的相应的位并使能,模拟那边就会从always on寄存器
          中获取trimming值来调整BGR的电压

*/
void efuse_from_always_on_trimming_for_voltage_test(unsigned long trimming)
{
	unsigned long reg_value=0;

	//RTC and PMU write/read enable
	REG32_EFUSE(RTC_CFG_REG) |= ((1<<24)|(1<<25));

	reg_value = read_ao_module_reg(RTC_SIGNAL_TO_ANALOG_REG);

	//设置TRIMMING 值
    reg_value &= ~(0x3f<<5);
    reg_value |= ((0x1<<10) | (trimming<<5));

	write_ao_module_reg(RTC_SIGNAL_TO_ANALOG_REG,(reg_value));

	return;
}



/**
* @BRIEF  efuse_read
* @AUTHOR Zou Tianxiang
* @DATE   2016-11 -5
* @PARAM
* @PARAM
* @RETURN
* @NOTE: 该函数将在芯片内部的BGR的电压输出送到VCM2引脚，方便万用表测量
*/
#if 0
unsigned long  efuse_make_BGR_output_to_vcm2()
{

	REG32_EFUSE(AUDIO_CODEC_CFG2_REG) &= (~(0x3<<2));
	REG32_EFUSE(AUDIO_CODEC_CFG2_REG) &= (~(0xf<<10));
	REG32_EFUSE(AUDIO_CODEC_CFG3_REG) |= (1<<31);
	REG32_EFUSE(AUDIO_CODEC_CFG1_REG) &= (~(0x1f<<6));


	printf("REG32(%x) = %x\n", AUDIO_CODEC_CFG2_REG, REG32_EFUSE(AUDIO_CODEC_CFG2_REG));
	printf("REG32(%x) = %x\n", AUDIO_CODEC_CFG3_REG, REG32_EFUSE(AUDIO_CODEC_CFG3_REG));
	printf("REG32(%x) = %x\n", AUDIO_CODEC_CFG1_REG, REG32_EFUSE(AUDIO_CODEC_CFG1_REG));

}
#endif

unsigned long efuse_read(void)
{
	unsigned long  trimming,count = 0;
//	unsigned long efuse_reg1_val;
	unsigned long efuse_reg2_val;

#if 0
	//将在芯片内部的BGR的电压输出送到VCM2 引脚，方便万用表测量
	efuse_make_BGR_output_to_vcm2();

	printf("wait 10s \n");
	mdelay(10000);
	printf("wait finish\n");
#endif

	REG32_EFUSE(AUDIO_CODEC_CFG1_REG) |= (0x3<<1);
	//efuse close vcm2
	REG32_EFUSE(AUDIO_CODEC_CFG1_REG) |= ((0x1F<<6) | 0x1);

	//delay 3ms
	mdelay(3);

	//wait bit[31:30]=0 ,wait read & write idle
	while(REG32_EFUSE(EFUSE_CFG_REG) & (3<<30) )
	{

		mdelay(1);
		count++;
		if(count == 200)
		{
			printf("wait read & write idle timeout!\n");
			break;
		}
	}
	
	REG32_EFUSE(EFUSE_CFG_REG) &= ~(0xf<<3);
	//read efuse & set trimming bit to effect voltage
	REG32_EFUSE(EFUSE_CFG_REG) |= ((0x3<<3) | (0x1<<31) | (0x1<<29));
	//REG32(EFUSE_CFG_REG) |= ((0x1<<31));
	count = 0;
	//wait efuse read finish
	while(REG32_EFUSE(EFUSE_CFG_REG) & (1<<31) )
	{
		mdelay(1);
		count ++;
		if(count == 50)
		{
			printf("wait efuse read finish timeout!\n");
			break;
		}
	}
	//efuse_reg1_val = REG32_EFUSE(EFUSE_RDATA1_REG);
	efuse_reg2_val = REG32_EFUSE(EFUSE_RDATA2_REG);
	//debug("efuse_reg1_val = %x\n", (u32)efuse_reg1_val);
	//printf("efuse_reg2_val = %x\n", (u32)efuse_reg2_val);

	trimming = ((efuse_reg2_val>>16) & 0x1f);
	efuse_from_always_on_trimming_for_voltage_test(trimming);
	//printf("trimming:%x\n", trimming);
	return trimming;
}

static u64 spl_memsize_parse (const char *const ptr, const char **retptr)
{
	u64 ret = simple_strtoull(ptr, (char **)retptr, 0);

	switch (**retptr) {
		case 'G':
		case 'g':
			ret <<= 10;
		case 'M':
		case 'm':
			ret <<= 10;
		case 'K':
		case 'k':
			ret <<= 10;
			(*retptr)++;
		default:
			break;
	}

	return ret;
}

static int spl_part_parse(const char *const partdef, const char **ret, struct spl_part_info **retpart, u64 last_partition_size)
{
	struct spl_part_info *part;
	u64 size;
	u64 offset;
	const char *name;
	int name_len;
	unsigned int mask_flags;
	const char *p;


	p = partdef;
	*retpart = NULL;
	*ret = NULL;

	/* fetch the partition size */
	if (*p == '-') {
		/* assign all remaining space to this partition */
		debug("'-': remaining size assigned\n");
		size = SIZE_REMAINING;
		p++;
	} else {
		size = spl_memsize_parse(p, &p);
		if (size < MIN_PART_SIZE) {
			printf("partition size too small (%llx)\n", size);
			return 1;
		}
	}

	/* check for offset */
	offset = OFFSET_NOT_SPECIFIED;
	if (*p == '@') {
		p++;
		offset = spl_memsize_parse(p, &p);
	}

	/* now look for the name */
	if (*p == '(') {
		name = ++p;
		if ((p = strchr(name, ')')) == NULL) {
			printf("no closing ) found in partition name\n");
			return 1;
		}
		name_len = p - name + 1;
		if ((name_len - 1) == 0) {
			printf("empty partition name\n");
			return 1;
		}
		p++;
	} else {
		/* 0x00000000@0x00000000 */
		name_len = 22;
		name = NULL;
	}

	/* test for options */
	mask_flags = 0;
	if (strncmp(p, "ro", 2) == 0) {
		mask_flags |= MTD_WRITEABLE_CMD;
		p += 2;
	}

	/* check for next partition definition */
	if (*p == ',') {
		if (size == SIZE_REMAINING) {
			*ret = NULL;
			printf("no partitions allowed after a fill-up partition\n");
			return 1;
		}
		*ret = ++p;
	} else if ((*p == ';') || (*p == '\0')) {
		if (size == SIZE_REMAINING)
			size = last_partition_size;
		*ret = p;
	} else {
		printf("unexpected character '%c' at the end of partition\n", *p);
		*ret = NULL;
		return 1;
	}

	/*  allocate memory */
	part = (struct spl_part_info *)malloc(sizeof(struct spl_part_info) + name_len);
	if (!part) {
		printf("out of memory\n");
		return 1;
	}
	memset(part, 0, sizeof(struct spl_part_info) + name_len);
	part->size = size;
	part->offset = offset;
	part->mask_flags = mask_flags;
	part->name = (char *)(part + 1);

	if (name) {
		/* copy user provided name */
		strncpy(part->name, name, name_len - 1);
		part->auto_name = 0;
	} else {
		/* auto generated name in form of size@offset */
		sprintf(part->name, "0x%08llx@0x%08llx", size, offset);
		part->auto_name = 1;
	}

	part->name[name_len - 1] = '\0';

	debug("+ partition: name %-22s size 0x%08llx offset 0x%08llx mask flags %d\n",
			part->name, part->size,
			part->offset, part->mask_flags);

	*retpart = part;
	return 0;
}

int spl_device_parse(const char *const mtd_dev, ulong flash_size)
{
	const char *mtd_id;
	const char *p;
	//unsigned int mtd_id_len;
	struct spl_part_info *part;
	int err = 1;
	u32 num_parts;
	u64 last_flash_size = (u64)flash_size;

	p = mtd_dev;
	if (strncmp(p, "mtdparts=", 9) != 0) {
		printf("mtdparts variable doesn't start with 'mtdparts='\n");
		return err;
	}
	p += 9;

	/* fetch <mtd-id> */
	mtd_id = p = mtd_dev;
	if (!(p = strchr(mtd_id, ':'))) {
		printf("no <mtd-id> identifier\n");
		return 0;
	}
	//mtd_id_len = p - mtd_id + 1;
	p++;

	/* parse partitions */
	num_parts = 0;
	while (p && (*p != '\0') && (*p != ';')) {
		err = 1;
		if ((spl_part_parse(p, &p, &part, last_flash_size) != 0) || (!part))
			break;
		spl_mtdparts[num_parts] = part;
		if(spl_mtdparts[num_parts]->offset == 0xffffffffffffffff){
			if(num_parts == 0)
				spl_mtdparts[num_parts]->offset = 0x0;
			else
				spl_mtdparts[num_parts]->offset = spl_mtdparts[num_parts - 1]->offset + spl_mtdparts[num_parts - 1]->size;
		}

		debug("1+ partition: name %-22s size 0x%08llx offset 0x%08llx mask flags %d\n",
		spl_mtdparts[num_parts]->name, spl_mtdparts[num_parts]->size,
		spl_mtdparts[num_parts]->offset, spl_mtdparts[num_parts]->mask_flags);

		if(!strncmp(part->name, "KERNEL", 6)){
			setenv_hex("kernel_offset", part->offset);
			setenv_hex("kernel_size", part->size);
			debug("mtd cfg kernel offset&size ok!\n");
		}

		if(!strncmp(part->name, "DTB", 3)){
			setenv_hex("dtb_offset", part->offset);
			setenv_hex("dtb_size", part->size);
			debug("mtd cfg dtb offset&size ok!\n");
		}
		last_flash_size -= spl_mtdparts[num_parts]->size;
		
		num_parts++;
		err = 0;

	}

	if (err == 1) {
		printf("no partitions for device(%s)\n", mtd_id);
		return 0;
	}

	debug("\ntotal partitions: %d\n", num_parts);

	return num_parts;

}

int get_mtdparts_by_partition_name (char *partition_name, unsigned long partition_num, unsigned long *run_add, unsigned long *run_len)
{
	unsigned long i = 0;
	//unsigned long addr = 0, len = 0;

	//find UBOOT ENV DTB
	for(i = 0; i < partition_num; i++){
		//printf("00KERNEL: size:0x%08llx, offset:0x%08llx\n", spl_mtdparts[i]->size, spl_mtdparts[i]->offset);
		if(!strncmp(spl_mtdparts[i]->name, partition_name, strlen(partition_name))){
			//debug("KERNEL: size:0x%08llx, offset:0x%08llx\n", spl_mtdparts[i]->size, spl_mtdparts[i]->offset);
			*run_add =  spl_mtdparts[i]->offset;
			*run_len = spl_mtdparts[i]->size;
			break;
		}
	}


	if(i == partition_num)
	{
		printf("no find %s,pls check\r\n", partition_name);
		return -1;
	}

	return 0;
}


int get_mtdparts_by_partition_flash_offset (unsigned long flash_offset, unsigned long partition_num, unsigned long *run_add, unsigned long *run_len)
{
	unsigned long i = 0;
	//unsigned long addr = 0, len = 0;

	//find UBOOT ENV DTB
	for(i = 0; i < partition_num; i++){
		//printf("00KERNEL: size:0x%08llx, offset:0x%08llx\n", spl_mtdparts[i]->size, spl_mtdparts[i]->offset);
		if(spl_mtdparts[i]->offset == flash_offset){
			printf("%s: size:0x%08llx, offset:0x%08llx\n", spl_mtdparts[i]->name, spl_mtdparts[i]->size, spl_mtdparts[i]->offset);
			*run_add =  spl_mtdparts[i]->offset;
			*run_len = spl_mtdparts[i]->size;
			break;
		}
	}


	if(i == partition_num)
	{
		printf("no find %ld,pls check\r\n", flash_offset);
		return -1;
	}

	return 0;
}


int read_logo_data(unsigned long base_addr)
{
	unsigned long run_add = 0;
	unsigned long run_len = 0;
	int ret = -1;
	
	if(get_mtdparts_by_partition_name (CONFIG_LOGO_NAME, part_num, &run_add, &run_len) == -1){
		printf("eror:get logo partition info fail\n");
		return -1;
	}
	
	//debug("gd->fb_base:0x%x, run_add:0x%x, run_len:%d.\n", base_addr, run_add, run_len);
#ifdef  CONFIG_SPINAND_FLASH
	struct spinand_flash *spi = spinand_flash_probe(CONFIG_ENV_SPI_BUS, CONFIG_ENV_SPI_CS,
		   	CONFIG_ENV_SPI_MAX_HZ, CONFIG_ENV_SPI_MODE);
	if (!spi) {
		printf("%s: spi_flash probe error.\n", __func__);
		return 1;
	}

	ret = spinand_read_partition(spi, run_add, run_len, (u8 *)base_addr, run_len);
	if (ret == -1) {
		printf("%s: spi_flash read error.\n", __func__);
		spinand_flash_free(spi);
		return -1;
	}
	spinand_flash_free(spi);
#else
	struct spi_flash *spi = spi_flash_probe(CONFIG_ENV_SPI_BUS, CONFIG_ENV_SPI_CS,
		   	CONFIG_ENV_SPI_MAX_HZ, CONFIG_ENV_SPI_MODE);
	if (!spi) {
		printf("%s: spi_flash probe error.\n", __func__);
		return -1;
	}
	
	ret = spi_flash_read(spi, run_add, run_len, (void *)base_addr);
	if (ret) {
		printf("%s: spi_flash read error.\n", __func__);
		
		return -1;
	}
	spi_flash_free(spi);
#endif
	return 0;

}


/* cdh:ok */
static inline void spl_fdt_set_off_dt_strings(void *fdt, uint32_t val) 
{ 
	struct fdt_header *fdth = (struct fdt_header*)fdt; 
	fdth->off_dt_strings = cpu_to_fdt32(val); 
}

/* cdh:ok */
static inline void spl_fdt_set_size_dt_struct(void *fdt, uint32_t val) 
{ 
	struct fdt_header *fdth = (struct fdt_header*)fdt; 
	fdth->size_dt_struct = cpu_to_fdt32(val); 
}

/* cdh:ok */
void spl_fdt_set_totalsize(void *fdt, uint32_t val) 
{ 
	struct fdt_header *fdth = (struct fdt_header*)fdt; 
	fdth->totalsize = cpu_to_fdt32(val); 
}



/* cdh:ok */
int spl_fdt_check_header(const void *fdt)
{
	if (fdt_magic(fdt) == FDT_MAGIC) {
		/* Complete tree */
		if (fdt_version(fdt) < FDT_FIRST_SUPPORTED_VERSION){
			return -FDT_ERR_BADVERSION;
		}
		if (fdt_last_comp_version(fdt) > FDT_LAST_SUPPORTED_VERSION){
			return -FDT_ERR_BADVERSION;
		}
	} else if (fdt_magic(fdt) == FDT_SW_MAGIC) {
		/* Unfinished sequential-write blob */
		if (fdt_size_dt_struct(fdt) == 0){
			return -FDT_ERR_BADSTATE;
		}
	} else {
		return -FDT_ERR_BADMAGIC;
	}

	return 0;
}

/* cdh:ok */
static inline const void *spl_1_fdt_offset_ptr(const void *fdt, int offset)
{
	return (const char *)fdt + fdt_off_dt_struct(fdt) + offset;
}

/* cdh:ok */
const void *spl_fdt_offset_ptr(const void *fdt, int offset, unsigned int len)
{
	const char *p;

	/* cdh:if fdt version >= 0x11 and offset beyond dt struct size range, then report err */
	if (fdt_version(fdt) >= 0x11)
		if (((offset + len) < offset)
		    || ((offset + len) > fdt_size_dt_struct(fdt)))
			return NULL;

	/* calcuate real p to dtb offset address */
	p = spl_1_fdt_offset_ptr(fdt, offset);
	if (p + len < p)
		return NULL;
	return p;
}

/* cdh:ok */
uint32_t spl_fdt_next_tag(const void *fdt, int startoffset, int *nextoffset)
{
	const fdt32_t *tagp, *lenp;
	uint32_t tag;
	int offset = startoffset;
	const char *p;

	*nextoffset = -FDT_ERR_TRUNCATED;
	tagp = spl_fdt_offset_ptr(fdt, offset, FDT_TAGSIZE);
	if (!tagp)
		return FDT_END; /* premature end */
	tag = fdt32_to_cpu(*tagp);
	offset += FDT_TAGSIZE;
	
	*nextoffset = -FDT_ERR_BADSTRUCTURE;
	switch (tag) {
	case FDT_BEGIN_NODE:
		/* skip name */
		do {
			p = spl_fdt_offset_ptr(fdt, offset++, 1);
		} while (p && (*p != '\0'));
		if (!p)
			return FDT_END; /* premature end */
		break;

	case FDT_PROP:
		lenp = spl_fdt_offset_ptr(fdt, offset, sizeof(*lenp));
		if (!lenp)
			return FDT_END; /* premature end */
		/* skip-name offset, length and value */
		offset += sizeof(struct fdt_property) - FDT_TAGSIZE
			+ fdt32_to_cpu(*lenp);
		break;

	case FDT_END:
	case FDT_END_NODE:
	case FDT_NOP:
		break;

	default:
		return FDT_END;
	}

	if (!spl_fdt_offset_ptr(fdt, startoffset, offset - startoffset))
		return FDT_END; /* premature end */

	*nextoffset = FDT_TAGALIGN(offset);
	return tag;
}


/* cdh:ok */
int spl_1_fdt_check_node_offset(const void *fdt, int offset)
{
	if ((offset < 0) || (offset % FDT_TAGSIZE)
	    || (spl_fdt_next_tag(fdt, offset, &offset) != FDT_BEGIN_NODE))
		return -FDT_ERR_BADOFFSET;

	return offset;
}

/* cdh:ok */
static int spl_1_nextprop(const void *fdt, int offset)
{
	uint32_t tag;
	int nextoffset;

	do {
		tag = spl_fdt_next_tag(fdt, offset, &nextoffset);

		switch (tag) {
		case FDT_END:
			if (nextoffset >= 0)
				return -FDT_ERR_BADSTRUCTURE;
			else
				return nextoffset;

		case FDT_PROP:
			return offset;
		}
		offset = nextoffset;
	} while (tag == FDT_NOP);

	return -FDT_ERR_NOTFOUND;
}

/* cdh:ok */
int spl_fdt_first_property_offset(const void *fdt, int nodeoffset)
{
	int offset;

	if ((offset = spl_1_fdt_check_node_offset(fdt, nodeoffset)) < 0)
		return offset;

	return spl_1_nextprop(fdt, offset);
}

/* cdh:ok */
int spl_1_fdt_check_prop_offset(const void *fdt, int offset)
{
	if ((offset < 0) || (offset % FDT_TAGSIZE)
	    || (spl_fdt_next_tag(fdt, offset, &offset) != FDT_PROP))
		return -FDT_ERR_BADOFFSET;

	return offset;
}

/* cdh:ok */
int spl_fdt_next_property_offset(const void *fdt, int offset)
{
	if ((offset = spl_1_fdt_check_prop_offset(fdt, offset)) < 0)
		return offset;

	return spl_1_nextprop(fdt, offset);
}

/* cdh:ok */
const struct fdt_property *spl_fdt_get_property_by_offset(const void *fdt,
						      int offset,
						      int *lenp)
{
	int err;
	const struct fdt_property *prop;

	if ((err = spl_1_fdt_check_prop_offset(fdt, offset)) < 0) {
		if (lenp)
			*lenp = err;
		return NULL;
	}

	prop = spl_1_fdt_offset_ptr(fdt, offset);

	if (lenp)
		*lenp = fdt32_to_cpu(prop->len);

	return prop;
}

/* cdh:ok */
const char *spl_fdt_string(const void *fdt, int stroffset)
{
	return (const char *)fdt + fdt_off_dt_strings(fdt) + stroffset;
}

/* cdh:ok */
static int spl_1_fdt_string_eq(const void *fdt, int stroffset,
			  const char *s, int len)
{
	const char *p = spl_fdt_string(fdt, stroffset);

	return (strlen(p) == len) && (memcmp(p, s, len) == 0);
}

/* cdh:ok */
const struct fdt_property *spl_fdt_get_property_namelen(const void *fdt,
						    int offset,
						    const char *name,
						    int namelen, int *lenp)
{
	for (offset = spl_fdt_first_property_offset(fdt, offset);
	     (offset >= 0);
	     (offset = spl_fdt_next_property_offset(fdt, offset))) {
		const struct fdt_property *prop;

		if (!(prop = spl_fdt_get_property_by_offset(fdt, offset, lenp))) {
			offset = -FDT_ERR_INTERNAL;
			break;
		}
		if (spl_1_fdt_string_eq(fdt, fdt32_to_cpu(prop->nameoff),
				   name, namelen))
			return prop;
	}

	if (lenp)
		*lenp = offset;
	return NULL;
}

/* ok */
const void *spl_fdt_getprop_namelen(const void *fdt, int nodeoffset,
				const char *name, int namelen, int *lenp)
{
	const struct fdt_property *prop;

	prop = spl_fdt_get_property_namelen(fdt, nodeoffset, name, namelen, lenp);
	if (! prop)
		return NULL;

	return prop->data;
}

/* ok */
const char *spl_fdt_get_alias_namelen(const void *fdt,
				  const char *name, int namelen)
{
	int aliasoffset;

	aliasoffset = spl_fdt_path_offset(fdt, "/aliases");
	if (aliasoffset < 0)
		return NULL;

	return spl_fdt_getprop_namelen(fdt, aliasoffset, name, namelen, NULL);
}

/* ok */
int spl_fdt_next_node(const void *fdt, int offset, int *depth)
{
	int nextoffset = 0;
	uint32_t tag;

	if (offset >= 0){
		if ((nextoffset = spl_1_fdt_check_node_offset(fdt, offset)) < 0){
			return nextoffset;
		}
	}

	do {
		offset = nextoffset;
		tag = spl_fdt_next_tag(fdt, offset, &nextoffset);
		switch (tag) {
		case FDT_PROP:
		case FDT_NOP:
			break;

		case FDT_BEGIN_NODE:
			if (depth)
				(*depth)++;
			break;

		case FDT_END_NODE:
			if (depth && ((--(*depth)) < 0)){
				return nextoffset;
			}
			break;

		case FDT_END:
			if ((nextoffset >= 0)
			    || ((nextoffset == -FDT_ERR_TRUNCATED) && !depth)){
				return -FDT_ERR_NOTFOUND;
			}else{
				return nextoffset;
			}
		}
	} while (tag != FDT_BEGIN_NODE);

	return offset;
}

/* ok */
static int spl_1_fdt_nodename_eq(const void *fdt, int offset,
			    const char *s, int len)
{
	const char *p = spl_fdt_offset_ptr(fdt, offset + FDT_TAGSIZE, len+1);

	if (! p)
		/* short match */
		return 0;

	if (memcmp(p, s, len) != 0)
		return 0;

	if (p[len] == '\0')
		return 1;
	else if (!memchr(s, '@', len) && (p[len] == '@'))
		return 1;
	else
		return 0;
}

/* ok */
int spl_fdt_subnode_offset_namelen(const void *fdt, int offset,
			       const char *name, int namelen)
{
	int depth;

	for (depth = 0;
	     (offset >= 0) && (depth >= 0);
	     offset = spl_fdt_next_node(fdt, offset, &depth)){

		if ((depth == 1)
		    && spl_1_fdt_nodename_eq(fdt, offset, name, namelen)){
			return offset;
		}
	}

	if (depth < 0)
		return -FDT_ERR_NOTFOUND;
	return offset; 
}

/* ok */
int spl_fdt_path_offset(const void *fdt, const char *path)
{
	const char *end = path + strlen(path);
	const char *p = path;
	int offset = 0;

	if (*path != '/') {
		const char *q = strchr(path, '/');
		if (!q)
			q = end;

		p = spl_fdt_get_alias_namelen(fdt, p, q - p);
		if (!p)
			return -FDT_ERR_BADPATH;
		offset = spl_fdt_path_offset(fdt, p);

		p = q;
	}
	
	while (*p) {
		const char *q;

		while (*p == '/')
			p++;
		if (! *p)
			return offset;
		q = strchr(p, '/');
		if (! q)
			q = end;

		offset = spl_fdt_subnode_offset_namelen(fdt, offset, p, q-p);
		if (offset < 0)
			return offset;

		p = q;
	}

	return offset;
}

/* ok */
const void *spl_fdt_getprop(const void *fdt, int nodeoffset,
			const char *name, int *lenp)
{
	return spl_fdt_getprop_namelen(fdt, nodeoffset, name, strlen(name), lenp);
}

/* ok */
const struct fdt_property *spl_fdt_get_property(const void *fdt,
					    int nodeoffset,
					    const char *name, int *lenp)
{
	return spl_fdt_get_property_namelen(fdt, nodeoffset, name,
					strlen(name), lenp);
}

/* ok */
static inline struct fdt_property *spl_fdt_get_property_w(void *fdt, int nodeoffset,
						      const char *name,
						      int *lenp)
{
	return (struct fdt_property *)(uintptr_t)spl_fdt_get_property(fdt, nodeoffset, name, lenp);
}

/* ok */
static inline int spl_1_fdt_data_size(void *fdt)
{
	return fdt_off_dt_strings(fdt) + fdt_size_dt_strings(fdt);
}

/* ok */
static int spl_1_fdt_splice(void *fdt, void *splicepoint, int oldlen, int newlen)
{
	char *p = splicepoint;
	char *end = (char *)fdt + spl_1_fdt_data_size(fdt);

	if (((p + oldlen) < p) || ((p + oldlen) > end)){
		return -FDT_ERR_BADOFFSET;
	}
	
	if ((end - oldlen + newlen) > ((char *)fdt + fdt_totalsize(fdt))){
		return -FDT_ERR_NOSPACE;
	}
	memmove(p + newlen, p + oldlen, end - p - oldlen);

	return 0;
}

/* ok */
static int spl_1_fdt_splice_struct(void *fdt, void *p,
			      int oldlen, int newlen)
{
	int delta = newlen - oldlen;
	int err;

	if ((err = spl_1_fdt_splice(fdt, p, oldlen, newlen))){
		return err;
	}
	spl_fdt_set_size_dt_struct(fdt, fdt_size_dt_struct(fdt) + delta);
	spl_fdt_set_off_dt_strings(fdt, fdt_off_dt_strings(fdt) + delta);

	return 0;
}

/* ok */
static int spl_1_fdt_resize_property(void *fdt, int nodeoffset, const char *name,
				int len, struct fdt_property **prop)
{
	int oldlen;
	int err;

	*prop = spl_fdt_get_property_w(fdt, nodeoffset, name, &oldlen);
	if (! (*prop)){
		printf("%s, line:%d, use uboot bootargs!\n", __func__, __LINE__);
		return oldlen;
	}

	if ((err = spl_1_fdt_splice_struct(fdt, (*prop)->data, FDT_TAGALIGN(oldlen),
				      FDT_TAGALIGN(len)))){
		printf("%s, line:%d, use uboot bootargs!\n", __func__, __LINE__);		      
		return err;
	}

	(*prop)->len = cpu_to_fdt32(len);

	return 0;
}

/* ok */
int spl_fdt_setprop(void *fdt, int nodeoffset, const char *name,
		const void *val, int len)
{
	struct fdt_property *prop;
	int err;

	err = spl_1_fdt_resize_property(fdt, nodeoffset, name, len, &prop);
	if (err == -FDT_ERR_NOTFOUND){
		//printf("%s, line:%d, use uboot bootargs!\n", __func__, __LINE__);
		//err = _fdt_add_property(fdt, nodeoffset, name, len, &prop);
		//printf("%s, line:%d, use uboot bootargs!\n", __func__, __LINE__);
	}
	
	if (err){
		return err;
	}
	memcpy(prop->data, val, len);

	return 0;
}

int pare_str_form_env(char *buf, char *str, char *out_buf)
{
	int i = 0, idex = 0;
	char line_header_flag = 1;
	
	for(i = 4; i < CONFIG_ENV_SIZE; i++)
	{
		if(line_header_flag && memcmp(&buf[i], str, strlen(str)) == 0)
		{
			idex = i;// + strlen(str);
			memcpy(out_buf, &buf[idex], strlen(&buf[idex]));
			//printf("test:%s", &buf[i]);
			break;
		}
		else
		{
			line_header_flag = 0;
			if(buf[i] == 0x00){
				line_header_flag = 1;
			}
		}
	}
	
	return 0;


}


int spl_fdt_chosen(void *fdt, int force)
{
	int   nodeoffset;
	int   err;
	char  *str;		
	const char *path;
	char out_buf[1024] = {0};
	char temp_buf[1024] = {0};
	int idex = 0;

	err = spl_fdt_check_header(fdt);
	if (err < 0) {
		printf("fdt_chosen: error\n");
		return err;
	}

	/*
	 * Find the "chosen" node.
	 */
	nodeoffset = spl_fdt_path_offset(fdt, "/chosen");

	/*
	 * Create /chosen properites that don't exist in the fdt.
	 * If the property exists, update it only if the "force" parameter
	 * is true.
	 */
	if(gd->env_valid == 0){
		str = getenv("bootargs");
	}
	else{
		memset(out_buf, 0, 1024);
		memset(temp_buf, 0, 1024);
		pare_str_form_env(g_env_buf, "bootargs=", temp_buf);
		memcpy(out_buf, temp_buf, strlen(temp_buf));
		idex = strlen(out_buf);
		out_buf[idex] = 0x20;

		memset(temp_buf, 0, 1024);
		pare_str_form_env(g_env_buf, "mtdparts=mtdparts=", temp_buf);
		memcpy(&out_buf[idex + 1], &temp_buf[9], strlen(&temp_buf[9]));
		idex = strlen(out_buf);
		out_buf[idex] = 0x20;

		memset(temp_buf, 0, 1024);
		pare_str_form_env(g_env_buf, "mem=mem=", temp_buf);
		memcpy(&out_buf[idex + 1], &temp_buf[4], strlen(&temp_buf[4]));
		idex = strlen(out_buf);
		out_buf[idex] = 0x20;

		memset(temp_buf, 0, 1024);
		pare_str_form_env(g_env_buf, "memsize=memsize=", temp_buf);
		memcpy(&out_buf[idex + 1], &temp_buf[8], strlen(&temp_buf[8]));
		str = &out_buf[1];
	}
	if (str != NULL) {
		path = spl_fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
		
		if ((path == NULL) || force) {
			err = spl_fdt_setprop(fdt, nodeoffset,
				"bootargs", str, strlen(str)+1);
			if (err < 0)
				printf("WARNING: could not set bootargs.\n");
		}
	}

	//path2 = spl_fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	//printf("path2:%s\n", path2);
	return err;
}




int read_dtb_data(unsigned long base_addr)
{
	unsigned long run_add = 0;
	unsigned long run_len = 0;
	int ret = -1;
	
	if(get_mtdparts_by_partition_name (CONFIG_DTB_NAME, part_num, &run_add, &run_len) == -1){
		printf("eror:get logo partition info fail\n");
		return -1;
	}
	
	//debug("gd->fb_base:0x%x, run_add:0x%x, run_len:%d.\n", base_addr, run_add, run_len);
#ifdef  CONFIG_SPINAND_FLASH
	struct spinand_flash *spi = spinand_flash_probe(CONFIG_ENV_SPI_BUS, CONFIG_ENV_SPI_CS,
		   	CONFIG_ENV_SPI_MAX_HZ, CONFIG_ENV_SPI_MODE);
	if (!spi) {
		printf("%s: spi_flash probe error.\n", __func__);
		return 1;
	}

	ret = spinand_read_partition(spi, run_add, run_len, (u8 *)base_addr, run_len);
	if (ret == -1) {
		printf("%s: spi_flash read error.\n", __func__);
		spinand_flash_free(spi);
		return -1;
	}
	spinand_flash_free(spi);
#else
	struct spi_flash *spi = spi_flash_probe(CONFIG_ENV_SPI_BUS, CONFIG_ENV_SPI_CS,
		   	CONFIG_ENV_SPI_MAX_HZ, CONFIG_ENV_SPI_MODE);
	if (!spi) {
		printf("%s: spi_flash probe error.\n", __func__);
		return -1;
	}
	
	ret = spi_flash_read(spi, run_add, run_len, (void *)base_addr);
	if (ret) {
		printf("%s: spi_flash read error.\n", __func__);
		
		return -1;
	}
	spi_flash_free(spi);
#endif
	return 0;

}

