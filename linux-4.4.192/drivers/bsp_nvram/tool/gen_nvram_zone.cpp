/*
 * (C) Copyright 2020
 * Cheng Lei, TUYA Software Engineering, chenglei@tuya.com.
 * Tuya pack code
 */

#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <ftw.h>
#include <string>

#include "file_nvram.h"

using namespace std;
using std::string;

#define PROP_NAME_MAX 64
#define PROP_VALUE_MAX 256
#define MAX_FILE_LEN 512

static void load_properties(char *data, const char *filter);

int __system_property_add(const char *name, unsigned int namelen,
		const char *value, unsigned int valuelen)
{
	if (namelen >= PROP_NAME_MAX)
		return -1;
	if (valuelen >= PROP_VALUE_MAX)
		return -1;
	if (namelen < 1)
		return -1;

	//printf("name: %s value: %s\n", name, value);

	return nvram_set(name, value, 0);
}

static bool is_legal_property_name(const char* name, size_t namelen)
{
	size_t i;
	if (namelen >= PROP_NAME_MAX) return false;
	if (namelen < 1) return false;
	if (name[0] == '.') return false;
	if (name[namelen - 1] == '.') return false;

	/* Only allow alphanumeric, plus '.', '-', or '_' */
	/* Don't allow ".." to appear in a property name */
	for (i = 0; i < namelen; i++) {
		if (name[i] == '.') {
			// i=0 is guaranteed to never have a dot. See above.
			if (name[i-1] == '.') return false;
			continue;
		}
		if (name[i] == '_' || name[i] == '-') continue;
		if (name[i] >= 'a' && name[i] <= 'z') continue;
		if (name[i] >= 'A' && name[i] <= 'Z') continue;
		if (name[i] >= '0' && name[i] <= '9') continue;
		return false;
	}

	return true;
}

static int property_set_impl(const char* name, const char* value)
{
	size_t namelen = strlen(name);
	size_t valuelen = strlen(value);

	if (!is_legal_property_name(name, namelen))
		return -1;

	if (valuelen >= PROP_VALUE_MAX)
		return -1;

	int rc = __system_property_add(name, namelen, value, valuelen);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

int init_property_set(const char *name, const char *value)
{
	int rc = property_set_impl(name, value);
	if (rc == -1) {
		printf("property_set(\"%s\", \"%s\" failed\n", name, value);
	}

	return rc;
}

bool ReadFdToString(int fd, std::string* content)
{
	content->clear();

	char buf[BUFSIZ];
	ssize_t n;
	while (n = read(fd, &buf[0], sizeof(buf)) > 0) {
		n = BUFSIZ; // TODO: fix read return length
		content->append(buf, n);
	}
	return (n == 0) ? true : false;
}

bool read_file(const char* path, std::string* content)
{
	content->clear();

	int fd = open(path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
	if (fd == -1) {
		return false;
	}

	// For security reasons, disallow world-writable
	// or group-writable files.
	struct stat sb; 
	if (fstat(fd, &sb) == -1) {
		printf("fstat failed for '%s'\n", path);
		return false;
	}
	if ((sb.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
		printf("skipping insecure file '%s' mode %x %x %x\n", path, sb.st_mode, S_IWGRP,  S_IWOTH);
		return false;
	}   

	bool okay = ReadFdToString(fd, content);

	close(fd);

	return okay;
}

static void load_properties_from_file(const char* filename, const char* filter)
{
	std::string data;
	if (read_file(filename, &data)) {
		data.push_back('\n');
		load_properties(&data[0], filter);
	}

	printf("(Loading properties from %s)\n", filename);
}

/*                         
 * Filter is used to decide which properties to load: NULL loads all keys,
 * "ro.foo.*" is a prefix match, and "ro.foo.bar" is an exact match.
 */
static void load_properties(char *data, const char *filter)
{
	char *key, *value, *eol, *sol, *tmp, *fn;
	size_t flen = 0;

	if (filter) {
		flen = strlen(filter);
	}

	sol = data;
	while ((eol = strchr(sol, '\n'))) {
		key = sol;
		*eol++ = 0;
		sol = eol;

		while (isspace(*key)) key++;
		if (*key == '#') continue;

		tmp = eol - 2;
		while ((tmp > key) && isspace(*tmp)) *tmp-- = 0;

		if (!strncmp(key, "import ", 7) && flen == 0) {
			fn = key + 7;
			while (isspace(*fn)) fn++;

			key = strchr(fn, ' ');
			if (key) {
				*key++ = 0;
				while (isspace(*key)) key++;
			}

			load_properties_from_file(fn, key);

		} else {
			value = strchr(key, '=');
			if (!value) continue;
			*value++ = 0;

			tmp = value - 2;
			while ((tmp > key) && isspace(*tmp)) *tmp-- = 0;

			while (isspace(*value)) value++;

			if (flen > 0) {
				if (filter[flen - 1] == '*') {
					if (strncmp(key, filter, flen - 1)) continue;
				} else {
					if (strcmp(key, filter)) continue;
				}
			}

			init_property_set(key, value);
		}
	}
}

static void usage(char *programname)
{
	printf("%s (compiled %s)\n", programname, __DATE__);
	printf("Usage %s [OPTION]\n", programname);
	printf(
		" -s, --size       : zone size for nvram\n"
		" -c, --config     : key/value file to init nvram zone\n"
		" -o, --output     : output file name of name\n"
		" -h, --help       : print this help and exit\n"
	     );
}

static struct option long_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"config", required_argument, NULL, 'c'},
	{"size", required_argument, NULL, 's'},
	{"output", required_argument, NULL, 'o'},
	{NULL, 0, NULL, 0}
};

int main(int argc, char **argv)
{
	int opt_c = 0;
        int opt_s = 0;
        int opt_o = 0;
	int c;
	int size = 0;
	int ret = -1;
	char cfg_fname[MAX_FILE_LEN] = {0};
	char img_fname[MAX_FILE_LEN] = {0};

	while ((c = getopt_long(argc, argv, "hs:o:c:",
				long_options, NULL)) != EOF) {
		switch (c) {
			case 'c':
				strncpy(cfg_fname, optarg, sizeof(cfg_fname));
				opt_c = 1;
				break;
			case 's':
				char *ptr;
				size = strtol(optarg, &ptr, 16);
				if (size <= 0)
					break;
				opt_s = 1;
				break;
			case 'o':
				strncpy(img_fname, optarg, sizeof(img_fname));
				opt_o = 1;
				break;
			case 'h':
				usage(argv[0]);
				exit(0);
				break;
			default:
				usage(argv[0]);
				exit(1);
				break;
		}
	}

	if (opt_c == 0 || opt_s == 0 || opt_o == 0) {
		usage(argv[0]);
		exit(1);
	}

	printf("cfg: %s size: 0x%x\n", cfg_fname, size);

	ret = nvram_init(img_fname, size);
	if (ret) {
		printf("nvram init failed\n");
		exit(1);
	}

	load_properties_from_file(cfg_fname, NULL);

	ret = nvram_commit();
	ret |= nvram_commit();
	if (ret)
		printf("nvram commit failed\n");
	else
		nvram_show();

	nvram_deinit();

	return ret;
}
