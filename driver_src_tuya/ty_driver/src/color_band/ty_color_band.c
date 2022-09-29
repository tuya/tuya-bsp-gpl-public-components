/*
* ty_color_band.c - Tuya
*
* Copyright (C) 2020 Tuya Technology Corp.
*
* Author: dyh
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/
#include <ty_color_band.h>

extern struct ty_colorband_dev g_colorband_init __attribute__((weak));
struct ty_colorband_dev g_colorband_init;

static struct ty_colorband_dev *g_colorband_dev = NULL;

int ty_colorband_open(struct inode *node, struct file *filp)
{
//	struct ty_colorband_dev *ty_colorband_dev = container_of(filp->private_data, struct ty_colorband_dev, misc_dev);
	
    return 0;
}

int ty_colorband_release(struct inode *node, struct file *filp)
{
//	struct ty_colorband_dev *ty_colorband_dev = container_of(filp->private_data, struct ty_colorband_dev, misc_dev);

    return 0;
}

static ssize_t ty_colorband_write(struct file *filp, const char __user *buf, size_t count, loff_t * ppos)
{
    int ret 		= 0;
	struct spi_transfer *t;
    struct spi_message m;
	struct ty_colorband_dev *ty_colorband_dev = container_of(filp->private_data, struct ty_colorband_dev, misc_dev);
    struct spi_device *spi = (struct spi_device *)ty_colorband_dev->private_data;

	mutex_lock(&ty_colorband_dev->colorband_mutex);
	if (count > BYTE_MAX_NUM) {				//取决于SPI的发送能力
		MS_ERROR("out of sending range!\n");
		goto err0;
	}

	ty_colorband_dev->rgb_data = kzalloc(sizeof(BYTE) * count, GFP_KERNEL);
    if((ty_colorband_dev->rgb_data) == NULL) {
        MS_ERROR("kzalloc rgb_data error!\n");
        goto err0;
    }

    if(copy_from_user(ty_colorband_dev->rgb_data, (void *)buf, count)) {
		MS_ERROR("error: copy from user\n");
		goto err1;
    }

	ty_colorband_dev->channel 	= ty_colorband_dev->rgb_data[0];		//通道号
	ty_colorband_dev->byte_num 	= count - 1;							//需要发送的字节数
	if ((ty_colorband_dev->channel + 1) > ty_colorband_dev->pins_num) {
		MS_ERROR("channel must be less than %d\n", ty_colorband_dev->pins_num);
		goto err1;
	}

    t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
    if(t == NULL) {
        MS_ERROR("kzalloc spi_transfer error!\r\n");
		goto err1;
    }

    t->tx_buf 	= ty_colorband_dev->rgb_data + 1;                 /* 发送的数据，第一个数据是通道号，需要调过 */
    t->len 		= ty_colorband_dev->byte_num;
	t->tx_nbits	= ty_colorband_dev->tx_nbits;
	spi->mode	= ((ty_colorband_dev->tx_nbits) == 0x02) ? SPI_TX_DUAL : (((ty_colorband_dev->tx_nbits) == 0x04) ? SPI_TX_QUAD : SPI_MODE_0);
    spi_message_init(&m);
    spi_message_add_tail(t, &m);
    ret = spi_sync(spi, &m);
    if(ret < 0) {
        MS_ERROR("spi_sync failed!\r\n");
		goto err2;
    }

    kfree(t);
	kfree(ty_colorband_dev->rgb_data);
	mutex_unlock(&ty_colorband_dev->colorband_mutex);
	
	return 0;

err2:
    kfree(t);
err1:
	kfree(ty_colorband_dev->rgb_data);
err0:
	mutex_unlock(&ty_colorband_dev->colorband_mutex);

    return -1;
}

LONG ty_colorband_ioctl(struct file *filp, UINT cmd, DWORD arg)
{
	DWORD speed_hz = 0;
	struct ty_colorband_dev *ty_colorband_dev = container_of(filp->private_data, struct ty_colorband_dev, misc_dev);
	struct spi_device *spi = (struct spi_device *)ty_colorband_dev->private_data;

	mutex_lock(&ty_colorband_dev->colorband_mutex);
	if(copy_from_user(&speed_hz, (void *)arg, sizeof(speed_hz))) {
		MS_ERROR("copy mparam data error\n");
		mutex_unlock(&ty_colorband_dev->colorband_mutex);
		return -1;
	}
	
	switch(cmd) {
		case 0:
			spi->max_speed_hz = speed_hz;
			spi_setup(spi);
			break;
		default:
			break;
	}	
	mutex_unlock(&ty_colorband_dev->colorband_mutex);
	
	return 0;
}

struct file_operations ty_colorband_fops = {
    .owner          = THIS_MODULE,
    .open           = ty_colorband_open,
    .release        = ty_colorband_release,
    .write 			= ty_colorband_write,
    .unlocked_ioctl = ty_colorband_ioctl,
};

static int colorband_probe(struct spi_device *spi)
{
	int ret = 0;

	g_colorband_dev = kzalloc(sizeof(struct ty_colorband_dev), GFP_KERNEL);
	if (!g_colorband_dev) {
        MS_ERROR("kmalloc stepmotor dev fail.\n");
        return -1;
    }

	memcpy(g_colorband_dev, &g_colorband_init, sizeof(struct ty_colorband_dev));
	
	g_colorband_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
    g_colorband_dev->misc_dev.name 	= COLORBAND_DEV_NAME;
    g_colorband_dev->misc_dev.fops  = &ty_colorband_fops;
	
    ret = misc_register(&g_colorband_dev->misc_dev);
    if(unlikely(ret < 0)) {
        MS_ERROR("register misc fail\n");
    }
	mutex_init(&g_colorband_dev->colorband_mutex);
	/* 初始化 spi_device */
	spi->mode = SPI_MODE_0;  /* MODE0, CPOL = 0, CPHA = 0 */
	spi_setup(spi);

	g_colorband_dev->private_data = spi; 

	return ret;
}

static int colorband_remove(struct spi_device *spi)
{ 
    misc_deregister(&g_colorband_dev->misc_dev);

	kfree(g_colorband_dev);
	g_colorband_dev = NULL;
	
    return 0;
}

static const struct spi_device_id colorband_id_table[] = 
{
	{
	"tuya,colorband", 0},
	{
	},
};

static const struct of_device_id colorband_of_match[] = 
{   
	{
	.compatible = "tuya,colorband"},
	{
	},
};

/* spi 驱动结构体 */
static struct spi_driver colorband_driver = 
{
	.probe = colorband_probe,
	.remove = colorband_remove,
	.driver = {
		.name = "colorband",
		.owner = THIS_MODULE,
		.of_match_table = colorband_of_match,
	},
	.id_table = colorband_id_table,
};

int ty_colorband_init(void)
{
	spi_register_driver(&colorband_driver);
	
    MS_INFO("%s module is installed\n", __func__);

    return 0;
}

void ty_colorband_exit(void)
{	
	spi_unregister_driver(&colorband_driver);
	
    MS_INFO("%s module is removed.\n", __func__);
	
    return;
}
