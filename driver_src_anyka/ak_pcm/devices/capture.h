#ifndef _AKPCM_CAPTURE_H_
#define _AKPCM_CAPTURE_H_
#include "ak_pcm_defs.h"
#include "ak_pcm_debug.h"

void capture_pause(struct akpcm_runtime *rt);

long capture_ioctl(struct akpcm_runtime *rt, unsigned int cmd, unsigned long arg);
ssize_t capture_read(struct akpcm_runtime *rt, char __user *buf, size_t count, loff_t *f_pos, unsigned int f_flags);
int capture_open(struct akpcm_runtime *rt);
int capture_close(struct akpcm_runtime *rt);

#endif
