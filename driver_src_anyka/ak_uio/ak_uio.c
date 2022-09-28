/**
 * Userspace I/O driver for anyka soc video hardware codec.
 * drivers/uio/ak39_uio.c
 *
 * Copyright (C) 2018 Anyka(Guangzhou) Microelectronics Technology Co., Ltd.
 *
 * Author: Feilong Dong <dong_feilong@anyka.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/platform_device.h>
#include <linux/uio_driver.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/stringify.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/clk.h>

#include <asm/io.h>  //lhd add for debug

struct ak_vcodec {
	struct uio_info *uioinfo;
	void __iomem *base;	

	int irq;
	struct clk *clk;
	unsigned int open_count;
	spinlock_t lock;
	unsigned long flags;
};

/* Bits in ak39_vcodec.flags */
enum {
	UIO_IRQ_DISABLED = 0,
};

static ssize_t uio_mmap_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct ak_vcodec *priv;
	struct uio_info *uioinfo;
	struct uio_mem *uiomem;

    priv = dev_get_drvdata(dev);
	uioinfo = priv->uioinfo;
	uiomem = &uioinfo->mem[0];

	return sprintf(buf, "address:0x%x size:0x%x.\n", uiomem->addr, uiomem->size);
}

static DEVICE_ATTR(uio_mmap, 0400 , uio_mmap_show, NULL);

static struct attribute *akuio_attributes[] = {
        &dev_attr_uio_mmap.attr,
        NULL,
};

static const struct attribute_group akuio_attrs = {
        .attrs = akuio_attributes,
};

/* hw codec special irq handle
 * irq status are handled in lib venc,nothing to do here
 */
irqreturn_t vcodec_irq_handler(int irq, struct uio_info *dev_info)
{
	struct ak_vcodec *priv = dev_info->priv;
	//pr_debug("enter vcodec_irq_handler, status=%x\n",__raw_readl(ioremap(0x20038018, 4)));
	
	if (!test_and_set_bit(0, &priv->flags))
	disable_irq_nosync(irq);
	
	return IRQ_HANDLED;
}

/**
 * When application open the akuio device, enable clk and reset the hw codec.
 */
static int ak_uio_open(struct uio_info *info, struct inode *inode)
{
	struct ak_vcodec *priv = info->priv;
	int ret = 0;
#if defined(CONFIG_MACH_AK37E)
	unsigned long clk_rate;
#endif

    //Add code here to make sure uio dev can be multi-opened.
    if( priv->open_count++ > 0 ) {
        pr_debug("uio has opened! open_count=%d\n", priv->open_count);
        return 0;
    }
#if defined(CONFIG_MACH_AK37E)
	/*
	* Due to 37E's decoder ip is different with 37Ds.
	* The platform will reset the decoder by themselves.
	* BUT 37E's decoder core donot has the reset register.
	* SO need to reset the decoder by kernel.
	* Since our clk driver enable the clk then will reset the modeule
	* by this clk. Here to check if the clock is enable or not.
	* IF enable, disable it at first. Then we can use the clk_prepare_enable
	* to enable the clock and reset the decoder.
	*/
	clk_rate = clk_get_rate(priv->clk);
	//pr_info("%s: rate: %lu\n", __func__, clk_rate);
	if (clk_rate > 0)
		clk_disable_unprepare(priv->clk);
#endif
	ret = clk_prepare_enable(priv->clk);
	if(ret)
	{
		pr_err("Failed to enable video codec clk!\n");
		return -1;
	}
	return 0;
}

/**
 * @brief     When application close the akuio device, disable clk of hw codec
 */
static int ak_uio_release(struct uio_info *info, struct inode *inode)
{
	struct ak_vcodec *priv = info->priv;

    //Add code here to make sure uio dev can be multi-opened.
	if(--priv->open_count > 0) {
	    pr_debug("uio does's closed to 0! open_count=%d\n", priv->open_count);
	    return 0;
	}

	//clk_disable_unprepare(priv->clk);

	return 0;
}


int ak_uio_irqcontrol(struct uio_info *info, s32 irq_on)
{
	struct ak_vcodec *priv = info->priv;
	unsigned long flags;

	/* Allow user space to enable and disable the interrupt
	 * in the interrupt controller, but keep track of the
	 * state to prevent per-irq depth damage.
	 *
	 * Serialize this operation to support multiple tasks and concurrency
	 * with irq handler on SMP systems.
	 */

	spin_lock_irqsave(&priv->lock, flags);
	if (irq_on) {
		if (__test_and_clear_bit(UIO_IRQ_DISABLED, &priv->flags))
			enable_irq(info->irq);
	} else {
		if (!__test_and_set_bit(UIO_IRQ_DISABLED, &priv->flags))
			disable_irq_nosync(info->irq);
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

static int ak_uio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ak_vcodec *priv;
	struct device_node *np = dev->of_node;
	
	struct uio_info *uioinfo;
	struct uio_mem *uiomem;
	
	int ret = -1;
	int i;

	pr_debug("enter %s", __func__);

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "%s: err_alloc struct ak_vcodec\n", __func__);
		goto err_alloc_priv;
	}

	/* allocate UIO structure */
	uioinfo = kzalloc(sizeof(*uioinfo), GFP_KERNEL);
	if (!uioinfo) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "%s: err_alloc struct uioinfo\n", __func__);
		goto err_alloc_uio;
	}

	priv->irq = platform_get_irq(pdev, 0);
	if(priv->irq == 0) {
		dev_err(&pdev->dev, "failed to get IRQ\n");
		ret = -EINVAL;
		goto err_get_irq;
	}

	priv->clk = clk_get(dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(&pdev->dev, "failed to find clock source.\n");
		ret = PTR_ERR(priv->clk);
		priv->clk = NULL;
		goto err_get_clk;
	}

	uiomem = &uioinfo->mem[0];

	for (i = 0; i < pdev->num_resources; ++i) {
		struct resource *r = &pdev->resource[i];

		if (r->flags != IORESOURCE_MEM)
			continue;

		if (uiomem >= &uioinfo->mem[MAX_UIO_MAPS]) {
			dev_warn(&pdev->dev, "device has more than "
					__stringify(MAX_UIO_MAPS)
					" I/O memory resources.\n");
			break;
		}

		uiomem->memtype = UIO_MEM_PHYS;
		uiomem->addr = r->start;
		uiomem->size = resource_size(r);
		uiomem->name = r->name;
		++uiomem;
	}

	while (uiomem < &uioinfo->mem[MAX_UIO_MAPS]) {
		uiomem->size = 0;
		++uiomem;
	}

	/* This driver requires no hardware specific kernel code to handle
	 * interrupts. Instead, the interrupt handler simply disables the
	 * interrupt in the interrupt controller. User space is responsible
	 * for performing hardware specific acknowledge and re-enabling of
	 * the interrupt in the interrupt controller.
	 *
	 * Interrupt sharing is not supported.
	 */
	/* ak39 video irq */
	/* interrupt is enabled to begin with */
	uioinfo->irq = priv->irq;
	uioinfo->irq_flags = 0; 
	uioinfo->priv = priv;
	
	uioinfo->name = np->name;
	uioinfo->version = "0.1.0";
	
	/* file handle */
	uioinfo->handler = vcodec_irq_handler;
	uioinfo->open = ak_uio_open;
	uioinfo->release = ak_uio_release;
	uioinfo->irqcontrol = ak_uio_irqcontrol;

	priv->uioinfo = uioinfo;

	/* open count */
    priv->open_count = 0;
	
	spin_lock_init(&priv->lock);
	ret = uio_register_device(&pdev->dev, priv->uioinfo);
	if (ret) {
		dev_err(&pdev->dev, "unable to register uio device\n");
		goto err_register;
	}

	platform_set_drvdata(pdev, priv);

    sysfs_create_group(&pdev->dev.kobj, &akuio_attrs);

	pr_err("register uio device successfully with irq: %d!\n", priv->irq);
	//pr_err("uioinfo=%p,priv->uioinfo=%p\n", uioinfo,priv->uioinfo);

	return 0;
	
err_register:
err_get_clk:
err_get_irq:
	kfree(uioinfo);
err_alloc_uio:
	kfree(priv);
err_alloc_priv:
	return ret;
}

static int ak_uio_remove(struct platform_device *pdev)
{
	struct ak_vcodec *priv = platform_get_drvdata(pdev);

    sysfs_remove_group(&pdev->dev.kobj, &akuio_attrs);

	uio_unregister_device(priv->uioinfo);

	kfree(priv->uioinfo);
	kfree(priv);
	clk_put(priv->clk);
	
	return 0;
}

static const struct of_device_id ak_uio_match[] = {
	{ .compatible = "anyka,ak39-uio-vencoder" },
	{ .compatible = "anyka,ak39-uio-vdecoder" },
	{ .compatible = "anyka,ak37d-uio-vencoder" },
	{ .compatible = "anyka,ak37d-uio-vdecoder" },
	{ .compatible = "anyka,ak37e-uio-vencoder" },
	{ .compatible = "anyka,ak37e-uio-vdecoder" },
	{ .compatible = "anyka,ak39ev330-uio-vencoder" },
	{},
};
MODULE_DEVICE_TABLE(of, ak_uio_match);

/* driver definition */
static struct platform_driver ak_uio_driver = {
	.probe = ak_uio_probe,
	.remove = ak_uio_remove,
	.driver = {
		.name = "ak-uio",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak_uio_match),
	},
};

/**
 * kernel module init function, register the driver to kernel
 */
static int __init ak_uio_init(void)
{
	return platform_driver_register(&ak_uio_driver);
}

/**
 * kernel module finally function, unregister the driver from kernel
 */
static void __exit ak_uio_exit(void)
{
	platform_driver_unregister(&ak_uio_driver);
}
module_init(ak_uio_init);
module_exit(ak_uio_exit);

MODULE_DESCRIPTION("Anyka UIO driver");
MODULE_AUTHOR("Anyka Microelectronic Ltd.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.06");
