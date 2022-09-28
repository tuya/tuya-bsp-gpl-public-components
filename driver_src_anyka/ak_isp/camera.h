/*
 * camera.h
 *
 * low level camera interface header file
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
#ifndef __CAMERA_H__
#define __CAMERA_H__

enum mipi_port {
	MIPI_PORT_0 = 0,
	MIPI_PORT_1,
};

enum mipi_mode {
	MIPI_MODE_SINGLE = 0,
	MIPI_MODE_DUAL,
};

enum camera_io_level {
	CAMERA_IO_LEVEL_1V8 = 0,
	CAMERA_IO_LEVEL_2V5,
	CAMERA_IO_LEVEL_3V3,
};

/**************************************
 * ISP
**************************************/
int camera_isp_get_bits_width(void *isp_struct);
AK_ISP_PCLK_POLAR camera_isp_get_pclk_polar(void *isp_struct);
int camera_isp_vi_apply_mode(void *isp_struct,enum isp_working_mode mode);
int camera_isp_vo_enable_target_lines_done(void *isp_struct, int lines);
int camera_isp_vo_enable_irq_status(void *isp_struct, int bit);
int camera_isp_vi_start_capturing(void *isp_struct, int yuv420_type);
int camera_isp_vo_set_main_channel_scale(void *isp_struct, int width, int height);
int camera_isp_vo_set_sub_channel_scale(void *isp_struct, int width, int height);
int camera_isp_vi_set_crop(void *isp_struct, int sx, int sy, int width, int height);
int camera_isp_vi_stop_capturing(void *isp_struct);
int camera_isp_set_td(void *isp_struct);
int camera_isp_reload_td(void *isp_struct);
int camera_isp_vo_set_main_buffer_addr
	(void *isp_struct,
	 enum buffer_id id,
	 unsigned long yaddr_main_chan_addr);
int camera_isp_vo_set_sub_buffer_addr
	(void *isp_struct,
	 enum buffer_id id,
	 unsigned long yaddr_sub_chan_addr);
int camera_isp_vo_set_ch3_buffer_addr
	(void *isp_struct,
	 enum buffer_id id,
	 unsigned long yaddr_chan3_addr);
int camera_isp_enable_buffer_main(void *isp_struct);
int camera_isp_enable_buffer_sub(void *isp_struct);
int camera_isp_enable_buffer_ch3(void *isp_struct);
int camera_isp_vo_enable_buffer_main(void *isp_struct, enum buffer_id id);
int camera_isp_vo_enable_buffer_sub(void *isp_struct, enum buffer_id id);
int camera_isp_vo_enable_buffer_ch3(void *isp_struct, enum buffer_id id);
int camera_isp_is_continuous(void *isp_struct);
int camera_isp_irq_work(void *isp_struct);
int camera_isp_vo_get_using_frame_main_buf_id(void *isp_struct);
int camera_isp_vo_get_using_frame_sub_buf_id(void *isp_struct);
int camera_isp_vo_get_using_frame_ch3_buf_id(void *isp_struct);
int camera_isp_vo_disable_buffer_main(void *isp_struct, enum buffer_id id);
int camera_isp_vo_disable_buffer_sub(void *isp_struct, enum buffer_id id);
int camera_isp_vo_disable_buffer_ch3(void *isp_struct, enum buffer_id id);
int camera_isp_vi_capturing_one(void *isp_struct);
int camera_isp_vo_check_update_status(void *isp_struct);
int camera_isp_vo_check_irq_status (void *isp_struct);
int camera_isp_vo_clear_update_status(void *isp_struct, int bit);
int camera_isp_vo_clear_irq_status(void *isp_struct, int bit);
int camera_isp_pause_isp_capturing(void *isp_struct);
int camera_isp_resume_isp_capturing(void *isp_struct);
int camera_isp_vi_get_input_data_format(void *isp_struct, struct input_data_format *idf);
int camera_isp_vi_get_crop(void *isp_struct, int *sx, int *sy, int *width, int *height);
int camera_isp_set_misc_attr_ex(void *isp_struct, int oneline, int fsden, int hblank, int fsdnum);
int camera_isp_set_ae_fast_struct_default(void *isp_struct, struct ae_fast_struct *ae_fast);

/**************************************
 * mipi ipregisters
**************************************/
void camera_mipi_ip_prepare(enum mipi_mode mode);
void camera_mipi_ip_port_cfg(enum mipi_port port, int mhz, int lanes);

/**************************************
 * controller registers
**************************************/
int camera_ctrl_set_dvp_port(int dvp_port, enum camera_io_level io_level, int bits_width);
unsigned long camera_ctrl_set_mipi_csi_pclk(unsigned long internal_pclk);
void camera_ctrl_mipi_port_sel(enum mipi_port port);
void camera_ctrl_set_pclk_polar(int port, int is_rising);
void camera_ctrl_set_sclk1(int output_mhz);
void camera_ctrl_clks_reset(void);

#endif
