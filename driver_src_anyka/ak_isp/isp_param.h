/*
 * isp_param.h
 *
 * isp paramter interface header file
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
#ifndef __ISP_PARAM_H__
#define __ISP_PARAM_H__
int isp_param_init(struct isp_attr *isp);
int isp_param_uninit(struct isp_attr *isp);
int isp_param_register(struct isp_attr *isp);
int isp_param_unregister(struct isp_attr *isp);
void isp_param_process_irq_start(struct isp_attr *isp);
void isp_param_process_irq_end(struct isp_attr *isp);
bool isp_param_check_working(struct isp_attr *isp);
#endif
