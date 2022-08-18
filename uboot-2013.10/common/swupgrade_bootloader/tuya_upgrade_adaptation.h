/*
 * (C) Copyright 2020
 * Cheng Lei, Tuya Software Engineering, chenglei@tuya.com.
 */

#ifndef tuya_upgrade_adaptation__h
#define tuya_upgrade_adaptation__h
 
#define MAX_OTA_ZONE		10
#define MAX_OTA_FILE		2
#define OTA_HEAD_SIZE		4096
#define HASH_LENGTH     	32
#define TUYA_SIGN_SIZE		256

typedef struct ota_sign {
        int len;
        unsigned char data[TUYA_SIGN_SIZE];
}__attribute__ ((packed)) ota_sign;

typedef struct ota_zone {
	char name[32];
	char version[32];
	int offset;
	int image_size;
	char device[2][32];
	int device_offset;
	char filesystem[32];
	int copy_in_boot;
	char sha256[HASH_LENGTH];
}__attribute__ ((packed)) ota_zone;

typedef struct ota_file {
	char name[32];
	char version[32];
	int image_size;
	char storage_path[128];
	char installer[128];
	char sha256[HASH_LENGTH];
}__attribute__ ((packed)) ota_file;

typedef struct ota_head {
	char magic[32]; /* "TUYA_OTA" */
	char flash_type[32];
	char ota_mode[32]; /* single-copy double-copy recovery */
	int zone_num;
	ota_zone zone[MAX_OTA_ZONE];
	int file_num;
	ota_file file[MAX_OTA_FILE];
}__attribute__ ((packed)) ota_head;


/* used by single-copy */

int restore_img_from_backup();

/* used by recovery */

int get_boot_recovery_marker();

int set_boot_recovery_marker(int mark);

/* used by double-copy */

int get_active_slot();

int set_active_slot(int slot);

/* upgrade from memory */

int device_upgrade(char *buf, int len);

#endif
