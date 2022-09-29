#ifndef __TY_MOTOR_H__
#define __TY_MOTOR_H__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <ty_typedefs.h>
#include <linux/slab.h>
#include <linux/mutex.h>


#include <ty_gpio.h>

#define MOTOR_DEV_NAME		"tymotor"

typedef enum {						//TPM0596移位寄存器
	MOTOR_CLR_PIN,
	MOTOR_RCK_PIN,
	MOTOR_G_PIN,
	MOTOR_SRCK_PIN,
	MOTOR_SER_IN_PIN,
	MOTOR_SER_OUT_PIN,
	MOTOR_TPM0596_NR,
}ty_motor_tpm0596_pins_e;

typedef enum {						//74HC595译码器
	MOTOR_SH_PIN,
	MOTOR_ST_PIN,
	MOTOR_DS_PIN,
	MOTOR_74HC595_NR,
}ty_motor_74hc595_pins_e;

typedef enum {						//8路达林顿管直接控制
	MOTOR_DARLINGTON_NR = 8,
}ty_motor_darlington_pins_e;

typedef enum {						//4路达林顿管直接控制
	MOTOR_DARLINGTON_MUX_NR = 4,
}ty_motor_darlington_mux_pins_e;

typedef enum {						//MS41929移位寄存器
	MOTO_SPI_CS_PIN,
	MOTO_SPI_CLK_PIN,
	MOTO_SPI_MOSI_PIN,
	MOTO_SPI_MISO_PIN,
	MOTO_VD_FZ_PIN,
	MOTOR_RST_PIN,
	MOTOR_MS41929_NR,
}ty_motor_ms41929_pins_e;

typedef enum {
	CHIP_TPM0596,					//6路
	CHIP_74HC595,					//3路
	CHIP_DARLINGTON,				//8路达林顿管直接控制	
	CHIP_MS41929,
	CHIP_DARLINGTON_MUX,            //4路达林顿管分时复用
	CHIP_DARLINGTON_MUXALL,			//4路达林顿管分时复用模式2
}ty_chip_type_e;

typedef enum {
	OPTO_LEFT,					//左路
	OPTO_RIGHT,					//右路
	OPTO_PINS_NR,
}ty_opto_e;

typedef enum {						//互斥电机
	MOTOR_PAN_DIR,
	MOTOR_TILT_DIR,
	MOTOR_BOTH_DIR,
}ty_motor_dir_e;

#define OPTO_TOTAL_NUM			10			//用于光敏去除抖动
//#define DIS_TEST 				1			//无极限电机测试行程宏

#define MS41929_TILT_VIRTUAL	0x24
#define MS41929_PAN_VIRTUAL		0x29
#define MS41929_CLK				0x3403
#define MS41929_ANT_CLK			0x3503
#define MS41929_STOP			0x3600


typedef struct ty_motor_ms41929 {
	BYTE	addr;
	WORD	data;
}ty_motor_ms41929_t;

typedef struct ty_motor_status {
    BOOL 	pan_status;				//水平1 代表电机运行中；0代表电机结束
    BOOL 	tilt_status;		
    BOOL 	pan_direction;			//水平1代表正方向；0代表反方向
    BOOL 	tilt_direction;
	BOOL	pan_check_status;
	BOOL	pan_pos_stop_flag;		//特定位置时候需要停止，左右转不需要，针对无极限电机
	BOOL 	pan_opto_left_flag;		//到达左边光耦位置标志
	BOOL 	pan_opto_right_flag;	//
    int 	pls_pan_step;			//水平当前位置点
    int 	pls_tilt_step;
	int		pan_goal_pos;
	int		tilt_goal_pos;
}ty_motor_status_t;

typedef struct ty_mux_flag {
    BOOL 	pan_flag;			//用于电机互斥
    BOOL 	tilt_flag;
	BYTE	dir_flag;			//0：水平方向模式；1：垂直方向模式；2：存在正常双向模式；
}ty_mux_flag_t;

typedef struct ty_motor_dev {
    struct miscdevice 	misc_dev;
	ktime_t 			kt;			//高精度定时器定义
	struct hrtimer 		ms_timer;
	ty_motor_status_t 	m_status;
	ty_mux_flag_t		mux_flag;
	ty_used_gpio_t    	used_pins[MOTOR_DARLINGTON_NR];			//定义最多管脚

	ty_used_gpio_t      ctrl_pin;
	ty_used_gpio_t      ctrl_pin2;

	BYTE				opto_en;
	ty_used_gpio_t    	opto_pins[OPTO_PINS_NR];

	int					pan_roll_step;
	int					pan_step_max;
	int					tilt_step_max;
	DWORD				pan_speed;
	DWORD				tilt_speed;
	DWORD				base_period;

	BYTE				infi_rot_en;
	BYTE				motor_select;
	BYTE				pan_dir;
	BYTE				tilt_dir;
	
	BYTE				chip;
	BYTE				chip_pins_num;
	
	struct mutex 		motor_mutex;
	
	int (*ty_chip_send)(struct ty_motor_dev *ty_motor_dev, void *data);
	int (*ty_motor_sel)(struct ty_motor_dev *ty_motor_dev, BYTE port);							//针对互斥电机，选择电机的片选。port = pan or tilt，0：垂直；1: 水平；其他值：不支持
}ty_motor_dev_t;

#endif


