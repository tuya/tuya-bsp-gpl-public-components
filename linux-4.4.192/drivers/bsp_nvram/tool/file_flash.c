#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "nvram/bcmnvram.h"

int NVRAM_FILE_FD = 0;
static unsigned long current_write_counter = 0;
static u_int32_t current_offs = 0;

extern unsigned short verify_zbuf_chksum(char *zbuf_with_header);

int ra_mtd_write_nm(int fd, loff_t to, u_int32_t len, char *buf)
{
	int ret;

	if (fd <= 0 || len <= 0 || !buf)
		return -1;

	lseek(fd, to, SEEK_SET);

	ret = write(fd, buf, len);
	if (ret < 0)
		printf("write failed: %s\n", strerror(errno));

	return ret;
}

int ra_mtd_read_nm(int fd, loff_t from, u_int32_t len, char *buf)
{
	int ret;

	if (fd <= 0 || len <= 0 || !buf)
		return -1;

	lseek(fd, from, SEEK_SET);

	ret = read(fd, buf, len);
	if (ret < 0)
		printf("read failed: %s\n", strerror(errno));

	return ret;
}

int _nvram_check(struct nvram_header *header)
{
	if (header->magic == 0xFFFFFFFF || header->magic == 0)
		return 1; // NVRAM MTD is clear

	if (header->magic != NVRAM_MAGIC)
		return -1; // NVRAM is garbage

	if (header->len < sizeof(struct nvram_header))
		return -2; // NVRAM size underflow

	if (header->len > NVRAM_SPACE)
		return -3; // NVRAM size overflow

	if (verify_zbuf_chksum((char*)header))
		return -4; // NVRAM content is corrupted

	return 0;
}

int nvram_flash_read_zbuf(char *zbuf, u_int32_t len)
{
	int ii;
	int ret;
	struct nvram_header *header = (struct nvram_header*)zbuf;

	current_write_counter = 0;

	for (ii = 0; ii < 2; ii++) {
		// TODO: check bad blocks
		ret = ra_mtd_read_nm(NVRAM_FILE_FD, ii*NVRAM_SPACE, len, zbuf);
		if (ret)
			continue;

		ret = _nvram_check(header);
		if (ret)
			continue;

		if (current_write_counter <= header->write_counter) {
			current_write_counter = header->write_counter;
			current_offs = ii*NVRAM_SPACE;
		}
	}

	ret = ra_mtd_read_nm(NVRAM_FILE_FD, current_offs, len, zbuf);
	if (ret < 0) {
		printf("%s: read flash failed, ret: %d\n", __func__, ret);
		return -1;
	}

	printf("%s: offs: 0x%x, len: %d\n", __func__, current_offs, len);

	return 0;
}

int nvram_flash_write_zbuf(char *zbuf, u_int32_t len)
{
	int ret;
	struct nvram_header *header = (struct nvram_header*)zbuf;

	current_write_counter++;
	header->write_counter = current_write_counter;

	current_offs += NVRAM_SPACE;
	if (current_offs + NVRAM_SPACE > 2*NVRAM_SPACE)
		current_offs = 0;

	ret = ra_mtd_write_nm(NVRAM_FILE_FD, current_offs, len, zbuf);
	if (ret < 0) {
		printf("%s: write flash failed, ret: %d\n", __func__, ret);
		return -1;
	}

	printf("%s: offs: 0x%x, len: %d, header->len %d\n", __func__, current_offs, len, header->len);

	return 0;
}
