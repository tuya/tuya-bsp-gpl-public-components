#ifndef _AKPCM_LOOPBACK_H_
#define _AKPCM_LOOPBACK_H_
#include "ak_pcm_debug.h"

ssize_t loopback_read(struct akpcm_runtime *rt, char __user *buf, size_t count, loff_t *f_pos, unsigned int f_flags);
int loopback_open(struct akpcm_runtime *rt);
int loopback_close(struct akpcm_runtime *rt);
long loopback_ioctl(struct akpcm_runtime *rt, unsigned int cmd, unsigned long arg);
int copy_to_loopback(unsigned char *src_addr, unsigned int playback_hw_off, unsigned long actual_rate);
int wake_up_loopback(void);

#endif
