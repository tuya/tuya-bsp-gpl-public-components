/*
 * Anyka OTG HS HCD (Full-Speed Host Controller Driver) for USB.
 *
 * Derived from the SL811 HCD, rewritten for AKOTG HS HCD.
 * Copyright (C) 2010 ANYKA LTD.
 *
 * Periodic scheduling is based on Roman's OHCI code
 * 	Copyright (C) 1999 Roman Weissgaerber
 *
 * The AK OTG HS Host controller handles host side USB. For Documentation,
 * refer to chapter 22 USB Controllers of Anyka chip Mobile Multimedia Application
 * Processor Programmer's Guide.
 *
 */

/*
 * Status:  Enumeration of USB Optical Mouse, USB Keyboard, USB Flash Disk, Ralink 2070/3070 USB WiFi OK.
 *          Pass basic test with USB Optical Mouse/USB Keyboard/USB Flash Disk.
 *          Ralink 2070/3070 USB WiFI Scanning/WEP basic test OK. Full Functions TBD.
 *
 * TODO:
 * - Use up to 6 active queues of FS HC(for now only 2 queues: EP0 & EPX(1-6))
 * - USB Suspend/Resume support
 * - Use urb->iso_frame_desc[] with ISO transfers
 * - Optimize USB FIFO data transfer/receive(4B->2B->1B)
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/dma-mapping.h>

#include "ak_otg_hshcd.h"
#include "ak_usb_hc.h"

static char *host_sw = "onboard";
module_param(host_sw, charp, S_IRUGO);

static const char hcd_name[] = "ak-hshcd";
extern struct akotghc_epfifo_mapping akotg_epfifo_mapping;;
extern struct workqueue_struct *g_otghc_wq;
extern struct delayed_work	g_otg_rest;
static struct proc_dir_entry* usb_proc;

static struct hc_driver akhs_otg_driver = {
	.description		= hcd_name,
	.product_desc 		= "Anyka usb host controller",
	.hcd_priv_size 		= sizeof(struct akotg_usbhc),

	/*
	 * generic hardware linkage
	 */
	.irq			= akotg_usbhc_irq,
	.flags			= HCD_USB2 | HCD_MEMORY,

	/* Basic lifecycle operations */
	.start			= akotg_usbhc_start,
	.stop			= akotg_usbhc_stop,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_dequeue		= akotg_usbhc_urb_dequeue,
	.urb_enqueue		= akotg_usbhc_urb_enqueue,
	.endpoint_reset		= akotg_usbhc_endpoint_reset,
	.endpoint_disable	= akotg_usbhc_endpoint_disable,

	/*
	 * periodic schedule support
	 */
	.get_frame_number	= akotg_usbhc_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= akotg_usbhc_hub_status_data,
	.hub_control		= akotg_usbhc_hub_control,
	#ifdef	CONFIG_PM
	.bus_suspend		= akotg_usbhc_bus_suspend,
	.bus_resume		= akotg_usbhc_bus_resume,
	#else
	.bus_suspend		= NULL,
	.bus_resume		= NULL,
	#endif
};


static int 
akotg_hc_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	//struct akotghc_usb_platform_data *pdata = NULL;

	struct akotg_usbhc *akotghc;

	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);

	akotghc = hcd_to_akotg_usbhc(hcd);
#ifdef CONFIG_USB_AKOTG_DMA        
	//free dma irq
    if(akotghc->dma_irq > 0)
    {
        free_irq(akotghc->dma_irq, hcd);
        akotghc->dma_irq = -1;
    }  
     //clear all dma
     akotg_dma_clear(akotghc, 0);  
#endif
	clk_disable_unprepare(akotghc->clk);
	akotghc->clk = NULL;
	
	proc_remove(usb_proc);

	return 0;
}


/*
 * /proc/usb
 */
static int proc_usb_show(struct seq_file *m, void *v)
{
	seq_printf(m,
			   "usb_role: %s\n"
			   ,
			   (get_usb_role() == USB_HOST) ? "host":((get_usb_role() == USB_SLAVE)?"slave":"unknown"));
	return 0;
}
static int ion_info_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, proc_usb_show, NULL);
}

static const struct file_operations proc_usb_operations = {
	.open		= ion_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int 
akotg_hc_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct akotg_usbhc *akotghc;
	struct resource	*res;
	int			irq;
	int			retval = 0;
	unsigned long		irqflags;
	int i, j;
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(!res) 
		return -ENODEV;

	if(!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	/* usb_create_hcd requires dma_mask != NULL */
    if (!pdev->dev.dma_mask)
        pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	/* allocate and initialize hcd */
	hcd = usb_create_hcd(&akhs_otg_driver, &pdev->dev, akhs_otg_driver.product_desc);
	if (!hcd) {
		retval = -ENOMEM;
		goto err_nomem;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	akotghc = hcd_to_akotg_usbhc(hcd);
	
	//usb_poweron_for_device(pdata);
	
	akotghc->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(akotghc->clk)) {
		dev_err(&pdev->dev,	"usb otg hs clocks missing\n");
		akotghc->clk = NULL;
		goto err_nomem;
	}

    clk_prepare_enable(akotghc->clk);

	/* basic sanity checks first.  board-specific init logic should
	 * have initialized these three resources and probably board
	 * specific platform_data.  we don't probe for IRQs, and do only
	 * minimal sanity checking.
	 */
	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev,	"Found HC with no IRQ. Check %s setup!\n", 
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto err_nodev;
	}
	akotghc->mcu_irq = irq;

	spin_lock_init(&akotghc->lock);
	INIT_LIST_HEAD(&akotghc->async_ep0);
	for(i=0; i<MAX_EP_NUM; i++)
		INIT_LIST_HEAD(&akotghc->async_epx[i]);

	akotghc->active_ep0 = NULL;
	for(j=0; j<MAX_EP_NUM; j++)
		akotghc->active_epx[j] = NULL;

	init_timer(&akotghc->timer);
	akotghc->timer.function = akotg_usbhc_timer;
	akotghc->timer.data = (unsigned long) akotghc;
	
	hcd->speed = HCD_USB2;

	/* The chip's IRQ is level triggered, active high.  A requirement
	 * for platform device setup is to cope with things like signal
	 * inverters (e.g. CF is active low) or working only with edge
	 * triggers (e.g. most ARM CPUs).  Initial driver stress testing
	 * was on a system with single edge triggering, so most sorts of
	 * triggering arrangement should work.
	 *
	 * Use resource IRQ flags if set by platform device setup.
	 */
	irqflags = IRQF_SHARED;
	retval = usb_add_hcd(hcd, irq, irqflags);
	if (retval != 0)
		goto err_addhcd;

	init_epfifo_mapping(&akotg_epfifo_mapping);

#ifdef CONFIG_USB_AKOTG_DMA        
	akotg_dma_init(akotghc);

    //request dma irq
    irq = platform_get_irq(pdev, 1);
	retval = request_irq(irq, akotg_dma_irq, 0, "otgdma", hcd);
	if (retval < 0){
    	akotghc->dma_irq = -1;
		dev_err(&pdev->dev,"request irq %d failed\n", irq);
	}
    else {
    	akotghc->dma_irq = irq;
    }

#endif
	usb_proc = proc_create("usb", 0, NULL, &proc_usb_operations);

	dev_info(&pdev->dev,"Usb otg-hs controller driver initialized\n");
	return retval;

 err_addhcd:
	usb_put_hcd(hcd);
 err_nomem:
 err_nodev:
	dev_err(&pdev->dev,"Usb otg-hs controller driver initialized error\n");
	return retval;
}

#ifdef	CONFIG_PM

/* for this device there's no useful distinction between the controller
 * and its root hub, except that the root hub only gets direct PM calls
 * when CONFIG_USB_SUSPEND is enabled.
 */
static int
akotg_hc_suspend(struct platform_device *pdev, pm_message_t state)
{
	int		retval = 0;

	switch (state.event) {
	case PM_EVENT_FREEZE:
		break;
	case PM_EVENT_SUSPEND:
	case PM_EVENT_HIBERNATE:
	case PM_EVENT_PRETHAW:		/* explicitly discard hw state */
		break;
	}
	return retval;
}

static int
akotg_hc_resume(struct platform_device *dev)
{
	return 0;
}

#else
#define	akotg_hc_suspend	NULL
#define	akotg_hc_resume	NULL
#endif


static const struct of_device_id akotg_hc_match[] = {
	{ .compatible = "anyka,ak37d-usb"},
	{ .compatible = "anyka,ak37e-usb"},
	{ .compatible = "anyka,ak39ev330-usb"},
	{}
};
MODULE_DEVICE_TABLE(of, akotg_hc_match);

struct platform_driver akotg_hc_driver = {
	.probe =	akotg_hc_probe,
	.remove =	akotg_hc_remove,

	.suspend =	akotg_hc_suspend,
	.resume =	akotg_hc_resume,
	.driver = {
		.name =	(char *) hcd_name,		
        .of_match_table	= of_match_ptr(akotg_hc_match),
		.owner = THIS_MODULE,
	},
};

static int __init akotg_hc_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	return platform_driver_register(&akotg_hc_driver);
}
module_init(akotg_hc_init);

static void __exit akotg_hc_cleanup(void)
{
	platform_driver_unregister(&akotg_hc_driver);

	if(g_otghc_wq)
	{
		cancel_delayed_work_sync(&g_otg_rest);
		flush_workqueue(g_otghc_wq);
		destroy_workqueue(g_otghc_wq);
		g_otghc_wq = NULL;
	}
}
module_exit(akotg_hc_cleanup);

MODULE_DESCRIPTION("Anyka USB Host driver");
MODULE_AUTHOR("Anyka Microelectronic Ltd.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.14");
