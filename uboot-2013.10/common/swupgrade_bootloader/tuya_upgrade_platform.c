/*
 * (C) Copyright 2020
 * Cheng Lei, Tuya Software Engineering, chenglei@tuya.com.
 */

#include <common.h>
#include <fat.h>
#include "tuya_upgrade_platform.h"
#include "nvram/bcmnvram.h"

#define FS_TYPE_FAT 1

int prop_get(const char *key, char *value)
{
	if (!key || !value)
		return -1;

	return nvram_get(key, value);
}

int prop_set(const char *key, const char *value)
{
	int ret = 0;

	if (!key || !value)
		return -1;

	ret = nvram_set(key, value, ATTR_PERSIST);

	return ret;
}

int tuya_flash_read(int offs, int len, char **buf)
{
	*buf = (char*)CONFIG_USER_LOAD_ADDR;

	return spinor_flash_read(*buf, offs, &len);
}

int tuya_flash_write(char *buf, int len, int offs)
{
	return spinor_flash_write(buf, offs, len);
}

int load_fw_from_sd(void)
{
	long sz;
	char *filename;
	char *sign_filename;


	if (fs_set_blk_dev("mmc", "0:1", FS_TYPE_FAT))
		if (fs_set_blk_dev("mmc", "0:0", FS_TYPE_FAT))
			if (fs_set_blk_dev("mmc", "0:4", FS_TYPE_FAT)) {
				printf("failed to read sd card\n");
				return -1;
			}
#if defined(CONFIG_TUYA_PRODUCT_SC082WS03) || defined(CONFIG_TUYA_PRODUCT_SC034WNV3) || defined(CONFIG_TUYA_PRODUCT_SC015WU4) || defined(CONFIG_TUYA_PRODUCT_SC015WU4V2) || defined(CONFIG_TUYA_PRODUCT_SC016WZ02) || defined(CONFIG_TUYA_PRODUCT_SC045WY2) || defined(CONFIG_TUYA_PRODUCT_SC192W13Z)
	filename = "ak3918_16m_ota.bin";
#else	
	filename = "ak3918_8m_ota.bin";
#endif
	
	printf("read file:%s\n",filename);

    	sz = file_fat_read(filename, (void *)CONFIG_USER_LOAD_ADDR, sz);

	if (sz <= 0) {
		printf("error to read the file, sz: %d\n", sz);
		return -1;
	}

	return sz;
}

int check_update_status(void)
{
	int status = 0;
#if defined(CONFIG_TUYA_PRODUCT_SC082WS03) || defined(CONFIG_TUYA_PRODUCT_SC034WNV3) || defined(CONFIG_TUYA_PRODUCT_SC045WY2) || defined(CONFIG_TUYA_PRODUCT_SC009W22) || defined(CONFIG_TUYA_PRODUCT_SC192W13Z) || defined(CONFIG_TUYA_PRODUCT_SC015WZ2) || defined(CONFIG_TUYA_PRODUCT_SC085WN2) || defined(CONFIG_TUYA_PRODUCT_SC015WZ3) || defined(CONFIG_TUYA_PRODUCT_SC085WN3) || defined(CONFIG_TUYA_PRODUCT_SC015W12ZC)
	unsigned int pin = 84;
#else	
	unsigned int pin = 10;
#endif

	gpio_set_pin_as_gpio(pin);
	gpio_direction_input(pin);
	gpio_set_pull_up_r(pin,1);

	mdelay(50);
	status = gpio_get_value(pin);

	printf("key value:%d\n",status);

	return status;
}

int upgrade_progress(int status)
{
	int rled_pin = 0;
	int gled_pin = 0;
	printf("upgrade status: %d\n", status);

#if defined(CONFIG_TUYA_PRODUCT_SC045WY2) || defined(CONFIG_TUYA_PRODUCT_SC034WNV3) || defined(CONFIG_TUYA_PRODUCT_SC192W13Z) || defined(CONFIG_TUYA_PRODUCT_SC015WZ2) || defined(CONFIG_TUYA_PRODUCT_SC085WN2) || defined(CONFIG_TUYA_PRODUCT_SC015WZ3) || defined(CONFIG_TUYA_PRODUCT_SC085WN3)
	rled_pin = 42;
	gled_pin = 41;
#else
	rled_pin = 5;
	gled_pin = 0;
#endif
	if(status == 0){
		// green on
		gpio_set_pin_as_gpio(gled_pin);
		gpio_direction_output(gled_pin, 0);
		// red off	
		gpio_set_pin_as_gpio(rled_pin);
		gpio_direction_output(rled_pin, 1);
	}else{
		// green off
		gpio_set_pin_as_gpio(gled_pin);
		gpio_direction_output(gled_pin, 1);
		// red on	
		gpio_set_pin_as_gpio(rled_pin);
		gpio_direction_output(rled_pin, 0);
	}

	return 0;
}
