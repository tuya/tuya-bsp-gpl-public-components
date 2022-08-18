/*
 * (C) Copyright 2020
 * Cheng Lei, Tuya Software Engineering, chenglei@tuya.com.
 */

#include "tuya_upgrade_platform.h"
#include "nvram/bcmnvram.h"

#define IMG_LOAD_ADDR ((unsigned char *)0xa0a00000)

int prop_get(const char *key, char *value)
{
	if (!key || !value)
		return -1;

	return nvram_get(key, value);
}

int prop_set(const char *key, const char *value)
{
	if (!key || !value)
		return -1;

	return nvram_set(key, value);
}

int flash_read(int offs, int len, char **buf)
{
	*buf = (char*)IMG_LOAD_ADDR;

	return flashread(*buf, offs, len);
}

int flash_write(char *buf, int len, int offs)
{
	return spi_flw_image(0, offs, (unsigned char *)buf, (unsigned int)len);
}

int upgrade_progress(int status)
{
	// LED ON/OFF
	dprintf("upgrade status: %d\n", status);
	return 0;
}
