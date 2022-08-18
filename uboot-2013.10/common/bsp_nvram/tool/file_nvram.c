#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "nvram/bcmnvram.h"
#include "aes.h"
#include "uni_base64.h"
#include "file_nvram.h"

#include "../nvram_core.c"

static unsigned char* AES_128_KEY = (unsigned char*)"aa11pH9bj8GiexUI";

#define NVRAM_DRIVER_VERSION	"0.08"
#define NVRAM_VALUES_SPACE	(NVRAM_MTD_SIZE*2)

#define NVDBG(...) \
	do { printf(__VA_ARGS__); } while (0);

#define NVERR(...) \
	do { printf(__VA_ARGS__); } while (0);

extern int nvram_flash_write_zbuf(char *zbuf, u_int32_t len);
extern int nvram_flash_read_zbuf(char *zbuf, u_int32_t len);
extern int _nvram_purge_unlocked(void);
int nvram_commit(void);

/* Globals */
static char *nvram_values = NULL;
static unsigned long nvram_offset = 0;

struct nvram_tuple * _nvram_realloc(struct nvram_tuple *t, const char *name,
		const char *value, int is_temp)
{
	struct nvram_tuple *rt = t;
	uint32_t val_len = strlen(value);

	/* Copy name */
	if (!rt) {
		if (!(rt = (struct nvram_tuple *)malloc(sizeof(struct nvram_tuple)
						+ strlen(name) + 1))) {
			NVERR("%s: alloc failed\n", __func__);
			return NULL;
		}
		rt->name = (char *) &rt[1];
		strcpy(rt->name, name);
		rt->value = NULL;
		rt->val_len = 0;
		rt->flag = 0;
		rt->next = NULL;
	}

	/* Mark for temp tuple */
	rt->val_tmp = (is_temp) ? 1 : 0;

	/* Copy value */
	if (!rt->value || strcmp(rt->value, value)) {
		if (!rt->value || val_len > rt->val_len) {
			if ((nvram_offset + val_len + 1) >= NVRAM_VALUES_SPACE) {
				if (rt != t)
					free(rt);
				return NULL;
			}
			rt->value = &nvram_values[nvram_offset];
			rt->val_len = val_len;
			nvram_offset += (val_len + 1);
		}

		strcpy(rt->value, value);
	}

	return rt;
}

void _nvram_free(struct nvram_tuple *t)
{
	if (t)
		free(t);
}

void _nvram_reset(void)
{
	nvram_offset = 0;
	if (nvram_values)
		memset(nvram_values, 0, NVRAM_VALUES_SPACE);
}

// Note: user need free out
static int nvram_encode(const char *in, char **out)
{
	int ret;
	unsigned char *encode_output = NULL;
	unsigned int encode_len;
	char *base64_output = NULL;

	ret = aes128_data_encode((unsigned char*)in, strlen(in),
			&encode_output, &encode_len, AES_128_KEY);
	if (ret != 0 || encode_len <= 0) {
		NVERR("encode failed\n");
		return ret;
	}

	base64_output = (char*)malloc((encode_len/3)*4 + ((encode_len%3)?4:0) + 20 + 1);
	if (NULL == base64_output) {
		NVERR("alloc %d bytes fails\n", encode_len);
		return -1;
	}

	tuya_base64_encode(encode_output, base64_output, encode_len);
	free(encode_output);

	*out = base64_output;

	//NVDBG("encode done len: %d\n", strlen(base64_output));

	return 0;
}

// Note: user need free out
static int nvram_decode(const char *in, char **out)
{
	int ret;
	unsigned char *decode_output = NULL;
	unsigned int decode_len = 0;
	unsigned char *base64_output = NULL;

	base64_output = (unsigned char*)malloc(strlen(in));
	if (NULL == base64_output) {
		NVERR("alloc %d bytes fails\n", strlen(in));
		return -1;
	}

	ret = tuya_base64_decode(in, base64_output);
	if (ret <= 0) {
		NVERR("%s: base64 decode failed, ret: %d\n", __func__, ret);
		return -1;
	}

	ret = aes128_data_decode(base64_output, ret, &decode_output,
			&decode_len, AES_128_KEY);
	if (ret != 0 || decode_len <= 0) {
		NVERR("%s: decode failed, ret: %d\n", __func__, ret);
		return ret;
	}

	free(base64_output);

	if (strlen((char*)decode_output) > decode_len)
		decode_output[decode_len] = 0;

	*out = (char*)decode_output;

	//NVDBG("decode done len: %d decode_len: %d\n", strlen((char*)decode_output), decode_len);

	return 0;
}

int kernel_nvram_decode(const char *in, char *out)
{
	int ret = 0;
	char *dec_out = NULL;

	if (nvram_decode(in, &dec_out))
		return -1;

	memcpy(out, dec_out, strlen(dec_out));

	//NVDBG("%s: in: %s out: %s, outlen: %d\n", __func__, in, out, strlen(dec_out));

	if (dec_out)
		free(dec_out);

	return ret;
}

static int nvram_set_temp(const char *name, const char *value, int is_temp)
{
	int ret;
	char *out = NULL;

	if (!name)
		return -1;

	if (nvram_encode(value, &out) != 0)
		return -1;

	if ((ret = _nvram_set(name, out, is_temp)) && ret != -2) {
		NVERR("%s: nvram set failed, ret: %d\n", __func__, ret);
		struct nvram_header *header;
		/* Consolidate space and try again */
		if ((header = (struct nvram_header*)malloc(NVRAM_SPACE))) {
			memset(header, 0, NVRAM_SPACE);
			if (_nvram_generate(header, 1) == 0)
				ret = _nvram_set(name, out, is_temp);
			free(header);
		} else {
			NVERR("%s: alloc failed.\n", __func__);
		}
	}

	if (out)
		free(out);

	if (!ret && (is_temp & ATTR_PERSIST))
		ret = nvram_commit();

	return ret;
}

// external interfaces

int nvram_set(const char *name, const char *value, int attr)
{
	return nvram_set_temp(name, value, attr);
}

int nvram_unset(const char *name)
{
	if (!name)
		return -1;

	return _nvram_unset(name);
}

int nvram_get(const char *name, char *value)
{
	char *enc_val;

	if (!name || !value)
		return -1;

	enc_val = _nvram_get(name);
	if (!enc_val)
		return 0;

	return kernel_nvram_decode(enc_val, value);
}

int nvram_getall(char *buf, int count, int include_temp)
{
	if (!buf || count < 1)
		return -1;

	memset(buf, 0, count);

	return _nvram_getall(buf, count, include_temp);
}

int nvram_commit(void)
{
	char *bufw;
	int ret;

	bufw = (char *)malloc(NVRAM_SPACE);
	if (!bufw) {
		NVERR("nvram_commit: out of memory\n");
		return -1;
	}

	ret = _nvram_generate((struct nvram_header *)bufw, 0);
	if (ret)
		goto done;

	/* Write partition up to end of data area */
	ret = nvram_flash_write_zbuf(bufw, NVRAM_SPACE);
	if (ret) {
		NVERR("nvram_commit: write error\n");
	}

done:
	free(bufw);
	return ret;
}

int nvram_clear(void)
{
	_nvram_uninit();

	return 0;
}

int nvram_erase(void)
{
	return _nvram_purge_unlocked();
}

void dev_nvram_exit(void)
{
	_nvram_uninit();

	if (nvram_values) {
		free(nvram_values);
		nvram_values = NULL;
	}
}

int dev_nvram_init(void)
{
	int ret, check_res = 1;
	const char *istatus;
	struct nvram_header *header;

	nvram_values = (char*)malloc(NVRAM_VALUES_SPACE);
	if (!nvram_values) {
		NVERR("%s: alloc failed\n", __func__);
		return -1;
	}

	/* Initialize hash table */
	header = (struct nvram_header *)malloc(NVRAM_SPACE);
	if (header) {
		ret = nvram_flash_read_zbuf((char*)header, NVRAM_SPACE);
		if (ret == 0)
			check_res = _nvram_init(header);
		else
			NVERR("nvram not inited\n");
		free(header);
	} else {
		NVERR("%s: alloc failed\n", __func__);
	}

	istatus = "MTD is empty";
	if (check_res == 0)
		istatus = "OK";
	else if (check_res == -1)
		istatus = "Signature FAILED!";
	else if (check_res == -2)
		istatus = "Size underflow!";
	else if (check_res == -3)
		istatus = "Size overflow!";
	else if (check_res == -4)
		istatus = "CRC FAILED!";
	else
		printf("check_res %d\n", check_res);

	printf("ASUS NVRAM, v%s. Available space: %d. Integrity: %s\n",
		NVRAM_DRIVER_VERSION, NVRAM_SPACE, istatus);

	return 0;

err:
	dev_nvram_exit();
	return ret;
}

extern int NVRAM_FILE_FD;

int nvram_init(char *img_fname, int size)
{
	int fd, ret;
	char *buf;

	if (!img_fname || size <= 0)
		return -1;

	remove(img_fname);

	fd = open(img_fname, O_RDWR|O_CREAT, 0666);
	if (fd < 0) {
		printf("open file %s failed. errno %d", img_fname, errno);
		return -ENOENT;
	}

	buf = (char *)malloc(size);
	if (!buf) {
		printf("alloc 0x%x bytes failed\n", size);
		close(fd);
		return -1;
	}

	memset(buf, 0xFF, size);

	ret = write(fd, buf, size);
	if (ret != size) {
		printf("wirte file %s failed. errno %d", img_fname, errno);
		free(buf);
		close(fd);
		return -1;
	}

	fsync(fd);

	NVRAM_FILE_FD = fd;

	return dev_nvram_init();
}

void nvram_deinit()
{
	if (NVRAM_FILE_FD)
		close(NVRAM_FILE_FD);

	NVRAM_FILE_FD = 0;
}

static void nvram_dump(char *header)
{
	char *name, *value, *end, *eq;

	name = header;
	end = (char *) header + NVRAM_SPACE - 2;
	end[0] = end[1] = '\0';
	for (; *name; name = value + strlen(value) + 1) {
		if (!(eq = strchr(name, '=')))
			break;
		*eq = '\0';
		value = eq + 1;
		printf("[%s]: [%s]\n", name, value);
		*eq = '=';
	}
}

int nvram_show()
{
	char *buf = (char*)calloc(NVRAM_MTD_SIZE, 1);
	if (!buf)
		return -1;

	nvram_getall(buf, NVRAM_MTD_SIZE, 1);

	printf("\n\ndump content:\n");
	nvram_dump(buf);

	free(buf);

	return 0;
}
