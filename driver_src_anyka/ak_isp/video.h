/*
 * video.h
 *
 * isp video data process header file
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
#ifndef __VIDEO_H__
#define __VIDEO_H__
int input_video_init(struct input_video_attr *input_video);
int input_video_uninit(struct input_video_attr *input_video);
int input_video_register(struct input_video_attr *input_video);
int input_video_unregister(struct input_video_attr *input_video);
void input_video_process_irq_start(struct input_video_attr *input_video);
void input_video_process_irq_end(struct input_video_attr *input_video);
#endif
