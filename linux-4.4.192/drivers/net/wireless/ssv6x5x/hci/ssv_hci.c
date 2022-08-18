/*
 * Copyright (c) 2015 iComm-semi Ltd.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/firmware.h>
#include <linux/completion.h>
#include <ssv6200.h>

#include <smac/dev.h>
#include <hal.h>
#include "hctrl.h"

//include firmware binary header
#include "ssv6x5x-sw.h"


MODULE_AUTHOR("iComm-semi, Ltd");
MODULE_DESCRIPTION("HCI driver for SSV6xxx 802.11n wireless LAN cards.");
MODULE_SUPPORTED_DEVICE("SSV6xxx WLAN cards");
MODULE_LICENSE("Dual BSD/GPL");

#define MAX_HW_RESOURCE_FULL_CNT  100 

static int ssv6xxx_hci_check_tx_resource(struct ssv6xxx_hci_ctrl *hci_ctrl, struct ssv_sw_txq *sw_txq, struct ssv6xxx_hw_resource *p_hw_resource, int *max_count);
static int ssv6xxx_hci_aggr_xmit(struct ssv6xxx_hci_ctrl *hci_ctrl, struct ssv_sw_txq *sw_txq, int max_count, struct ssv6xxx_hw_resource *phw_resource);
static int ssv6xxx_hci_xmit(struct ssv6xxx_hci_ctrl *hci_ctrl, struct ssv_sw_txq *sw_txq, int max_count, struct ssv6xxx_hw_resource *phw_resource);

static void ssv6xxx_hci_trigger_tx(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    /* if polling, wake up thread to send frame */
    wake_up_interruptible(&hci_ctrl->tx_wait_q);
}

static int ssv6xxx_hci_irq_enable(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    /* enable interrupt */
    HCI_IRQ_ENABLE(hci_ctrl);
    HCI_IRQ_SET_MASK(hci_ctrl, ~(hci_ctrl->int_mask));
    return 0;
}

static int ssv6xxx_hci_irq_disable(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    /* disable interrupt */
    HCI_IRQ_SET_MASK(hci_ctrl, 0xffffffff);
    HCI_IRQ_DISABLE(hci_ctrl);
    return 0;
}

static int ssv6xxx_hci_hwif_cleanup(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    HCI_HWIF_CLEANUP(hci_ctrl);
    return 0;
}

static int ssv6xxx_reset_skb(struct sk_buff *skb)
{
    struct skb_shared_info *s = skb_shinfo(skb);
    memset(s, 0, offsetof(struct skb_shared_info, dataref));
    atomic_set(&s->dataref, 1);
    memset(skb, 0, offsetof(struct sk_buff, tail));
    // skb->data = skb->head;  //align issue ???
    skb_reset_tail_pointer(skb);
    return 0;
}

#ifdef CONFIG_PM
static int ssv6xxx_hci_irq_suspend(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    /* disable interrupt except RX INT for host wakeup */
    HCI_IRQ_SET_MASK(hci_ctrl, 0xffffffff);
    HCI_IRQ_DISABLE(hci_ctrl);
    //HCI_IRQ_SET_MASK(hci_ctrl, 0xfffffffe);
    //HCI_IRQ_ENABLE(hci_ctrl);
    return 0;
}
#endif

static void *ssv6xxx_hci_open_firmware(char *user_mainfw)
{
    struct file *fp;

    fp = filp_open(user_mainfw, O_RDONLY, 0);
    if (IS_ERR(fp)) 
        fp = NULL;

    return fp;
}

static int ssv6xxx_hci_read_fw_block(char *buf, int len, void *image)
{
    struct file *fp = (struct file *)image;
    int rdlen;

    if (!image)
        return 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
    rdlen = kernel_read(fp, buf, len, &fp->f_pos);
#else    
    rdlen = kernel_read(fp, fp->f_pos, buf, len);
    if (rdlen > 0)
        fp->f_pos += rdlen;
#endif    

    return rdlen;
}

static void ssv6xxx_hci_close_firmware(void *image)
{
    if (image)
        filp_close((struct file *)image, NULL);
}

static u32 _ssv6xxx_hci_get_sram_mapping_address(bool need_chk, u32 sram_addr)
{
#define REMAPPING_ADDR_THRESHOLD    (0x00010000)
#define EXTRA_MAPPING_ADDR          (0x00100000-REMAPPING_ADDR_THRESHOLD)
    if((true == need_chk) && (REMAPPING_ADDR_THRESHOLD <= sram_addr))
    {
        return sram_addr+EXTRA_MAPPING_ADDR;
    }
    return sram_addr;
}

static int _ssv6xxx_hci_load_firmware(struct ssv6xxx_hci_ctrl *hci_ctrl, u8 *firmware_name, u8 openfile)
{
    int ret = 0;
    u8   *fw_buffer = NULL;
    u32   sram_addr = FW_START_SRAM_ADDR;
    u32   block_count = 0;
    u32   res_size=0, len=0, tolen=0;
    void *fw_fp = NULL;
    u8    interface = HCI_DEVICE_TYPE(hci_ctrl);
#ifdef ENABLE_FW_SELF_CHECK
    u32   checksum = FW_CHECKSUM_INIT;
    u32   fw_checksum, fw_clkcnt;
    u32   retry_count = 3;
    u32  *fw_data32;
#else
    int   writesize = 0;
    u32   retry_count = 1;
#endif
    u32   word_count, i;
    u32   fw_res_size = ssv6x5x_sw_bin_len;
    bool need_chk_map = false;

    //if (hci_ctrl->redownload == 1) {
    /* force mcu jump to rom for always*/
    // Redownload firmware
    HCI_DBG_PRINT(hci_ctrl, "Re-download FW\n");
    HCI_JUMP_TO_ROM(hci_ctrl);
    //}

	/* Get SRAM mapping mode:
	 * 0: ILM  64KB, DLM 128KB 
	 * 1: ILM 160KB, DLM 32KB
	 */
    if(HAL_GET_SRAM_MODE(hci_ctrl->shi->sh) == 0)
    {
        need_chk_map = true;
    }
    else
    {
        need_chk_map = false;
    }

    // Load firmware
    if(openfile)
    {
        fw_fp = ssv6xxx_hci_open_firmware(firmware_name);
        if (!fw_fp) {
            HCI_DBG_PRINT(hci_ctrl, "failed to find firmware (%s)\n", firmware_name);
            ret = -1;
            goto out;
        }
    }
    else
    {
        fw_fp = (void *)ssv6x5x_sw_bin;
    }

    // Allocate buffer firmware aligned with FW_BLOCK_SIZE and padding with 0xA5 in empty space.
    fw_buffer = (u8 *)kzalloc(FW_BLOCK_SIZE, GFP_KERNEL);
    if (fw_buffer == NULL) {
        HCI_DBG_PRINT(hci_ctrl, "Failed to allocate buffer for firmware.\n");
        goto out;
    }

    do {
#ifdef ENABLE_FW_SELF_CHECK
        checksum = FW_CHECKSUM_INIT;
        tolen = 0;

        if (openfile)
            ((struct file *)fw_fp)->f_pos = 0;
#endif

        // Disable MCU (USB ROM code must be alive for downloading FW, so USB doesn't do it)
        if (!(interface == SSV_HWIF_INTERFACE_USB)) {
            ret = SSV_LOAD_FW_DISABLE_MCU(hci_ctrl);
            if (ret == -1)
                goto out;
        }

        // Write firmware to SRAM address 0
        HCI_DBG_PRINT(hci_ctrl, "Writing firmware to SSV6XXX...\n");
        memset(fw_buffer, 0xA5, FW_BLOCK_SIZE);
        while(1)
        {
            if(openfile)
            {
                if(!(len = ssv6xxx_hci_read_fw_block((char*)fw_buffer, FW_BLOCK_SIZE, fw_fp)))
                {
                    break;
                }
            }
            else
            {
                if(0 == fw_res_size)
                {
                    break;
                }
                if(fw_res_size > FW_BLOCK_SIZE)
                {
                    len = FW_BLOCK_SIZE;
                }
                else
                {
                    len = fw_res_size;
                }
                memcpy((void *)fw_buffer, (const void *)fw_fp, len);
                fw_fp += len;
                fw_res_size -= len;
            }
            tolen += len;
            //printk("read len=%d,sram_addr=%d\n",len,sram_addr);
            if (len < FW_BLOCK_SIZE) {
                res_size = len;
                break;
            }
            fw_data32 = (u32 *)fw_buffer;
            //Add to avoid USB load code with turismo C0/D0 failed, skip 0x10000~0x10aff.
            if((SSV_HWIF_INTERFACE_USB == interface) &&
                (0x10000 <= sram_addr) && (0x10b00 > sram_addr))
            {
                if(0x10b00 < (sram_addr+len))
                {
                    if ((ret = HCI_LOAD_FW(hci_ctrl, _ssv6xxx_hci_get_sram_mapping_address(need_chk_map, 0x10b00), (u8 *)fw_buffer+(0x10b00-sram_addr), (sram_addr+len-0x10b00))) != 0)
                    {
                        break;
                    }
                    word_count = (len / sizeof(u32));
                    for (i = ((0x10b00-sram_addr)/sizeof(u32)); i < word_count; i++) {
                        checksum += fw_data32[i];
                    }
                }
            }
            else
            {
                if ((ret = HCI_LOAD_FW(hci_ctrl, _ssv6xxx_hci_get_sram_mapping_address(need_chk_map, sram_addr), (u8 *)fw_buffer, FW_BLOCK_SIZE)) != 0)
                {
                    break;
                }
                word_count = (len / sizeof(u32));
                for (i = 0; i < word_count; i++) {
                    checksum += fw_data32[i];
                }
            }
            sram_addr += FW_BLOCK_SIZE;
            //printk("\nper blk cks=%x\n",checksum);
            memset(fw_buffer, 0xA5, FW_BLOCK_SIZE);
        }

        //if(res_size)
        if(res_size)
        {
            u32 cks_blk_cnt,cks_blk_res;
            cks_blk_cnt = res_size / CHECKSUM_BLOCK_SIZE;
            cks_blk_res = res_size % CHECKSUM_BLOCK_SIZE;
            //printk("res_size = %d,cks_blk_cnt=%d\n",res_size,cks_blk_cnt);
            ret = HCI_LOAD_FW(hci_ctrl, _ssv6xxx_hci_get_sram_mapping_address(need_chk_map, sram_addr), (u8 *)fw_buffer, (cks_blk_cnt+1)*CHECKSUM_BLOCK_SIZE);
            word_count = (cks_blk_cnt * CHECKSUM_BLOCK_SIZE / sizeof(u32));
            fw_data32 = (u32 *)fw_buffer;
            for (i = 0; i < word_count; i++)
                checksum += *fw_data32++;

            //printk("last cks=%x,tolen=%d,res_size=%d\n",checksum,tolen,res_size);
            if(cks_blk_res) {
                word_count = (CHECKSUM_BLOCK_SIZE / sizeof(u32));
                //fw_data32 = (u32 *)&fw_buffer[cks_blk_cnt * CHECKSUM_BLOCK_SIZE];
                for (i = 0; i < word_count; i++) {
                    checksum += *fw_data32++;
                }
            }
            //printk("over cks=%x\n",checksum);
        }

        // Calculate the final checksum.
        checksum = ((checksum >> 24) + (checksum >> 16) + (checksum >> 8) + checksum) & 0x0FF;
        checksum <<= 16;

        if (ret == 0) {
            // Reset CPU for USB switching ROM to firmware
            if (interface == SSV_HWIF_INTERFACE_USB) {
                ret = SSV_RESET_CPU(hci_ctrl);
                if (ret == -1)
                    goto out;
            }
            // Set ILM/DLM
            HAL_SET_SRAM_MODE(hci_ctrl->shi->sh, SRAM_MODE_ILM_160K_DLM_32K);

            block_count = tolen / CHECKSUM_BLOCK_SIZE;
            res_size = tolen % CHECKSUM_BLOCK_SIZE;
            if(res_size)
                block_count++;
            // Inform FW that how many blocks is downloaded such that FW can calculate the checksum.
            SSV_LOAD_FW_SET_STATUS(hci_ctrl, (block_count << 16));
            SSV_LOAD_FW_GET_STATUS(hci_ctrl, &fw_clkcnt);
            HCI_DBG_PRINT(hci_ctrl, "(block_count << 16) = %x,reg =%x\n", (block_count << 16),fw_clkcnt);
            // Release reset to let CPU run.
            SSV_LOAD_FW_ENABLE_MCU(hci_ctrl);
            HCI_DBG_PRINT(hci_ctrl, "Firmware \"%s\" loaded\n", firmware_name);
            // Wait FW to calculate checksum.
            msleep(50);
            // Check checksum result and set to complement value if checksum is OK.
            SSV_LOAD_FW_GET_STATUS(hci_ctrl, &fw_checksum);
            fw_checksum = fw_checksum & FW_STATUS_MASK;
            if (fw_checksum == checksum) {
                SSV_LOAD_FW_SET_STATUS(hci_ctrl, (~checksum & FW_STATUS_MASK));
                ret = 0;
                HCI_DBG_PRINT(hci_ctrl, "Firmware check OK.%04x = %04x\n", fw_checksum, checksum);
                break;
            } else {
                HCI_DBG_PRINT(hci_ctrl, "FW checksum error: %04x != %04x\n", fw_checksum, checksum);
                ret = -1;
            }

        } else {
            HCI_DBG_PRINT(hci_ctrl, "Firmware \"%s\" download failed. (%d)\n", firmware_name, ret);
            ret = -1;
        }
    } while (--retry_count);

    if (ret)
        goto out;

        // hci_ctrl->redownload = 1;

    ret = 0;

out:
    if(openfile)
    {
        if(fw_fp)
        {
            ssv6xxx_hci_close_firmware(fw_fp);
        }
    }

    if (fw_buffer != NULL)
    {
        kfree(fw_buffer);
    }

    return ret;
}

static int ssv6xxx_hci_load_firmware_openfile(struct ssv6xxx_hci_ctrl *hci_ctrl, u8 *firmware_name)
{
    return _ssv6xxx_hci_load_firmware(hci_ctrl, firmware_name, 1);
}

#if 0 //Keep it, but not used.
static int ssv6xxx_hci_get_firmware(struct device *dev, char *user_mainfw, const struct firmware **mainfw)
{
    int ret;

    //hBUG_ON(helper == NULL);
    BUG_ON(mainfw == NULL);

    /* Try user-specified firmware first */
    if (*user_mainfw) {
        ret = request_firmware(mainfw, user_mainfw, dev);
        if (ret) {
            goto fail;
        }
        if (*mainfw)
            return 0;
    }

fail:
    /* Failed */
    if (*mainfw) {
        release_firmware(*mainfw);
        *mainfw = NULL;
    }

    return -ENOENT;
}

static int ssv6xxx_hci_load_firmware_request(struct ssv6xxx_hci_ctrl *hci_ctrl, u8 *firmware_name)
{
    int ret = 0;
    const struct firmware *ssv6xxx_fw = NULL;
    u8   *fw_buffer = NULL;
    u32   sram_addr = FW_START_SRAM_ADDR;
    u32   block_count = 0;
    u32   block_idx = 0;
    u32   res_size;
    u8   *fw_data;
    u8    interface = HCI_DEVICE_TYPE(hci_ctrl);
#ifdef ENABLE_FW_SELF_CHECK
    u32   checksum = FW_CHECKSUM_INIT;
    u32   fw_checksum;
    u32   retry_count = 3;
    u32  *fw_data32;
#else
    int   writesize = 0;
    u32   retry_count = 1;
#endif

    //if (hci_ctrl->redownload == 1) {
    /* force mcu jump to rom for always*/
    // Redownload firmware    
    HCI_DBG_PRINT(hci_ctrl, "Re-download FW\n");
    HCI_JUMP_TO_ROM(hci_ctrl);    
    //}

    // Load firmware
    ret = ssv6xxx_hci_get_firmware(hci_ctrl->shi->dev, firmware_name, &ssv6xxx_fw);
    if (ret) {
        HCI_DBG_PRINT(hci_ctrl, "failed to find firmware (%d)\n", ret);
        goto out;
    }

    // Allocate buffer firmware aligned with FW_BLOCK_SIZE and padding with 0xA5 in empty space.
    fw_buffer = (u8 *)kzalloc(FW_BLOCK_SIZE, GFP_KERNEL);
    if (fw_buffer == NULL) {
        HCI_DBG_PRINT(hci_ctrl, "Failed to allocate buffer for firmware.\n");
        goto out;
    }

#ifdef ENABLE_FW_SELF_CHECK
    block_count = ssv6xxx_fw->size / CHECKSUM_BLOCK_SIZE;
    res_size = ssv6xxx_fw->size % CHECKSUM_BLOCK_SIZE;
    {
        int word_count = (int)(block_count * CHECKSUM_BLOCK_SIZE / sizeof(u32));
        int i;
        fw_data32 = (u32 *)ssv6xxx_fw->data;
        for (i = 0; i < word_count; i++)
            checksum += fw_data32[i];

        if (res_size) {
            memset(fw_buffer, 0xA5, CHECKSUM_BLOCK_SIZE);
            memcpy(fw_buffer, &ssv6xxx_fw->data[block_count * CHECKSUM_BLOCK_SIZE], res_size);

            // Accumulate checksum for the incomplete block
            word_count = (int)(CHECKSUM_BLOCK_SIZE / sizeof(u32));
            fw_data32 = (u32 *)fw_buffer;
            for (i = 0; i < word_count; i++) {
                checksum += fw_data32[i];
            }
        }
    }

    // Calculate the final checksum.
    checksum = ((checksum >> 24) + (checksum >> 16) + (checksum >> 8) + checksum) & 0x0FF;
    checksum <<= 16;
#endif // ENABLE_FW_SELF_CHECK

    do {                
        // Disable MCU (USB ROM code must be alive for downloading FW, so USB doesn't do it)        
        if (!(interface == SSV_HWIF_INTERFACE_USB)) {
            ret = SSV_LOAD_FW_DISABLE_MCU(hci_ctrl);
            if (ret == -1)
                goto out;
        }

        // Write firmware to SRAM address 0
#ifdef ENABLE_FW_SELF_CHECK
        block_count = ssv6xxx_fw->size / FW_BLOCK_SIZE;
        res_size = ssv6xxx_fw->size % FW_BLOCK_SIZE;

        HCI_DBG_PRINT(hci_ctrl, "Writing %d blocks to SSV6XXX...", block_count);
        for (block_idx = 0, fw_data = (u8 *)ssv6xxx_fw->data, sram_addr = 0;block_idx < block_count;
                block_idx++, fw_data += FW_BLOCK_SIZE, sram_addr += FW_BLOCK_SIZE) {
            memcpy(fw_buffer, fw_data, FW_BLOCK_SIZE);
            if ((ret = HCI_LOAD_FW(hci_ctrl, sram_addr, (u8 *)fw_buffer, FW_BLOCK_SIZE)) != 0)
                break;
        }

        if(res_size) {
            memset(fw_buffer, 0xA5, FW_BLOCK_SIZE);
            memcpy(fw_buffer, &ssv6xxx_fw->data[block_count * FW_BLOCK_SIZE], res_size);
            if ((ret = HCI_LOAD_FW(hci_ctrl, sram_addr, (u8 *)fw_buffer,
                            ((res_size/CHECKSUM_BLOCK_SIZE)+1)*CHECKSUM_BLOCK_SIZE)) != 0)
                break;
        }
#else // ENABLE_FW_SELF_CHECK
        block_count = ssv6xxx_fw->size / FW_BLOCK_SIZE;
        res_size = ssv6xxx_fw->size % FW_BLOCK_SIZE;
        writesize = sdio_align_size(func,res_size);

        HCI_DBG_PRINT(hci_ctrl, "Writing %d blocks to SSV6XXX...", block_count);
        for (block_idx = 0, fw_data = (u8 *)ssv6xxx_fw->data, sram_addr = 0;block_idx < block_count;
                block_idx++, fw_data += FW_BLOCK_SIZE, sram_addr += FW_BLOCK_SIZE) {
            memcpy(fw_buffer, fw_data, FW_BLOCK_SIZE);
            if ((ret = HCI_LOAD_FW(hci_ctrl, sram_addr, (u8 *)fw_buffer, FW_BLOCK_SIZE)) != 0)
                break;
        }
        if(res_size) {
            memcpy(fw_buffer, &ssv6xxx_fw->data[block_count * FW_BLOCK_SIZE], res_size);
            if ((ret = HCI_LOAD_FW(hci_ctrl, sram_addr, (u8 *)fw_buffer, writesize)) != 0)
                break;
        }
#endif // ENABLE_FW_SELF_CHECK

        if (ret == 0) {
            // Reset CPU for USB switching ROM to firmware
            if (interface == SSV_HWIF_INTERFACE_USB) {
                ret = SSV_RESET_CPU(hci_ctrl);
                if (ret == -1)
                    goto out;
            }

#ifdef ENABLE_FW_SELF_CHECK
            block_count = ssv6xxx_fw->size / CHECKSUM_BLOCK_SIZE;
            res_size = ssv6xxx_fw->size % CHECKSUM_BLOCK_SIZE;
            if(res_size)
                block_count++;
            // Inform FW that how many blocks is downloaded such that FW can calculate the checksum.
            SSV_LOAD_FW_SET_STATUS(hci_ctrl, (block_count << 16));
#endif // ENABLE_FW_SELF_CHECK

            // Release reset to let CPU run.
            SSV_LOAD_FW_ENABLE_MCU(hci_ctrl);
            HCI_DBG_PRINT(hci_ctrl, "Firmware \"%s\" loaded\n", firmware_name);
#ifdef ENABLE_FW_SELF_CHECK
            // Wait FW to calculate checksum.
            msleep(50);
            // Check checksum result and set to complement value if checksum is OK.
            SSV_LOAD_FW_GET_STATUS(hci_ctrl, &fw_checksum);
            fw_checksum = fw_checksum & FW_STATUS_MASK;
            if (fw_checksum == checksum) {
                SSV_LOAD_FW_SET_STATUS(hci_ctrl, (~checksum & FW_STATUS_MASK));
                ret = 0;
                HCI_DBG_PRINT(hci_ctrl, "Firmware check OK.\n");
                break;
            } else {
                HCI_DBG_PRINT(hci_ctrl, "FW checksum error: %04x != %04x\n", fw_checksum, checksum);
                ret = -1;
            }
#endif
        } else {
            HCI_DBG_PRINT(hci_ctrl, "Firmware \"%s\" download failed. (%d)\n", firmware_name, ret);
            ret = -1;
        }
    } while (--retry_count);

    if (ret)
        goto out;

    // hci_ctrl->redownload = 1;

    ret = 0;    

out:
    if (ssv6xxx_fw)
        release_firmware(ssv6xxx_fw);

    if (fw_buffer != NULL)
        kfree(fw_buffer);

    return ret;
}
#endif

static int ssv6xxx_hci_load_firmware_fromheader(struct ssv6xxx_hci_ctrl *hci_ctrl, u8 *firmware_name)
{
    return _ssv6xxx_hci_load_firmware(hci_ctrl, firmware_name, 0);
}

// Due to USB Rx task always keeps running in background
// It must start/stop USB acc of Rx endpoint(Ep4) while HCI start/stop
// Don't stop Ep 1,2 here for accessing register and don't stop Ep 3 here for flush buffered TX data in HCI.
static int ssv6xxx_hci_start_acc(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    HCI_START_USB_ACC(hci_ctrl, 4);
    return 0;
}

static int ssv6xxx_hci_stop_acc(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    HCI_STOP_USB_ACC(hci_ctrl, 4);    
    return 0;
}

static int ssv6xxx_hci_start(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    /* 
     * Because of function offload, don't trun off IO.
     * Otherwise, it may encounter some trouble for interface down, 
     * ex: phy cali, phy enable/disable host command
     */
    //ssv6xxx_hci_irq_enable(hci_ctrl);
    //ssv6xxx_hci_start_acc(hci_ctrl);
    hci_ctrl->hci_start = true;
    //HCI_IRQ_TRIGGER(hci_ctrl);
    return 0;
}

static int ssv6xxx_hci_stop(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    //ssv6xxx_hci_irq_disable(hci_ctrl);
    //ssv6xxx_hci_stop_acc(hci_ctrl);
    hci_ctrl->hci_start = false;
    return 0;
}

#ifdef CONFIG_PM
static int ssv6xxx_hci_suspend(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    hci_ctrl->hci_start = false;
    ssv6xxx_hci_irq_suspend(hci_ctrl);
    ssv6xxx_hci_stop_acc(hci_ctrl);
    return 0;
}

static int ssv6xxx_hci_resume(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    ssv6xxx_hci_irq_enable(hci_ctrl);
    ssv6xxx_hci_start_acc(hci_ctrl);
    hci_ctrl->hci_start = true;
    HCI_IRQ_TRIGGER(hci_ctrl);
    return 0;
}
#endif

static void ssv6xxx_hci_write_hw_config(struct ssv6xxx_hci_ctrl *hci_ctrl, int val)
{
    hci_ctrl->write_hw_config = val;
}

static int ssv6xxx_hci_read_word(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 addr, u32 *regval)
{
    int ret = HCI_REG_READ(hci_ctrl, addr, regval);
    return ret;
}

static int ssv6xxx_hci_safe_read_word(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 addr, u32 *regval)
{
    int ret = HCI_REG_SAFE_READ(hci_ctrl, addr, regval);
    return ret;
}

#ifdef CONFIG_USB_EP0_RW_REGISTER
static int ssv6xxx_hci_mcu_read_word(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 addr, u32 *regval)
{
    int ret = HCI_REG_MCU_READ(hci_ctrl, addr, regval);
    return ret;
}
#endif

static int ssv6xxx_hci_write_word(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 addr, u32 regval)
{
    if (hci_ctrl->write_hw_config && (hci_ctrl->shi->write_hw_config_cb != NULL))
        hci_ctrl->shi->write_hw_config_cb((void *)SSV_SC(hci_ctrl), addr, regval);

    return HCI_REG_WRITE(hci_ctrl, addr, regval);
}

static int ssv6xxx_hci_safe_write_word(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 addr, u32 regval)
{
    if (hci_ctrl->write_hw_config && (hci_ctrl->shi->write_hw_config_cb != NULL))
        hci_ctrl->shi->write_hw_config_cb((void *)SSV_SC(hci_ctrl), addr, regval);

    return HCI_REG_SAFE_WRITE(hci_ctrl, addr, regval);
}

#ifdef CONFIG_USB_EP0_RW_REGISTER
static int ssv6xxx_hci_mcu_write_word(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 addr, u32 regval)
{
    if (hci_ctrl->write_hw_config && (hci_ctrl->shi->write_hw_config_cb != NULL))
           hci_ctrl->shi->write_hw_config_cb((void *)SSV_SC(hci_ctrl), addr, regval);
    
    return HCI_REG_MCU_WRITE(hci_ctrl, addr, regval);
}
#endif

static int ssv6xxx_hci_burst_read_word(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 *addr, u32 *regval, u8 reg_amount)
{
    int ret = HCI_BURST_REG_READ(hci_ctrl, addr, regval, reg_amount);
    return ret;
}

static int ssv6xxx_hci_burst_write_word(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 *addr, u32 *regval, u8 reg_amount)
{
    return HCI_BURST_REG_WRITE(hci_ctrl, addr, regval, reg_amount);
}

static int ssv6xxx_hci_burst_safe_read_word(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 *addr, u32 *regval, u8 reg_amount)
{
    int ret = HCI_BURST_REG_SAFE_READ(hci_ctrl, addr, regval, reg_amount);
    return ret;
}

static int ssv6xxx_hci_burst_safe_write_word(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 *addr, u32 *regval, u8 reg_amount)
{
    return HCI_BURST_REG_SAFE_WRITE(hci_ctrl, addr, regval, reg_amount);
}

static int ssv6xxx_hci_load_fw(struct ssv6xxx_hci_ctrl *hci_ctrl, u8 *firmware_name, u8 openfile)
{
    int ret = 0;

    SSV_LOAD_FW_PRE_CONFIG_DEVICE(hci_ctrl);

    if (openfile)
        ret = ssv6xxx_hci_load_firmware_openfile(hci_ctrl, firmware_name);
    else
        ret = ssv6xxx_hci_load_firmware_fromheader(hci_ctrl, firmware_name);
#if 0 //Keep it, but not used.
    ret = ssv6xxx_hci_load_firmware_request(hci_ctrl, firmware_name);
#endif

    // Sleep to let SSV6XXX get ready.
    msleep(50);

    if (ret == 0)
        SSV_LOAD_FW_POST_CONFIG_DEVICE(hci_ctrl);

    return ret;
}

static int ssv6xxx_hci_write_sram(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 addr, u8 *data, u32 size)
{
    return HCI_SRAM_WRITE(hci_ctrl, addr, data, size);
}

static int ssv6xxx_hci_pmu_wakeup(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    HCI_PMU_WAKEUP(hci_ctrl);
    return 0;
}

static int ssv6xxx_hci_interface_reset(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    HCI_IFC_RESET(hci_ctrl);
    return 0;
}

static int ssv6xxx_hci_sysplf_reset(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 addr, u32 value)
{
    HCI_SYSPLF_RESET(hci_ctrl, addr, value);
    return 0;
}

#ifdef CONFIG_PM
static int ssv6xxx_hci_hwif_suspend(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    HCI_HWIF_SUSPEND(hci_ctrl);
    return 0;
}

static int ssv6xxx_hci_hwif_resume(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    HCI_HWIF_RESUME(hci_ctrl);
    return 0;
}
#endif

static int ssv6xxx_hci_set_cap(struct ssv6xxx_hci_ctrl *hci_ctrl, enum ssv_hci_cap cap, bool enable)
{
    if (enable) {
        hci_ctrl->hci_cap |= BIT(cap);
    } else {
        hci_ctrl->hci_cap &= (~BIT(cap));
    }

    return 0;
}

static int ssv6xxx_hci_set_trigger_conf(struct ssv6xxx_hci_ctrl *hci_ctrl, bool en, u32 qlen, u32 pkt_size, u32 timeout)
{
    hci_ctrl->tx_trigger_en = en;
    hci_ctrl->tx_trigger_qlen = qlen;
    hci_ctrl->tx_trigger_pkt_size = pkt_size;
    hci_ctrl->task_timeout = timeout;
    return 0;
}

static void ssv6xxx_hci_cmd_done(struct ssv6xxx_hci_ctrl *hci_ctrl, struct sk_buff *skb)
{
    struct cfg_host_event *h_evt = NULL;

    if (hci_ctrl->ignore_cmd) 
        return;
    
    h_evt = (struct cfg_host_event *)skb->data;
    if ((h_evt->c_type == HOST_EVENT) && 
            (hci_ctrl->blocking_seq_no == h_evt->blocking_seq_no)) {
        hci_ctrl->blocking_seq_no = 0;
        complete(&hci_ctrl->cmd_done);
    }
}

static void ssv6xxx_hci_ignore_cmd(struct ssv6xxx_hci_ctrl *hci_ctrl, bool val)
{
    hci_ctrl->ignore_cmd = val;
    if (hci_ctrl->blocking_seq_no != 0) {
        hci_ctrl->blocking_seq_no = 0;
        complete(&hci_ctrl->cmd_done);
    }
}

static int ssv6xxx_hci_send_cmd(struct ssv6xxx_hci_ctrl *hci_ctrl, struct sk_buff *skb)
{
#define MAX_BLOCKING_CMD_WAIT_PERIOD 5000
    int ret = -1;
    unsigned long expire = msecs_to_jiffies(MAX_BLOCKING_CMD_WAIT_PERIOD);
    u32 blocking_seq_no = 0;
    struct cfg_host_cmd *host_cmd = NULL;
    struct sk_buff *p_aggr_skb = NULL;
    u32 aggr_skb_len = skb->len+32;
    static u32 left_len = 0;
    u32 cpy_len = 0; 
    static u8 *p_cpy_dest = NULL;
    static u8 host_cmd_seq_no = 1;

    if (hci_ctrl->ignore_cmd) {
        return 0;
    }

    mutex_lock(&hci_ctrl->cmd_mutex);
    host_cmd = (struct cfg_host_cmd *)skb->data;
    blocking_seq_no = host_cmd->blocking_seq_no;
    if (blocking_seq_no) {
        // update host cmd blocking seqno, blocking seqno must be unique
        host_cmd->blocking_seq_no |= (host_cmd_seq_no << 24);
        blocking_seq_no = host_cmd->blocking_seq_no;
        hci_ctrl->blocking_seq_no = host_cmd->blocking_seq_no; 
        host_cmd_seq_no++;
        if (host_cmd_seq_no == 128)
            host_cmd_seq_no = 1;
    }

    if (hci_ctrl->hci_cap & BIT(HCI_CAP_TX_AGGR)) {
        p_aggr_skb = hci_ctrl->shi->skb_alloc((void *)(SSV_SC(hci_ctrl)), aggr_skb_len);
        if (NULL == p_aggr_skb) {
            BUG_ON(1);
        }
        p_cpy_dest = p_aggr_skb->data;
        left_len = aggr_skb_len;

        // hci tx aggr data len must be 4*n
        if ((skb->len) & 0x3) {
            cpy_len = (skb->len) + (4 - ((skb->len) & 0x3));
        } else {
            cpy_len = skb->len;
        }

        if (left_len > (cpy_len + 8)) {
            /* add hci tx aggr header
               0| jmp_len_0[15:0]=L1   | jmp_len_1[15:0]=K1    |31
               0| extra_info_word_0[31:0]                      |31
               L1 is "data len(4*n) + header len(8)"
               K1 must be equal to L1
             */
            *((u32 *)(p_cpy_dest)) = ((cpy_len + 8) | ((cpy_len + 8) << 16));
            *((u32 *)(p_cpy_dest+4)) = 0;
            p_cpy_dest += 8;
            memcpy(p_cpy_dest, skb->data, cpy_len);
            p_cpy_dest +=  cpy_len;
            left_len -= (8 + cpy_len);
            skb_put(p_aggr_skb, 8 + cpy_len);

            /* add hci tx aggr tail
               0| jmp_len_0[15:0]=0 | jmp_len_1[15:0]=0 |31
             */
            *((u32 *)(p_cpy_dest)) = 0;
            p_cpy_dest += 4;
            skb_put(p_aggr_skb, 4);

            ret=IF_SEND(hci_ctrl, (void *)p_aggr_skb, p_aggr_skb->len, 0);

            hci_ctrl->shi->skb_free((void *)(SSV_SC(hci_ctrl)), p_aggr_skb);
            p_aggr_skb = NULL;
        }
    } else {
        ret = IF_SEND(hci_ctrl, (void *)skb, skb->len, 0);
    }

    if (ret < 0) {
        HCI_DBG_PRINT(hci_ctrl, "ssv6xxx_hci_send_cmd fail......\n");
        goto out;
    }

    if (blocking_seq_no) {
        if (!wait_for_completion_interruptible_timeout(&hci_ctrl->cmd_done, expire)) {
            printk("Cannot finish the command, host_cmd->h_cmd = 0x%08x, blocking_seq_no = 0x%08x\n", 
                    host_cmd->h_cmd, host_cmd->blocking_seq_no);
        }
    }
out:
    mutex_unlock(&hci_ctrl->cmd_mutex);

    return ret;

}

static int ssv6xxx_hci_enqueue(struct ssv6xxx_hci_ctrl *hci_ctrl, struct sk_buff *skb, int txqid,
            bool force_trigger, u32 tx_flags)
{
    struct ssv_sw_txq *sw_txq;
    //unsigned long flags;
    int qlen = 0;
    int pkt_size = skb->len;

    BUG_ON(txqid >= SSV_SW_TXQ_NUM || txqid < 0);
    if (txqid >= SSV_SW_TXQ_NUM || txqid < 0)
        return -1;

    if (hci_ctrl->hci_start == false) {
        dev_kfree_skb_any(skb);
        return 0;
    }

    sw_txq = &hci_ctrl->sw_txq[txqid];

    sw_txq->tx_flags = tx_flags;
    if (tx_flags & HCI_FLAGS_ENQUEUE_HEAD)
        skb_queue_head(&sw_txq->qhead, skb);
    else
        skb_queue_tail(&sw_txq->qhead, skb);

    atomic_inc(&hci_ctrl->sw_txq_cnt);
    qlen = (int)skb_queue_len(&sw_txq->qhead);
    
    if (0 != hci_ctrl->tx_trigger_en) {
        // check data txq len to trigger task
        if ((txqid < (SSV_SW_TXQ_NUM-1)) && 
            (false == force_trigger) &&
            (qlen < hci_ctrl->tx_trigger_qlen) && 
            (hci_ctrl->tx_trigger_pkt_size <= pkt_size)) {
            
            return qlen;
        }
    }

    ssv6xxx_hci_trigger_tx(hci_ctrl);

    return qlen;
}

static int ssv6xxx_hci_txq_len(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    int tx_count = 0;

    tx_count = atomic_read(&hci_ctrl->sw_txq_cnt);
    return tx_count;
}

static bool ssv6xxx_hci_is_txq_empty(struct ssv6xxx_hci_ctrl *hci_ctrl, int txqid)
{
    struct ssv_sw_txq *sw_txq;
    BUG_ON(txqid >= SSV_SW_TXQ_NUM);
    if (txqid >= SSV_SW_TXQ_NUM)
        return false;

    sw_txq = &hci_ctrl->sw_txq[txqid];
    if (skb_queue_len(&sw_txq->qhead) <= 0)
        return true;
    return false;
}

static int ssv6xxx_hci_txq_flush(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    struct ssv_sw_txq *sw_txq;
    struct sk_buff *skb = NULL;
    int txqid;

    for(txqid=0; txqid<SSV_SW_TXQ_NUM; txqid++) {
        sw_txq = &hci_ctrl->sw_txq[txqid];
        while((skb = skb_dequeue(&sw_txq->qhead))) {
            hci_ctrl->shi->hci_tx_buf_free_cb (skb, (void *)SSV_SC(hci_ctrl));
            atomic_dec(&hci_ctrl->sw_txq_cnt);
        }
    }
    /* free tx frame, it should update flowctl status */
    hci_ctrl->shi->hci_update_flowctl_cb((void *)(SSV_SC(hci_ctrl)));
    return 0;
}

static int ssv6xxx_hci_txq_flush_by_sta(struct ssv6xxx_hci_ctrl *hci_ctrl, int txqid)
{
    struct ssv_sw_txq *sw_txq;
    struct sk_buff *skb = NULL;

    if ((txqid >= (SSV_SW_TXQ_NUM - 1)) || (txqid < 0))     // TXQ 8 is Mgmt
        return -1;

    sw_txq = &hci_ctrl->sw_txq[txqid];
    while((skb = skb_dequeue(&sw_txq->qhead))) {
        hci_ctrl->shi->hci_tx_buf_free_cb (skb, (void *)SSV_SC(hci_ctrl));
        atomic_dec(&hci_ctrl->sw_txq_cnt);
    }
    
    /* free tx frame, it should update flowctl status */
    hci_ctrl->shi->hci_update_flowctl_cb((void *)(SSV_SC(hci_ctrl)));
    return 0;
}

static void ssv6xxx_hci_txq_lock_by_sta(struct ssv6xxx_hci_ctrl *hci_ctrl, int txqid)
{
    mutex_lock(&hci_ctrl->sw_txq[txqid].txq_lock);
}

static void ssv6xxx_hci_txq_unlock_by_sta(struct ssv6xxx_hci_ctrl *hci_ctrl, int txqid)
{
    mutex_unlock(&hci_ctrl->sw_txq[txqid].txq_lock);
}

static int ssv6xxx_hci_txq_pause(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 hw_txq_mask)
{
    struct ssv_sw_txq *sw_txq;    
    int txqid;

    if (SSV_SC(hci_ctrl) == NULL){
        HCI_DBG_PRINT(hci_ctrl, "%s: can't pause due to software structure not initialized !!\n", __func__);
        return 1;
    }    

    mutex_lock(&hci_ctrl->hw_txq_mask_lock);
    hci_ctrl->hw_txq_mask |= (hw_txq_mask & 0x1F);
    
    /* Pause all SWTXQ */
    for(txqid=0; txqid<SSV_SW_TXQ_NUM; txqid++) {
        sw_txq = &hci_ctrl->sw_txq[txqid];
        sw_txq->paused = true;
    }

    /* halt hardware tx queue */
    HAL_UPDATE_TXQ_MASK(SSV_SH(hci_ctrl), hci_ctrl->hw_txq_mask);
    mutex_unlock(&hci_ctrl->hw_txq_mask_lock);


    //HCI_DBG_PRINT(hci_ctrl, "%s(): hci_ctrl->txq_mas=0x%x\n", __FUNCTION__, hci_ctrl->hw_txq_mask);    
    return 0;
}

static int ssv6xxx_hci_txq_resume(struct ssv6xxx_hci_ctrl *hci_ctrl, u32 hw_txq_mask)
{
    struct ssv_sw_txq *sw_txq;
    int txqid;

    if (SSV_SC(hci_ctrl) == NULL){
        HCI_DBG_PRINT(hci_ctrl, "%s: can't resume due to software structure not initialized !!\n", __func__);
        return 1;
    }    
    
    /* resume hardware tx queue */
    mutex_lock(&hci_ctrl->hw_txq_mask_lock);
    hci_ctrl->hw_txq_mask &= ~(hw_txq_mask & 0x1F);
    HAL_UPDATE_TXQ_MASK(SSV_SH(hci_ctrl), hci_ctrl->hw_txq_mask);
    
    /* Resume all SWTXQ */
    for(txqid=0; txqid<SSV_SW_TXQ_NUM; txqid++) {
        sw_txq = &hci_ctrl->sw_txq[txqid];
        sw_txq->paused = false;
    }
    mutex_unlock(&hci_ctrl->hw_txq_mask_lock);

    ssv6xxx_hci_trigger_tx(hci_ctrl);

    //HCI_DBG_PRINT(hci_ctrl, "%s(): hci_ctrl->txq_mas=0x%x\n", __FUNCTION__, hci_ctrl->txq_mask);    
    return 0;
}

static void ssv6xxx_hci_txq_pause_by_sta(struct ssv6xxx_hci_ctrl *hci_ctrl, int txqid)
{
    struct ssv_sw_txq *sw_txq;    

    if ((txqid >= SSV_SW_TXQ_NUM) || (txqid < 0))
        return;

    sw_txq = &hci_ctrl->sw_txq[txqid];
    sw_txq->paused = true;
}

static void ssv6xxx_hci_txq_resume_by_sta(struct ssv6xxx_hci_ctrl *hci_ctrl, int txqid)
{
    struct ssv_sw_txq *sw_txq;
    
    if ((txqid >= SSV_SW_TXQ_NUM) || (txqid < 0))
        return;

    sw_txq = &hci_ctrl->sw_txq[txqid];
    sw_txq->paused = false;
}

static int _do_force_tx (struct ssv6xxx_hci_ctrl *hci_ctrl, int *err)
{
    int                q_num;
    int                tx_count = 0;
    struct ssv_sw_txq *sw_txq;
    struct ssv6xxx_hw_resource hw_resource;
    int max_count = 0;
    int (*handler)(struct ssv6xxx_hci_ctrl *, struct ssv_sw_txq *, int, struct ssv6xxx_hw_resource *);
    int ret = 0;

    if (hci_ctrl->hci_cap & BIT(HCI_CAP_TX_AGGR)) { 
        handler = ssv6xxx_hci_aggr_xmit;
    } else {
        handler = ssv6xxx_hci_xmit;
    }
    for (q_num = (SSV_SW_TXQ_NUM - 1); q_num >= 0; q_num--)
    {
        sw_txq = &hci_ctrl->sw_txq[q_num];
        max_count = skb_queue_len(&sw_txq->qhead);
        if (0 == max_count) {
            continue;
        }

        *err = ssv6xxx_hci_check_tx_resource(hci_ctrl, sw_txq, &hw_resource, &max_count);
        if ((*err == 0) || 
            ((*err == 2) && (max_count > 0))) {
            ret = handler(hci_ctrl, sw_txq, max_count, &hw_resource);
            if (ret < 0) {
                *err = ret;
            } else {
                tx_count += ret;
            }
        } else if (*err < 0) {
            break;
        }
    }

    return tx_count;
}

/**
 * int ssv6xxx_hci_aggr_xmit() - send the specified number of frames for a specified 
 *                          tx queue to SDIO with hci_tx_aggr enabled.
 *
 * @ struct ssv_sw_txq *sw_txq: the output queue to send.
 * @ int max_count: the maximal number of frames to send.
 */
static int ssv6xxx_hci_aggr_xmit(struct ssv6xxx_hci_ctrl *hci_ctrl, struct ssv_sw_txq *sw_txq, int max_count, struct ssv6xxx_hw_resource *phw_resource)
{
    struct sk_buff_head tx_cb_list;
    struct sk_buff *skb = NULL;
    int tx_count = 0, ret = 0;
    int reason = -1;
    // start of variable for hci tx aggr
    struct sk_buff *p_aggr_skb = hci_ctrl->p_tx_aggr_skb;
    static u32 left_len = 0;
    u32 cpy_len = 0; 
    static u8 *p_cpy_dest = NULL;
    static bool aggr_done = false;
    static bool new_round  = 1;
    // end of variable for hci tx aggr

    skb_queue_head_init(&tx_cb_list);

    for(tx_count=0; tx_count < max_count; tx_count++) {

        if ((hci_ctrl->hci_start == false) || (sw_txq->paused)){
            // Is it possible to stop/paused in middle of for loop?
            HCI_DBG_PRINT(hci_ctrl, "ssv6xxx_hci_xmit - hci_start = false\n");
            aggr_done = true;
            goto xmit_out;
        }

        mutex_lock(&sw_txq->txq_lock);
        skb = skb_dequeue(&sw_txq->qhead);
        if (!skb){
            HCI_DBG_PRINT(hci_ctrl, "ssv6xxx_hci_xmit - queue empty\n");
            goto xmit_out;
        }

        if (hci_ctrl->shi->hci_txtput_reason_cb)
            reason = hci_ctrl->shi->hci_txtput_reason_cb(skb, (void *)(SSV_SC(hci_ctrl)));

        if ((reason != ID_TRAP_SW_TXTPUT) && hci_ctrl->shi->hci_update_wep_cb)
            hci_ctrl->shi->hci_update_wep_cb(skb, (void *)(SSV_SC(hci_ctrl)));

        if ((reason != ID_TRAP_SW_TXTPUT) && hci_ctrl->shi->hci_tx_desc_cb)
            hci_ctrl->shi->hci_tx_desc_cb(skb, (void *)(SSV_SC(hci_ctrl)));

        mutex_unlock(&sw_txq->txq_lock);
        //ampdu1.3
        if(1 == new_round) {
            p_cpy_dest = p_aggr_skb->data;
            left_len = HCI_TX_AGGR_SKB_LEN;
            new_round = 0;
        }

        if ((skb->len) & 0x3) {
            cpy_len = (skb->len) + (4 - ((skb->len) & 0x3));
        } else {
            cpy_len = skb->len;
        }

        if (left_len > (cpy_len + 8)) {
            *((u32 *)(p_cpy_dest)) = ((cpy_len + 8) | ((cpy_len + 8) << 16));
            *((u32 *)(p_cpy_dest+4)) = 0;
            p_cpy_dest += 8;
            memcpy(p_cpy_dest, skb->data, cpy_len);
            p_cpy_dest +=  cpy_len;

            left_len -= (8 + cpy_len);

            skb_put(p_aggr_skb, 8 + cpy_len);
            sw_txq->tx_pkt++;

            if (reason != ID_TRAP_SW_TXTPUT) {
                skb_queue_tail(&tx_cb_list, skb);
            } else {
                hci_ctrl->shi->skb_free((void *)(SSV_SC(hci_ctrl)), skb);
            }

        } else {
            // remove tx desc, which will be add again in next round
            skb_pull(skb, TXPB_OFFSET);
            skb_queue_head(&sw_txq->qhead, skb);
            aggr_done = true;
            break;
        }

    } //end of for(tx_count=0; tx_count<max_count; tx_count++) 
    if (tx_count == max_count) {
        aggr_done = true;
    }
xmit_out:
    if (aggr_done) {
        if ((p_aggr_skb) && (new_round==0)) {
            *((u32 *)(p_cpy_dest)) = 0;
            p_cpy_dest += 4;
            skb_put(p_aggr_skb, 4);

            //printk(KERN_ERR "hci tx aggr len=%d\n", p_aggr_skb->len);
            ret = IF_SEND(hci_ctrl, (void *)p_aggr_skb, p_aggr_skb->len, sw_txq->txq_no);
            if (ret < 0) {
                BUG_ON(1);
                //printk("ssv6xxx_hci_aggr_xmit fail, err %d\n", ret);
                //ret = -1;
            } 
            aggr_done = false;
            ssv6xxx_reset_skb(p_aggr_skb);
            new_round = 1;
        }
    }

    if (hci_ctrl->shi->hci_post_tx_cb && reason != -1 && reason != ID_TRAP_SW_TXTPUT) {
        hci_ctrl->shi->hci_post_tx_cb (&tx_cb_list, (void *)(SSV_SC(hci_ctrl)));
    }

    return (ret == 0) ? tx_count : ret;
}
/**
 * int ssv6xxx_hci_xmit() - send the specified number of frames for a specified 
 *                          tx queue to SDIO.
 *
 * @ struct ssv_sw_txq *sw_txq: the output queue to send.
 * @ int max_count: the maximal number of frames to send.
 */
static int ssv6xxx_hci_xmit(struct ssv6xxx_hci_ctrl *hci_ctrl, struct ssv_sw_txq *sw_txq, int max_count, struct ssv6xxx_hw_resource *phw_resource)
{
    struct sk_buff_head tx_cb_list;
    struct sk_buff *skb = NULL;
    int tx_count = 0, ret = 0;
    int reason = -1;

    skb_queue_head_init(&tx_cb_list);

    for(tx_count=0; tx_count<max_count; tx_count++) {

        if ((hci_ctrl->hci_start == false) || (sw_txq->paused)){
            HCI_DBG_PRINT(hci_ctrl, "ssv6xxx_hci_xmit - hci_start = false\n");
            goto xmit_out;
        }

        mutex_lock(&sw_txq->txq_lock);
        skb = skb_dequeue(&sw_txq->qhead);
        if (!skb){
            HCI_DBG_PRINT(hci_ctrl, "ssv6xxx_hci_xmit - queue empty\n");
            mutex_unlock(&sw_txq->txq_lock);
            goto xmit_out;
        }

        if (hci_ctrl->shi->hci_txtput_reason_cb)
            reason = hci_ctrl->shi->hci_txtput_reason_cb(skb, (void *)(SSV_SC(hci_ctrl)));

        if ((reason != ID_TRAP_SW_TXTPUT) && hci_ctrl->shi->hci_update_wep_cb)
            hci_ctrl->shi->hci_update_wep_cb(skb, (void *)(SSV_SC(hci_ctrl)));

        if ((reason != ID_TRAP_SW_TXTPUT) && hci_ctrl->shi->hci_tx_desc_cb)
            hci_ctrl->shi->hci_tx_desc_cb(skb, (void *)(SSV_SC(hci_ctrl)));

        mutex_unlock(&sw_txq->txq_lock);

        //ampdu1.3
        ret = IF_SEND(hci_ctrl, (void *)skb, skb->len, sw_txq->txq_no);
        if (ret < 0) {
            BUG_ON(1);
            //HCI_DBG_PRINT(hci_ctrl, "ssv6xxx_hci_xmit fail......\n");
            // remove tx desc, which will be add again in next round
            //skb_pull(skb, TXPB_OFFSET);
            //skb_queue_head(&sw_txq->qhead, skb);
            break;
        }

        if (reason != ID_TRAP_SW_TXTPUT) {
            skb_queue_tail(&tx_cb_list, skb);
        } else {
            hci_ctrl->shi->skb_free((void *)(SSV_SC(hci_ctrl)), skb);
        }

        sw_txq->tx_pkt++;
    } //end of for(tx_count=0; tx_count<max_count; tx_count++) 
xmit_out:

    if (hci_ctrl->shi->hci_post_tx_cb && reason != -1 && reason != ID_TRAP_SW_TXTPUT) {
        hci_ctrl->shi->hci_post_tx_cb (&tx_cb_list, (void *)(SSV_SC(hci_ctrl)));
    }

    return (ret == 0) ? tx_count : ret;
}


/* return value 
 * 0, success
 * 1, hci stop or hwq paused
 * 2, no hw resource
 */
static int ssv6xxx_hci_check_tx_resource(struct ssv6xxx_hci_ctrl *hci_ctrl, struct ssv_sw_txq *sw_txq, struct ssv6xxx_hw_resource *p_hw_resource, int *p_max_count)
{
#define RESERVED_HCI_CMD_PAGE_NUM   (8)
    struct ssv6xxx_hci_txq_info txq_info;
    int hci_used_id = -1;
    int ret = 0;
    int tx_count = 0;
    struct sk_buff *skb;
    int page_count = 0;
    bool b_no_tx_resource = false;
    unsigned long flags;
#if defined(CONFIG_PREEMPT_NONE) && defined(CONFIG_SDIO_FAVOR_RX)
    struct ssv6xxx_hwif_ops *ifops = hci_ctrl->shi->if_ops;
    int sdio_intr_status;
#endif
    int tx_req_cnt=0;

    if ((hci_ctrl->hci_start == false) || (sw_txq->paused)) {
        return 1;
    }

    if (SSV_SC(hci_ctrl) != NULL) {
        ret = HAL_READRG_TXQ_INFO(hci_ctrl->shi->sh ,(u32 *)&txq_info, &hci_used_id);
        hci_ctrl->shi->sc->read_reg_cnt++;
    } else {
        HCI_DBG_PRINT(hci_ctrl, "%s: can't read txq_info due to software structure not initialized !!\n",  __func__);
        return -EIO;
    }        
    if (ret < 0) {
        hci_ctrl->read_rs0_info_fail++;
        return -EIO;
    }

    if (SSV6XXX_PAGE_TX_THRESHOLD(hci_ctrl) < txq_info.tx_use_page) {
        BUG_ON(1);
    }
    if (SSV6XXX_ID_TX_THRESHOLD(hci_ctrl) < txq_info.tx_use_id) {
        BUG_ON(1);
    }
    HCI_HWIF_GET_TX_REQ_CNT(hci_ctrl,&tx_req_cnt);

    p_hw_resource->free_tx_page = 
        (int) SSV6XXX_PAGE_TX_THRESHOLD(hci_ctrl) - (int)txq_info.tx_use_page - RESERVED_HCI_CMD_PAGE_NUM-tx_req_cnt*7;
    p_hw_resource->free_tx_id = (int) SSV6XXX_ID_TX_THRESHOLD(hci_ctrl) - (int) txq_info.tx_use_id-tx_req_cnt;
    if ((p_hw_resource->free_tx_page <= 0) || (p_hw_resource->free_tx_id <= 0)) {
        hci_ctrl->hw_tx_resource_full_cnt++;
        if (hci_ctrl->hw_tx_resource_full_cnt > MAX_HW_RESOURCE_FULL_CNT) {
            hci_ctrl->hw_tx_resource_full = true;
            hci_ctrl->hw_tx_resource_full_cnt = 0;
        }
        (*p_max_count) = 0;
        b_no_tx_resource = true;
        return 2;
    }

    spin_lock_irqsave(&(sw_txq->qhead.lock), flags);
    for(tx_count = 0; tx_count < (*p_max_count); tx_count++) {
        if (0 == tx_count) {
            skb = skb_peek(&sw_txq->qhead);
        } else {
            skb = ssv6xxx_hci_skb_peek_next(skb, &sw_txq->qhead);
        }
        if (!skb){
            break;
        }
        page_count = (skb->len + SSV6200_ALLOC_RSVD);
        if (page_count & HW_MMU_PAGE_MASK)
            page_count = (page_count >> HW_MMU_PAGE_SHIFT) + 1;
        else
            page_count = page_count >> HW_MMU_PAGE_SHIFT;

        if ((p_hw_resource->free_tx_page < page_count) || (p_hw_resource->free_tx_id <= 0)) {
            // If it cannot get tx resource more than 100 consecutive times,
            // it will set resource full flag and wait timeout.
            hci_ctrl->hw_tx_resource_full_cnt++;
            if (hci_ctrl->hw_tx_resource_full_cnt > MAX_HW_RESOURCE_FULL_CNT) {
                hci_ctrl->hw_tx_resource_full = true;
                hci_ctrl->hw_tx_resource_full_cnt = 0;
            }
            b_no_tx_resource = true;
            break;
        } else {
            hci_ctrl->hw_tx_resource_full_cnt = 0;
        }

        p_hw_resource->free_tx_page -= page_count;
        p_hw_resource->free_tx_id--;
    }
    spin_unlock_irqrestore(&(sw_txq->qhead.lock), flags);
    (*p_max_count) = tx_count;


#if defined(CONFIG_PREEMPT_NONE) && defined(CONFIG_SDIO_FAVOR_RX)
    if (SSV_HWIF_INTERFACE_SDIO == HCI_DEVICE_TYPE(hci_ctrl)) {
        //HCI_REG_READ(hci_ctrl, 0xc0000808, &sdio_intr_status);
        ifops->irq_getstatus(hci_ctrl->shi->dev, &sdio_intr_status);
        if (sdio_intr_status & SSV6XXX_INT_RX) {
            hci_ctrl->hw_sdio_rx_available = true;
            schedule();
        } 
    } else {
        hci_ctrl->hw_sdio_rx_available = false;
    }
#endif

    return (b_no_tx_resource ? 2 : 0);
}


static bool ssv6xxx_hci_is_frame_send(struct ssv6xxx_hci_ctrl *hci_ctrl)
{
    int q_num;
    struct ssv_sw_txq *sw_txq;

    if (hci_ctrl->hw_tx_resource_full)
        return false;

#if defined(CONFIG_PREEMPT_NONE) && defined(CONFIG_SDIO_FAVOR_RX)
    if (hci_ctrl->hw_sdio_rx_available) {
        hci_ctrl->hw_sdio_rx_available = false;
        return false;
    }
#endif
    for (q_num = (SSV_SW_TXQ_NUM - 1); q_num >= 0; q_num--) {
        sw_txq = &hci_ctrl->sw_txq[q_num];

        if (!sw_txq->paused && !ssv6xxx_hci_is_txq_empty(hci_ctrl, q_num))
            return true;
    }

    return false;
}

static void ssv6xxx_hci_isr_reset(struct work_struct *work)
{
    struct ssv6xxx_hci_ctrl *hci_ctrl;

    hci_ctrl = container_of(work, struct ssv6xxx_hci_ctrl, isr_reset_work);

    HCI_DBG_PRINT(hci_ctrl, "ISR Reset!!!");
    ssv6xxx_hci_irq_disable(hci_ctrl);
    ssv6xxx_hci_irq_enable(hci_ctrl);
}

static int ssv6xxx_hci_tx_task (void *data)
{
#define MAX_HCI_TX_TASK_SEND_FAIL       1000
    struct ssv6xxx_hci_ctrl *hci_ctrl = (struct ssv6xxx_hci_ctrl *)data;
    unsigned long     wait_period = 0;
    int err = 0, err_cnt = 0;
    u32 timeout = (-1);

    printk(KERN_ERR "SSV6XXX HCI TX Task started.\n");

    while (!kthread_should_stop())
    {
        wait_period = msecs_to_jiffies(hci_ctrl->task_timeout);
        timeout = wait_event_interruptible_timeout(hci_ctrl->tx_wait_q,
                (  kthread_should_stop()
                   || ssv6xxx_hci_is_frame_send(hci_ctrl)),
                wait_period);

        if (kthread_should_stop())
        {
            hci_ctrl->hci_tx_task = NULL;
            printk(KERN_ERR "Quit HCI TX task loop...\n");
            break;
        }

        if (timeout == 0) {
            hci_ctrl->hw_tx_resource_full = false;
        }

        if ((hci_ctrl->hci_flags & SSV6XXX_HCI_OP_INVALID) ||
                (hci_ctrl->hci_flags & SSV6XXX_HCI_OP_IFERR) ||
                (err_cnt > MAX_HCI_TX_TASK_SEND_FAIL)) {
            // To Do: Don't flush txq , it should reset hw for hw error???
            //ssv6xxx_hci_txq_flush(hci_ctrl);
            //printk(KERN_ERR " hci_tx error, should check hci flow, err_cnt %d, hci_flags 0x%x\n", err_cnt, hci_ctrl->hci_flags);
            //WARN_ON(1);
            err_cnt = 0;
        } else { 
            if (0 != timeout)
            {
                err = 0;
                _do_force_tx(hci_ctrl, &err);
                if ((err < 0) && (err != -1)) {
                    err_cnt++;
                    /* 
                     * If error, wait for next wake up.
                     * It should include the all error case, 
                     */
                } else if (err == -1) {
                    /* the error code means that hwif is disconnnect */
                    hci_ctrl->hci_flags |= SSV6XXX_HCI_OP_IFERR;
                }
                /* 
                 * err 0, success
                 * err = 1, hci stop or hwq paused
                 * err = 2, no hw resource
                 * It should wait the next round to send frame*/
            }
        }
    }

    hci_ctrl->hci_tx_task = NULL;

    printk(KERN_ERR "SSV6XXX HCI TX Task end.\n");
    return 0;
}

static struct ssv6xxx_hci_ops hci_ops = 
{
    .hci_start            = ssv6xxx_hci_start,
    .hci_stop             = ssv6xxx_hci_stop,
#ifdef CONFIG_PM
    .hci_suspend          = ssv6xxx_hci_suspend,
    .hci_resume           = ssv6xxx_hci_resume,
#endif
    .hci_write_hw_config  = ssv6xxx_hci_write_hw_config,
    .hci_read_word        = ssv6xxx_hci_read_word,
    .hci_write_word       = ssv6xxx_hci_write_word,
    .hci_safe_read_word   = ssv6xxx_hci_safe_read_word,
    .hci_safe_write_word  = ssv6xxx_hci_safe_write_word,
#ifdef CONFIG_USB_EP0_RW_REGISTER
    .hci_mcu_read_word    = ssv6xxx_hci_mcu_read_word,
    .hci_mcu_write_word   = ssv6xxx_hci_mcu_write_word,
#endif
    .hci_burst_read_word  = ssv6xxx_hci_burst_read_word,
    .hci_burst_write_word = ssv6xxx_hci_burst_write_word,    
    .hci_burst_safe_read_word  = ssv6xxx_hci_burst_safe_read_word,
    .hci_burst_safe_write_word = ssv6xxx_hci_burst_safe_write_word,    
    .hci_tx               = ssv6xxx_hci_enqueue,
    .hci_tx_pause         = ssv6xxx_hci_txq_pause,
    .hci_tx_resume        = ssv6xxx_hci_txq_resume,
    .hci_tx_pause_by_sta  = ssv6xxx_hci_txq_pause_by_sta,
    .hci_tx_resume_by_sta = ssv6xxx_hci_txq_resume_by_sta,
    .hci_txq_flush        = ssv6xxx_hci_txq_flush,
    .hci_txq_flush_by_sta = ssv6xxx_hci_txq_flush_by_sta,
    .hci_txq_lock_by_sta  = ssv6xxx_hci_txq_lock_by_sta,
    .hci_txq_unlock_by_sta= ssv6xxx_hci_txq_unlock_by_sta,
    .hci_txq_len          = ssv6xxx_hci_txq_len,
    .hci_txq_empty        = ssv6xxx_hci_is_txq_empty,
    .hci_load_fw          = ssv6xxx_hci_load_fw,
    .hci_pmu_wakeup       = ssv6xxx_hci_pmu_wakeup,
    .hci_cmd_done         = ssv6xxx_hci_cmd_done,
    .hci_send_cmd         = ssv6xxx_hci_send_cmd,
    .hci_ignore_cmd       = ssv6xxx_hci_ignore_cmd,
    .hci_write_sram       = ssv6xxx_hci_write_sram,
    .hci_interface_reset  = ssv6xxx_hci_interface_reset,
    .hci_sysplf_reset     = ssv6xxx_hci_sysplf_reset,
#ifdef CONFIG_PM
    .hci_hwif_suspend     = ssv6xxx_hci_hwif_suspend,
    .hci_hwif_resume      = ssv6xxx_hci_hwif_resume,
#endif
    .hci_set_cap          = ssv6xxx_hci_set_cap,
    .hci_set_trigger_conf = ssv6xxx_hci_set_trigger_conf,
};


int ssv6xxx_hci_deregister(struct ssv6xxx_hci_info *shi)
{
    struct ssv6xxx_hci_ctrl *hci_ctrl;

    /**
     * Wait here until there is no frame on the hardware. Before
     * call this function, the RF shall be turned off to make sure
     * no more incoming frames. This function also disable interrupt
     * once no more frames.
     */
    printk(KERN_ERR "%s(): \n", __FUNCTION__);


    if (shi->hci_ctrl == NULL)
        return -1;

    hci_ctrl = shi->hci_ctrl;
    hci_ctrl->ignore_cmd = true;
    hci_ctrl->hci_flags |= SSV6XXX_HCI_OP_INVALID;
    // flush txq packet 
    ssv6xxx_hci_txq_flush(hci_ctrl);
    ssv6xxx_hci_irq_disable(hci_ctrl);
    ssv6xxx_hci_stop_acc(hci_ctrl);
    ssv6xxx_hci_hwif_cleanup(hci_ctrl);
    flush_workqueue(hci_ctrl->hci_work_queue);
    destroy_workqueue(hci_ctrl->hci_work_queue);
    if (hci_ctrl->hci_tx_task != NULL)
    {
        printk(KERN_ERR "Stopping HCI TX task...\n");
        kthread_stop(hci_ctrl->hci_tx_task);
        while(hci_ctrl->hci_tx_task != NULL) {
            msleep(1);
        }
        printk(KERN_ERR "Stopped HCI TX task.\n");
    }

    if (hci_ctrl->p_tx_aggr_skb) {
        // free hci tx aggr buffer
        shi->skb_free((void *)(SSV_SC(hci_ctrl)), hci_ctrl->p_tx_aggr_skb);
    }

    shi->hci_ctrl = NULL;
    kfree(hci_ctrl);

    return 0;
}
EXPORT_SYMBOL(ssv6xxx_hci_deregister);


int ssv6xxx_hci_register(struct ssv6xxx_hci_info *shi, bool hci_tx_aggr)
{
    int i;
    struct ssv6xxx_hci_ctrl *hci_ctrl;

    if (shi == NULL/* || hci_ctrl->shi*/) {
        printk(KERN_ERR "NULL sh when register HCI.\n");
        return -1;
    }

    hci_ctrl = kzalloc(sizeof(*hci_ctrl), GFP_KERNEL);
    if (hci_ctrl == NULL)
        return -ENOMEM;

    memset((void *)hci_ctrl, 0, sizeof(*hci_ctrl));

    /* HCI & hw/sw mac interface binding */
    shi->hci_ctrl = hci_ctrl;
    shi->hci_ops = &hci_ops;
    hci_ctrl->shi = shi;

    hci_ctrl->task_timeout = 3; //default timeout 3ms
    hci_ctrl->hw_txq_mask = 0;
    mutex_init(&hci_ctrl->hw_txq_mask_lock);
    mutex_init(&hci_ctrl->hci_mutex);
    mutex_init(&hci_ctrl->cmd_mutex);
    init_completion(&hci_ctrl->cmd_done);

    /* TX queue initialization */
    for (i=0; i < SSV_SW_TXQ_NUM; i++) {
        memset(&hci_ctrl->sw_txq[i], 0, sizeof(struct ssv_sw_txq));
        skb_queue_head_init(&hci_ctrl->sw_txq[i].qhead);
        mutex_init(&hci_ctrl->sw_txq[i].txq_lock);
        hci_ctrl->sw_txq[i].txq_no = (u32)i;
    }

    hci_ctrl->hci_work_queue = create_singlethread_workqueue("ssv6xxx_hci_wq");
    INIT_WORK(&hci_ctrl->isr_reset_work, ssv6xxx_hci_isr_reset);
    /* 
     * This layer holds a tx task to send frames.
     */
    init_waitqueue_head(&hci_ctrl->tx_wait_q);
    hci_ctrl->hci_tx_task = kthread_run(ssv6xxx_hci_tx_task, hci_ctrl, "ssv6xxx_hci_tx_task");
    /* 
     * Under layer holds ISR to receive frames.
     */
    hci_ctrl->int_mask = SSV6XXX_INT_RX;
#if 0
    HCI_RX_TASK(hci_ctrl, hci_ctrl->shi->hci_rx_cb, hci_ctrl->shi->hci_is_rx_q_full, 
            (void *)(SSV_SC(hci_ctrl)), &hci_ctrl->rx_pkt);

#endif
    //ssv6xxx_hci_irq_enable(hci_ctrl);
    //HCI_IRQ_TRIGGER(hci_ctrl);
    ssv6xxx_hci_start_acc(hci_ctrl);
    hci_ctrl->ignore_cmd = false;

    /* pre-alloc tx aggr skb */
    if (true == hci_tx_aggr) {
        hci_ctrl->p_tx_aggr_skb = shi->skb_alloc((void *)(SSV_SC(hci_ctrl)), HCI_TX_AGGR_SKB_LEN);
        if (NULL == hci_ctrl->p_tx_aggr_skb) {
            printk("Cannot alloc tx aggr skb size %d\n", HCI_TX_AGGR_SKB_LEN);
        }
    }

    return 0;
}
EXPORT_SYMBOL(ssv6xxx_hci_register);


#if (defined(CONFIG_SSV_SUPPORT_ANDROID)||defined(CONFIG_SSV_BUILD_AS_ONE_KO))
int ssv6xxx_hci_init(void)
#else
static int __init ssv6xxx_hci_init(void)
#endif
{
    printk("%s() start\n", __FUNCTION__);
    return 0;
}

#if (defined(CONFIG_SSV_SUPPORT_ANDROID)||defined(CONFIG_SSV_BUILD_AS_ONE_KO))
void ssv6xxx_hci_exit(void)
#else
static void __exit ssv6xxx_hci_exit(void)
#endif
{
    printk("%s() start\n", __FUNCTION__);
}


#if (defined(CONFIG_SSV_SUPPORT_ANDROID)||defined(CONFIG_SSV_BUILD_AS_ONE_KO))
EXPORT_SYMBOL(ssv6xxx_hci_init);
EXPORT_SYMBOL(ssv6xxx_hci_exit);
#else
module_init(ssv6xxx_hci_init);
module_exit(ssv6xxx_hci_exit);
#endif




