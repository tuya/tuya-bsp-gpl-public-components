#include <linux/string.h>
#include <nvram/bcmnvram.h>
#include <common.h>
#include <malloc.h>
#include "aes.h"
#include "uni_base64.h"

#include "nvram_core.c"

#define NVRAM_HEADER_LOAD_ADDR ((unsigned char *)0x42080000)
#define NVRAM_VALUE_LOAD_ADDR ((unsigned char *)0x43000000)
#define NVRAM_COMMIT_ADDR ((unsigned char *)0x43040000)

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
		if (!(rt = malloc(sizeof(struct nvram_tuple) + strlen(name) + 1))) {
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

	base64_output = malloc((encode_len/3)*4 + ((encode_len%3)?4:0) + 20 + 1);
	if (NULL == base64_output) {
		NVERR("alloc %d bytes fails\n", encode_len);
		return -1;
	}

	tuya_base64_encode(encode_output, base64_output, encode_len);
	free(encode_output);

	*out = base64_output;

	NVDBG("encode done len: %d\n", strlen(base64_output));

	return 0;
}

// Note: user need free out
static int nvram_decode(const char *in, char **out)
{
	int ret;
	unsigned char *decode_output = NULL;
	unsigned int decode_len = 0;
	unsigned char *base64_output = NULL;

	base64_output = malloc(strlen(in));
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

	if (strlen(decode_output) > decode_len)
		decode_output[decode_len] = 0;

	*out = (char*)decode_output;

	NVDBG("decode done len: %d decode_len: %d\n", strlen((char*)decode_output), decode_len);

	return 0;
}

int kernel_nvram_decode(const char *in, char *out)
{
	int ret = 0;
	char *dec_out = NULL;

	if (nvram_decode(in, &dec_out))
		return -1;

	memcpy(out, dec_out, strlen(dec_out));

	NVDBG("%s: in: %s out: %s, outlen: %d\n", __func__, in, out, strlen(dec_out));

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

	NVDBG("%s: key: %s value: %s encode: %s istemp: 0x%x\n", __func__, name, value, out, is_temp);

	if ((ret = _nvram_set(name, out, is_temp)) && ret != -2) {
		NVERR("%s: nvram set failed, ret: %d\n", __func__, ret);
		struct nvram_header *header;
		/* Consolidate space and try again */
		if ((header = malloc(NVRAM_SPACE))) {
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

#ifdef USE_MALLOC
	bufw = malloc(NVRAM_SPACE);
#else
	bufw = NVRAM_COMMIT_ADDR;
#endif
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
#ifdef USE_MALLOC
	free(bufw);
#endif
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

#ifdef USE_MALLOC
	nvram_values = malloc(NVRAM_VALUES_SPACE);
#else
	nvram_values = NVRAM_VALUE_LOAD_ADDR;
#endif
	if (!nvram_values) {
		NVERR("%s: alloc failed\n", __func__);
		return -1;
	}

	/* Initialize hash table */
#ifdef USE_MALLOC
	header = malloc(NVRAM_SPACE);
#else
	header = NVRAM_HEADER_LOAD_ADDR;
#endif
	if (header) {
		ret = nvram_flash_read_zbuf((char*)header, NVRAM_SPACE);
		if (ret == 0)
			check_res = _nvram_init(header);
		else
			NVERR("nvram not inited\n");
#ifdef USE_MALLOC
		free(header);
#endif
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

// cmd interface

static void usage(char *programname)
{
	printf("Usage: %s get key\n", programname);
	printf("Usage: %s set key value [OPTION]\n", programname);
	printf(
		" -t, --temp       : temp kv not write to flash when commit\n"
		" -p, --persist    : write to flash immediately when set\n"
		" -l, --lock       : kv can not be reset/clear when set\n"
		" -h, --help       : print this help and exit\n"
	      );
	printf("Usage: %s commit\n", programname);
}

int nvram_parse_attr(int argc, char **argv)
{
        int opt_t = 0;
        int opt_p = 0;
        int opt_l = 0;
        int i;
        int attr = 0;

	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--temp") || !strcmp(argv[i], "-t"))
			opt_t = 1;
		else if (!strcmp(argv[i], "--persist") || !strcmp(argv[i], "-p"))
			opt_p = 1;
		else if (!strcmp(argv[i], "--lock") || !strcmp(argv[i], "-l"))
			opt_l = 1;
		else
			return -1;
	}

        if (opt_t && (opt_p || opt_l))
                return -1;

        attr = (opt_t?ATTR_TEMP:0) | (opt_p?ATTR_PERSIST:0) | (opt_l?ATTR_LOCK:0);

        NVDBG("t: %d p: %d l: %d attr: 0x%x\n", opt_t, opt_p, opt_l, attr);
 
        return attr;
}

int do_nvram(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int stat = 0;
	int attr = 0;
	char value[NVRAM_MAX_VALUE_LEN] = {0};

	if (!nvram_values) {
		stat = dev_nvram_init();
		if (stat)
			return -1;
	}

	if (argc == 3 && !strcmp(argv[1], "get")) {
		stat = nvram_get(argv[2], value);
		printf("%s\n", value);
		return stat;
	}

	if (argc >= 4 && !strcmp(argv[1], "set")) {
		if (argc > 4) {
			attr = nvram_parse_attr(argc-4, &argv[4]);
			if (attr < 0) {
				usage(argv[0]);
				return -1;
			}
		}
		return nvram_set(argv[2], argv[3], attr);
	}

	if (argc == 2 && !strcmp(argv[1], "commit"))
		return nvram_commit();

	usage(argv[0]);

	return -1;
}

U_BOOT_CMD(
	nvram, 6, 0, do_nvram,
	"read/write key value pair",
	"nvram get key\n"
	"nvram set key value\n"
	"nvram commit\n"
);
