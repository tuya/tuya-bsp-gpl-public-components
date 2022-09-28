/*
 * isp_stats.h
 *
 * isp statistics interface header file
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
#ifndef __ISP_STATS_H__
#define __ISP_STATS_H__

#define STATS_HEADER_SIZE 128
#define STATS_ALIGN_SIZE 64

#define STATS_3DNR_MAGIC	0x33444e52
#define STATS_AWB_MAGIC		0x4157422a
#define STATS_AF_MAGIC		0x41462a2a
#define STATS_AE_MAGIC		0x41452a2a

#define STATS_3DNR_VERSION	1
#define STATS_AWB_VERSION	1
#define STATS_AF_VERSION	1
#define STATS_AE_VERSION	1

struct isp_stats_3dnr_header {
	/*total 128 BYTES*/
	unsigned int magic;//4bytes
	unsigned short version;//2bytes
	unsigned short width_block_num;//2bytes
	unsigned short height_block_num;//2bytes
	unsigned short block_size;//2bytes
	unsigned char reserved[116];//116bytes
};

struct isp_stats_awb_header {
	/*total 128 BYTES*/
	unsigned int magic;//4bytes
	unsigned short version;//2bytes
	unsigned char reserved[122];//122bytes
};

struct isp_stats_af_header {
	/*total 128 BYTES*/
	unsigned int magic;//4bytes
	unsigned short version;//2bytes
	unsigned char reserved[122];//122bytes
};

struct isp_stats_ae_header {
	/*total 128 BYTES*/
	unsigned int magic;//4bytes
	unsigned short version;//2bytes
	int ae_attr_offset;//4bytes
	int ae_run_offset;//4bytes
	unsigned char reserved[114];//114bytes
};

int isp_stats_init(struct isp_stats_attr *isp_stats);

int isp_stats_uninit(struct isp_stats_attr *isp_stats);

int isp_stats_register(struct isp_stats_attr *isp_stats);

int isp_stats_unregister(struct isp_stats_attr *isp_stats);

void isp_stats_process_irq_start(struct isp_stats_attr *isp_stats);

void isp_stats_process_irq_end(struct isp_stats_attr *isp_stats);

#endif
