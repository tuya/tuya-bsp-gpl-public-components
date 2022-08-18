/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* #define	DEBUG	*/

#include <common.h>
#include <command.h>
#include <environment.h>
#include <malloc.h>
#include <fs.h>
#include <asm/io.h>

#ifdef debug
#undef debug
#endif

#define debug(fmt, args...) printf(fmt, ##args)

#if 1
static int tuya_verify(char *filename, char *sign_filename)
{
	unsigned char *hash_input, hash_output[32];
	unsigned char *sign_tmp, sign[160];
	int hash_len, sign_len;
	uint8_t sign1, sign2;
	int i, ret;

	if (!filename || !sign_filename)
		return -1;

	hash_len = file_fat_read(filename,CONFIG_USER_LOAD_ADDR, 0);
	if (hash_len <= 0) {
		printf("missing target file or read failed\n");
		return -1;
	}

	extern void sha256_csum_wd(const unsigned char *input, unsigned int ilen,
			unsigned char *output, unsigned int chunk_sz);
	sha256_csum_wd(CONFIG_USER_LOAD_ADDR, hash_len, hash_output, 0);

#if 0
	printf("hash len is %d\n", hash_len);
	printf("output is: \n");
	for(i=0; i<32; i++) {
		printf("0x%x ", hash_output[i]);
	}
	printf("\n");
#endif

	sign_tmp = (unsigned char *)map_physmem(CONFIG_USER_LOAD_ADDR, 0, MAP_WRBACK);
	if (!sign_tmp) {
		printf("failed to map_physmem\n");
		return -1;
	}

	sign_len = file_fat_read(sign_filename, CONFIG_USER_LOAD_ADDR, 0);

	if (sign_len <= 0) {
		printf("missing sign file or read failed\n");
		unmap_physmem(sign_tmp, sign_len);
		return -1;
	}

	for(i=0; i<sign_len; i=i+2) {
		if ((*(sign_tmp + i) >= '0') && (*(sign_tmp + i) <= '9'))
			sign1 = *(sign_tmp + i) - '0';
		if ((*(sign_tmp + i) >= 'a') && (*(sign_tmp + i) <= 'f'))
			sign1 = *(sign_tmp + i) - 'a' + 10;
		if ((*(sign_tmp + i + 1) >= '0') && (*(sign_tmp + i + 1) <= '9'))
			sign2 = *(sign_tmp + i + 1) - '0';
		if ((*(sign_tmp + i + 1) >= 'a') && (*(sign_tmp + i + 1) <= 'f'))
			sign2 = *(sign_tmp + i + 1) - 'a' + 10;
		sign[i/2] = sign1 * 16 + sign2;
	}
	sign_len = sign_len/2;

#if 0
	printf("the signature is :\n");
	for(i=0; i<sign_len; i++)
		printf("0x%02x, ", sign[i]);
	printf("\n");
#endif

	extern int ecdsa_verify_old(unsigned char *hash, int hash_size, unsigned char *sign, int sign_size);
	ret = ecdsa_verify_old(hash_output, sizeof(hash_output), sign, sign_len);

	unmap_physmem(sign_tmp, sign_len);

	return ret;
}
#endif

int boot_from_typt(void)
{
    long sz, sign_sz;

    char *filename = "ty_uImage";
    char *sign_filename = "ty_uImage.sign";

    char *cmd_buf = malloc(20);
    unsigned int bootaddr;
    char bootaddr_buf[12];
    unsigned int val, val_ori;

    if (fs_set_blk_dev("mmc", "0:1", FS_TYPE_FAT))
	if (fs_set_blk_dev("mmc", "0:0", FS_TYPE_FAT))
	     if (fs_set_blk_dev("mmc", "0:4", FS_TYPE_FAT)) {
			printf("failed to read sd card\n");
			return -1;
		}

    
    if (!fat_exists(filename) && !fat_exists(sign_filename)) {
        return -1;
    }

#if 1  
    if (0 != tuya_verify(filename, sign_filename)) {
        printf("production test verify failed\n");
        return -1;
    }
#endif

    sz = file_fat_read(filename, (void *)CONFIG_USER_LOAD_ADDR, sz);
    if (sz <= 0) {
        return -1;
    }

    // set sd boot io here

    strcpy(cmd_buf, "bootm ");
    bootaddr = CONFIG_USER_LOAD_ADDR;
    sprintf(bootaddr_buf, "0x%x - 0x81300000", bootaddr);
    strcat(cmd_buf, bootaddr_buf);

    run_command_list(cmd_buf, -1, 0);

    return 0;
}
