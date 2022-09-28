/*
 *  pcm for anyka chip
 *  Copyright (c) by Anyka, Inc.
 *  Create by chen_yanhong 2020-06-19
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <mach/ak_l2.h>

#include "ak_pcm_defs.h"
#include "capture.h"
#include "playback.h"
#include "loopback.h"
#include "soundcard.h"
#include "ak_pcm_sys.h"

#define AKPCM_CLASS      "akpcm"
#define AKPCM_MAJOR      0
#define AKPCM_MAX_TYPE_SUPPORT_COUNT  2 /*MAX count for single device type*/

#define AKPCM_DEFAULT_PLAYBACK_DEV  (SND_CARD_DAC_PLAYBACK)
#define AKPCM_DEFAULT_CAPTURE_DEV  (SND_CARD_ADC_CAPTURE)

static int pcm_major = AKPCM_MAJOR;
static struct class *pcm_class;

/*
* akpcm
*   |---- playback_runtime --|
*   |---- loopback_runtime---|
*   |---- capture_runtime
*   |---- SND_PARAM
*               |--> soundcard
*/

static int akpcm_playback_open(struct inode *inode, struct file *filp)
{
	struct akpcm_runtime *rt = container_of(inode->i_cdev, struct akpcm_runtime, cdev);

	filp->private_data = rt;

	return playback_open(rt);
}

static int akpcm_playback_close(struct inode *inode, struct file *filp)
{
	struct akpcm_runtime *rt = filp->private_data;

	return playback_close(rt);
}

static long akpcm_playback_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct akpcm_runtime *rt = filp->private_data;

	return playback_ioctl(rt, cmd, arg);
}

static ssize_t akpcm_playback_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct akpcm_runtime *rt = filp->private_data;

	return playback_write(rt, buf, count, f_pos, filp->f_flags);
}

static int akpcm_capture_open(struct inode *inode, struct file *filp)
{
	struct akpcm_runtime *rt = container_of(inode->i_cdev, struct akpcm_runtime, cdev);

	filp->private_data = rt;

	return capture_open(rt);
}

static int akpcm_capture_close(struct inode *inode, struct file *filp)
{
	struct akpcm_runtime *rt = filp->private_data;

	return capture_close(rt);
}

static long akpcm_capture_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct akpcm_runtime *rt = filp->private_data;

	return capture_ioctl(rt, cmd, arg);
}

static ssize_t akpcm_capture_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct akpcm_runtime *rt = filp->private_data;

	return capture_read(rt, buf, count, f_pos, filp->f_flags);
}

static int akpcm_loopback_open(struct inode *inode, struct file *filp)
{
	struct akpcm_runtime *rt = container_of(inode->i_cdev, struct akpcm_runtime, cdev);

	filp->private_data = rt;

	return loopback_open(rt);
}

static int akpcm_loopback_close(struct inode *inode, struct file *filp)
{
	struct akpcm_runtime *rt = filp->private_data;

	return loopback_close(rt);
}

static long akpcm_loopback_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct akpcm_runtime *rt = filp->private_data;

	return loopback_ioctl(rt, cmd, arg);
}

static ssize_t akpcm_loopback_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct akpcm_runtime *rt = filp->private_data;

	return loopback_read(rt, buf, count, f_pos, filp->f_flags);
}

static const struct file_operations pcm_fops[AK_PCM_DEV_MAX] =
{
	[0] = {
		.owner   = THIS_MODULE,
		.open    = akpcm_playback_open,
		.release = akpcm_playback_close,
		.unlocked_ioctl = akpcm_playback_ioctl,
		.write   = akpcm_playback_write,
	},
	[1] = {
		.owner   = THIS_MODULE,
		.open    = akpcm_capture_open,
		.release = akpcm_capture_close,
		.unlocked_ioctl = akpcm_capture_ioctl,
		.read    = akpcm_capture_read,
	},
	[2] = {
		.owner   = THIS_MODULE,
		.open    = akpcm_loopback_open,
		.release = akpcm_loopback_close,
		.unlocked_ioctl = akpcm_loopback_ioctl,
		.read    = akpcm_loopback_read,
	},
};

static char* akpcm_device_name[] =
{
	"akpcm_dac",
	"akpcm_adc",
	"akpcm_loopback",
};

/*
 * akpcm_class_setup -
 * create pcm class node
 *
 * return 0 if successfully or negative error value if fail.
 */
static int akpcm_class_setup(void)
{
	int err = 0;
	dev_t devno;

	pcm_class = class_create(THIS_MODULE, AKPCM_CLASS);
	if (IS_ERR(pcm_class)) {
		err = PTR_ERR(pcm_class);
		ak_pcm_err("akpcm: can't register %s\n", AKPCM_CLASS);
		goto exit_class_setup;
	}

	if (pcm_major) {
		devno = MKDEV(pcm_major, 0);
		err = register_chrdev_region(devno, AK_PCM_DEV_MAX, "pcmchar");
		if (err < 0) {
			ak_pcm_err("akpcm: can't alloc chrdev\n");
			goto exit_class_setup;
		}
	} else {
		err = alloc_chrdev_region(&devno, 0, AK_PCM_DEV_MAX, "pcmchar");
		if (err < 0) {
			ak_pcm_err("akpcm: can't region chrdev\n");
			goto exit_class_setup;
		}
		pcm_major = MAJOR(devno);
	}

	ak_pcm_info("akpcm: pcm_major 0x%x", pcm_major);

	return 0;

exit_class_setup:
	class_destroy(pcm_class);

	return err;
}

/*
 * akpcm_class_cleanup -
 * remove pcm class node
 */
static void akpcm_class_cleanup(void)
{
	if (pcm_class) {
		class_destroy(pcm_class);
		pcm_class = NULL;
	}
}

/*
 * akpcm_cdev_setup -
 * create pcm cdev node
 *
 * @runtime: device runtime, provide the cdev member
 * @dev_type: which device type for runtime related with the enum akpcm_device
 *
 * return 0 if successfully or negative error value if fail.
 */
static int akpcm_cdev_setup(struct akpcm_runtime *runtime, int dev_type, int index)
{
	struct device *dev;
	int err = 0;
	dev_t devno = MKDEV(pcm_major, (dev_type*AKPCM_MAX_TYPE_SUPPORT_COUNT + index));

	cdev_init(&(runtime->cdev), &pcm_fops[dev_type]);
	err = cdev_add(&(runtime->cdev), devno, 1);
	if (err)
		return err;

	runtime->dev = device_create(pcm_class , NULL, devno, runtime, "%s%d", akpcm_device_name[dev_type], index);
	if (IS_ERR(runtime->dev)) {
		err = PTR_ERR(runtime->dev);
		ak_pcm_err("akpcm: can't create %s", akpcm_device_name[dev_type]);
	}

	return err;
}

/*
 * akpcm_cdev_release -
 * release pcm cdev node
 *
 * @cdev: the cdev which to release
 * @dev_type: which device type for runtime related with the enum akpcm_device
 *
 */
static void akpcm_cdev_release(struct cdev *cdev, int dev_type, int index)
{
	dev_t devno = MKDEV(pcm_major, (dev_type*AKPCM_MAX_TYPE_SUPPORT_COUNT + index));

	device_destroy(pcm_class, devno);
	cdev_del(cdev);
}

/*
 * akpcm_playback_runtime_init -
 * init the playback rumtime
 *
 * @pcm: the platform private data which manager the runtimes
 *
 * return 0 if successfully or negative error value if fail.
 */
static int akpcm_playback_runtime_init(struct akpcm *pcm)
{
	int err;
	int snd_dev = AKPCM_DEFAULT_PLAYBACK_DEV;
	struct device_node *np = pcm->np;

	if (of_find_property(np, "dac_list", NULL)) {
		if (of_property_read_u32(np, "dac_list", &snd_dev)) {
			ak_pcm_err("playback dac_list error");
			return -EINVAL;
		}
	}

	pcm->play_rt = kzalloc(sizeof(struct akpcm_runtime), GFP_KERNEL);
	if (pcm->play_rt == NULL) {
		ak_pcm_err("playback rt allocate failed");
		return -ENOMEM;
	}

	pcm->play_rt->snd_dev = snd_dev;
	err = akpcm_cdev_setup(pcm->play_rt, AK_PCM_DEV_PLAYBACK, 0);
	if (err) {
		ak_pcm_err("playback cdev fail %d", err);
		goto exit_playback_runtime_init;
	}

	init_waitqueue_head(&(pcm->play_rt->wq));
	clear_bit(STATUS_START_STREAM, &(pcm->play_rt->runflag));
	clear_bit(STATUS_WORKING, &(pcm->play_rt->runflag));
	clear_bit(STATUS_PAUSING, &(pcm->play_rt->runflag));
	clear_bit(STATUS_OPENED, &(pcm->play_rt->runflag));
	clear_bit(STATUS_PREPARED, &(pcm->play_rt->runflag));
	pcm->play_rt->dma_area = NULL;
	pcm->play_rt->cache_buff = NULL;
	pcm->play_rt->buffer_bytes = 0;
	pcm->play_rt->cfg.rate = 8000;
	pcm->play_rt->cfg.channels = 2;
	pcm->play_rt->cfg.sample_bits = 16;
	pcm->play_rt->cfg.period_bytes = 16384;
	pcm->play_rt->cfg.periods = 32;

	if (of_property_read_u32(np, "hp_gain", &pcm->play_rt->hp_gain_cur)) {
		pcm->play_rt->hp_gain_cur = DEFAULT_HPVOL;
	}

	ak_pcm_func("playback_runtime pcm(0x%p) rt 0x%p dev %d", pcm, pcm->play_rt, snd_dev);

	return 0;

exit_playback_runtime_init:
	kfree(pcm->play_rt);

	return err;
}

/*
 * akpcm_capture_runtime_init -
 * init the capture rumtime
 *
 * @pcm: the platform private data which manager the runtimes
 *
 * return 0 if successfully or negative error value if fail.
 */
static int akpcm_capture_runtime_init(struct akpcm *pcm, int index)
{
	int err;
	int snd_dev = AKPCM_DEFAULT_CAPTURE_DEV;
	struct device_node *np = pcm->np;
	struct akpcm_runtime *runtime;

	if (of_find_property(np, "adc_list", NULL)) {
		if (of_property_read_u32_index(np, "adc_list", index, &snd_dev)) {
			ak_pcm_err("capture no adc_list");
			return -EINVAL;
		}
	}

	runtime = kzalloc(sizeof(struct akpcm_runtime), GFP_KERNEL);
	if (runtime == NULL) {
		ak_pcm_err("capture rt allocate failed");
		return -ENOMEM;
	}

	err = akpcm_cdev_setup(runtime, AK_PCM_DEV_CAPTURE, index);
	if (err) {
		ak_pcm_err("capture cdev fail %d", err);
		goto exit_capture_runtime_init;
	}

	init_waitqueue_head(&(runtime->wq));
	clear_bit(STATUS_START_STREAM, &(runtime->runflag));
	clear_bit(STATUS_WORKING, &(runtime->runflag));
	clear_bit(STATUS_PAUSING, &(runtime->runflag));
	clear_bit(STATUS_OPENED, &(runtime->runflag));
	clear_bit(STATUS_PREPARED, &(runtime->runflag));
	runtime->dma_area = NULL;
	runtime->cache_buff = NULL;
	runtime->buffer_bytes = 0;
	runtime->cfg.rate = 8000;
	runtime->cfg.channels = 1;
	runtime->cfg.sample_bits = 16;
	runtime->cfg.period_bytes = 16384;
	runtime->cfg.periods = 16;
	runtime->snd_dev = snd_dev;

	if (of_property_read_u32(np, "mic_gain", &runtime->mic_gain_cur)) {
		runtime->mic_gain_cur = DEFAULT_MICVOL;
	}

	if (of_property_read_u32(np, "linein_gain", &runtime->linein_gain_cur)) {
		runtime->linein_gain_cur = DEFAULT_LINEINVOL;
	}

	if (index == 0)
		pcm->cptr_rt = runtime;
	else
		pcm->cptr_aux_rt = runtime;

	ak_pcm_func("capture_runtime pcm(0x%p) rt 0x%p dev %d", pcm, runtime, snd_dev);

	return 0;

exit_capture_runtime_init:
	kfree(runtime);

	return err;
}

/*
 * akpcm_loopback_runtime_init -
 * init the loopback rumtime
 *
 * @pcm: the platform private data which manager the runtimes
 *
 * return 0 if successfully or negative error value if fail.
 */
static int akpcm_loopback_runtime_init(struct akpcm *pcm)
{
	int err;

	pcm->loopback_rt = kzalloc(sizeof(struct akpcm_runtime), GFP_KERNEL);
	if (pcm->loopback_rt == NULL) {
		ak_pcm_err("loopback rt allocate failed");
		return -ENOMEM;
	}

	err = akpcm_cdev_setup(pcm->loopback_rt, AK_PCM_DEV_LOOPBACK, 0);
	if (err) {
		ak_pcm_err("loopback cdev fail %d", err);
		goto exit_loopback_runtime_init;
	}

	init_waitqueue_head(&(pcm->loopback_rt->wq));

	pcm->loopback_rt->dma_area = NULL;
	pcm->loopback_rt->cache_buff = NULL;
	pcm->loopback_rt->buffer_bytes = 0;
	clear_bit(STATUS_START_STREAM, &(pcm->loopback_rt->runflag));
	clear_bit(STATUS_WORKING, &(pcm->loopback_rt->runflag));
	clear_bit(STATUS_PAUSING, &(pcm->loopback_rt->runflag));
	clear_bit(STATUS_OPENED, &(pcm->loopback_rt->runflag));
	clear_bit(STATUS_PREPARED, &(pcm->loopback_rt->runflag));

	ak_pcm_func("loopback_runtime pcm(0x%p) rt 0x%p", pcm, pcm->loopback_rt);

	return 0;

exit_loopback_runtime_init:
	kfree(pcm->loopback_rt);

	return err;
}

/*
 * ak_free_runtimes -
 * release all akpcm_runtime
 *
 * @pcm: the platform private data which manager the runtimes
 *
 * return 0 if successfully or negative error value if fail.
 */
static int ak_free_runtimes(struct akpcm *pcm)
{
	if (pcm == NULL)
		return -EINVAL;

	if (pcm->play_rt) {
		if (pcm->play_rt->dma_area) {
			dma_unmap_single(pcm->play_rt->dev, pcm->play_rt->dma_addr, pcm->play_rt->buffer_bytes, DMA_TO_DEVICE);
			kfree(pcm->play_rt->dma_area);
			pcm->play_rt->dma_area = NULL;
		}
		if (pcm->play_rt->cache_buff) {
			vfree(pcm->play_rt->cache_buff);
			pcm->play_rt->cache_buff = NULL;
		}
		kfree(pcm->play_rt);
	}

	if (pcm->cptr_rt) {
		if (pcm->cptr_rt->dma_area) {
			dma_unmap_single(pcm->cptr_rt->dev, pcm->cptr_rt->dma_addr, pcm->cptr_rt->buffer_bytes, DMA_FROM_DEVICE);
			kfree(pcm->cptr_rt->dma_area);
			pcm->cptr_rt->dma_area = NULL;
		}
		/*
		* NOTICE:capture donot use the cache_buff
		*/
		if (pcm->cptr_rt->cache_buff) {
			kfree(pcm->cptr_rt->cache_buff);
			pcm->cptr_rt->cache_buff = NULL;
		}
		kfree(pcm->cptr_rt);
	}

	if (pcm->cptr_aux_rt) {
		if (pcm->cptr_aux_rt->dma_area) {
			dma_unmap_single(pcm->cptr_aux_rt->dev, pcm->cptr_aux_rt->dma_addr, pcm->cptr_aux_rt->buffer_bytes, DMA_FROM_DEVICE);
			kfree(pcm->cptr_aux_rt->dma_area);
			pcm->cptr_aux_rt->dma_area = NULL;
		}
		/*
		* NOTICE:capture donot use the cache_buff
		*/
		if (pcm->cptr_aux_rt->cache_buff) {
			kfree(pcm->cptr_aux_rt->cache_buff);
			pcm->cptr_aux_rt->cache_buff = NULL;
		}
		kfree(pcm->cptr_aux_rt);
	}

	if (pcm->loopback_rt) {
		if (pcm->loopback_rt->cache_buff != NULL) {
			vfree(pcm->loopback_rt->cache_buff);
			pcm->loopback_rt->cache_buff = NULL;
		}

		if (pcm->loopback_rt->dma_area) {
			vfree(pcm->loopback_rt->dma_area);
			pcm->loopback_rt->dma_area = NULL;
		}
		kfree(pcm->loopback_rt);
	}

	pcm->play_rt = NULL;
	pcm->cptr_rt = NULL;
	pcm->cptr_aux_rt = NULL;
	pcm->loopback_rt = NULL;

	return 0;
}

/*
 * akpcm_probe -
 * pcm driver probe
 *
 * @pdev: pointer to platform device
 *
 * return: 0-successful, <0- failed
 */
static int  akpcm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct akpcm *pcm = NULL;
	int err = 0, list_count = 0;
	int enable_level;
	struct resource *res;
	SND_PARAM *param;

	pcm = (struct akpcm *)kzalloc(sizeof(struct akpcm), GFP_KERNEL);
	if (pcm == NULL) {
		dev_err(dev,"akpcm probe alloc memory fail\n");
		return -ENOMEM;
	}

	param = &pcm->param;
	param->speaker_gpio = of_get_named_gpio(np, "speak-gpios", 0);
	if (param->speaker_gpio >= 0) {
		err = of_property_read_u32(np, "speak-gpios-en", &enable_level);
		if (err < 0) {
			enable_level = 1;
		}
		/* set PA off */
		param->speaker_en_level = enable_level;
	}
	ak_pcm_info("SPEAKER @%d(lv %d)", param->speaker_gpio, param->speaker_en_level);

	pcm->dev = dev;

	platform_set_drvdata(pdev, pcm);

	err = akpcm_class_setup();
	if (err) {
		dev_err(dev,"akpcm_class_setup %d\n", err);
		goto exit_akpcm_class_setup;
	}

	err = akpcm_playback_runtime_init(pcm);
	if (err) {
		dev_err(dev,"playback rt init fail %d\n", err);
		goto exit_playback_runtime_init;
	}

	if (of_find_property(np, "adc_list", NULL)) {
		list_count = of_property_count_u32_elems(np, "adc_list");
		if ((list_count < 1) || (list_count > AKPCM_MAX_TYPE_SUPPORT_COUNT)) {
			dev_err(dev,"capture list out of range %d\n", list_count);
			goto exit_capture_runtime_init;
		}
	} else {
		list_count = 1;
	}
	ak_pcm_info("adc_list:%d", list_count);

	err = akpcm_capture_runtime_init(pcm, 0);
	if (err) {
		dev_err(dev,"capture rt init fail %d\n", err);
		goto exit_capture_runtime_init;
	}
	if (list_count > 1) {
		err = akpcm_capture_runtime_init(pcm, 1);
		if (err) {
			dev_err(dev,"capture2 rt init fail %d\n", err);
			goto exit_capture_runtime_init2;
		}
	}

	err = akpcm_loopback_runtime_init(pcm);
	if (err) {
		dev_err(dev,"loopback rt init fail %d\n", err);
		goto exit_loopback_runtime_init;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);    /* get SD_ADC/DAC clock control register */
	if (!res) {
		dev_err(dev,"no memory resource for analog_ctrl_res\n");
		err = -ENXIO;
		goto exit_akpcm_get_resource;
	}

	param->sysctrl_base = devm_ioremap(dev, res->start, res->end - res->start + 1);
	if (!param->sysctrl_base) {
		dev_err(dev,"could not remap analog_ctrl_res memory\n");
		err = -ENXIO;
		goto exit_akpcm_get_resource;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);     /* get ADC2 mode registers */
	if (!res) {
		dev_err(dev,"no memory resource for adda_cfg_res\n");
		err = -ENXIO;
		goto exit_akpcm_get_resource;
	}

	param->adda_cfg_base = devm_ioremap(dev, res->start, res->end - res->start + 1);
	if (!param->adda_cfg_base) {
		dev_err(dev,"could not remap adda_cfg_res memory\n");
		err = -ENXIO;
		goto exit_akpcm_get_resource;
	}

	/* adc clock */
	param->sdadc_gclk = of_clk_get_by_name(np, "sdadc_gclk");
	param->sdadc_clk = of_clk_get_by_name(np, "sdadc_clk");
	param->sdadchs_clk = of_clk_get_by_name(np, "sdadchs_clk");

	/* dac clock */
	param->sddac_gclk = of_clk_get_by_name(np, "sddac_gclk");
	param->sddac_clk = of_clk_get_by_name(np, "sddac_clk");
	param->sddachs_clk = of_clk_get_by_name(np, "sddachs_clk");

	err = snd_card_init(param);
	if (err) {
		dev_err(dev,"sound card init fail %d\n", err);
		goto exit_snd_card_init;
	}

	if (ak_pcm_sys_init(pcm)) {
		dev_err(dev,"sys fs init fail\n");
	}

	ak_pcm_info("successful!");

	return 0;

exit_snd_card_init:
exit_akpcm_get_resource:
	akpcm_cdev_release(&(pcm->loopback_rt->cdev), AK_PCM_DEV_LOOPBACK, 0);
exit_loopback_runtime_init:
	akpcm_cdev_release(&(pcm->cptr_rt->cdev), AK_PCM_DEV_CAPTURE, 1);
exit_capture_runtime_init2:
	akpcm_cdev_release(&(pcm->cptr_rt->cdev), AK_PCM_DEV_CAPTURE, 0);
exit_capture_runtime_init:
	akpcm_cdev_release(&(pcm->play_rt->cdev), AK_PCM_DEV_PLAYBACK, 0);
exit_playback_runtime_init:
	class_destroy(pcm_class);
exit_akpcm_class_setup:
	if (err != 0) {
		ak_free_runtimes(pcm);
		kfree(pcm);
	}
	return err;
}

/*
 * akpcm_remove -
 * pcm platfrom driver remove
 *
 * @devptr: pointer to platform device
 *
 * return: 0-successful, <0- failed
 */
static int akpcm_remove(struct platform_device *pdev)
{
	struct akpcm *pcm = platform_get_drvdata(pdev);
	dev_t devno = MKDEV(pcm_major, 0);

	ak_pcm_info("akpcm_remove pcm 0x%p", pcm);

	ak_pcm_sys_exit(pcm);

	snd_card_deinit();

	/* release cdev */
	akpcm_cdev_release(&(pcm->play_rt->cdev), AK_PCM_DEV_PLAYBACK, 0);
	akpcm_cdev_release(&(pcm->cptr_rt->cdev), AK_PCM_DEV_CAPTURE, 0);
	if (pcm->cptr_aux_rt)
		akpcm_cdev_release(&(pcm->cptr_aux_rt->cdev), AK_PCM_DEV_CAPTURE, 1);
	akpcm_cdev_release(&(pcm->loopback_rt->cdev), AK_PCM_DEV_LOOPBACK, 0);

	unregister_chrdev_region(devno, AK_PCM_DEV_MAX);
	akpcm_class_cleanup();

	/* free runtimes */
	ak_free_runtimes(pcm);
	kfree(pcm);

	return 0;
}

static const struct of_device_id ak39_akpcm_of_ids[] = {
	{ .compatible = "anyka,ak39ev330-dac" },
	{ .compatible = "anyka,ak39ev330-adc-dac" },
	{ .compatible = "anyka,ak37d-adc-dac" },
	{ .compatible = "anyka,ak37e-adc-dac" },
	{},
};
MODULE_DEVICE_TABLE(of, ak39_akpcm_of_ids);

static struct platform_driver akpcm_driver = {
	.probe      = akpcm_probe,
	.remove     = akpcm_remove,
	.driver     = {
		.name   = "ak-codec",
		.of_match_table = of_match_ptr(ak39_akpcm_of_ids),
	},
};

static int __init akpcm_init(void)
{
	ak_pcm_info("akpcm_init...");
	return platform_driver_register(&akpcm_driver);
}

static void __exit akpcm_exit(void)
{
	platform_driver_unregister(&akpcm_driver);
}

module_init(akpcm_init);
module_exit(akpcm_exit);

MODULE_DESCRIPTION("Anyka PCM driver");
MODULE_AUTHOR("Anyka Microelectronic Ltd.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1.07");
