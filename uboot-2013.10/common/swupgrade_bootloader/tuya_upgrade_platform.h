/*
 * (C) Copyright 2020
 * Cheng Lei, Tuya Software Engineering, chenglei@tuya.com.
 */

#ifndef tuya_upgrade_platform__h
#define tuya_upgrade_platform__h
 
#define PROP_MAX_LEN	256
#define TRACE		printf
#define ERROR		printf

int prop_get(const char *key, char *value);

int prop_set(const char *key, const char *value);

int tuya_flash_read(int offs, int len, char **buf);

int tuya_flash_write(char *buf, int len, int offs);

int upgrade_progress(int status);

#endif
