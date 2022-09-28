/*
 * isp_stats.c
 *
 * isp statistics interface
 *
 * Copyright (C) 2020 anyka
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include <mach/map.h>

#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

/*#include "../media/video/plat-anyka/ak39_isp2.h"
#include "../media/video/plat-anyka/ak39_isp2_3a.h"
#include "../media/video/plat-anyka/ak39_isp2_reg.h"
*/
#include "../include/ak_isp_drv.h"
//#include <plat-anyka/ak_sensor.h>
//#include <plat-anyka/ak_camera.h>
#include "../include/ak_isp_char.h"
#include "cif.h"
#include "isp_param.h"
#include "isp_stats.h"

struct isp_stats_type_to_name {
	enum isp_stats_type type;
	const char *name;
};

static struct isp_stats_type_to_name stats_type_to_name[ISP_STATS_TYPE_NUM] = {
	{ISP_STATS_3DNR,	"3dnr"},
	{ISP_STATS_AWB,		"awb"},
	{ISP_STATS_AF,		"af"},
	{ISP_STATS_AE,		"ae"}
};

/*
 * Videobuf operations
 */

/*
 * isp_stats_vb2_queue_setup -
 * calculate the __buffer__ (not data) size and number of buffers
 *
 * @vq:				queue of vb2
 * @parq:
 * @count:			return count of filed
 * @num_planes:		number of planes in on frame
 * @size:
 * @alloc_ctxs:
 */
static int isp_stats_vb2_queue_setup(struct vb2_queue *vq,
		const void *parg,
		unsigned int *count, unsigned int *num_planes,
		unsigned int sizes[], void *alloc_ctxs[])
{
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vq);
	struct isp_stats_chn_attr *chn = ak_cam_fh_to_stats_chn(akfh);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s input_id:%d, chn_id:%d *count:%d, sizeimage:%u\n",
			__func__, input_id, chn_id, *count, chn->format.fmt.pix.sizeimage);

	alloc_ctxs[0] = chn->alloc_ctx;
	*num_planes = 1;
	chn->vb_num = *count;
	sizes[0] = chn->format.fmt.pix.sizeimage;

	return 0;
}

/*
 * isp_stats_vb2_buf_init -
 * vb2 init
 *
 * @vb2_b:				vb2 buffer
 */
static int isp_stats_vb2_buf_init(struct vb2_buffer *vb2_b)
{
	struct vb2_v4l2_buffer *vb2_v4l2_b = vb2_b_to_vb2_v4l2_b(vb2_b);
	struct ak_camera_buffer *ak_b = vb2_v4l2_b_to_ak_b(vb2_v4l2_b);
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vb2_b->vb2_queue);
	struct isp_stats_chn_attr *chn = ak_cam_fh_to_stats_chn(akfh);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	unsigned int yaddr_ch;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int index = vb2_b->index;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	/*fill phyaddr for every buf*/
	yaddr_ch = vb2_dma_contig_plane_dma_addr(vb2_b, 0);
	chn->phyaddr[index] = yaddr_ch;

	/*add to list*/
	INIT_LIST_HEAD(&ak_b->queue);

	return 0;
}

/*
 * isp_stats_vb2_buf_prepare -
 * vb2 prepare
 *
 * @vb2_b:				vb2 buffer
 */
static int isp_stats_vb2_buf_prepare(struct vb2_buffer *vb2_b)
{
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vb2_b->vb2_queue);
	struct isp_stats_chn_attr *chn = ak_cam_fh_to_stats_chn(akfh);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	return 0;
}

/*
 * isp_stats_vb2_buf_queue -
 * vb2 queue
 *
 * @vb2_b:				vb2 buffer
 */
static void isp_stats_vb2_buf_queue(struct vb2_buffer *vb2_b)
{
	struct vb2_v4l2_buffer *vb2_v4l2_b = vb2_b_to_vb2_v4l2_b(vb2_b);
	struct ak_camera_buffer *ak_b = vb2_v4l2_b_to_ak_b(vb2_v4l2_b);
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vb2_b->vb2_queue);
	struct isp_stats_chn_attr *chn = ak_cam_fh_to_stats_chn(akfh);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	unsigned long flags;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	size_t new_size;

	pr_err("%s %d, input_id:%d, chn_id:%d sizeimage:%d\n",
			__func__, __LINE__, input_id, chn_id, chn->format.fmt.pix.sizeimage);

	new_size = chn->format.fmt.pix.sizeimage;

	if (vb2_plane_size(vb2_b, 0) < new_size) {
		pr_err("Buffer #%d too small (%lu < %zu)\n",
				vb2_v4l2_b->vb2_buf.index, vb2_plane_size(vb2_b, 0), new_size);
		goto error;
	}

	vb2_set_plane_payload(vb2_b, 0, new_size);

	/*
	 *	after adding to capture list, then the buffer is visible in irq_handle
	 */
	spin_lock_irqsave(&hw->lock, flags);
	list_add_tail(&ak_b->queue, &chn->capture);
	spin_unlock_irqrestore(&hw->lock, flags);

error:
	return;
}

/*
 * isp_stats_vb2_wait_prepare -
 * wait prepare
 *
 * @vq:				the queue of vb
 */
static void isp_stats_vb2_wait_prepare(struct vb2_queue *vq)
{
	/*no called*/
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vq);
	struct isp_stats_chn_attr *chn = ak_cam_fh_to_stats_chn(akfh);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	vb2_ops_wait_prepare(vq);
}

/*
 * isp_stats_vb2_wait_finish -
 * wait finish
 *
 * @vq:				the queue of vb
 */
static void isp_stats_vb2_wait_finish(struct vb2_queue *vq)
{
	/*no called*/
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vq);
	struct isp_stats_chn_attr *chn = ak_cam_fh_to_stats_chn(akfh);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	vb2_ops_wait_finish(vq);
}

/*
 * isp_stats_vb2_start_streaming -
 * start streaming
 *
 * @q:				the queue of vb
 */
static int isp_stats_vb2_start_streaming(struct vb2_queue *q, unsigned int count)
{
	/*streamon call the function*/
	struct ak_cam_fh *akfh = vb2_get_drv_priv(q);
	struct isp_stats_chn_attr *chn = ak_cam_fh_to_stats_chn(akfh);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d count:%u\n",
			__func__, __LINE__, input_id, chn_id, count);

	/*
	 * initial poll_empty is no true,
	 * isp irq handler post a buffer to pend_done_list by default
	 *
	 */
	chn->poll_empty = 0;

	chn->chn_state = CHN_STATE_START_STREAMING;

	return 0;
}

static void clear_all_buf_queue(struct isp_stats_chn_attr *chn)
{
	struct ak_camera_buffer *ak_b, *tmp;

	/*
	 * all buffers from capture are delete, otherwise it cause VB2 warning
	 * */
	list_for_each_entry_safe(ak_b, tmp, &chn->capture, queue) {
		vb2_buffer_done(&ak_b->vb2_v4l2_b.vb2_buf, VB2_BUF_STATE_ERROR);
		list_del_init(&ak_b->queue);
	}

	/*
	 * and all buffers from pend_done_list are delete,
	 * otherwise it cause VB2 warning
	 * */
	list_for_each_entry_safe(ak_b, tmp, &chn->pend_done_list, queue) {
		vb2_buffer_done(&ak_b->vb2_v4l2_b.vb2_buf, VB2_BUF_STATE_ERROR);
		list_del_init(&ak_b->queue);
	}
}

/*
 * isp_stats_vb2_stop_streaming -
 * stop streaming
 *
 * @q:				the queue of vb
 */
static void isp_stats_vb2_stop_streaming(struct vb2_queue *q)
{
	struct ak_cam_fh *akfh = vb2_get_drv_priv(q);
	struct isp_stats_chn_attr *chn = ak_cam_fh_to_stats_chn(akfh);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	unsigned long flags;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	spin_lock_irqsave(&hw->lock, flags);

	/*
	 * all channel states are clear
	 * */
	chn->chn_state = CHN_STATE_STOP_STREAMING;
	spin_unlock_irqrestore(&hw->lock, flags);
}

/*
 * isp_stats_vb2_buf_cleanup -
 * vb2 release
 *
 * @vb2_b:				vb2 buffer
 */
static void isp_stats_vb2_buf_cleanup(struct vb2_buffer *vb2_b)
{
	struct vb2_v4l2_buffer *vb2_v4l2_b = vb2_b_to_vb2_v4l2_b(vb2_b);
	struct ak_cam_fh *akfh = vb2_get_drv_priv(vb2_b->vb2_queue);
	struct isp_stats_chn_attr *chn = ak_cam_fh_to_stats_chn(akfh);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct ak_camera_buffer *ak_b = vb2_v4l2_b_to_ak_b(vb2_v4l2_b);
	unsigned long flags;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	spin_lock_irqsave(&hw->lock, flags);

	/* Doesn't hurt also if the list is empty */
	list_del_init(&ak_b->queue);

	/*
	 * if buffers be release, then release BUSY flag to notify
	 * irq_handle this channel is no working
	 * */
	chn->chn_state = CHN_STATE_CLOSED;

	spin_unlock_irqrestore(&hw->lock, flags);
}

/*vb2 ops*/
static struct vb2_ops isp_stats_vb2_ops = {
	.queue_setup	= isp_stats_vb2_queue_setup,
	.buf_init		= isp_stats_vb2_buf_init,
	.buf_prepare	= isp_stats_vb2_buf_prepare,
	.buf_queue		= isp_stats_vb2_buf_queue,
	.wait_prepare	= isp_stats_vb2_wait_prepare,
	.wait_finish	= isp_stats_vb2_wait_finish,
	.start_streaming= isp_stats_vb2_start_streaming,
	.stop_streaming	= isp_stats_vb2_stop_streaming,
	.buf_cleanup	= isp_stats_vb2_buf_cleanup,
};

/*
 * V4L2 file operations
 */

static bool check_chn_multi_opend(struct isp_stats_chn_attr *chn)
{
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int chn_state = chn->chn_state;

	if (chn->chn_opend_count > 1) {
		pr_warning("%s current multi-opend, input_id:%d, chn_id:%d, chn_state:%d,"
				"chn_opend_count:%d, input_opend_count:%d\n",
				__func__, input_id, chn_id, chn_state,
				chn->chn_opend_count, input_video->input_opend_count);
		return true;
	}

	return false;
}

/*
 * isp_stats_file_open -
 * processor when videoX is opened
 *
 * @file:	pointer to videoX file
 */
static int isp_stats_file_open(struct file *file)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct ak_cam_fh *akfh;
	struct vb2_queue *queue;
	struct device *dev = hw->dev;
	unsigned long flags;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;
	bool is_multi_opend;

	pr_debug("%s %d input_id:%d, chn_id:%d\n",
			__func__, __LINE__, input_id, chn_id);

	/*initing state*/
	//	chn->chn_state = CHN_STATE_INITING;

	/*init: disable the channel buffers*/
	//	disable_current_chn_all_hw_buffer(hw->isp_struct, chn_id);

	spin_lock_irqsave(&hw->lock, flags);

	chn->chn_opend_count++;
	input_video->input_opend_count++;
	hw->camera_opend_count++;//all chn of all sensor input

	is_multi_opend = check_chn_multi_opend(chn);

	spin_unlock_irqrestore(&hw->lock, flags);

	if (is_multi_opend) {
		pr_err("%s current multi-opend, input_id:%d, chn_id:%d, chn_state:%d,"
				"chn_opend_count:%d, input_opend_count:%d\n",
				__func__, input_id, chn_id, chn->chn_state,
				chn->chn_opend_count, input_video->input_opend_count);
			ret = -EBUSY;
			goto alloc_fail;
	} else if (chn->chn_state == CHN_STATE_CLOSED) {
		/*when not multi-open the chn_state is CLOSED, then change it*/
		akfh = devm_kzalloc(dev, sizeof(*akfh), GFP_KERNEL);
		if (akfh == NULL) {
			pr_err("%s %d malloc for akfh faile\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto alloc_fail;
		}

		v4l2_fh_init(&akfh->vfh, &chn->vdev);
		v4l2_fh_add(&akfh->vfh);

		queue = &akfh->queue;
		queue->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;/*new kernel support V4L2_BUF_TYPE_META_CAPTURE*/
		queue->io_modes = VB2_MMAP | VB2_USERPTR;
		queue->drv_priv = akfh;
		queue->ops = &isp_stats_vb2_ops;
		queue->mem_ops = &vb2_dma_contig_memops;
		queue->buf_struct_size = sizeof(struct ak_camera_buffer);
		queue->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
		queue->lock = &chn->vlock;

		ret = vb2_queue_init(queue);
		if (ret < 0) {
			pr_err("%s %d vb2_queue_init faile\n", __func__, __LINE__);
			goto queue_init_fail;
		}

		akfh->priv = chn;
		file->private_data = &akfh->vfh;

		chn->chn_state = CHN_STATE_OPEND;
	} else {
		pr_warning("%s do nothing!!\\n", __func__);
	}

	return 0;

queue_init_fail:
	v4l2_fh_del(&akfh->vfh);
	v4l2_fh_exit(&akfh->vfh);
	devm_kfree(dev, akfh);
alloc_fail:
	return ret;
}

/*
 * isp_stats_file_release -
 * processor when videoX is closed
 *
 * @file:			point to videoX file
 */
static int isp_stats_file_release(struct file *file)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct v4l2_fh *vfh = file->private_data;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct device *dev = hw->dev;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	unsigned long flags;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	/*disable the channel buffers*/
	//	disable_current_chn_all_hw_buffer(hw->isp_struct, chn_id);

	spin_lock_irqsave(&hw->lock, flags);

	chn->chn_opend_count--;

	if (chn->chn_opend_count <= 0) {
		/*current channel no used*/

		clear_all_buf_queue(chn);
		/*
		 * vb2_queue_release() to free v4l2 memory
		 * that alloc by frame_vector_create()
		 */
		vb2_queue_release(&akfh->queue);

		/*reset chn state*/
		chn->chn_state = CHN_STATE_CLOSED;

		/*Release the file handle.*/
		v4l2_fh_del(&akfh->vfh);
		v4l2_fh_exit(&akfh->vfh);
		devm_kfree(dev, akfh);
		file->private_data = NULL;
	}

	spin_unlock_irqrestore(&hw->lock, flags);

	return 0;
}

/*
 * check_done_list_empty - check done_list empty
 * @chn:		pointer to channel
 *
 * @RETURN:	0-no empty, 1-empty
 */
static int check_done_list_empty(struct isp_stats_chn_attr *chn, struct vb2_queue *q)
{
	struct list_head *done_list = &q->done_list;

	if (list_empty(done_list))
		return 1;
	return 0;
}

/*
 * move_to_done_list - move a buffer from pend_done_list to done_list
 * @chn:		pointer to channel
 *
 * @RETURN:	0-fail, 1-success
 */
static int move_to_done_list(struct isp_stats_chn_attr *chn)
{
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct vb2_buffer *vb2_b;
	struct vb2_v4l2_buffer *vb2_v4l2_b;
	struct ak_camera_buffer *ak_b;
	struct list_head *pend_done_list = &chn->pend_done_list;
	unsigned long flags;
	int move;

	spin_lock_irqsave(&hw->lock, flags);

	if (list_empty(pend_done_list)) {
		/*notice irq post to done_list directly*/
		move = 0;
	} else {
		ak_b = list_first_entry(pend_done_list, struct ak_camera_buffer, queue);
		vb2_v4l2_b = &ak_b->vb2_v4l2_b;
		vb2_b = &vb2_v4l2_b->vb2_buf;
		/*remove a buffer from pend_done_list to v4l2 done_list*/
		list_del_init(&ak_b->queue);
		/*post done*/
		vb2_buffer_done(vb2_b, VB2_BUF_STATE_DONE);

		/*notice irq post to pend_done_list directly but not to done_list*/
		move = 1;
	}

	spin_unlock_irqrestore(&hw->lock, flags);

	return move;
}

/*
 * isp_stats_file_poll -
 * processor when poll camera host
 *
 * @file:			point to videoX file
 * @wait:			poll wait
 */
static unsigned int isp_stats_file_poll(struct file *file, poll_table *wait)
{
	struct v4l2_fh *vfh = file->private_data;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;
	int empty;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	/*1. check done_list if empty*/
	empty = check_done_list_empty(chn, &akfh->queue);

	/*2. get buffer from pend_done_list to done_list*/
	if (empty)
		empty = !move_to_done_list(chn);

	/*
	 * poll_empty = 0: notice irq post to pend_done_list directly not to done_list
	 * poll_empty = 1: notice irq post to done_list directly
	 */
	chn->poll_empty = empty ? 1:0;

	/*3. poll done_list*/
	ret = vb2_poll(&akfh->queue, file, wait);
	return ret;
}

/*
 * isp_stats_file_mmap -
 * processor when mmap camera host
 *
 * @file:			point to videoX file
 * @wma:			vm struct
 */
static int isp_stats_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct v4l2_fh *vfh = file->private_data;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	return vb2_mmap(&akfh->queue, vma);
}

/*file callbacks*/
static struct v4l2_file_operations isp_stats_file_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
	.open			= isp_stats_file_open,
	.release		= isp_stats_file_release,
	.poll			= isp_stats_file_poll,
	.mmap			= isp_stats_file_mmap,
};

/* -----------------------------------------------------------------------------
 * V4L2 ioctls
 */

/*
 * isp_stats_vidioc_querycap -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @cap:			capability of camera host
 */
static int isp_stats_vidioc_querycap(
		struct file *file, void *fh, struct v4l2_capability *cap)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	/* cap->name is set by the firendly caller:-> */
	strlcpy(cap->card, "AK ISP STATS", sizeof(cap->card));
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

/*
 * isp_stats_vidioc_get_format -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @format:			video data format
 */
static int isp_stats_vidioc_get_format(
		struct file *file, void *fh, struct v4l2_format *format)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	if (format->type != queue->type) {
		pr_err("%s %d input_id:%d, chn_id:%d type err\n",
				__func__, __LINE__, input_id, chn_id);
		return -EINVAL;
	}

	*format = chn->format;

	return 0;
}

struct isp_format_info {
	u32 code;
	u32 pixelformat;
	unsigned int width;
};
static struct isp_format_info formats[] = {
	{ MEDIA_BUS_FMT_Y8_1X8, V4L2_PIX_FMT_YUYV, 12},
};

static void isp_stats_pix_to_mbus(const struct v4l2_pix_format *pix,
		struct v4l2_mbus_framefmt *mbus)
{
	unsigned int i;

	memset(mbus, 0, sizeof(*mbus));
	mbus->width = pix->width;
	mbus->height = pix->height;

	/* Skip the last format in the loop so that it will be selected if no
	 *      * match is found.
	 *           */
	for (i = 0; i < ARRAY_SIZE(formats) - 1; ++i) {
		if (formats[i].pixelformat == pix->pixelformat)
			break;
	}

	mbus->code = formats[i].code;
	mbus->colorspace = pix->colorspace;
	mbus->field = pix->field;
}

static int isp_stats_mbus_to_pix(const struct isp_stats_chn_attr *chn,
		const struct v4l2_mbus_framefmt *mbus,
		struct v4l2_pix_format *pix)
{
	int chn_id = chn->chn_id;
	int ret = 0;

	memset(pix, 0, sizeof(*pix));

	switch (chn_id) {
		case ISP_STATS_3DNR:
			{
				struct input_video_attr *input_video = stats_chn_to_input_video(chn);
				struct isp_attr *isp = &input_video->isp;
				void *isp_struct = isp->isp_struct;
				int width_block_num;
				int height_block_num;
				int block_size;

				ak_isp_get_md_array_max_size(isp_struct,
						&width_block_num, &height_block_num, &block_size);
				pix->width = width_block_num;
				pix->height = height_block_num;
				pix->bytesperline = pix->width* block_size;
				pix->sizeimage = pix->bytesperline * pix->height;
				pr_err("%s width_block_num=%d, height_block_num:%d, block_size:%d\n",
						__func__, width_block_num, height_block_num, block_size);
			}
			break;

		case ISP_STATS_AWB:
			pix->width = sizeof(AK_ISP_AWB_STAT_INFO);
			pix->height = 1;
			pix->bytesperline = pix->width* 1;
			pix->sizeimage = pix->bytesperline * pix->height;
			break;

		case ISP_STATS_AF:
			pix->width = sizeof(AK_ISP_AF_STAT_INFO);
			pix->height = 1;
			pix->bytesperline = pix->width* 1;
			pix->sizeimage = pix->bytesperline * pix->height;
			break;

		case ISP_STATS_AE:
			pix->width = ALIGN(sizeof(AK_ISP_AE_ATTR), STATS_ALIGN_SIZE) +
				ALIGN(sizeof(AK_ISP_AE_RUN_INFO), STATS_ALIGN_SIZE);
			pix->height = 1;
			pix->bytesperline = pix->width* 1;
			pix->sizeimage = pix->bytesperline * pix->height;
			break;

		default:
			ret = -1;
			break;
	}

	if (!ret) {
		/*set stats header*/
		pix->sizeimage += STATS_HEADER_SIZE;

		pix->colorspace = mbus->colorspace;
		pix->field = mbus->field;
	}

	return ret;
}

/*
 * isp_stats_vidioc_set_format -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @format:			video data format
 */
static int isp_stats_vidioc_set_format(
		struct file *file, void *fh, struct v4l2_format *format)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct v4l2_pix_format *pix = &format->fmt.pix;
	struct v4l2_mbus_framefmt fmt;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret = 0;

	pr_debug("%s %d, input_id:%d, chn_id:%d,pix->width:%d, pix->height:%d\n",
			__func__, __LINE__, input_id, chn_id, pix->width, pix->height);

//	chn->width = pix->width;
//	chn->height = pix->height;

	isp_stats_pix_to_mbus(pix, &fmt);
	ret = isp_stats_mbus_to_pix(chn, &fmt, pix);
	if (ret) {
		pr_err("%s %d mbus_to_pix fail\n", __func__, __LINE__);
		return ret;
	}

//	chn->video_data_size = pix->sizeimage;

	chn->format = *format;

	return ret;
}

/*
 * isp_stats_vidioc_try_format -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @format:			video data format
 */
static int isp_stats_vidioc_try_format(
		struct file *file, void *fh, struct v4l2_format *format)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	struct v4l2_subdev_format fmt;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	if (format->type != queue->type)
		return -EINVAL;

	isp_stats_pix_to_mbus(&format->fmt.pix, &fmt.format);
	return isp_stats_mbus_to_pix(chn, &fmt.format, &format->fmt.pix);
}

/*
 * isp_stats_vidioc_cropcap -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @cropcap:		capibility of crop
 */
static int isp_stats_vidioc_cropcap(
		struct file *file, void *fh, struct v4l2_cropcap *cropcap)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	return 0;
}

/*
 * isp_stats_vidioc_get_crop -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @crop:			return crop
 */
static int isp_stats_vidioc_get_crop(
		struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	*crop = isp->crop;
	return 0;
}

/*
 * isp_stats_vidioc_set_crop -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @crop:			crop to set
 */
static int isp_stats_vidioc_set_crop(
		struct file *file, void *fh, const struct v4l2_crop *crop)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	return 0;
}

/*
 * isp_stats_vidioc_get_param -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @parm:			parameter of stream
 */
static int isp_stats_vidioc_get_param(
		struct file *file, void *fh, struct v4l2_streamparm *parm)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	return 0;
}

/*
 * isp_stats_vidioc_set_param -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @parm:			parameter of stream
 */
static int isp_stats_vidioc_set_param(
		struct file *file, void *fh, struct v4l2_streamparm *parm)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	return 0;
}

/*
 * isp_stats_vidioc_reqbufs -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @rb:				request buffers of the v4l2
 */
static int isp_stats_vidioc_reqbufs(
		struct file *file, void *fh, struct v4l2_requestbuffers *rb)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	ret = vb2_reqbufs(queue, rb);

	return ret;
}

/*
 * isp_stats_vidioc_querybuf -
 *
 * @file:		file of the camera device
 * @fh:			private data
 * @v4l2_b:		the buffer of v4l2
 */
static int isp_stats_vidioc_querybuf(
		struct file *file, void *fh, struct v4l2_buffer *v4l2_b)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	ret = vb2_querybuf(queue, v4l2_b);

	return ret;
}

/*
 * isp_stats_vidioc_qbuf -
 *
 * @file:		file of the camera device
 * @fh:			private data
 * @v4l2_b:		the buffer of v4l2
 */
static int isp_stats_vidioc_qbuf(struct file *file, void *fh, struct v4l2_buffer *v4l2_b)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	ret = vb2_qbuf(queue, v4l2_b);

	return ret;
}

/*
 * isp_stats_vidioc_dqbuf -
 *
 * @file:		file of the camera device
 * @fh:			private data
 * @v4l2_b:		the buffer of v4l2
 */
static int isp_stats_vidioc_dqbuf(struct file *file, void *fh, struct v4l2_buffer *v4l2_b)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	ret = vb2_dqbuf(queue, v4l2_b, file->f_flags);
	/*set video data size, md follow the video*/
//	v4l2_b->reserved = chn->video_data_size;

	return ret;
}

/*
 * isp_stats_vidioc_streamon -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @type:			type of video bus
 */
static int isp_stats_vidioc_streamon(
		struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;
	int ret;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	ret = vb2_streamon(queue, type);

	return ret;
}

/*
 * isp_stats_vidioc_streamoff -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @type:			type of video bus
 */
static int isp_stats_vidioc_streamoff(
		struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct v4l2_fh *vfh = fh;
	struct ak_cam_fh *akfh = vfh_to_ak_cam_fh(vfh);
	struct vb2_queue *queue = &akfh->queue;
	unsigned long flags;
	unsigned int streaming;
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	streaming = vb2_is_streaming(queue);
	if (!streaming)
		goto done;

	spin_lock_irqsave(&hw->lock, flags);
	/*
	 * clear_all_buf_queue() must exec fisrt than vb2_streamoff(),
	 * otherwise cause kernel WARNING in vb2_streamoff().
	 */
	clear_all_buf_queue(chn);
	spin_unlock_irqrestore(&hw->lock, flags);

	vb2_streamoff(queue, type);

done:

	return 0;
}

/*
 * isp_stats_vidioc_enum_input -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @input:			input info
 */
static int isp_stats_vidioc_enum_input(
		struct file *file, void *fh, struct v4l2_input *input)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	if (input->index > 0)
		return -EINVAL;

	strlcpy(input->name, "camera", sizeof(input->name));
	input->type = V4L2_INPUT_TYPE_CAMERA;

	return 0;
}

/*
 * isp_stats_vidioc_g_input -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @input:			input info
 */
static int isp_stats_vidioc_g_input(struct file *file, void *fh, unsigned int *input)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);

	*input = 0;

	return 0;
}

/*
 * isp_stats_vidioc_s_input -
 *
 * @file:			file of the camera device
 * @fh:				private data
 * @input:			input info
 */
static int isp_stats_vidioc_s_input(struct file *file, void *fh, unsigned int input)
{
	struct isp_stats_chn_attr *chn = video_drvdata(file);
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	int input_id = input_video->input_id;
	int chn_id = chn->chn_id;

	pr_debug("%s %d input_id:%d, chn_id:%d\n", __func__, __LINE__, input_id, chn_id);
	return input == 0 ? 0 : -EINVAL;
}

/*v4l2 ops*/
static const struct v4l2_ioctl_ops isp_stats_ioctl_ops = {
	.vidioc_querycap		= isp_stats_vidioc_querycap,
	.vidioc_g_fmt_vid_cap	= isp_stats_vidioc_get_format,
	.vidioc_s_fmt_vid_cap	= isp_stats_vidioc_set_format,
	.vidioc_try_fmt_vid_cap	= isp_stats_vidioc_try_format,
	.vidioc_g_fmt_vid_out	= isp_stats_vidioc_get_format,
	.vidioc_s_fmt_vid_out	= isp_stats_vidioc_set_format,
	.vidioc_try_fmt_vid_out	= isp_stats_vidioc_try_format,
	.vidioc_cropcap			= isp_stats_vidioc_cropcap,
	.vidioc_g_crop			= isp_stats_vidioc_get_crop,
	.vidioc_s_crop			= isp_stats_vidioc_set_crop,
	.vidioc_g_parm			= isp_stats_vidioc_get_param,
	.vidioc_s_parm			= isp_stats_vidioc_set_param,
	.vidioc_reqbufs			= isp_stats_vidioc_reqbufs,
	.vidioc_querybuf		= isp_stats_vidioc_querybuf,
	.vidioc_qbuf			= isp_stats_vidioc_qbuf,
	.vidioc_dqbuf			= isp_stats_vidioc_dqbuf,
	.vidioc_streamon		= isp_stats_vidioc_streamon,
	.vidioc_streamoff		= isp_stats_vidioc_streamoff,
	.vidioc_enum_input		= isp_stats_vidioc_enum_input,
	.vidioc_g_input			= isp_stats_vidioc_g_input,
	.vidioc_s_input			= isp_stats_vidioc_s_input,
};

static void store_stats(struct isp_stats_chn_attr *chn, void *stats_vaddr)
{
	struct input_video_attr *input_video = stats_chn_to_input_video(chn);
	struct isp_attr *isp = &input_video->isp;
	void *isp_struct = isp->isp_struct;
	int type = chn->chn_id;

	switch (type) {
		case ISP_STATS_3DNR:
			{
				AK_ISP_3D_NR_STAT_INFO *_3d_nr_stat_para;
				struct isp_stats_3dnr_header *header;
				int width_block_num;
				int height_block_num;
				int block_size;

				ak_isp_get_mdinfo(isp_struct, &_3d_nr_stat_para,
						&width_block_num, &height_block_num, &block_size);
				header = stats_vaddr;
				header->magic			= STATS_3DNR_MAGIC;
				header->version			= STATS_3DNR_VERSION;
				header->width_block_num	= width_block_num;
				header->height_block_num	= height_block_num;
				header->block_size		= block_size;
				memcpy(stats_vaddr + STATS_HEADER_SIZE,
						_3d_nr_stat_para->MD_stat,
						width_block_num * height_block_num * block_size);
			}
			break;

		case ISP_STATS_AWB:
			{
				AK_ISP_AWB_STAT_INFO *p_awb_stat_info;
				struct isp_stats_awb_header *header;

				p_awb_stat_info = stats_vaddr + STATS_HEADER_SIZE;
				ak_isp_vp_get_awb_stat_info(isp_struct, p_awb_stat_info);
				header = stats_vaddr;
				header->magic			= STATS_AWB_MAGIC;
				header->version			= STATS_AWB_VERSION;
			}
			break;

		case ISP_STATS_AF:
			{
				AK_ISP_AF_STAT_INFO *p_af_stat_info;
				struct isp_stats_af_header *header;

				p_af_stat_info = stats_vaddr + STATS_HEADER_SIZE;
				ak_isp_vp_get_af_stat_info(isp_struct, p_af_stat_info);
				header = stats_vaddr;
				header->magic			= STATS_AF_MAGIC;
				header->version			= STATS_AF_VERSION;
			}
			break;

		case ISP_STATS_AE:
			{
				AK_ISP_AE_ATTR *ae_attr;
				AK_ISP_AE_RUN_INFO *ae_run;
				struct isp_stats_ae_header *header;
				int ae_attr_offset = STATS_HEADER_SIZE;
				int ae_run_offset = STATS_HEADER_SIZE +
					ALIGN(sizeof(AK_ISP_AE_ATTR), STATS_ALIGN_SIZE);

				ae_attr = stats_vaddr + ae_attr_offset;
				ae_run = stats_vaddr + ae_run_offset;
				ak_isp_vp_get_ae_attr(isp_struct, ae_attr);
				ak_isp_vp_get_ae_run_info(isp_struct, ae_run);
				header = stats_vaddr;
				header->magic			= STATS_AE_MAGIC;
				header->version			= STATS_AE_VERSION;
				header->ae_attr_offset	= ae_attr_offset;
				header->ae_run_offset	= ae_run_offset;
			}
			break;

		default:
			break;
	}
}

/*
 * list_entries_num -
 * calac number of list
 *
 * @head:			point to list
 */
static inline int list_entries_num(struct list_head *head)
{
	int i;
	struct list_head *list = head;

	for (i = 0; list->next != head; i++)
		list = list->next;

	return i;
}

/*
 * pop_one_buf_from_pend_done_list - pop one buffer from pend done list
 * @chn:		pointer to channel
 *
 * @RETURN:	0-success, others-fail
 */
static inline int pop_one_buf_from_pend_done_list(struct isp_stats_chn_attr *chn)
{
	struct vb2_buffer *vb2_b;
	struct vb2_v4l2_buffer *vb2_v4l2_b;
	struct ak_camera_buffer *ak_b;
	struct list_head *pend_done_list = &chn->pend_done_list;
	int chn_id = chn->chn_id;
	int index;

	if (list_empty(pend_done_list)) {
		/*shoud never to here*/
		pr_debug("pop empty\n");
		return -EINVAL;
	}

	/*remove the oldest buffer from pend done list*/
	ak_b = list_first_entry(pend_done_list, struct ak_camera_buffer, queue);
	list_del_init(&ak_b->queue);

	/*enable hw buffer & add the buffer to capture*/
	vb2_v4l2_b = &ak_b->vb2_v4l2_b;
	vb2_b = &vb2_v4l2_b->vb2_buf;
	index = vb2_b->index;

	/*
	 *	after adding to capture list, then the buffer is visible in irq_handle
	 */
	list_add_tail(&ak_b->queue, &chn->capture);

	pr_debug("pop chn_id:%d, id:%d\n", chn_id, vb2_b->index);

	return 0;
}

static struct ak_camera_buffer *get_first_entry(struct isp_stats_chn_attr *chn)
{
	struct ak_camera_buffer *ak_b;

	list_for_each_entry(ak_b, &chn->capture, queue) {
		return ak_b;
	}

	return NULL;
}

static void isp_stats_work(struct work_struct *work)
{
	struct isp_stats_attr *isp_stats = container_of(work, struct isp_stats_attr, stats_work.work);
	struct input_video_attr *input_video = isp_stats_to_input_video(isp_stats);
	struct isp_attr *isp = &input_video->isp;
	struct isp_stats_chn_attr *chn;
	struct video_device *vdev;
	struct ak_camera_buffer *ak_b;
	struct vb2_v4l2_buffer *vb2_v4l2_b;
	struct vb2_buffer *vb2_b;
	struct timeval *timestamp = &input_video->input_timestamp;
	void *stats_vaddr;
	int input_id = input_video->input_id;
	int chn_id;
	int num;

	pr_debug("%s input_id:%d\n", __func__, input_id);

	if (!isp_param_check_working(isp))
		/*isp is closed so no longer call it*/
		return;

	for (chn_id = 0; chn_id < ISP_STATS_TYPE_NUM; chn_id++) {
		chn = &isp_stats->chn[chn_id];
		vdev = &chn->vdev;

		/*START_STREAMING means the chn is ready in single mode*/
		if (chn->chn_state != CHN_STATE_START_STREAMING)
			continue;

		/*check number of buffers*/
		num = list_entries_num(&chn->capture);
		if (num <= 0) {
			/*
			 * run to here indicate less than 1 active hw buffer,
			 * so move one buffer from pend_done_list to capture list.
			 *
			 * no need check return value at here. if fail I cannot do anything.
			 */
			pop_one_buf_from_pend_done_list(chn);

			pr_debug("chn:%d,num:%d\n", chn_id, num);
		}

		/*get first buffer in list*/
		ak_b = get_first_entry(chn);
		if (!ak_b) {
			pr_debug("%s NO ak_b\n", __func__);
			continue;
		}

		vb2_v4l2_b = &ak_b->vb2_v4l2_b;
		vb2_b= &vb2_v4l2_b->vb2_buf;

		stats_vaddr = vb2_plane_vaddr(vb2_b, 0);
		store_stats(chn, stats_vaddr);

		/*fill vb2_v4l2_b*/
		vb2_v4l2_b->timestamp = *timestamp;
//		vb2_v4l2_b->field = RAWDATA_FRAME;
		vb2_v4l2_b->sequence = chn->phyaddr[vb2_b->index];

		if (chn->poll_empty) {
			/*poll had call into so remove the buffer to done_list*/
			chn->poll_empty = 0;
			list_del_init(&ak_b->queue);
			vb2_buffer_done(vb2_b, VB2_BUF_STATE_DONE);
		} else {
			/*remove the buffer to pend_done_list*/
			list_move_tail(&ak_b->queue, &chn->pend_done_list);
		}
	}
}

int isp_stats_init(struct isp_stats_attr *isp_stats)
{
	struct input_video_attr *input_video = isp_stats_to_input_video(isp_stats);
	struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	struct hw_attr *hw = &ak_cam->hw;
	struct v4l2_device *v4l2_dev = &hw->v4l2_dev;
	struct isp_stats_chn_attr *chn;
	struct video_device *vdev;
	struct device *dev = hw->dev;
	void *err_alloc_ctx = NULL;
	int chn_id;

	pr_debug("%s %d\n", __func__, __LINE__);

	for (chn_id = 0; chn_id < ISP_STATS_TYPE_NUM; chn_id++) {
		chn = &isp_stats->chn[chn_id];
		chn->chn_id = chn_id;
		chn->input_video = input_video;

		/*set state*/
		chn->chn_state = CHN_STATE_CLOSED;

		/* list of video-buffers */
		INIT_LIST_HEAD(&chn->capture);
		INIT_LIST_HEAD(&chn->pend_done_list);

		chn->alloc_ctx = vb2_dma_contig_init_ctx(dev);
		err_alloc_ctx = chn->alloc_ctx;
		if (IS_ERR(chn->alloc_ctx)) {
			pr_err("%s %d chn_id:%d get alloc_ctx:%p err\n",
					__func__, __LINE__, chn_id, chn->alloc_ctx);
			goto error_alloc_ctx;
		}

#if 0
		mutex_init(&chn->queue_lock);
		mutex_init(&chn->stream_lock);
		mutex_init(&chn->fmt_lock);
#endif

		vdev = &chn->vdev;

		mutex_init(&chn->vlock);

		/*
		 * Provide a mutex to v4l2 core. It will be used
		 * to protect all fops and v4l2 ioctls.
		 */
		vdev->lock = &chn->vlock;//video device lock
		vdev->v4l2_dev = v4l2_dev;
		vdev->fops = &isp_stats_file_fops;
		snprintf(vdev->name, sizeof(vdev->name), ISP_STATS_NAME,
				stats_type_to_name[chn_id].name, input_video->input_id);
		vdev->vfl_type = VFL_TYPE_GRABBER;
		vdev->release = video_device_release_empty;
		vdev->ioctl_ops = &isp_stats_ioctl_ops;

		video_set_drvdata(vdev, chn);
	}

	INIT_DELAYED_WORK(&isp_stats->stats_work, isp_stats_work);

	return 0;

error_alloc_ctx:
	for (chn_id = 0; chn_id < ISP_STATS_TYPE_NUM; chn_id++) {
		chn = &isp_stats->chn[chn_id];
		if (!IS_ERR(chn->alloc_ctx))
			vb2_dma_contig_cleanup_ctx(chn->alloc_ctx);
		chn->alloc_ctx = NULL;
		chn->chn_id = 0;
		chn->input_video = NULL;
	}

	return PTR_ERR(err_alloc_ctx);
}

int isp_stats_uninit(struct isp_stats_attr *isp_stats)
{
	struct input_video_attr *input_video = isp_stats_to_input_video(isp_stats);
	//struct ak_camera_dev *ak_cam = input_video_to_ak_cam(input_video);
	//struct hw_attr *hw = &ak_cam->hw;
	//struct device *dev = hw->dev;
	struct isp_stats_chn_attr *chn;
	int input_id = input_video->input_id;
	int chn_id;

	pr_debug("%s input_id:%d\n", __func__, input_id);

	for (chn_id = 0; chn_id < ISP_STATS_TYPE_NUM; chn_id++) {
		chn = &isp_stats->chn[chn_id];
		if (!IS_ERR(chn->alloc_ctx))
			vb2_dma_contig_cleanup_ctx(chn->alloc_ctx);
		chn->alloc_ctx = NULL;
		chn->chn_id = 0;
		chn->input_video = NULL;
	}

	return 0;
}

int isp_stats_register(struct isp_stats_attr *isp_stats)
{
	struct input_video_attr *input_video = isp_stats_to_input_video(isp_stats);
	struct video_device *vdev;
	struct isp_stats_chn_attr *chn;
	int input_id = input_video->input_id;
	int chn_id;
	int ret;

	pr_debug("%s input_id:%d\n", __func__, input_id);

	for (chn_id = 0; chn_id < ISP_STATS_TYPE_NUM; chn_id++) {
		chn = &isp_stats->chn[chn_id];
		vdev = &chn->vdev;

		/*set video node name*/
		vdev->dev.init_name = vdev->name;

		/*register video devide*/
		ret = video_register_device(vdev, vdev->vfl_type, -1);
		if (ret < 0) {
			pr_err("%s register video device fail. input_video:%d, ret:%d\n",
					__func__, input_id, ret);
			goto error_register_video_dev;
		}
	}

	return 0;

error_register_video_dev:
	for (chn_id = 0; chn_id < ISP_STATS_TYPE_NUM; chn_id++) {
		chn = &isp_stats->chn[chn_id];
		vdev = &chn->vdev;

		if (video_is_registered(vdev)) {
			/*unregister video device*/
			video_unregister_device(vdev);
		}
	}

	return ret;
}

int isp_stats_unregister(struct isp_stats_attr *isp_stats)
{
	struct input_video_attr *input_video = isp_stats_to_input_video(isp_stats);
	struct video_device *vdev;
	struct isp_stats_chn_attr *chn;
	int input_id = input_video->input_id;
	int chn_id;

	pr_err("%s input_id:%d\n", __func__, input_id);

	for (chn_id = 0; chn_id < ISP_STATS_TYPE_NUM; chn_id++) {
		chn = &isp_stats->chn[chn_id];
		vdev = &chn->vdev;

		if (video_is_registered(vdev)) {
			/*unregister video device*/
			video_unregister_device(vdev);
		}
	}

	return 0;
}

void isp_stats_process_irq_start(struct isp_stats_attr *isp_stats)
{
	struct input_video_attr *input_video = isp_stats_to_input_video(isp_stats);
	struct isp_attr *isp = &input_video->isp;
	unsigned int fend_count = input_video->fend_count;

	if (!isp_param_check_working(isp))
		/*isp is closed so no longer call it*/
		return;

	if (isp_stats->record_fend_count != fend_count) {
		isp_stats->record_fend_count = fend_count;
		isp_stats->stats_count++;
		schedule_delayed_work(&isp_stats->stats_work, 0);
	}
}

void isp_stats_process_irq_end(struct isp_stats_attr *isp_stats)
{
	struct input_video_attr *input_video = isp_stats_to_input_video(isp_stats);
	int input_id = input_video->input_id;
	pr_debug("%s input_id:%d\n", __func__, input_id);
}
