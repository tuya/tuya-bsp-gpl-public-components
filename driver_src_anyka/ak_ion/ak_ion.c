/*
 * drivers/staging/android/ion/anyka/ak_ion.c
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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>

#include "ion.h"
#include "ion_priv.h"

/* for proc interface*/
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static struct ion_device *idev;
static int num_heaps;
static struct ion_heap **heaps;
static struct ion_platform_heap **heap_data;
static struct proc_dir_entry* ion_proc;

/*
 * /proc/ion
 */
static int ion_proc_show(struct seq_file *m, void *v)
{
	int i;
	char *ion_type;

	for( i=0; i<num_heaps; i++ )
	{
		seq_printf(m,
			   "heap-name: %s\n"
			   "heap-id: %d\n"
			   "base-address: 0x%lx\n"
			   "size: 0x%x\n"
			   "type: %s\n"
			   ,
			   heaps[i]->name,
			   heaps[i]->id,
			   (unsigned long)heap_data[i]->base,
			   (unsigned int)heap_data[i]->size,
			   (heap_data[i]->type ==ION_HEAP_TYPE_DMA) ? "cma":"prereserved");
	}
	return 0;
}
static int ion_info_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, ion_proc_show, NULL);
}

static const struct file_operations proc_ion_operations = {
	.open		= ion_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};


int ak_ion_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct device_node *child;
	
	int err = 0;
	int i;

	pr_debug("ak_ion_probe entry\n");
	of_property_read_u32(node, "heaps-nr", &num_heaps);

#ifdef CONFIG_BOOTARGS_RESERVED_MEM
	/* Now only support one bootargs reserved memory*/
	pr_debug("define CONFIG_BOOTARGS_RESERVED_MEM, bootargs_reserved_mem_status=%d\n",bootargs_reserved_mem_status);
	if (bootargs_reserved_mem_status == BRM_STA_INITED)
		num_heaps++;
#endif

	heaps = kzalloc(sizeof(struct ion_heap *) * num_heaps, GFP_KERNEL);
	heap_data = kzalloc(sizeof(struct ion_platform_heap*) * num_heaps, GFP_KERNEL);
	idev = ion_device_create(NULL);
	if (IS_ERR_OR_NULL(idev)) {
		kfree(heaps);
		return PTR_ERR(idev);
	}

	of_reserved_mem_device_init(dev);
	i = 0;

#ifdef CONFIG_BOOTARGS_RESERVED_MEM
	if (bootargs_reserved_mem_status == BRM_STA_INITED){
		heap_data[i] = kzalloc(sizeof(struct ion_platform_heap), GFP_KERNEL);
		heap_data[i]->base = (ion_phys_addr_t)bootargs_reserved_mem_base;
		heap_data[i]->size = (size_t)bootargs_reserved_mem_size;

		/*fix bootargs_reserved_mem'id = 0*/
		heap_data[i]->id = 0;
		heap_data[i]->align = (ion_phys_addr_t)bootargs_rmem_ion_align;
		if( bootargs_rmem_ion_type == 0)
			heap_data[i]->type = ION_HEAP_TYPE_DMA;
		else if( bootargs_rmem_ion_type == 1)
			heap_data[i]->type = ION_HEAP_TYPE_CARVEOUT;

		heap_data[i]->name = "bootargs_rmem_heap";
		heap_data[i]->priv = dev;

		pr_debug("bootargs_reserved_mem:heap_data[%d]:base=%x, size=%x, id=%d, align=%x, type=%d\n",
					i, bootargs_reserved_mem_base,bootargs_reserved_mem_size, heap_data[i]->id,bootargs_rmem_ion_align,bootargs_rmem_ion_type);

		heaps[i] = ion_heap_create(heap_data[i]);
		if (IS_ERR_OR_NULL(heaps[i])) {
				err = PTR_ERR(heaps[i]);
				goto error;
		}
		ion_device_add_heap(idev, heaps[i]);
		i++;
	}
#endif

	/* create the heaps as specified in the dts file */
	for_each_available_child_of_node(node, child){
		int ion_type;
		u32 base, size, align;
		
		heap_data[i] = kzalloc(sizeof(struct ion_platform_heap), GFP_KERNEL);
		of_property_read_u32(child, "ion-id", &heap_data[i]->id);
		of_property_read_u32(child, "ion-type", &ion_type);
		of_property_read_u32(child, "base-address", &base);
		of_property_read_u32(child, "size", &size);
		of_property_read_u32(child, "align", &align);

		heap_data[i]->base = (ion_phys_addr_t)base;
		heap_data[i]->size = (size_t)size;
		heap_data[i]->align = (ion_phys_addr_t)align;
		if( ion_type == 0)
			heap_data[i]->type = ION_HEAP_TYPE_DMA;
		if( ion_type == 1)
			heap_data[i]->type = ION_HEAP_TYPE_CARVEOUT;

		heap_data[i]->name = child->name;
		heap_data[i]->priv = dev;

		heaps[i] = ion_heap_create(heap_data[i]);
		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto error;
		}
		ion_device_add_heap(idev, heaps[i]);
		i++;
	}

	if (i > num_heaps)
		goto error;
	
	platform_set_drvdata(pdev, idev);

	ion_proc = proc_create("ion", 0, NULL, &proc_ion_operations);
	
	pr_debug("ak_ion_probe exit\n");
	return 0;
error:
	for (i = 0; i < num_heaps; i++) {
		if (heaps[i])
			ion_heap_destroy(heaps[i]);
	}
	kfree(heaps);
	return err;
}

int ak_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	of_reserved_mem_device_release(&pdev->dev);
	ion_device_destroy(idev);
	for (i = 0; i < num_heaps; i++)
	{
		kfree(heap_data[i]);
		ion_heap_destroy(heaps[i]);
	}
	kfree(heap_data);
	kfree(heaps);

	proc_remove(ion_proc);
	
	return 0;
}

static const struct of_device_id ak_ion_match[] = {
	{ .compatible = "anyka,ak39-ion-reserve" },
	{ .compatible = "anyka,ak37d-ion-reserve" },
	{ .compatible = "anyka,ak37e-ion-reserve" },
	{ .compatible = "anyka,ak39ev330-ion-reserve" },
	{ }
};
MODULE_DEVICE_TABLE(of, ak_ion_match);

static struct platform_driver ak_ion_driver = {
	.probe = ak_ion_probe,
	.remove = ak_ion_remove,
	.driver = { 
		.name = "ak-ion",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak_ion_match),
	}
};

static int __init ak_ion_init(void)
{
	return platform_driver_register(&ak_ion_driver);
}

static void __exit ak_ion_exit(void)
{
	platform_driver_unregister(&ak_ion_driver);
}

module_init(ak_ion_init);
module_exit(ak_ion_exit);

MODULE_DESCRIPTION("Anyka ION driver");
MODULE_AUTHOR("Anyka Microelectronic Ltd.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.04");
