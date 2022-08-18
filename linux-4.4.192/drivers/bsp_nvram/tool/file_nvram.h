#ifndef _NVRAM_LINUX_H
#define _NVRAM_LINUX_H
 
#ifdef __cplusplus
        extern "C" {
#endif
 
int nvram_init(char *img_fname, int size);
 
void nvram_deinit();

int nvram_set(const char *name, const char *value, int attr);

int nvram_commit(void);

int nvram_show();

#ifdef __cplusplus
}
#endif

#endif

