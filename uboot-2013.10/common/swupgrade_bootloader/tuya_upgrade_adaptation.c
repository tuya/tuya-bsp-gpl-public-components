/*
 * (C) Copyright 2020
 * Cheng Lei, Tuya Software Engineering, chenglei@tuya.com.
 */

#include <common.h>
#include "tuya_upgrade_platform.h"
#include "tuya_upgrade_adaptation.h"
#include "bsp_verify/tuya_verify.h"

extern int sha256(const void *data, int len, uint8_t *hash);

#define TEST_UBOOT_UPGRADE	1
#define TEST_UBOOT_UPGRADE	1
//#define TEST_RTL8197F_BOOT_UPGRADE	1

// common staff
#define snprintf(buf, size, fmt, args...) sprintf(buf, fmt, ##args)

#ifndef INT_MAX
#define INT_MAX		((int)(~0U>>1))
#endif

#ifndef NULL
#define NULL	0
#endif

#if defined(__8197BOOT__)
#define printf dprintf
#endif

static int tuya_atoi(const char* str)
{
    int sign = 1, base = 0, i = 0;

    if (!str)
        return -1;

    /* if whitespaces then ignore. */
    while (str[i] == ' ') {
        i++;
    }
    /* sign of number */
    if (str[i] == '-' || str[i] == '+') {
        sign = 1 - 2 * (str[i++] == '-');
    }

    while (str[i] >= '0' && str[i] <= '9') {
        base = 10 * base + (str[i++] - '0');
        if (base > INT_MAX) {
            if (sign == 1)
                return INT_MAX;
            else
                return (0 - INT_MAX);
        }
    }

    return base * sign;
}

// double-copy staff

int get_active_slot()
{
	int ret;
	char value[PROP_MAX_LEN] = {0};

	ret = prop_get("persist.ota.slot", value);
	if (ret < 0) {
                TRACE("use default slot 0.");
		return 0;
	}

	return tuya_atoi(value);
}

int set_active_slot(int slot)
{
	int ret;
	char value[PROP_MAX_LEN] = {0};

	snprintf(value, PROP_MAX_LEN, "%d", slot);
	//SprintF(value, "%d", slot);

	ret = prop_set("persist.ota.slot", value);
	if (ret < 0) {
                TRACE("set slot failed.\n");
		return 0;
	}

	return 0;
}

// recovery staff

int get_boot_recovery_marker()
{
	int ret;
	char value[PROP_MAX_LEN] = {0};

	ret = prop_get("persist.boot.recovery", value);
	if (ret < 0) {
                TRACE("get value of boot.recovery failed.\n");
		return 0;
	}

	if (!strcmp(value, "true"))
		return 1;

	return 0;
}

int set_boot_recovery_marker(int mark)
{
	prop_set("persist.boot.recovery", mark?"true":"false");

	return 0;
}

// single-copy staff

static int set_boot_restore_marker(int dst, int src, int len)
{
	char value[PROP_MAX_LEN] = {0};

	snprintf(value, PROP_MAX_LEN, "%d", dst);
	//SprintF(value, "%d", dst);
	prop_set("persist.ota.dst", value);
	snprintf(value, PROP_MAX_LEN, "%d", src);
	//SprintF(value, "%d", src);
	prop_set("persist.ota.src", value);
	snprintf(value, PROP_MAX_LEN, "%d", len);
	//SprintF(value, "%d", len);
	prop_set("persist.ota.len", value);

	return 0;
}

int restore_img_from_backup()
{
	int ret;
	int dst;
	int src;
	int len;
	char value[PROP_MAX_LEN] = {0};
	char *buf = NULL;

	ret = prop_get("persist.ota.len", value);
	if (ret < 0) {
                TRACE("get value of ota.len failed.\n");
		return 0;
	}
	len = tuya_atoi(value);
	if (len <= 0)
		return 0;

	memset(value, 0, PROP_MAX_LEN);
	ret = prop_get("persist.ota.src", value);
	if (ret < 0) {
                TRACE("get value of ota.src failed.\n");
		return 0;
	}
	src = tuya_atoi(value);

	memset(value, 0, PROP_MAX_LEN);
	ret = prop_get("persist.ota.dst", value);
	if (ret < 0) {
                TRACE("get value of ota.dst failed.\n");
		return 0;
	}
	dst = tuya_atoi(value);

	ret = tuya_flash_read(src, len, &buf);
	if (ret < 0) {
                TRACE("read flash failed.\n");
		return 0;
	}

	ret = tuya_flash_write(buf, len, dst);
	if (ret < 0) {
                TRACE("write flash failed.\n");
		return 0;
	}

	set_boot_restore_marker(0, 0, 0);

	return 0;
}

// upgrade from boot staff
#if 0
static void format_sign(unsigned char *data, int len, unsigned char* sign_data)
{
	int i;
	int j, s_index;
	unsigned char sign[160] = {0};
	unsigned char sign_hex[160] = {0};
	char hex[3] = {0};

	for (i = 0; i < len - 1; i += 2) {
		hex[0] = sign[i];
		hex[1] = sign[i + 1];
		sscanf(hex, "%2hhx", &sign_hex[i/2]);
	}

	/* r len */
	if (sign_hex[3] == 0x21) {
		memcpy(sign_data, &(sign_hex[5]), 32);
		s_index = 38;
	} else {
		memcpy(sign_data, &(sign_hex[4]), 32);
		s_index = 37;
	}

	/* s len */
	if (sign_hex[s_index] == 0x21)
		memcpy(&(sign_data[32]), &(sign_hex[s_index+2]), 32);
	else
		memcpy(&(sign_data[32]), &(sign_hex[s_index+1]), 32);
}
#else
static void format_sign(unsigned char *data, int len, unsigned char* sign_data)
{
	int i;
	unsigned char sign1, sign2;
	unsigned char sign[160] = {0};
	int s_index;

	for (i = 0; i < len; i = i + 2) {
		if ((*(data + i) >= '0') && (*(data + i) <= '9'))
			sign1 = *(data + i) - '0';
		if ((*(data + i) >= 'a') && (*(data + i) <= 'f'))
			sign1 = *(data + i) - 'a' + 10;
		if ((*(data + i + 1) >= '0') && (*(data + i + 1) <= '9'))
			sign2 = *(data + i + 1) - '0';
		if ((*(data + i + 1) >= 'a') && (*(data + i + 1) <= 'f'))
			sign2 = *(data + i + 1) - 'a' + 10;
		sign[i/2] = sign1 * 16 + sign2;
	}
	//sign_len = sign_len/2;

	/* r len */
	if (sign[3] == 0x21) {
		memcpy(sign_data, &(sign[5]), 32);
		s_index = 38;
	} else {
		memcpy(sign_data, &(sign[4]), 32);
		s_index = 37;
	}

	/* s len */
	if (sign[s_index] == 0x21)
		memcpy(&(sign_data[32]), &(sign[s_index+2]), 32);
	else
		memcpy(&(sign_data[32]), &(sign[s_index+1]), 32);
}

#endif
static int extract_and_verify_head(char *buf, ota_head *phead, int *offs)
{
	int ret;
	ota_sign *psign = NULL;
	unsigned char sign_hex[64] = {0};

#ifdef CONFIG_TUYA_PACK_SIGN
	psign = (ota_sign *)(buf + OTA_HEAD_SIZE);
	//ret = tuya_verify_data(buf, OTA_HEAD_SIZE, psign->data, psign->len);
	format_sign(psign->data, psign->len, sign_hex);
	ret = tuya_verify_data(buf, OTA_HEAD_SIZE, sign_hex);
	if (ret < 0) {
		TRACE("verify head failed.\n");
		return -1;
	}
#endif
	memcpy(phead, buf, sizeof(ota_head));
	*offs += (OTA_HEAD_SIZE + sizeof(ota_sign));

	return 0;
}

static int tuya_verify_img(char *buf, int buf_len, unsigned char sum[32])
{
#ifdef CONFIG_TUYA_PACK_UNSIGN
	return 0;
#else
	unsigned char tmp[32] = {0};

	//sha256sum(buf, buf_len, tmp);
	sha256(buf, buf_len, tmp);

	if (memcmp(sum, tmp, 32) == 0)
		return 0;
	else
		return -1;
#endif
}

static int extract_and_verify_img(char *imgbuf, ota_zone *pzone, int *offs)
{
	char *sum;
	int ret;

	sum = pzone->sha256;
	ret = tuya_verify_img(imgbuf, pzone->image_size, sum);
	if (ret < 0) {
		ERROR("verify image %s failed.\n", pzone->name);
		return -1;
	}

	*offs += pzone->image_size;

	return 0;
}

static int update_img_version(ota_zone *pzone)
{
	char key[PROP_MAX_LEN] = {0};
	char value[PROP_MAX_LEN] = {0};

	snprintf(key, PROP_MAX_LEN, "persist.%s.version", pzone->name);
	snprintf(value, PROP_MAX_LEN, "%s", pzone->version);
	//SprintF(key, "persist.%s.version", pzone->name);
	//SprintF(value, "%d", pzone->version);
	prop_set(key, value);

	return 0;
}

static int clear_ota_marker(char *upg_mode)
{
	if (!strncmp(upg_mode, "single-copy", 32))
		set_boot_restore_marker(0, 0, 0);

	if (!strncmp(upg_mode, "double-copy", 32))
		set_active_slot(0);

	if (!strncmp(upg_mode, "recovery", 32))
		set_boot_recovery_marker(0);

	return 0;
}

static void dump_head(ota_head head)
{
	int i, j;

        printf("magic: %s\n", head.magic);
        printf("flash_type: %s\n", head.flash_type);
        printf("ota mode: %s\n", head.ota_mode);
        printf("zone_num: %d\n", head.zone_num);
        for (i = 0; i < head.zone_num; i++) {
                printf("\tname: %s\n", head.zone[i].name);
                printf("\tversion: %s\n", head.zone[i].version);
                printf("\toffset: 0x%x\n", head.zone[i].offset);
                printf("\timage_size: %d\n", head.zone[i].image_size);
                printf("\tdevice[0]: %s\n", head.zone[i].device[0]);
                printf("\tdevice[1]: %s\n", head.zone[i].device[1]);
                printf("\tdevice_offset: 0x%x\n", head.zone[i].device_offset);
                printf("\tfilesystem: %s\n", head.zone[i].filesystem);
                printf("\tcopy_in_boot: %d\n", head.zone[i].copy_in_boot);
                printf("\thash: ");
                for (j = 0; j < 32; j++)
                        printf("%02x", head.zone[i].sha256[j]);
                printf("\n");
                printf("\t-----------------------------\n");
        }
}

int device_upgrade(char *buf, int len)
{
	int i;
	int ret = 0;
	int pos = 0;
	ota_head head;
	char *imgbuf = NULL;
	int imglen = 0;
	int offs = 0;

	upgrade_progress(0);

	printf("addr: 0x%x len: 0x%x\n", buf,len);
	ret = extract_and_verify_head(buf, &head, &pos);
	if (ret < 0)
		return -1;

	dump_head(head);

	for (i = 0; i < head.zone_num; i++) {
		imgbuf = buf + pos;
		imglen = head.zone[i].image_size;
		if (imglen <=0)
			continue;
		ret = extract_and_verify_img(imgbuf, &head.zone[i], &pos);
		if (ret < 0) {
			ERROR("verify image failed.\n");
			return -1;
		}
		offs = head.zone[i].offset + head.zone[i].device_offset;
		TRACE("imgbuf: %x imglen: %x offs: 0x%x\n", imgbuf, imglen, offs);
		ret = tuya_flash_write(imgbuf, imglen, offs);
		if (ret < 0) {
			ERROR("write image failed.\n");
			return -1;
		}
		update_img_version(&head.zone[i]);
	}

	clear_ota_marker(head.ota_mode);

	upgrade_progress(1);

	return 0;
}

#ifdef TEST_UBOOT_UPGRADE

//#include <command.h>

static int str2off(const char *p, int *num)
{
        char *endptr;

        *num = simple_strtoul(p, &endptr, 16);

        return *p != '\0' && *endptr == '\0';
}

int do_upgrade(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned long addr;
	char *endp;
	int ret;
	int size;

	if (argc < 2)
		goto usage;

	addr = simple_strtoul(argv[1], &endp, 16);
	if (*argv[1] == 0 || *endp != 0)
		return -1;

	if (!str2off(argv[2], &size)) {
		printf("'%s' is not a number\n", argv[2]);
		return -1;
	}

	printf("fw addr: 0x%x len: 0x%x\n", addr, size);

	ret = device_upgrade(addr, size);
	if (!ret)
		printf("device upgrade ok\n");
	else
		return -1;

usage:
	return CMD_RET_USAGE;
}

U_BOOT_CMD(
    upgrade, 3, 1, do_upgrade,
    "tuya upgrade test",
    "upgrade addr len\n"
);

#endif

#ifdef TEST_RTL8197F_BOOT_UPGRADE

int str2off(const char *p, int *num)
{
        char *endptr;

        *num = strtoul(p, &endptr, 16);

        return *p != '\0' && *endptr == '\0';
}

int do_upgrade(int argc, char * const argv[])
{
	unsigned long addr;
	char *endp;
	int ret;
	int size;

	if (argc < 2)
		goto usage;

	addr = strtoul(argv[0], &endp, 16);
	if (*argv[0] == 0 || *endp != 0) {
		printf("fail get addr from %s\n", argv[1]);
		return -1;
	}

	printf("addr: 0x%x\n", addr);

	size = strtoul(argv[1], &endp, 16);
	if (*argv[1] == 0 || *endp != 0) {
		printf("fail get size from %s\n", argv[1]);
		return -1;
	}
#if 0
	if (!str2off(argv[2], &size)) {
		printf("'%s' is not a number\n", argv[2]);
		return -1;
	}
#endif
	printf("fw addr: 0x%x len: 0x%x\n", addr, size);

	ret = device_upgrade((char*)addr, size);
	if (!ret)
		printf("device upgrade ok\n");
	else
		return -1;

usage:
	return 0;
}

#include "fs/ff.h"

#define DDR_UPLOAD_ADDR			0xA0A10000
#define CACHE_2_NONCACHE_ADDR(addr)	((addr) | 0x20000000)

int load_fw_form_sdcard(int argc, char * const argv[])
{
	unsigned int ret_val;
	FATFS fatFs;
	FIL fil;
	const char* filename_ota = "rtl8197fs_ota.img";
	unsigned char *addr = (unsigned char *)DDR_UPLOAD_ADDR;
	unsigned int br;
	int res = 0;
	char *endp;

	if (argc >= 2) {
		addr = (unsigned char *)strtoul(argv[1], &endp, 16);
		if (*argv[1] == 0 || *endp != 0) {
			dprintf("wrong addr: %s\n", argv[1]);
			return -1;
		}
	}

	fatFs.win = (unsigned char *)CACHE_2_NONCACHE_ADDR((unsigned int)fatFs.win1);
	ret_val = f_mount(0, (FATFS *)(&fatFs));
	dprintf("SD FW Name: %s\n", filename_ota);
	ret_val = f_open((FIL *)(&fil), filename_ota, FA_READ);
	if (ret_val) {
		dprintf("%s(%d): open file fail(0x%x) \n", __FUNCTION__, __LINE__, ret_val);
	} else {
		ret_val = f_read((FIL *)(&fil), addr, fil.fsize, &br);
		dprintf("%s(%d): read 0x%x byte to 0x%x\n", __func__, __LINE__, fil.fsize, addr);
		//res = spi_flw_image(0, 0, addr, fil.fsize);
                f_close(&fil);
                return 0;
        }

	return 0;
}

#endif
