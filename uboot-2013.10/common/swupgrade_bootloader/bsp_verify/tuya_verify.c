/*
 * (C) Copyright 2020
 * Cheng Lei, TUYA Software Engineering, chenglei@tuya.com.
 * Tuya pack code
 */
 
#define __UBOOT__

#if !defined(__UBOOT__)
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#endif
#include "sha256.h"
#include "ecdsa.h"
#include "tuya_verify.h"

#define PRDBG(...) \
    do { printf(__VA_ARGS__); } while (0);

#define PRERR(...) \
    do { printf(__VA_ARGS__); } while (0);

u8 public_key_new[64] = {
        0xda,0x18,0x57,0x25,0xed,0xe5,0x3e,0x64,
        0x46,0xf6,0x99,0xdc,0x0a,0xc9,0x5b,0x4c,
        0x6b,0x2f,0xbf,0x88,0x58,0x87,0x2d,0xd4,
        0xfd,0x8b,0xd5,0x20,0xee,0x2c,0x2e,0x72,
        0x8d,0xf2,0x35,0xac,0x06,0xa5,0xae,0x5b,
        0x06,0xaf,0x03,0x26,0x23,0x70,0x97,0x2e,
        0x6a,0x73,0xa9,0xff,0x3a,0x20,0xc3,0xc0,
        0x9b,0x33,0xce,0x60,0xd9,0xd0,0x26,0x62};

u8 public_key[64]  = {
        0x9b,0x03,0xf0,0x4f,0x45,0x1f,0x03,0xfd,
        0x7a,0x34,0xf7,0x8f,0x6a,0xa0,0x71,0xc0,
        0xa2,0x5c,0xdf,0x10,0x67,0x22,0xce,0x05,
        0xdb,0xf4,0x59,0xf0,0x66,0x00,0xa9,0x02,
        0x7b,0xa8,0x96,0xf1,0xf4,0xfb,0x5f,0x62,
        0xb4,0x66,0x10,0xb2,0x50,0xaf,0xf6,0xdd,
        0x16,0x27,0x4e,0x02,0xc6,0xe6,0xff,0xc6,
        0x7b,0xea,0x72,0x5e,0x15,0x7f,0x7e,0x04};

int tuya_verify_data(unsigned char* buf, unsigned int buf_len,
		unsigned char* sign)
{
#ifdef CONFIG_TUYA_PACK_UNSIGN
	return 0;
#else
	uint8_t hash[32] = {0};
	int ret = 0;

	ret = sha256(buf, buf_len, hash);
	if(ret){
		PRERR("%s sha256 failed buflen:%d ret:%d\n", __func__, buf_len, ret);
		return ret;
	}

	ret = ecdsa_verify(ECC_CURVE_SECP256K1, (u64*)public_key_new, (u64*)hash, (u64*)sign);
	if(ret)
		PRERR("%s ecdsa_verify failed ret:%d\n", __func__, ret);

	return ret;
#endif
}

#if !defined(__UBOOT__)

static int load_sign_file(char *name, unsigned char* sign_data)
{
	FILE *fp = NULL;
	int file_size = 0;
	int i, j, s_index;
	unsigned char sign[160] = {0};
	unsigned char sign_hex[160] = {0};
	unsigned int sign_hex_len = 0;
	char hex[3] = {0};

	if ((fp = fopen(name, "rb+")) == NULL) {
		printf("%s: open file failed\n", __func__);
		return -errno; 
	}

	fseek(fp, 0L, SEEK_END);
	file_size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	if (fread(sign, file_size - 1, 1, fp) != 1) {
		printf("read sign file failed\n");
		fclose(fp);
		return -errno;
	}

	fclose(fp);

	for (i = 0; i < file_size - 1; i+=2) {
		hex[0] = sign[i];
		hex[1] = sign[i + 1];
		sscanf(hex, "%2hhx", &sign_hex[i/2]);
	}
	sign_hex_len = (file_size - 1) / 2;

	/* r len */
	if(sign_hex[3] == 0x21){
		memcpy(sign_data, &(sign_hex[5]), 32);
		s_index = 38;
	}
	else{
		memcpy(sign_data, &(sign_hex[4]), 32);
		s_index = 37;
	}

	/* s len */
	if(sign_hex[s_index] == 0x21){
		memcpy(&(sign_data[32]), &(sign_hex[s_index+2]), 32);
	}
	else{
		memcpy(&(sign_data[32]), &(sign_hex[s_index+1]), 32);
	}

	return 0;
}

int tuya_verify_file(char *file_name, char *sig_file_name)
{
	int ret = -1;
	FILE *fp = NULL;
	unsigned char *buf = NULL;
	unsigned int file_size = 0;
	unsigned char sign_hex[64] = {0};


	if ((fp = fopen(file_name, "rb+")) == NULL) {
		PRERR("access file %s failed.\n", file_name);
		return -errno;
	}

	fseek(fp, 0L, SEEK_END);
	file_size = ftell(fp);
	if ((buf = calloc(file_size, sizeof(unsigned char))) == NULL) {
		PRERR("alloc memory failed.\n");
		fclose(fp);
		free(buf);
		return -2;
	}

	fseek(fp, 0L, SEEK_SET);
	if (fread(buf, file_size, 1, fp) != 1) {
		PRERR("read file failed.\n");
		fclose(fp);
		free(buf);
		return -errno;
	}

	ret = load_sign_file(sig_file_name, sign_hex);
	if(ret){
		PRERR("%s load_sign_file failed ret:%d.\n", __func__, ret);
		fclose(fp);
		free(buf);
		return ret;
	}

	ret = tuya_verify_data(buf, file_size, sign_hex);
	if(ret)
		PRERR("%s tuya_verify_data failed ret:%d.\n", __func__, ret);

	fclose(fp);
	free(buf);

	return ret;
}

#endif
