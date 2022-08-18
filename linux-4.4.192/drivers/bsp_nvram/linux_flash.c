#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/backing-dev.h>
#include <linux/compat.h>
#include <linux/mount.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/concat.h>
#include <linux/mtd/partitions.h>

#include <nvram/bcmnvram.h>

static unsigned long current_write_counter = 0;
static u_int32_t current_offs = 0;

extern unsigned short verify_zbuf_chksum(char *zbuf_with_header);

/*
 * Flash API: ra_mtd_read, ra_mtd_write
 * Arguments:
 *   - num: specific the mtd number
 *   - to/from: the offset to read from or written to
 *   - len: length
 *   - buf: data to be read/written
 * Returns:
 *   - return -errno if failed
 *   - return the number of bytes read/written if successed
 */
int ra_mtd_write_nm(char *name, loff_t to, size_t len, const u_char *buf)
{
	int ret = -1;
	size_t rdlen, wrlen;
	struct mtd_info *mtd;
	struct erase_info ei;
	u_char *bak = NULL;

	mtd = get_mtd_device_nm(name);

	if (IS_ERR(mtd)) {
		ret = (int)mtd;
		goto out;
	}
/*
	if (len > mtd->erasesize) {
		put_mtd_device(mtd);
		ret = -E2BIG;
		goto out;
	}
*/

	bak = kzalloc(len, GFP_KERNEL);
	if (bak == NULL) {
		put_mtd_device(mtd);
		ret = -ENOMEM;
		goto out;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	ret = mtd_read(mtd, to,len, &rdlen, bak);
#else
	ret = mtd->read(mtd, to,len, &rdlen, bak);
#endif
	if (ret) {
		goto free_out;
	}

	if (rdlen != len)
		printk("warning: ra_mtd_write_nm: rdlen is not equal to erasesize\n");

	memcpy(bak, buf, len);

	ei.mtd = mtd;
	ei.callback = NULL;
	ei.addr = to;
	ei.len = len;
	ei.priv = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	ret = mtd_erase(mtd, &ei);
#else
	ret = mtd->erase(mtd, &ei);
#endif
	if (ret != 0)
		goto free_out;


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	ret = mtd_write(mtd, to,len, &wrlen, bak);
#else
	ret = mtd->write(mtd, to,len, &wrlen, bak);
#endif

	udelay(10); /* add delay after write */

free_out:
	if (mtd)
		put_mtd_device(mtd);

	if (bak)
		kfree(bak);
out:
	return ret;
}

int ra_mtd_read_nm(char *name, loff_t from, size_t len, u_char *buf)
{
	int ret;
	size_t rdlen = 0;
	struct mtd_info *mtd;

	mtd = get_mtd_device_nm(name);
	if (IS_ERR(mtd))
		return (int)mtd;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	ret = mtd_read(mtd, from, len, &rdlen, buf);
#else
	ret = mtd->read(mtd, from, len, &rdlen, buf);
#endif

	if (rdlen != len)
		printk("warning: ra_mtd_read_nm: rdlen is not equal to len\n");

	put_mtd_device(mtd);
	return ret;
}

int _nvram_check(struct nvram_header *header)
{
	if (header->magic == 0xFFFFFFFF || header->magic == 0)
		return 1; // NVRAM MTD is clear

	if (header->magic != NVRAM_MAGIC)
		return -1; // NVRAM is garbage

	if (header->len < sizeof(struct nvram_header))
		return -2; // NVRAM size underflow

	if (header->len > NVRAM_SPACE)
		return -3; // NVRAM size overflow

	if (verify_zbuf_chksum((char*)header))
		return -4; // NVRAM content is corrupted

	return 0;
}

int nvram_flash_read_zbuf(char *zbuf, u_int32_t len)
{
	int ii;
	int ret;
	struct nvram_header *header = (struct nvram_header*)zbuf;

	current_write_counter = 0;

	for (ii = 0; ii < 2; ii++) {
		// TODO: check bad blocks
		ret = ra_mtd_read_nm(MTD_NVRAM_NAME, ii*NVRAM_SPACE, len, zbuf);
		if (ret)
			continue;

		ret = _nvram_check(header);
		if (ret)
			continue;

		if (current_write_counter <= header->write_counter) {
			current_write_counter = header->write_counter;
			current_offs = ii*NVRAM_SPACE;
		}
	}

	ret = ra_mtd_read_nm(MTD_NVRAM_NAME, current_offs, len, zbuf);
	if (ret < 0) {
		return -1;
	}

//	printk("%s: offs: 0x%x, len: %d\n", __func__, current_offs, len);

	return 0;
}

int nvram_flash_write_zbuf(char *zbuf, u_int32_t len)
{
	int ret;
	struct nvram_header *header = (struct nvram_header*)zbuf;

	current_write_counter++;
	header->write_counter = current_write_counter;

	current_offs += NVRAM_SPACE;
	if (current_offs + NVRAM_SPACE > 2*NVRAM_SPACE)
		current_offs = 0;

	ret = ra_mtd_write_nm(MTD_NVRAM_NAME, current_offs, len, zbuf);
	if (ret < 0) {
		return -1;
	}

//	printk("%s: offs: 0x%x, len: %d, header->len %d\n", __func__, current_offs, len, header->len);

	return 0;
}

EXPORT_SYMBOL(nvram_flash_write_zbuf);
EXPORT_SYMBOL(nvram_flash_read_zbuf);
