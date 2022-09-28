#ifndef _AKPCM_PLAYBACK_H_
#define _AKPCM_PLAYBACK_H_

#include "ak_pcm_defs.h"
#include "ak_pcm_common.h"
#include "ak_pcm_debug.h"

long playback_ioctl(struct akpcm_runtime *rt, unsigned int cmd, unsigned long arg);
ssize_t playback_write(struct akpcm_runtime *rt, const char __user *buf, size_t count, loff_t *f_pos, unsigned int f_flags);
int playback_open(struct akpcm_runtime *rt);
int playback_close(struct akpcm_runtime *rt);

#endif
