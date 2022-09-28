/*
 * PCM sys debug not
 *
 * Copyright (C) 2019
 * ANYKA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/io.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/cdev.h>

#include "ak_pcm_defs.h"

#define CACHE_SIZE 64

static ssize_t playback_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_runtime *play_rt = pcm->play_rt;

	size = scnprintf(buf, PAGE_SIZE,
		"playback info:\nDMA:0x%p Cache:0x%p Size:%d\nRate:%ld hp_gain:%d\nlast_write:%d isr:%d\nflag:0x%lx\nio:IN/%d OUT/%d\n",
		play_rt->dma_area, play_rt->cache_buff, play_rt->buffer_bytes,
		play_rt->actual_rate, play_rt->hp_gain_cur,
		play_rt->last_transfer_count, play_rt->last_isr_count,
		play_rt->runflag, play_rt->io_trace_in, play_rt->io_trace_out);

	return size;
}
static DEVICE_ATTR_RO(playback_info);

static ssize_t playback_cfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_pars *cfg = &(pcm->play_rt->cfg);

	size = scnprintf(buf, PAGE_SIZE, "playback cfg:\nID:%d\nRate:%d channels:%d\nsample_bits:%d\nperiod_bytes:%d periods:%d\n",
		cfg->id, cfg->rate, cfg->channels, cfg->sample_bits,
		cfg->period_bytes, cfg->periods);

	return size;
}
static DEVICE_ATTR_RO(playback_cfg);

static ssize_t playback_log_print_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_runtime *rt = pcm->play_rt;

	size = scnprintf(buf, PAGE_SIZE, "0x%x\n", rt->log_print);

	return size;
}

static ssize_t playback_log_print_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int err;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_runtime *rt = pcm->play_rt;

	err = kstrtou32(buf, 10, &(rt->log_print));
	if (err)
		return err;

	return len;
}
static DEVICE_ATTR_RW(playback_log_print);

static ssize_t loopback_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_runtime *loopback_rt = pcm->loopback_rt;

	size = scnprintf(buf, PAGE_SIZE,
		"loopback info:\nDMA:0x%p Cache:0x%p Size:%d\nRate:%ld\nlast_read:%d isr:%d\nflag:0x%lx\nio:IN/%d OUT/%d\n",
		loopback_rt->dma_area, loopback_rt->cache_buff, loopback_rt->buffer_bytes,
		loopback_rt->actual_rate, loopback_rt->last_transfer_count,
		loopback_rt->last_isr_count, loopback_rt->runflag,
		loopback_rt->io_trace_in, loopback_rt->io_trace_out);

	return size;
}
static DEVICE_ATTR_RO(loopback_info);

static ssize_t loopback_cfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_pars *cfg = &(pcm->loopback_rt->cfg);

	size = scnprintf(buf, PAGE_SIZE, "loopback cfg:\nID:%d\nRate:%d channels:%d\nsample_bits:%d\nperiod_bytes:%d periods:%d\n",
		cfg->id, cfg->rate, cfg->channels, cfg->sample_bits,
		cfg->period_bytes, cfg->periods);

	return size;
}
static DEVICE_ATTR_RO(loopback_cfg);

static ssize_t loopback_log_print_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_runtime *rt = pcm->loopback_rt;

	size = scnprintf(buf, PAGE_SIZE, "0x%x\n", rt->log_print);

	return size;
}

static ssize_t loopback_log_print_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int err;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_runtime *rt = pcm->loopback_rt;

	err = kstrtou32(buf, 10, &(rt->log_print));
	if (err)
		return err;

	return len;
}
static DEVICE_ATTR_RW(loopback_log_print);

static ssize_t capture_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_runtime *cptr_rt = pcm->cptr_rt;

	size = scnprintf(buf, PAGE_SIZE,
		"capture info:\nDMA:0x%p Cache:0x%p Size:%d\nRate:%ld\nlast_read:%d isr:%d\nflag:0x%lx\nio:IN/%d OUT/%d\n",
		cptr_rt->dma_area, cptr_rt->cache_buff, cptr_rt->buffer_bytes,
		cptr_rt->actual_rate, cptr_rt->last_transfer_count,
		cptr_rt->last_isr_count,cptr_rt->runflag, cptr_rt->io_trace_in, cptr_rt->io_trace_out);

	return size;
}
static DEVICE_ATTR_RO(capture_info);

static ssize_t capture_cfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_pars *cfg = &(pcm->cptr_rt->cfg);

	size = scnprintf(buf, PAGE_SIZE, "capture cfg:\nID:%d\nRate:%d channels:%d\nsample_bits:%d\nperiod_bytes:%d periods:%d\n",
		cfg->id, cfg->rate, cfg->channels, cfg->sample_bits,
		cfg->period_bytes, cfg->periods);

	return size;
}
static DEVICE_ATTR_RO(capture_cfg);

static ssize_t capture_log_print_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_runtime *rt = pcm->cptr_rt;

	size = scnprintf(buf, PAGE_SIZE, "0x%x\n", rt->log_print);

	return size;
}

static ssize_t capture_log_print_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int err;
	struct akpcm *pcm = dev_get_drvdata(dev);
	struct akpcm_runtime *rt = pcm->cptr_rt;

	err = kstrtou32(buf, 10, &(rt->log_print));
	if (err)
		return err;

	return len;
}
static DEVICE_ATTR_RW(capture_log_print);

static struct attribute *pcm_attrs[] = {
	&dev_attr_playback_info.attr,
	&dev_attr_loopback_info.attr,
	&dev_attr_capture_info.attr,
	&dev_attr_playback_cfg.attr,
	&dev_attr_loopback_cfg.attr,
	&dev_attr_capture_cfg.attr,
	&dev_attr_playback_log_print.attr,
	&dev_attr_loopback_log_print.attr,
	&dev_attr_capture_log_print.attr,
	NULL,
};

static struct attribute_group pcm_grp = {
	.name = "pcm",
	.attrs = pcm_attrs,
};

int ak_pcm_sys_init(struct akpcm *pcm)
{
	struct device *dev = pcm->dev;

	return sysfs_create_group(&dev->kobj, &pcm_grp);
}

int ak_pcm_sys_exit(struct akpcm *pcm)
{
	struct device *dev = pcm->dev;

	sysfs_remove_group(&dev->kobj, &pcm_grp);

	return 0;
}
