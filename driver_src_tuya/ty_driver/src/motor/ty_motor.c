/*
* ty_motor.c - Tuya
*
* Copyright (C) 2020 Tuya Technology Corp.
*
* Author: dyh
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/
#include <ty_motor.h>
#include <ty_motor_usr.h>

extern struct ty_motor_dev g_motor_init __attribute__((weak));
struct ty_motor_dev g_motor_init;

//电动4个象限系统的指令值
BYTE g_motor_steps		= 0;
BYTE *g_motor_value		= NULL;
BYTE g_motor_value_1[] 	= {0x01, 0x02, 0x04, 0x08};
BYTE g_motor_value_2[] 	= {0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09};

//static struct timeval st;
static struct ty_motor_dev *g_motor_dev = NULL;

static int ty_chip_tpm0596_write(struct ty_motor_dev *ty_motor_dev, void *data)
{
	BYTE	i 		= 0;
	BYTE 	rdata 	= 0;
	BYTE	val 	= 0;

//	struct timeval tv;
//	int us,div;
//	static int cnts = 0;

	rdata = *((BYTE *)data);

//	do_gettimeofday(&tv); 
//
//	us = tv.tv_sec * 1000000 + tv.tv_usec;
//	div = (tv.tv_sec * 1000000 + tv.tv_usec) - (st.tv_sec * 1000000 + st.tv_usec);
//	st.tv_sec = tv.tv_sec;
//	st.tv_usec = tv.tv_usec;
//
//	if(((cnts+1) %100)==0) {
//		printk("==us:%d,div:%d ===\n",us,div);
//		cnts = 0;
//	}
	if (-1 != ty_motor_dev->used_pins[MOTOR_G_PIN].gpio) {	//有些硬件版本直接把此管脚上拉
		gpio_set_value(ty_motor_dev->used_pins[MOTOR_G_PIN].gpio, 	1);				//MMOTOR_G
	}
	gpio_set_value(ty_motor_dev->used_pins[MOTOR_CLR_PIN].gpio, 1);				//MOTOR_CLR

	for (i = 0; i < 8; i++) {						//上升沿采样
		gpio_set_value(ty_motor_dev->used_pins[MOTOR_SRCK_PIN].gpio, 	0);
		val 	= rdata & 0x01;
		val 	= !!val;
		rdata 	= rdata >> 1;
		gpio_set_value(ty_motor_dev->used_pins[MOTOR_SER_IN_PIN].gpio, 	val);
		gpio_set_value(ty_motor_dev->used_pins[MOTOR_SRCK_PIN].gpio, 	1);
	}
	
	gpio_set_value(ty_motor_dev->used_pins[MOTOR_SER_IN_PIN].gpio,	0);
	gpio_set_value(ty_motor_dev->used_pins[MOTOR_SRCK_PIN].gpio, 	0);
	gpio_set_value(ty_motor_dev->used_pins[MOTOR_RCK_PIN].gpio, 	1);
	gpio_set_value(ty_motor_dev->used_pins[MOTOR_RCK_PIN].gpio, 	0);

	if (-1 != ty_motor_dev->used_pins[MOTOR_G_PIN].gpio) {	//有些硬件版本直接把此管脚上拉
		gpio_set_value(ty_motor_dev->used_pins[MOTOR_G_PIN].gpio, 	0);				//MMOTOR_G
	}

	return 0;
}

static int ty_chip_74hc595_write(struct ty_motor_dev *ty_motor_dev, void *data)
{
	BYTE	i 		= 0;
	BYTE 	rdata 	= 0;
	BYTE	val 	= 0;
	
	rdata = *((BYTE *)data);

	for (i = 0; i < 8; i++) {
		val 	= rdata & 0x01;
		val 	= !!val;
		rdata 	= rdata >> 1;
		gpio_set_value(ty_motor_dev->used_pins[MOTOR_DS_PIN].gpio, 	val);		//位送数据
		gpio_set_value(ty_motor_dev->used_pins[MOTOR_SH_PIN].gpio, 	0);			//输入数据时钟
		gpio_set_value(ty_motor_dev->used_pins[MOTOR_SH_PIN].gpio, 	1);
	}
	gpio_set_value(ty_motor_dev->used_pins[MOTOR_ST_PIN].gpio, 	0);				//输出数据时钟	
	gpio_set_value(ty_motor_dev->used_pins[MOTOR_ST_PIN].gpio, 	1);

	return 0;
}

static int ty_chip_ms41929_write(struct ty_motor_dev *ty_motor_dev, void *data)
{
	BYTE	i 			= 0;
	BYTE	val			= 0;	
	DWORD	data_send	= 0;
	ty_motor_ms41929_t 	*rdata;	
	
	rdata = (ty_motor_ms41929_t *)data;
	
	data_send	= ((rdata->data) << 8) | (rdata->addr);
	gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_CS_PIN].gpio, 	1);
	for (i = 0; i < 24; i++) {
		gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_CLK_PIN].gpio, 	0);		
		val 		= data_send & 0x01;
		val 		= !!val;
		data_send 	= data_send >> 1;
		gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_MOSI_PIN].gpio, val);		//位送数据
		gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_CLK_PIN].gpio, 	1);
	}
	gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_CS_PIN].gpio, 	0);
	
	return 0;
}

static void ty_motor_shutdown(struct ty_motor_dev *ty_motor_dev)
{
	struct ty_motor_ms41929 mdata;
	char stop_val = 0x00;

	if (ty_motor_dev->chip == CHIP_MS41929) {
		mdata.addr	= MS41929_PAN_VIRTUAL;
		mdata.data	= MS41929_STOP;
		ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
		mdata.addr	= MS41929_TILT_VIRTUAL;
		mdata.data	= MS41929_STOP;
		ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
		gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio, 	1);				//输出数据时钟
		udelay(20);
		gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio, 	0);
	}else {
		if ((CHIP_DARLINGTON_MUX == ty_motor_dev->chip) || (CHIP_DARLINGTON_MUXALL == ty_motor_dev->chip)) {
			if (ty_motor_dev->ty_motor_sel) {
				ty_motor_dev->ty_motor_sel(ty_motor_dev, MOTOR_PAN_DIR);
			}
			ty_motor_dev->ty_chip_send(ty_motor_dev, &stop_val);

			if (ty_motor_dev->ty_motor_sel) {
				ty_motor_dev->ty_motor_sel(ty_motor_dev, MOTOR_TILT_DIR);
			}
			ty_motor_dev->ty_chip_send(ty_motor_dev, &stop_val);
		}else {
			ty_motor_dev->ty_chip_send(ty_motor_dev, &stop_val);
		}
	}

	ty_motor_dev->m_status.pan_status 	= 0;
	ty_motor_dev->m_status.tilt_status 	= 0;
	ty_motor_dev->mux_flag.pan_flag		= 0;
	ty_motor_dev->mux_flag.tilt_flag	= 0;
}

#if 0
static BYTE ty_reverse_data(BYTE data)
{    
	BYTE 	i 	= 0;
	BYTE	tmp = 0;

	for (i = 0; i < 8; i++) {
		tmp = tmp << 1;
		tmp |= (data >> i) & 0x01;
	}
	
	return tmp;
}

static WORD ty_chip_ms41929_read(struct ty_motor_dev *ty_motor_dev, void *data)
{
	BYTE	i 			= 0;
	BYTE	val			= 0;
	BYTE	addr_val 	= 0;	
	WORD	data_rcv	= 0;
	ty_motor_ms41929_t 	*rdata;	
	
	rdata = (ty_motor_ms41929_t *)data;

	addr_val 	= ty_reverse_data(rdata->addr) | 0x3;
	gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_CS_PIN].gpio, 	1);
	for (i = 0; i < 8; i++) {
		gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_CLK_PIN].gpio, 	0);		
		val 		= addr_val & 0x80;
		val 		= !!val;
		addr_val 	= addr_val << 1;
		gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_MOSI_PIN].gpio, val);		//位送数据
		gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_CLK_PIN].gpio, 	1);
	}
	for (i = 0; i < 16; i++) {
		gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_CLK_PIN].gpio, 	0);
		val = gpio_get_value(ty_motor_dev->used_pins[MOTO_SPI_MISO_PIN].gpio);		
		val = !!val;
		if (val) {
			data_rcv = data_rcv | 0x01;
		}else {
			data_rcv = data_rcv & 0xFE;
		}
		data_rcv <<= 1;
		gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_CLK_PIN].gpio, 	1);
	}
	gpio_set_value(ty_motor_dev->used_pins[MOTO_SPI_CS_PIN].gpio, 	0);
	
	return data_rcv;
}
#endif
static int ty_chip_darlington_write(struct ty_motor_dev *ty_motor_dev, void *data)
{
	BYTE	i 		= 0;
	BYTE 	rdata 	= 0;
	BYTE	val 	= 0;
	
	rdata = *((BYTE *)data);

	for (i = 0; i < ty_motor_dev->chip_pins_num; i++) {
		val 	= rdata & 0x01;
		val 	= !!val;
		rdata 	= rdata >> 1;

		gpio_set_value(ty_motor_dev->used_pins[i].gpio, 	val);
	}

	return 0;
}

static int ty_motor_muxall_select(struct ty_motor_dev *ty_motor_dev, BYTE port)
{
	static BYTE pre_port = -1;

	if (pre_port != port) {
		pre_port = port;

		gpio_set_value(ty_motor_dev->ctrl_pin.gpio, ty_motor_dev->motor_select ? (port ? 0 : 1) : (port ? 1 : 0));
		gpio_set_value(ty_motor_dev->ctrl_pin2.gpio, ty_motor_dev->motor_select ? (port ? 1 : 0) : (port ? 0 : 1));
	}

	return 0;
}

static int ty_motor_mux_select(struct ty_motor_dev *ty_motor_dev, BYTE port)
{
	static BYTE pre_port = -1;

	if (pre_port != port) {
		pre_port = port;

		gpio_set_value(ty_motor_dev->ctrl_pin.gpio, ty_motor_dev->motor_select ? (port ? 0 : 1) : (port ? 1 : 0));
	}

	return 0;
}

int ty_motor_open(struct inode *node, struct file *filp)
{
	struct ty_motor_dev *ty_motor_dev = container_of(filp->private_data, struct ty_motor_dev, misc_dev);
	struct ty_motor_ms41929 mdata;
	
	if (ty_used_gpio_init(ty_motor_dev->used_pins, ty_motor_dev->chip_pins_num)){
		MS_ERROR("init gpio error.\n");
		return -1;
	}

	switch(ty_motor_dev->chip){
		case CHIP_TPM0596:
			ty_motor_dev->ty_chip_send 	= ty_chip_tpm0596_write;
			g_motor_steps				= 8;		//8 steps
			g_motor_value				= g_motor_value_2;
			break;
		case CHIP_74HC595:
			ty_motor_dev->ty_chip_send 	= ty_chip_74hc595_write;
			g_motor_steps				= 8;
			g_motor_value				= g_motor_value_2;
			break;
		case CHIP_DARLINGTON:
			ty_motor_dev->ty_chip_send 	= ty_chip_darlington_write;
			g_motor_steps				= 8;		//4 steps
			g_motor_value				= g_motor_value_2;
			break;
		case CHIP_DARLINGTON_MUXALL:
			ty_used_gpio_init(&ty_motor_dev->ctrl_pin, 1);
			ty_used_gpio_init(&ty_motor_dev->ctrl_pin2, 1);
			ty_motor_dev->ty_chip_send 	= ty_chip_darlington_write;
			ty_motor_dev->ty_motor_sel	= ty_motor_muxall_select;
			g_motor_steps				= 8;		//4 steps
			g_motor_value				= g_motor_value_2;
			break;
		case CHIP_DARLINGTON_MUX:
			ty_used_gpio_init(&ty_motor_dev->ctrl_pin, 1);
			ty_motor_dev->ty_chip_send 	= ty_chip_darlington_write;
			ty_motor_dev->ty_motor_sel	= ty_motor_mux_select;
			g_motor_steps				= 8;		//4 steps
			g_motor_value				= g_motor_value_2;
			break;
		case CHIP_MS41929:
			gpio_set_value(ty_motor_dev->used_pins[MOTOR_RST_PIN].gpio, 	0);
			ty_motor_dev->ty_chip_send 	= ty_chip_ms41929_write;
			gpio_set_value(ty_motor_dev->used_pins[MOTOR_RST_PIN].gpio, 	1);
			mdata.addr	= 0x0b;
			mdata.data	= 0x0080;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
			mdata.addr	= 0x21;
			mdata.data	= 0x0085;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
			mdata.addr	= 0x20;
			mdata.data	= 0x1e01;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
			mdata.addr	= 0x22;
			mdata.data	= 0x0001;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
			mdata.addr	= 0x27;
			mdata.data	= 0x0001;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
			mdata.addr	= 0x23;
			mdata.data	= 0xd8d8;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
			mdata.addr	= 0x28;
			mdata.data	= 0xd8d8;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
			mdata.addr	= 0x25;
			mdata.data	= 0x100;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
			mdata.addr	= 0x2a;
			mdata.data	= 0x100;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
			mdata.addr	= 0x24;
			mdata.data	= 0x3600;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
			mdata.addr	= 0x29;
			mdata.data	= 0x3600;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);			
			break;			
		default:
			MS_ERROR("please select the chip type of the board.\n");
			return -1;
	}
	
	if (ty_motor_dev->opto_en) {
		if (ty_used_gpio_init(ty_motor_dev->opto_pins, OPTO_PINS_NR)){
			MS_ERROR("init opto gpio error.\n");
			return -1;
		}
	}

    return 0;
}

int ty_motor_release(struct inode *node, struct file *filp)
{
	struct ty_motor_dev *ty_motor_dev = container_of(filp->private_data, struct ty_motor_dev, misc_dev);

	if (ty_used_gpio_deinit(ty_motor_dev->used_pins, ty_motor_dev->chip_pins_num)){
		MS_ERROR("deinit gpio error.\n");
		return -1;
	}

	switch(ty_motor_dev->chip){
		case CHIP_DARLINGTON_MUXALL:
			ty_used_gpio_deinit(&ty_motor_dev->ctrl_pin, 1);
			ty_used_gpio_deinit(&ty_motor_dev->ctrl_pin2, 1);
			break;
		case CHIP_DARLINGTON_MUX:
			ty_used_gpio_deinit(&ty_motor_dev->ctrl_pin, 1);
			break;
		default:
			break;
	}

	if (ty_motor_dev->opto_en) {
		if (ty_used_gpio_deinit(ty_motor_dev->opto_pins, OPTO_PINS_NR)){
			MS_ERROR("deinit opto gpio error.\n");
			return -1;
		}
	}

    return 0;
}

long  set_ptz_cmd(struct ty_motor_dev *phdl, DWORD arg)
{
    struct ty_motor_param_s mparam;
	struct ty_motor_ms41929 mdata;
	
	memset(&mparam, 0x00, sizeof(struct ty_motor_param_s));
    if(copy_from_user(&mparam, (void *)arg, sizeof(struct ty_motor_param_s))) {
        MS_ERROR("copy data error\n");
		
        return -1;
    }

	if(mparam.goal_pan_speed > 0) {
		phdl->pan_speed 	= (DWORD)mparam.goal_pan_speed; 	//设置速度
	}

	if(mparam.goal_tilt_speed > 0) {
		phdl->tilt_speed 	= (DWORD)mparam.goal_tilt_speed;
	}
	MS_DEBUG("mparam.motor_func=%d, pan=%d, tilt=%d\n", mparam.motor_func, phdl->m_status.pls_pan_step, phdl->m_status.pls_tilt_step);
	//最左边和最下面是原点//上一次没结束，下一次有发送，直接覆盖上一次
    switch(mparam.motor_func) {
        case GO_UP:
            phdl->m_status.tilt_status 		= 1;
            phdl->m_status.tilt_goal_pos 	= phdl->tilt_step_max;
			phdl->m_status.tilt_direction 	= 1;
			phdl->mux_flag.dir_flag			= MOTOR_TILT_DIR;		/* 用于互斥电机，1代表垂直方向模式 */
            break;
        case GO_DOWN:
            phdl->m_status.tilt_status 		= 1;
            phdl->m_status.tilt_goal_pos 	= 0;
            phdl->m_status.tilt_direction 	= 0;
			phdl->mux_flag.dir_flag			= MOTOR_TILT_DIR;		/* 用于互斥电机，1代表垂直方向模式 */
            break;
        case GO_LEFT:
            phdl->m_status.pan_status 		= 1;
            phdl->m_status.pan_goal_pos		= 0;
            phdl->m_status.pan_direction 	= 0;
			phdl->mux_flag.dir_flag			= MOTOR_PAN_DIR;		/* 用于互斥电机，0代表水平方向模式 */
            break;
        case GO_RIGHT:
            phdl->m_status.pan_status 		= 1;
            phdl->m_status.pan_goal_pos 	= phdl->pan_step_max;
            phdl->m_status.pan_direction 	= 1;
			phdl->mux_flag.dir_flag			= MOTOR_PAN_DIR;		/* 用于互斥电机，0代表水平方向模式 */
            break;
        case RIGHT_UP:
            phdl->m_status.pan_status 		= 1;
            phdl->m_status.tilt_status 		= 1;
			phdl->m_status.pan_goal_pos 	= phdl->pan_step_max;
			phdl->m_status.tilt_goal_pos 	= phdl->tilt_step_max;
            phdl->m_status.pan_direction 	= 1;
            phdl->m_status.tilt_direction 	= 1;
			phdl->mux_flag.pan_flag			= 1;
			phdl->mux_flag.tilt_flag		= 1;
			phdl->mux_flag.dir_flag			= MOTOR_BOTH_DIR;	/* 用于互斥电机，2代表存在双方向运行模式 */
			if ((CHIP_DARLINGTON_MUX == phdl->chip) || (CHIP_DARLINGTON_MUXALL == phdl->chip)){
				phdl->m_status.tilt_status 	= 0;		/* 互斥模式下，只能运行一路*/
			}
            break;
        case RIGHT_DOWN:
            phdl->m_status.pan_status 		= 1;
            phdl->m_status.tilt_status 		= 1;
	   		phdl->m_status.pan_goal_pos 	= phdl->pan_step_max;
	    	phdl->m_status.tilt_goal_pos 	= 0;
            phdl->m_status.pan_direction 	= 1;
            phdl->m_status.tilt_direction 	= 0;
			phdl->mux_flag.pan_flag			= 1;
			phdl->mux_flag.tilt_flag		= 1;
			phdl->mux_flag.dir_flag			= MOTOR_BOTH_DIR;	/* 用于互斥电机，2代表存在双方向运行模式 */
			if ((CHIP_DARLINGTON_MUX == phdl->chip) || (CHIP_DARLINGTON_MUXALL == phdl->chip)){
				phdl->m_status.tilt_status 	= 0;		/* 互斥模式下，只能运行一路*/
			}
            break;
        case LEFT_UP:
            phdl->m_status.pan_status 		= 1;
            phdl->m_status.tilt_status 		= 1;
			phdl->m_status.pan_goal_pos 	= 0;
			phdl->m_status.tilt_goal_pos 	= phdl->tilt_step_max;
            phdl->m_status.pan_direction 	= 0;
            phdl->m_status.tilt_direction 	= 1;
			phdl->mux_flag.pan_flag			= 1;
			phdl->mux_flag.tilt_flag		= 1;
			phdl->mux_flag.dir_flag			= MOTOR_BOTH_DIR;	/* 用于互斥电机，2代表存在双方向运行模式 */
			if ((CHIP_DARLINGTON_MUX == phdl->chip) || (CHIP_DARLINGTON_MUXALL == phdl->chip)){
				phdl->m_status.tilt_status 	= 0;		/* 互斥模式下，只能运行一路*/
			}
            break;
        case LEFT_DOWN:
            phdl->m_status.pan_status 		= 1;
            phdl->m_status.tilt_status 		= 1;
			phdl->m_status.pan_goal_pos 	= 0;
			phdl->m_status.tilt_goal_pos 	= 0;
            phdl->m_status.pan_direction 	= 0;
            phdl->m_status.tilt_direction 	= 0;
			phdl->mux_flag.pan_flag			= 1;
			phdl->mux_flag.tilt_flag		= 1;
			phdl->mux_flag.dir_flag			= MOTOR_BOTH_DIR;	/* 用于互斥电机，2代表存在双方向运行模式 */
			if ((CHIP_DARLINGTON_MUX == phdl->chip) || (CHIP_DARLINGTON_MUXALL == phdl->chip)){
				phdl->m_status.tilt_status 	= 0;		/* 互斥模式下，只能运行一路*/
			}
            break;
        case GO_POS:
            phdl->m_status.pan_goal_pos 	= mparam.goal_pan_pos;
            phdl->m_status.tilt_goal_pos 	= mparam.goal_tilt_pos;
			
            MS_DEBUG("cur,  pls_pan_step=%d  pls_tilt_step=%d\n", phdl->m_status.pls_pan_step, phdl->m_status.pls_tilt_step);
            MS_DEBUG("goal, pan_goal_step=%d  tilt_goal_pos=%d\n", phdl->m_status.pan_goal_pos, phdl->m_status.tilt_goal_pos);

			if (phdl->m_status.pan_goal_pos > phdl->pan_step_max)
				phdl->m_status.pan_goal_pos = phdl->pan_step_max;
			if (phdl->m_status.pan_goal_pos < 0)
				phdl->m_status.pan_goal_pos = 0;
			if (phdl->m_status.tilt_goal_pos > phdl->tilt_step_max)
				phdl->m_status.tilt_goal_pos = phdl->tilt_step_max;
			if (phdl->m_status.tilt_goal_pos < 0)
				phdl->m_status.tilt_goal_pos = 0;

			if (phdl->m_status.pan_goal_pos == phdl->m_status.pls_pan_step) {
				phdl->mux_flag.pan_flag					= 0;
				phdl->m_status.pan_status 				= 0;
			}else {
				phdl->mux_flag.pan_flag					= 1;
				phdl->m_status.pan_status 				= 1;
				if (phdl->infi_rot_en) {		////无极电机设备
					phdl->m_status.pan_pos_stop_flag		= 1;
					if (phdl->m_status.pan_goal_pos < phdl->m_status.pls_pan_step)
					{
						if ((phdl->m_status.pls_pan_step - phdl->m_status.pan_goal_pos) > (phdl->pan_step_max >> 1)){
							phdl->m_status.pan_direction	= 1;
						}else{
							phdl->m_status.pan_direction 	= 0;
						}
					}else{
						if ((phdl->m_status.pan_goal_pos - phdl->m_status.pls_pan_step) > (phdl->pan_step_max >> 1)){
							phdl->m_status.pan_direction	= 0;
						}else{
							phdl->m_status.pan_direction 	= 1;
						}
					}
				}else{
					if (phdl->m_status.pan_goal_pos < phdl->m_status.pls_pan_step) {		/*left*/
						phdl->m_status.pan_direction 	= 0;
					}else {																	/*right*/
						phdl->m_status.pan_direction 	= 1;
					}
				}
			}
			
			if (phdl->m_status.tilt_goal_pos == phdl->m_status.pls_tilt_step) {
				phdl->mux_flag.tilt_flag			= 0;
				phdl->m_status.tilt_status 			= 0;
			}else {
				phdl->mux_flag.tilt_flag			= 1;
				phdl->m_status.tilt_status 			= 1;
				if ((CHIP_DARLINGTON_MUX == phdl->chip) || (CHIP_DARLINGTON_MUXALL == phdl->chip)){
					if (phdl->m_status.pan_status) {									/* 互斥模式下，只能运行一路*/
						phdl->m_status.tilt_status 	= 0;
					}
				}
				if (phdl->m_status.tilt_goal_pos < phdl->m_status.pls_tilt_step) {		/*up*/
					phdl->m_status.tilt_direction 	= 0;
				}else { 																/*down*/
					phdl->m_status.tilt_direction 	= 1;
				}
			}
			phdl->mux_flag.dir_flag					= MOTOR_BOTH_DIR;	/* 用于互斥电机，2代表存在双方向运行模式 */
            break;
        case PAN_SELF_CHECK:
            phdl->m_status.pan_status 			= 1;
			phdl->mux_flag.dir_flag				= MOTOR_PAN_DIR;		/* 用于互斥电机，0代表水平方向模式 */
			phdl->m_status.pan_check_status		= 1;					/* 用于无极限电机 */
            phdl->m_status.pan_goal_pos 		= 0;
			phdl->m_status.pan_direction 		= 0;
			phdl->m_status.pls_pan_step 		= phdl->pan_step_max;
            break;
        case TILT_SELF_CHECK:
            phdl->m_status.tilt_status 			= 1;
			phdl->mux_flag.dir_flag				= MOTOR_TILT_DIR;		/* 用于互斥电机，1代表垂直方向模式 */
            phdl->m_status.tilt_goal_pos 		= 0;
			phdl->m_status.tilt_direction 		= 0;
            phdl->m_status.pls_tilt_step 		= phdl->tilt_step_max;
            break;
        case PAN_STOP:
            phdl->m_status.pan_status 	= 0;
			phdl->mux_flag.pan_flag		= 0;
			if (phdl->chip == CHIP_MS41929) {
				mdata.addr = phdl->motor_select ? MS41929_TILT_VIRTUAL : MS41929_PAN_VIRTUAL;
				mdata.data = MS41929_STOP;
				phdl->ty_chip_send(phdl, &mdata);
				gpio_set_value(phdl->used_pins[MOTO_VD_FZ_PIN].gpio, 	1);				////输出数据时钟
				gpio_set_value(phdl->used_pins[MOTO_VD_FZ_PIN].gpio, 	0);		
			}
            break;
        case TILT_STOP:
            phdl->m_status.tilt_status 	= 0;
			phdl->mux_flag.tilt_flag	= 0;
			if (phdl->chip == CHIP_MS41929) {
				mdata.addr = phdl->motor_select ? MS41929_PAN_VIRTUAL : MS41929_TILT_VIRTUAL;
				mdata.data = MS41929_STOP;
				phdl->ty_chip_send(phdl, &mdata);
				gpio_set_value(phdl->used_pins[MOTO_VD_FZ_PIN].gpio, 	1);				////输出数据时钟
				gpio_set_value(phdl->used_pins[MOTO_VD_FZ_PIN].gpio, 	0);
			}
            break;
        default:
            MS_ERROR("invalid argument\n");
            return -1;
    }

	if (phdl->m_status.pan_status | phdl->m_status.tilt_status) {
		phdl->kt = ktime_set(0, (phdl->base_period) * 1000);
	    hrtimer_start(&phdl->ms_timer, phdl->kt, HRTIMER_MODE_REL);
	}

    return 0;
}

long  get_ptz_state(struct ty_motor_dev *phdl, DWORD arg)
{
    struct ty_motor_param_g mparam;
    
    if(copy_from_user(&mparam, (void *)arg, sizeof(struct ty_motor_param_g))) {
		MS_ERROR("copy mparam data error\n");
        return -1;
    }
	
    switch(mparam.state_cmd)
    {
        case GET_PAN_POS:
            mparam.state_value	= phdl->m_status.pls_pan_step;
            break;
        case GET_TILT_POS:
            mparam.state_value	= phdl->m_status.pls_tilt_step;
            break;
        case GET_PAN_CHECK_STATE:
			mparam.state_value	= !(phdl->m_status.pls_pan_step);
			break;
		case GET_TILT_CHECK_STATE:
			mparam.state_value	= !(phdl->m_status.pls_tilt_step);
			break;
		case GET_LEFT_OPTO_FLAG:
			mparam.state_value	= phdl->m_status.pan_opto_left_flag;
			break;
		case GET_RIGHT_OPTO_FLAG:
			mparam.state_value	= phdl->m_status.pan_opto_right_flag;
			break;			
		case SET_CLEAR_OPTO_FLAG:
			phdl->m_status.pan_opto_left_flag	= 0;
			phdl->m_status.pan_opto_right_flag	= 0;
			break;			
        default:
            break;

    }
    if(copy_to_user((void *)arg, &mparam, sizeof(struct ty_motor_param_g))) {
		MS_ERROR("copy to mparam data error\n");
        return -1;
    }

    return 0;
}

long  set_init_ptz_cmd(struct ty_motor_dev *phdl, DWORD arg)
{
    struct ty_motor_param_init_s iparam;

	memset(&iparam, 0x00, sizeof(struct ty_motor_param_init_s));
    if(copy_from_user(&iparam, (void *)arg, sizeof(struct ty_motor_param_init_s))) {
        MS_ERROR("copy iparam data error\n");
		
        return -1;
    }

	(iparam._infi_rot_en 	!= 0) ? (phdl->infi_rot_en = iparam._infi_rot_en) : (phdl->infi_rot_en = phdl->infi_rot_en);
	(iparam._pan_roll_step 	!= 0) ? (phdl->pan_roll_step	= iparam._pan_roll_step): (phdl->pan_roll_step	= phdl->pan_roll_step);
	(iparam._pan_step_max 	!= 0) ? (phdl->pan_step_max		= iparam._pan_step_max) : (phdl->pan_step_max	= phdl->pan_step_max);
	(iparam._tilt_step_max	!= 0) ? (phdl->tilt_step_max	= iparam._tilt_step_max): (phdl->tilt_step_max	= phdl->tilt_step_max);


	MS_INFO("now, pan_max_step=%d, tilt_max_step=%d, pan_roll_step=%d, infi_rot_en=%d\n", phdl->pan_step_max, phdl->tilt_step_max, phdl->pan_roll_step, phdl->infi_rot_en);
	return 0;
}

long  set_init_ptz_cmd_else(struct ty_motor_dev *phdl, DWORD arg)
{
    struct ty_motor_param_init_s_else iparam;

	memset(&iparam, 0x00, sizeof(struct ty_motor_param_init_s_else));
    if(copy_from_user(&iparam, (void *)arg, sizeof(struct ty_motor_param_init_s_else))) {
        MS_ERROR("copy iparam data error\n");

        return -1;
    }

	(iparam._pls_pan_step	!= 0) ? (phdl->m_status.pls_pan_step	= iparam._pls_pan_step) : (phdl->m_status.pls_pan_step	= phdl->m_status.pls_pan_step);
	(iparam._pls_tilt_step	!= 0) ? (phdl->m_status.pls_tilt_step	= iparam._pls_tilt_step): (phdl->m_status.pls_tilt_step	= phdl->m_status.pls_tilt_step);

	MS_INFO("now, pls_pan_step=%d, pls_tilt_step=%d\n", phdl->m_status.pls_pan_step, phdl->m_status.pls_tilt_step);
	return 0;
}

LONG ty_motor_ioctl(struct file *filp, UINT cmd, DWORD arg)
{
	struct ty_motor_dev *ty_motor_dev = container_of(filp->private_data, struct ty_motor_dev, misc_dev);
	struct ty_motor_ms41929 mdata;
	char stop_val = 0x00;

	mutex_lock(&ty_motor_dev->motor_mutex);	
    switch(cmd) {
        case TY_MOTOR_IOCTL_STOP:		
			if (ty_motor_dev->chip == CHIP_MS41929) {
				mdata.addr	= MS41929_PAN_VIRTUAL;
				mdata.data	= MS41929_STOP;
				ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
				mdata.addr	= MS41929_TILT_VIRTUAL;
				mdata.data	= MS41929_STOP;
				ty_motor_dev->ty_chip_send(ty_motor_dev, &mdata);
				gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio, 	1);				//输出数据时钟
				udelay(20);
				gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio, 	0);				
			}else {
				if ((CHIP_DARLINGTON_MUX == ty_motor_dev->chip) || (CHIP_DARLINGTON_MUXALL == ty_motor_dev->chip)) {
					if (ty_motor_dev->ty_motor_sel) {
						ty_motor_dev->ty_motor_sel(ty_motor_dev, MOTOR_PAN_DIR);
					}
					ty_motor_dev->ty_chip_send(ty_motor_dev, &stop_val);

					if (ty_motor_dev->ty_motor_sel) {
						ty_motor_dev->ty_motor_sel(ty_motor_dev, MOTOR_TILT_DIR);
					}
					ty_motor_dev->ty_chip_send(ty_motor_dev, &stop_val);
				}else {
					ty_motor_dev->ty_chip_send(ty_motor_dev, &stop_val);
				}
			}
			ty_motor_dev->m_status.pan_status 	= 0;
            ty_motor_dev->m_status.tilt_status 	= 0;
			ty_motor_dev->mux_flag.pan_flag		= 0;
			ty_motor_dev->mux_flag.tilt_flag	= 0;
			MS_INFO("last pan=%d, tilt=%d\n", ty_motor_dev->m_status.pls_pan_step, ty_motor_dev->m_status.pls_tilt_step);	
            break;
        case TY_MOTOR_IOCTL_GET:
            get_ptz_state(ty_motor_dev, arg);
            break;
        case TY_MOTOR_IOCTL_SET:
            set_ptz_cmd(ty_motor_dev, arg);
            break;
		case TY_MOTOR_IOCTL_PARA_INIT_SET:
			set_init_ptz_cmd(ty_motor_dev, arg);
			break;
		case TY_MOTOR_IOCTL_PARA_INIT_SET2:
			set_init_ptz_cmd_else(ty_motor_dev, arg);
			break;
        default:
            break;
    }
	mutex_unlock(&ty_motor_dev->motor_mutex);
	
    return 0;
}

static void ty_opto_oper_func(struct ty_motor_dev *ty_motor_dev)
{
	static BYTE 	opto_lcnt	= 0;
	static BYTE 	opto_lflag	= 1;
	static BYTE 	opto_rcnt	= 0;
	static BYTE 	opto_rflag	= 1;

	if (!gpio_get_value(ty_motor_dev->opto_pins[OPTO_LEFT].gpio)) {
		if ((opto_lflag) && ((opto_lcnt++) > OPTO_TOTAL_NUM)) {
			opto_lcnt = 0;
			opto_lflag = 0;
			MS_DEBUG("motor in the left pointer.\n");
			ty_motor_dev->m_status.pls_pan_step 		= 0; 							//左边光敏位置点设零值
			ty_motor_dev->m_status.pan_opto_left_flag 	= 1;
			if (ty_motor_dev->m_status.pan_check_status) {
				ty_motor_dev->m_status.pan_check_status = 0;
				ty_motor_dev->m_status.pan_status		= 0;
				ty_motor_dev->m_status.pan_goal_pos		= 0;
			}	
		}
	}else{
		opto_lflag = 1;
	}

	if (!gpio_get_value(ty_motor_dev->opto_pins[OPTO_RIGHT].gpio)) {
		if ((opto_rflag) && ((opto_rcnt++) > OPTO_TOTAL_NUM)) {
			opto_rcnt = 0;
			opto_rflag = 0;
			MS_DEBUG("motor in the right pointer.\n");
            ty_motor_dev->m_status.pls_pan_step 		= ty_motor_dev->pan_roll_step; 	//右边光敏位置点设最大值
			ty_motor_dev->m_status.pan_opto_right_flag 	= 1;			
		}
	}else{
		opto_rflag = 1;
	}	
}

#ifdef DIS_TEST
static void ty_opto_oper_func_dis_test(struct ty_motor_dev *ty_motor_dev)
{
	static BYTE 	t_opto_lcnt1	= 0;
	static BYTE 	t_opto_lflag	= 1;
	static BYTE 	t_opto_rcnt1	= 0;
	static BYTE 	t_opto_rcnt2	= 0;	
	static BYTE 	t_opto_rflag1	= 0;
	static BYTE 	t_opto_rflag2	= 0;	

	if (!gpio_get_value(ty_motor_dev->opto_pins[OPTO_LEFT].gpio)) {
		if ((t_opto_lflag) && ((t_opto_lcnt1++) > OPTO_TOTAL_NUM)) {		////开始
			t_opto_lcnt1 	= 0;
			t_opto_lflag 	= 0;
			t_opto_rflag1	= 1;		//第一步
			
			MS_INFO("[test mode]motor in the left pointer.\n");
			ty_motor_dev->m_status.pls_pan_step = 0; 							//左边光敏位置点设零值
			if (ty_motor_dev->m_status.pan_check_status) {
				ty_motor_dev->m_status.pan_check_status = 0;
				ty_motor_dev->m_status.pan_goal_pos		= 0;
			}	
		}
	}
	
	if (!gpio_get_value(ty_motor_dev->opto_pins[OPTO_RIGHT].gpio)) {
		if ((t_opto_rflag1) && ((t_opto_rcnt1++) > OPTO_TOTAL_NUM)) {
			t_opto_rcnt1 	= 0;
			t_opto_rflag1 	= 0;
			t_opto_rflag2	= 1;		//第二步

			MS_INFO("[test mode]motor in the right pointer.\neffective distance is=%d\n", ty_motor_dev->m_status.pls_pan_step);          				
		}
	}else{
		if ((t_opto_rflag2) && ((t_opto_rcnt2++) > OPTO_TOTAL_NUM)) {
			t_opto_rcnt2 	= 0;
			t_opto_rflag2	= 0;
			t_opto_lflag 	= 1;		//回到开始
			ty_motor_dev->m_status.pan_status = 0;

			MS_INFO("[test mode]motor total distance is=%d\n", ty_motor_dev->m_status.pls_pan_step);
		}
	}	
}
#endif

static enum hrtimer_restart ms_timer_func(struct hrtimer *timer)
{
	BYTE			dir_val		= 0;
	static BYTE		pre_mux_val	= 0;
	static BYTE		mux_val		= 0;
	static BYTE		pre_val 	= 0;
	static BYTE 	value 		= 0;
	static BYTE 	pcnt  		= 0;
	static BYTE 	tcnt  		= 0;
	static BYTE		prun_flag	= 0;
	static BYTE		trun_flag	= 0;
	static DWORD	ptimes		= 0;
	static DWORD 	ttimes		= 0;
	static ty_motor_ms41929_t	pval_t;
	static ty_motor_ms41929_t	tval_t;

	struct ty_motor_dev *ty_motor_dev = container_of(timer, struct ty_motor_dev, ms_timer);
	
	if ((ty_motor_dev->opto_en) && (ty_motor_dev->m_status.pan_status)) {		//水平轴转的时候需要
		ty_opto_oper_func(ty_motor_dev);
	}

#ifdef DIS_TEST
	if (ty_motor_dev->m_status.pan_status)
		ty_opto_oper_func_dis_test(ty_motor_dev);
#endif

	//先判断是否已到位置点
	if (!(ty_motor_dev->infi_rot_en)) {
	    if ((ty_motor_dev->m_status.pls_pan_step == ty_motor_dev->m_status.pan_goal_pos) &&
			((ty_motor_dev->m_status.pan_status == 1))) {
			ty_motor_dev->m_status.pan_status 	= 0;
			
			if (ty_motor_dev->chip == CHIP_MS41929) {
				pval_t.addr = ty_motor_dev->motor_select ? MS41929_TILT_VIRTUAL : MS41929_PAN_VIRTUAL;
				pval_t.data = MS41929_STOP;
				ty_motor_dev->ty_chip_send(ty_motor_dev, &pval_t);
				gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio, 	1);				//输出数据时钟
				gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio, 	0);						
			}else{				
				pval_t.data 								= 0;
			}
	    }
	}else{
		if (ty_motor_dev->m_status.pan_pos_stop_flag) {		//走到特定位置模式需要停止；left,right等模式不停止//无极限电机
		    if ((ty_motor_dev->m_status.pls_pan_step == ty_motor_dev->m_status.pan_goal_pos) &&
				((ty_motor_dev->m_status.pan_status == 1))) {
				ty_motor_dev->m_status.pan_pos_stop_flag	= 0;
				ty_motor_dev->m_status.pan_status 			= 0;

				if (ty_motor_dev->chip == CHIP_MS41929) {
					pval_t.addr = ty_motor_dev->motor_select ? MS41929_TILT_VIRTUAL : MS41929_PAN_VIRTUAL;
					pval_t.data = MS41929_STOP;
					ty_motor_dev->ty_chip_send(ty_motor_dev, &pval_t);
					gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio, 	1);				//输出数据时钟
					gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio, 	0);					
				}else{				
					pval_t.data								= 0;
				}
		    }
		}
	}

    if ((ty_motor_dev->m_status.pls_tilt_step == ty_motor_dev->m_status.tilt_goal_pos) &&
		(ty_motor_dev->m_status.tilt_status == 1)) {
		ty_motor_dev->m_status.tilt_status 	= 0;

		if (ty_motor_dev->chip == CHIP_MS41929) {
			tval_t.addr = ty_motor_dev->motor_select ? MS41929_PAN_VIRTUAL : MS41929_TILT_VIRTUAL;
			tval_t.data = MS41929_STOP;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &tval_t);
			gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio, 	1);				//输出数据时钟
			gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio, 	0);
		}else{
			tval_t.data						= 0;
		}
    }

	prun_flag	= 0;
	trun_flag	= 0;
	//判断方向行走
	if (ty_motor_dev->m_status.pan_status == 1) {
		if ((++ptimes) >= ty_motor_dev->pan_speed) {
			prun_flag	= 1;
			ptimes 		= 0;
			if ((ty_motor_dev->infi_rot_en) || (ty_motor_dev->m_status.pls_pan_step != ty_motor_dev->m_status.pan_goal_pos)) {
		        if (ty_motor_dev->m_status.pan_direction) {
		            ty_motor_dev->m_status.pls_pan_step++;

					if (ty_motor_dev->chip == CHIP_MS41929) {
						pval_t.data = ty_motor_dev->pan_dir ? MS41929_CLK : MS41929_ANT_CLK;
					}else{				
						pval_t.data = (WORD)g_motor_value[ty_motor_dev->pan_dir ? (pcnt++) : (pcnt--)];
						pcnt = pcnt % g_motor_steps;
					}
					
					if (ty_motor_dev->infi_rot_en) {
			            if(ty_motor_dev->m_status.pls_pan_step > ty_motor_dev->pan_step_max)
			                ty_motor_dev->m_status.pls_pan_step = 0;
					}else{
			            if(ty_motor_dev->m_status.pls_pan_step > ty_motor_dev->pan_step_max)
			                ty_motor_dev->m_status.pls_pan_step = ty_motor_dev->pan_step_max;
					}
		        }else {
		            ty_motor_dev->m_status.pls_pan_step--;

					if (ty_motor_dev->chip == CHIP_MS41929) {
						pval_t.data = ty_motor_dev->pan_dir ? MS41929_ANT_CLK : MS41929_CLK;
					}else{					
						pval_t.data = (WORD)g_motor_value[ty_motor_dev->pan_dir ? (pcnt--) : (pcnt++)];
						pcnt = pcnt % g_motor_steps;
					}
					
					if (ty_motor_dev->infi_rot_en) {
						if(ty_motor_dev->m_status.pls_pan_step < 0)
							ty_motor_dev->m_status.pls_pan_step = ty_motor_dev->pan_step_max;
					}else{
						if(ty_motor_dev->m_status.pls_pan_step < 0)
							ty_motor_dev->m_status.pls_pan_step = 0;
					}
		        }
		    }
		}
	}

	if (ty_motor_dev->m_status.tilt_status == 1) {
		if ((++ttimes) >= ty_motor_dev->tilt_speed) {
			trun_flag	= 1;
			ttimes 		= 0;
		    if (ty_motor_dev->m_status.pls_tilt_step != ty_motor_dev->m_status.tilt_goal_pos) {
		        if (ty_motor_dev->m_status.tilt_direction) {
		            ty_motor_dev->m_status.pls_tilt_step++;

					if (ty_motor_dev->chip == CHIP_MS41929) {
						tval_t.data = ty_motor_dev->tilt_dir ? MS41929_CLK : MS41929_ANT_CLK;
					}else{
						tval_t.data = (WORD)g_motor_value[ty_motor_dev->tilt_dir ? (tcnt++) : (tcnt--)];
						tcnt = tcnt % g_motor_steps;
					}
					
		            if(ty_motor_dev->m_status.pls_tilt_step > ty_motor_dev->tilt_step_max)
		                ty_motor_dev->m_status.pls_tilt_step = ty_motor_dev->tilt_step_max;
		        }else {
		            ty_motor_dev->m_status.pls_tilt_step--;

					if (ty_motor_dev->chip == CHIP_MS41929) {
						tval_t.data = ty_motor_dev->tilt_dir ? MS41929_ANT_CLK : MS41929_CLK;
					}else{	
						tval_t.data = (WORD)g_motor_value[ty_motor_dev->tilt_dir ? (tcnt--) : (tcnt++)];
						tcnt = tcnt % g_motor_steps;
					}
					
		            if(ty_motor_dev->m_status.pls_tilt_step < 0)
		                ty_motor_dev->m_status.pls_tilt_step = 0;
		        }
		    }
		}
	}

	//控制两路电机在水平还是垂直方向
	if (CHIP_MS41929 == ty_motor_dev->chip) { //假如只有一路有效，需要禁止另一路运行，否则另一路也会跟着运动
		if (prun_flag && trun_flag) {
			pval_t.addr = ty_motor_dev->motor_select ? MS41929_TILT_VIRTUAL : MS41929_PAN_VIRTUAL;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &pval_t);
			tval_t.addr = ty_motor_dev->motor_select ? MS41929_PAN_VIRTUAL : MS41929_TILT_VIRTUAL;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &tval_t);
		}else if (prun_flag) {
			pval_t.addr = ty_motor_dev->motor_select ? MS41929_TILT_VIRTUAL : MS41929_PAN_VIRTUAL;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &pval_t);
			tval_t.addr = ty_motor_dev->motor_select ? MS41929_PAN_VIRTUAL : MS41929_TILT_VIRTUAL;
			tval_t.data = MS41929_STOP;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &tval_t);
		}else if (trun_flag) {
			pval_t.addr = ty_motor_dev->motor_select ? MS41929_TILT_VIRTUAL : MS41929_PAN_VIRTUAL;
			pval_t.data = MS41929_STOP;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &pval_t);
			tval_t.addr = ty_motor_dev->motor_select ? MS41929_PAN_VIRTUAL : MS41929_TILT_VIRTUAL;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &tval_t);			
		}else {
			pval_t.addr = ty_motor_dev->motor_select ? MS41929_TILT_VIRTUAL : MS41929_PAN_VIRTUAL;
			pval_t.data = MS41929_STOP;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &pval_t);
			tval_t.addr = ty_motor_dev->motor_select ? MS41929_PAN_VIRTUAL : MS41929_TILT_VIRTUAL;
			tval_t.data = MS41929_STOP;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &tval_t);			
		}
		gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio,	1); 			//输出数据时钟
		gpio_set_value(ty_motor_dev->used_pins[MOTO_VD_FZ_PIN].gpio,	0); 
	} else if ((CHIP_DARLINGTON_MUX == ty_motor_dev->chip) || (CHIP_DARLINGTON_MUXALL == ty_motor_dev->chip)) {
		if (MOTOR_PAN_DIR == ty_motor_dev->mux_flag.dir_flag) {
			pre_mux_val 						= (BYTE)pval_t.data;
			dir_val								= MOTOR_PAN_DIR;
		} else if (MOTOR_TILT_DIR == ty_motor_dev->mux_flag.dir_flag) {
			pre_mux_val 						= (BYTE)tval_t.data;
			dir_val								= MOTOR_TILT_DIR;
		} else {
			if ((ty_motor_dev->mux_flag.pan_flag) && (ty_motor_dev->m_status.pan_status)) {				//1、先水平转动，直到pan_status==0
				pre_mux_val 						= (BYTE)pval_t.data;
				dir_val								= MOTOR_PAN_DIR;
			} else if ((ty_motor_dev->mux_flag.tilt_flag) && (!(ty_motor_dev->m_status.pan_status))) {	//2、水平转动结束，启动垂直转
				ty_motor_dev->mux_flag.pan_flag 	= 0;
				ty_motor_dev->mux_flag.tilt_flag	= 0;
				ty_motor_dev->m_status.tilt_status 	= 1;
			} else if (ty_motor_dev->m_status.tilt_status){
				dir_val 							= MOTOR_TILT_DIR;
				pre_mux_val 						= (BYTE)tval_t.data;
			} else {
				ty_motor_dev->mux_flag.pan_flag 	= 0;
				ty_motor_dev->mux_flag.tilt_flag	= 0;
				pre_mux_val							= pre_mux_val;
			}
		}

		if (ty_motor_dev->ty_motor_sel) {
			ty_motor_dev->ty_motor_sel(ty_motor_dev, dir_val);
		}

		if (pre_mux_val != mux_val) {
			mux_val = pre_mux_val;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &mux_val);
		}
	} else {
		if (ty_motor_dev->m_status.tilt_status == 0)
			tval_t.data = 0x00;
		if (ty_motor_dev->m_status.pan_status == 0)
			pval_t.data = 0x00;
		pre_val = ty_motor_dev->motor_select ? (BYTE)((((tval_t.data) << 4) & 0xF0) | ((pval_t.data) & 0x0F)) : (BYTE)((((pval_t.data) << 4) & 0xF0) | ((tval_t.data) & 0x0F));
		if (pre_val != value) { 		//假如与上一次值一样，就不下发数据了
			value = pre_val;
			ty_motor_dev->ty_chip_send(ty_motor_dev, &value);
		}
	}

    if (ty_motor_dev->m_status.tilt_status == 0 && ty_motor_dev->m_status.pan_status == 0) {
		ty_motor_shutdown(ty_motor_dev);
		return HRTIMER_NORESTART;
    }
   
    ty_motor_dev->kt = ktime_set(0, (ty_motor_dev->base_period) * 1000);
    hrtimer_forward_now(timer, ty_motor_dev->kt);

    return HRTIMER_RESTART;
}

struct file_operations ty_motor_fops = {
    .owner          = THIS_MODULE,
    .open           = ty_motor_open,
    .release        = ty_motor_release,
    .unlocked_ioctl = ty_motor_ioctl,
};

int ty_motor_init(void)
{
    int ret = 0;
	
	g_motor_dev = kzalloc(sizeof(struct ty_motor_dev), GFP_KERNEL);
	if (!g_motor_dev) {
        MS_ERROR("kmalloc stepmotor dev fail.\n");
        return -1;
    }

	memcpy(g_motor_dev, &g_motor_init, sizeof(struct ty_motor_dev));
	
	g_motor_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
    g_motor_dev->misc_dev.name 	= MOTOR_DEV_NAME;
    g_motor_dev->misc_dev.fops  = &ty_motor_fops;
    ret = misc_register(&g_motor_dev->misc_dev);
    if(unlikely(ret < 0)) {
        MS_ERROR("register misc fail\n");
    }
	mutex_init(&g_motor_dev->motor_mutex);
	
    hrtimer_init(&g_motor_dev->ms_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    g_motor_dev->ms_timer.function = ms_timer_func;

	if (ty_used_gpio_init(g_motor_dev->used_pins, g_motor_dev->chip_pins_num)){
		MS_ERROR("init gpio error.\n");
		return -1;
	}
	
	if (ty_used_gpio_deinit(g_motor_dev->used_pins, g_motor_dev->chip_pins_num)){
		MS_ERROR("deinit gpio error.\n");
		return -1;
	}
    //do_gettimeofday(&st); 
    MS_INFO("%s module is installed\n", __func__);
	
    return ret;
}

void ty_motor_exit(void)
{
    misc_deregister(&g_motor_dev->misc_dev);

    hrtimer_cancel(&g_motor_dev->ms_timer);
	
	kfree(g_motor_dev);
	g_motor_dev = NULL;

    MS_INFO("%s module is removed.\n", __func__);
	
    return;
}
