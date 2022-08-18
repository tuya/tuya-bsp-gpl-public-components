/*
 * (C) Copyright 2000-2010
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <linux/stddef.h>
#include <search.h>
#include <errno.h>
#include <malloc.h>

DECLARE_GLOBAL_DATA_PTR;


/************************************************************************
 * Default settings to be used when no valid environment is found
 */
#include <env_default.h>

#ifdef CONFIG_SPINAND_FLASH
extern struct spinand_flash *env_flash;
#else
extern struct spi_flash *env_flash;
#endif

extern u32 g_config_env_offset;
extern u32 g_config_env_size;
extern u32 g_config_envbk_offset;
extern u32 g_config_envbk_size;


struct hsearch_data env_htab = {
	.change_ok = env_flags_validate,
};

static uchar __env_get_char_spec(int index)
{
	return *((uchar *)(gd->env_addr + index));
}
uchar env_get_char_spec(int)
	__attribute__((weak, alias("__env_get_char_spec")));

static uchar env_get_char_init(int index)
{
	/* if crc was bad, use the default environment */
	if (gd->env_valid)
		return env_get_char_spec(index);
	else
		return default_environment[index];
}

uchar env_get_char_memory(int index)
{
	return *env_get_addr(index);
}

uchar env_get_char(int index)
{
	/* if relocated to RAM */
	if (gd->flags & GD_FLG_RELOC) // cdh:yes
		return env_get_char_memory(index);
	else
		return env_get_char_init(index);
}

const uchar *env_get_addr(int index)
{
	if (gd->env_valid)
		return (uchar *)(gd->env_addr + index);
	else
		return &default_environment[index];
}

/*
 * Read an environment variable as a boolean
 * Return -1 if variable does not exist (default to true)
 */
int getenv_yesno(const char *var)
{
	char *s = getenv(var);

	if (s == NULL)
		return -1;
	return (*s == '1' || *s == 'y' || *s == 'Y' || *s == 't' || *s == 'T') ?
		1 : 0;
}

/*
 * Look up the variable from the default environment
 */
char *getenv_default(const char *name)
{
	char *ret_val;
	unsigned long really_valid = gd->env_valid;
	unsigned long real_gd_flags = gd->flags;

	/* Pretend that the image is bad. */
	gd->flags &= ~GD_FLG_ENV_READY;
	gd->env_valid = 0;
	ret_val = getenv(name);
	gd->env_valid = really_valid;
	gd->flags = real_gd_flags;
	return ret_val;
}

void set_default_env(const char *s)
{
	int flags = 0;

	if (sizeof(default_environment) > ENV_SIZE) {
		puts("*** Error - default environment is too large\n");
		return;
	}

	if (s) {
		if (*s == '!') {
			debug("*** Warning - %s, "
				"using default environment\n",
				s + 1);
		} else {
			flags = H_INTERACTIVE;
			puts(s);
		}
	} else {
		puts("Using default environment\n");
	}

	if (himport_r(&env_htab, (char *)default_environment,
			sizeof(default_environment), '\0', flags,
			0, NULL) == 0)
		error("Environment import failed: errno = %d\n", errno);

	gd->flags |= GD_FLG_ENV_READY;
}


/* [re]set individual variables to their value in the default environment */
int set_default_vars(int nvars, char * const vars[])
{
	/*
	 * Special use-case: import from default environment
	 * (and use \0 as a separator)
	 */
	return himport_r(&env_htab, (const char *)default_environment,
				sizeof(default_environment), '\0',
				H_NOCLEAR | H_INTERACTIVE, nvars, vars);
}
extern void rec_tick(int i);
/*
 * Check if CRC is valid and (if yes) import the environment.
 * Note that "buf" may or may not be aligned.
 */
int env_import(const char *buf, int check)
{
	env_t *ep = (env_t *)buf;

	if (check) {
		int ret = -1;
		uint32_t crc;
		uint32_t dst_crc;

		memcpy(&crc, &ep->crc, sizeof(crc));
		dst_crc=crc32( 0,ep->data,ENV_SIZE);
		//debug ("CRC32 for %08lx ... %08lx ==> %08lx\n",
		//	ep->data, ep->data + ENV_SIZE, dst_crc);

		if ( dst_crc != crc) {
			//check envbk is exit
			debug("g_config_envbk_offset:%d, g_config_envbk_size:%d\n", CONFIG_BKENV_OFFSET, CONFIG_BKENV_SIZE);
			if(CONFIG_BKENV_OFFSET != 0 && CONFIG_BKENV_SIZE != 0){
				printf("read envbk data\n");
				//read envbk
#ifdef CONFIG_SPINAND_FLASH
				ret = spinand_read_partition(env_flash, CONFIG_BKENV_OFFSET, CONFIG_BKENV_SIZE, (u8 *)buf, CONFIG_ENV_SIZE);
				if(ret == -1){
					printf("read envbk data fail\n");
					goto ENV_DEFAULT;
				}
#else
				ret = spi_flash_read(env_flash, CONFIG_BKENV_OFFSET, CONFIG_BKENV_SIZE, (void *)buf);
				if(ret){
					printf("read envbk data fail\n");
					goto ENV_DEFAULT;
				}
#endif


				memcpy(&crc, &ep->crc, sizeof(crc));
				dst_crc=crc32( 0,ep->data,ENV_SIZE);
				debug("dst_crc:%d, crc:%d\n", dst_crc, crc);
				if ( dst_crc == crc){
					printf("write envbk data to env\n");
					//from envbk write to env
#ifdef CONFIG_SPINAND_FLASH
					if( spinand_erase_partition(env_flash, CONFIG_ENV_OFFSET,  CONFIG_ENV_SIZE)  == -1){
						error("error: erase env partition fail : g_config_env_offset = %d\n", CONFIG_ENV_OFFSET);
						goto ENV_DEFAULT;
					}
					else{
						if( spinand_write_partition(env_flash, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE, (u8 *)buf, CONFIG_ENV_SIZE) == -1){
							error("error: write env partition fail : g_config_env_offset = %d\n", CONFIG_ENV_OFFSET);
							goto ENV_DEFAULT;
						}
					}
#else
					ret = spi_flash_erase(env_flash, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE);
					if (ret){
						printf("error:erase env partition fail, g_config_env_size:%d, %d", CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE);
						goto ENV_DEFAULT;
					}
					else{
						ret = spi_flash_write(env_flash, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE, buf);
						if (ret){
							printf("error:write env partition fail, g_config_env_size:%d, %d", CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE);
							goto ENV_DEFAULT;
						}
					}
#endif
					goto CHECK_OK;
				}
			}

			//printf("cdh:%s, !bad CRC!\n", __func__);
ENV_DEFAULT:
			printf("use defualt env\n");
			set_default_env("!bad CRC");
			return 0;
		}

		//printf("cdh:%s, !OK CRC!\n", __func__);
	}

CHECK_OK:
	if (himport_r(&env_htab, (char *)ep->data, ENV_SIZE, '\0', 0,
			0, NULL)) {
			//rec_tick(4);
		gd->flags |= GD_FLG_ENV_READY;
		debug("cdh:%s, !himport_r OK env_htab!\n", __func__);
		return 1;
	}

	printf("cdh:%s, !himport_r ERR env_htab!\n", __func__);

	error("Cannot import environment: errno = %d\n", errno);

	set_default_env("!import failed");

	return 0;
}


int env_import_spl(const char *buf, int check)
{
	env_t *ep = (env_t *)buf;

	if (check) {
		int ret = -1;
		uint32_t crc;
		uint32_t dst_crc;

		memcpy(&crc, &ep->crc, sizeof(crc));
		dst_crc=crc32( 0,ep->data,ENV_SIZE);
		//rec_tick(3);
		//debug ("CRC32 for %08lx ... %08lx ==> %08lx\n",
		//	ep->data, ep->data + ENV_SIZE, dst_crc);

		if ( dst_crc != crc) {
			//check envbk is exit
			debug("g_config_envbk_offset:%d, g_config_envbk_size:%d\n", CONFIG_BKENV_OFFSET, CONFIG_BKENV_SIZE);
			if(CONFIG_BKENV_OFFSET != 0 && CONFIG_BKENV_SIZE != 0){
				printf("read envbk data\n");
				//read envbk
#ifdef CONFIG_SPINAND_FLASH
				ret = spinand_read_partition(env_flash, CONFIG_BKENV_OFFSET, CONFIG_BKENV_SIZE, (u8 *)buf, CONFIG_ENV_SIZE);
				if(ret == -1){
					printf("read envbk data fail\n");
					goto ENV_DEFAULT;
				}
#else
				ret = spi_flash_read(env_flash, CONFIG_BKENV_OFFSET, CONFIG_BKENV_SIZE, (void *)buf);
				if(ret){
					printf("read envbk data fail\n");
					goto ENV_DEFAULT;
				}
#endif


				memcpy(&crc, &ep->crc, sizeof(crc));
				dst_crc=crc32( 0,ep->data,ENV_SIZE);
				debug("dst_crc:%d, crc:%d\n", dst_crc, crc);
				if ( dst_crc == crc){
					printf("write envbk data to env\n");
					//from envbk write to env
#ifdef CONFIG_SPINAND_FLASH
					if( spinand_erase_partition(env_flash, CONFIG_ENV_OFFSET,  CONFIG_ENV_SIZE)  == -1){
						error("error: erase env partition fail : g_config_env_offset = %d\n", CONFIG_ENV_OFFSET);
						goto ENV_DEFAULT;
					}
					else{
						if( spinand_write_partition(env_flash, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE, (u8 *)buf, CONFIG_ENV_SIZE) == -1){
							error("error: write env partition fail : g_config_env_offset = %d\n", CONFIG_ENV_OFFSET);
							goto ENV_DEFAULT;
						}
					}
#else
					ret = spi_flash_erase(env_flash, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE);
					if (ret){
						printf("error:erase env partition fail, g_config_env_size:%d, %d", CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE);
						goto ENV_DEFAULT;
					}
					else{
						ret = spi_flash_write(env_flash, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE, buf);
						if (ret){
							printf("error:write env partition fail, g_config_env_size:%d, %d", CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE);
							goto ENV_DEFAULT;
						}
					}
#endif
					goto CHECK_OK;
				}
			}

			//printf("cdh:%s, !bad CRC!\n", __func__);
ENV_DEFAULT:
			printf("use defualt env\n");
			set_default_env("!bad CRC");
			return 0;
		}

		//printf("cdh:%s, !OK CRC!\n", __func__);
	}

CHECK_OK:
	return 1;
	if (0)//himport_r(&env_htab, (char *)ep->data, ENV_SIZE, '\0', 0,0, NULL))
	{
			//rec_tick(4);
		gd->flags |= GD_FLG_ENV_READY;
		debug("cdh:%s, !himport_r OK env_htab!\n", __func__);
		return 1;
	}

	printf("cdh:%s, !himport_r ERR env_htab!\n", __func__);

	error("Cannot import environment: errno = %d\n", errno);

	set_default_env("!import failed");

	return 0;
}


void env_relocate(void)
{
#if defined(CONFIG_NEEDS_MANUAL_RELOC)
	env_reloc();
	env_htab.change_ok += gd->reloc_off;
	//printf("env_relocate1\n");
#endif
	if (gd->env_valid == 0) {
#if defined(CONFIG_ENV_IS_NOWHERE) || defined(CONFIG_SPL_BUILD)
		/* Environment not changable */
		set_default_env(NULL);
		//printf("env_relocate2\n");
#else
		bootstage_error(BOOTSTAGE_ID_NET_CHECKSUM);
		set_default_env("!bad CRC");
		//printf("env_relocate3\n");
#endif
	} else {
		env_relocate_spec(); // cdh:uboot240/common/env_sf.c
		//printf("env_relocate4\n");
	}
}

#if defined(CONFIG_AUTO_COMPLETE) && !defined(CONFIG_SPL_BUILD)
int env_complete(char *var, int maxv, char *cmdv[], int bufsz, char *buf)
{
	ENTRY *match;
	int found, idx;

	idx = 0;
	found = 0;
	cmdv[0] = NULL;

	while ((idx = hmatch_r(var, idx, &match, &env_htab))) {
		int vallen = strlen(match->key) + 1;

		if (found >= maxv - 2 || bufsz < vallen)
			break;

		cmdv[found++] = buf;
		memcpy(buf, match->key, vallen);
		buf += vallen;
		bufsz -= vallen;
	}

	qsort(cmdv, found, sizeof(cmdv[0]), strcmp_compar);

	if (idx)
		cmdv[found++] = "...";

	cmdv[found] = NULL;
	return found;
}
#endif
