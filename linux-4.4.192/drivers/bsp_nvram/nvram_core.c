/*
 * NVRAM variable manipulation (common)
 *
 * Copyright 2006, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: nvram.c,v 1.1 2007/06/08 07:38:05 arthur Exp $
 */

#include <linux/string.h>
#include <nvram/bcmnvram.h>

extern struct nvram_tuple * _nvram_realloc(struct nvram_tuple *t, const char *name,
                                           const char *value, int is_temp);
extern void _nvram_free(struct nvram_tuple *t);
extern void _nvram_reset(void);
extern int kernel_nvram_decode(const char *in, char *out);

char * _nvram_get(const char *name);
int _nvram_set(const char *name, const char *value, int is_temp);
int _nvram_unset(const char *name);
int _nvram_getall(char *buf, int count, int include_temp);
int _nvram_generate(struct nvram_header *header, int rehash);
int _nvram_init(struct nvram_header *header);
void _nvram_uninit(void);

static struct nvram_tuple *nvram_hash[257] = {NULL};

#define IS_LOCKED(t) (t->flag & ATTR_LOCK)
#define SET_LOCKED(t) do { t->flag |= ATTR_LOCK; } while (0)

int _nvram_purge_unlocked(void)
{
	size_t i;
	struct nvram_tuple *t, *next, **prev;

	/* Free hash table */
	for (i = 0; i < ARRAYSIZE(nvram_hash); i++) {
		for (t = nvram_hash[i], prev = &nvram_hash[i]; t; t = next) {
			next = t->next;
			if (!IS_LOCKED(t)) {
				*prev = t->next;
				_nvram_free(t);
			} else {
				prev = &t->next;
			}
		}
	}

	return 0;
}

/* Free all tuples. Should be locked. */
static void nvram_free(void)
{
	size_t i;
	struct nvram_tuple *t, *next;

	/* Free hash table */
	for (i = 0; i < ARRAYSIZE(nvram_hash); i++) {
		for (t = nvram_hash[i]; t; t = next) {
			next = t->next;
			_nvram_free(t);
		}
		nvram_hash[i] = NULL;
	}

	/* Indicate that all tuples have been freed */
	_nvram_reset();
}

/* String hash */
static inline uint hash(const char *s)
{
	uint hash = 0;

	while (*s)
		hash = 31 * hash + *s++;

	return hash;
}

/* (Re)initialize the hash table. Should be locked. */
static void nvram_rehash(struct nvram_header *header)
{
	char *name, *value, *end, *eq;
	char *ptr;
	int flag;

	/* Parse and set "name=value\0 ... \0\0" */
	name = (char *) &header[1];
	end = (char *) header + NVRAM_SPACE - 2;
	end[0] = end[1] = '\0';
	for (; *name; name = value + strlen(value) + 1) {
		if (!(eq = strchr(name, '=')))
			break;
		*eq = '\0';
		value = eq + 1;

		ptr = name;
		flag = 0;
		if (name[0] == ATTR_LOCK) {
			ptr = &name[1];
			flag = ATTR_LOCK;
		}

		_nvram_set(ptr, value, flag);
		*eq = '=';
	}
}

unsigned short calc_zbuf_chksum(int size, char *p)
{
        unsigned short sum = 0;
        unsigned short checksum = 0;
        int i;

        size = ((size + 1) >> 1) << 1;
        for (i = 0; i < size; i += 2) {
                sum += (p[i] << 8 | p[i + 1]); // fact config use be for checksum calc
        }

        checksum = 0xffff - sum + 0x1;

        return (checksum >> 8 | checksum << 8); // be
}

static unsigned short apply_zbuf_chksum(struct nvram_header *header, char *zbuf)
{
        header->chksum = calc_zbuf_chksum(header->len, zbuf);

        return 0;
}

unsigned short verify_zbuf_chksum(char *zbuf_with_header)
{
    struct nvram_header *header = (struct nvram_header*)zbuf_with_header;

    return (header->chksum == calc_zbuf_chksum(header->len,
			    zbuf_with_header + NVRAM_HEADER_SIZE) ? 0 : 1);
}

/* Get the value of an NVRAM variable. Should be locked. */
char * _nvram_get(const char *name)
{
	uint i;
	struct nvram_tuple *t;
	char *value;

	/* Hash the name */
	i = hash(name) % ARRAYSIZE(nvram_hash);

	/* Find the associated tuple in the hash table */
	for (t = nvram_hash[i]; t && strcmp(t->name, name); t = t->next);

	value = t ? t->value : NULL;

	return value;
}

/* Set the value of an NVRAM variable. Should be locked. */
int _nvram_set(const char *name, const char *value, int is_temp)
{
	uint i;
	struct nvram_tuple *t, *u, **prev;

	/* Hash the name */
	i = hash(name) % ARRAYSIZE(nvram_hash);

	/* Find the associated tuple in the hash table */
	for (prev = &nvram_hash[i], t = *prev; t && strcmp(t->name, name);
	     prev = &t->next, t = *prev);

	if (t != NULL && IS_LOCKED(t)) {
		//printk("key %s is locked\n", name);
		return -2;
	}

	if((strlen(value)>NVRAM_MAX_VALUE_LEN) || (strlen(name)>NVRAM_MAX_PARAM_LEN))
	{
		return -1;
	}

	/* (Re)allocate tuple */
	if (!(u = _nvram_realloc(t, name, value, is_temp & ATTR_TEMP)))
		return -1;

	/* Value reallocated */
	if (t && t == u)
		return 0;

	if (is_temp & ATTR_LOCK) {
		SET_LOCKED(u);
		//printk("set lock to key %s\n", name);
	}

	/* Store new tuple */
	*prev = u;

	/* Delete old tuple */
	if (t) {
		u->next = t->next;
		_nvram_free(t);
	}

	return 0;
}

/* Unset the value of an NVRAM variable. Should be locked. */
int _nvram_unset(const char *name)
{
	uint i;
	struct nvram_tuple *t, **prev;

	/* Hash the name */
	i = hash(name) % ARRAYSIZE(nvram_hash);

	/* Find the associated tuple in the hash table */
	for (prev = &nvram_hash[i], t = *prev; t && strcmp(t->name, name);
	     prev = &t->next, t = *prev);

	/* Delete old tuple */
	if (t) {
		*prev = t->next;
		_nvram_free(t);
	}

	return 0;
}

/* Get all NVRAM variables. Should be locked. */
int _nvram_getall(char *buf, int count, int include_temp)
{
	uint i;
	struct nvram_tuple *t;
	int len = 0;
	char value[NVRAM_MAX_VALUE_LEN];

	/* Write name=value\0 ... \0\0 */
	for (i = 0; i < ARRAYSIZE(nvram_hash); i++) {
		for (t = nvram_hash[i]; t; t = t->next) {
			if (!include_temp && t->val_tmp)
				continue;

			if((strlen(t->value)>NVRAM_MAX_VALUE_LEN) || (strlen(t->name)>NVRAM_MAX_PARAM_LEN))
			{
				continue;
			}

			memset(value, 0, NVRAM_MAX_VALUE_LEN);
			if (kernel_nvram_decode(t->value, value))
				break;
			if ((count - len) > (strlen(t->name) + 1 + strlen(value) + 1))
				len += sprintf(buf + len, "%s=%s", t->name, value) + 1;
			else
				break;
		}
	}

	return 0;
}

/* Regenerate NVRAM. Should be locked. */
int _nvram_generate(struct nvram_header *header, int rehash)
{
	int i;
	char *ptr, *end;
	struct nvram_tuple *t;
	char name[NVRAM_MAX_PARAM_LEN+1];

	/* Generate header */
	header->magic = NVRAM_MAGIC;
	header->version = NVRAM_VERSION;

	/* Pointer to data area */
	ptr = (char *)header + sizeof(struct nvram_header);

	/* Leave space for a double NUL at the end */
	end = (char *)header + NVRAM_SPACE - 2;

	/* Write out all tuples */
	for (i = 0; i < ARRAYSIZE(nvram_hash); i++) {
		for (t = nvram_hash[i]; t; t = t->next) {
			if (!rehash && t->val_tmp)
				continue;

			if((strlen(t->value)>NVRAM_MAX_VALUE_LEN) || (strlen(t->name)>NVRAM_MAX_PARAM_LEN))
			{
				continue;
			}

			/* set lock before 'name' when commit */
			memset(name, 0, NVRAM_MAX_PARAM_LEN+1);
			if (!rehash && IS_LOCKED(t)) {
				name[0] = ATTR_LOCK;
				strcpy(&name[1], t->name);
			} else {
				strcpy(name, t->name);
			}

			if ((ptr + strlen(name) + 1 + strlen(t->value) + 1) > end)
				break;
			ptr += sprintf(ptr, "%s=%s", name, t->value) + 1;
		}
	}

	/* End with a double NULL */
	ptr += 1;

	/* Set new length */
	header->len = ROUNDUP(ptr - (char *) header, 4);

	/* Fill residual by 0xFF */
	if (header->len < NVRAM_SPACE)
		memset((char *)header + header->len, 0xFF, NVRAM_SPACE - header->len);

	apply_zbuf_chksum(header, (char*)&header[1]);

	/* Reinitialize hash table */
	if (rehash) {
		nvram_free();
		nvram_rehash(header);
	}

	return 0;
}

/* Initialize hash table. Should be locked. */
int _nvram_init(struct nvram_header *header)
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

	nvram_rehash(header);

	return 0;
}

/* Free hash table. Should be locked. */
void _nvram_uninit(void)
{
	nvram_free();
}
