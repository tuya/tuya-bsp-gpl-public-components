#ifndef __TY_MOTOR_USR_H__
#define __TY_MOTOR_USR_H__

#define MOTOR_MAGIC  12

typedef enum
{
    GO_UP,
    RIGHT_UP,
    GO_RIGHT,
    RIGHT_DOWN,
    GO_DOWN,
    LEFT_DOWN,
    GO_LEFT = 6,
    LEFT_UP,

    PAN_STOP,
    PAN_SELF_CHECK,
    GO_POS,
    TILT_STOP,
    TILT_SELF_CHECK,
    NONE
}PTZ_FUNC;

struct ty_motor_param_s {
    PTZ_FUNC 	motor_func;
	int		 	goal_pan_speed;
	int			goal_tilt_speed;
    int      	goal_pan_pos;
    int      	goal_tilt_pos;
};

struct ty_motor_param_init_s {			/*后续增加成员使用ty_motor_param_init_s_else结构体，避免在此结构体添加，会引起兼容性问题*/
	int			_pan_roll_step;
	int		 	_pan_step_max;
	int			_tilt_step_max;
    int      	_infi_rot_en;
};

struct ty_motor_param_init_s_else {
    int 		_pls_pan_step;
    int 		_pls_tilt_step;
	int			reserved[8];			/*供后续扩展用*/
};

/*current value cmd*/
typedef enum 
{
	GET_PAN_POS,
	GET_TILT_POS,
	GET_PAN_ANGLE,
	GET_TILT_ANGLE,
	GET_PAN_CHECK_STATE,
	GET_TILT_CHECK_STATE,
	GET_PRESET_STATE,
	GET_LEFT_OPTO_FLAG,
	GET_RIGHT_OPTO_FLAG,
	SET_CLEAR_OPTO_FLAG,
}PTZ_STATE_CMD;

struct ty_motor_param_g {
	PTZ_STATE_CMD state_cmd;
	int           state_value;
};

#define TY_MOTOR_IOCTL_STOP     		_IO(MOTOR_MAGIC, 0)
#define TY_MOTOR_IOCTL_GET      		_IOR(MOTOR_MAGIC, 1, struct ty_motor_param_s)
#define TY_MOTOR_IOCTL_SET      		_IOW(MOTOR_MAGIC, 2, struct ty_motor_param_s)
#define TY_MOTOR_IOCTL_PARA_INIT_SET    _IOW(MOTOR_MAGIC, 3, struct ty_motor_param_init_s)
#define TY_MOTOR_IOCTL_PARA_INIT_SET2   _IOW(MOTOR_MAGIC, 4, struct ty_motor_param_init_s_else)

#endif

