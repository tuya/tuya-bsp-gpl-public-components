#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "../../include/usr/ty_motor_usr.h"

int flag		= 0;
void signal_handle(int sig)
{
    printf("################## get signal: %d\n", sig);

    switch(sig) {
    case SIGINT:
    case SIGKILL:
    case SIGTERM:
        flag = 1;
        break;

    default:
        break;
    }

    return;
}

void ptz_func_example(void)
{
	int ret			= 0;
	int fd 			= -1;
	int func_num 	= 0;
	struct ty_motor_param_s param;
	struct ty_motor_param_g gparam;

	memset(&gparam, 0x00, sizeof(struct ty_motor_param_g));
	memset(&param, 0x00, sizeof(struct ty_motor_param_s));
	
	fd = open("/dev/tymotor", O_RDWR);
	if (fd < 0) {
		printf("open error.\n");
		return;
	}
	while (!flag) {
		printf("please enter the function number\n");
		printf("0  go up\n");
		printf("1  go down\n");
		printf("2  tilt self check\n");
		printf("3  tilt stop\n\n");
		printf("4  go left\n");
		printf("5  go right\n");
		printf("6  pan_self check\n");		
		printf("7  pan stop\n\n");
		printf("8  go left_up\n");
        printf("9  go left_down\n");
		printf("10 go right_up\n");
        printf("11 go right_down\n");
		printf("12 go specific pos\n");
		printf("13 stop both\n\n");
        printf("14 set pan&tilt_speed\n");
		printf("15 get pan&tilt pos info\n");	
		printf("16 exit\n");

		ret = scanf("%d", &func_num);
		if (ret != 1)
			printf("scanf erro %d\n", ret);
		getchar();

		param.motor_func = func_num;
		switch(func_num) {
			case 0:
				param.motor_func = GO_UP;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 1:
				param.motor_func = GO_DOWN;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 2:
				param.motor_func = TILT_SELF_CHECK;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 3:
				param.motor_func = TILT_STOP;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 4:
				param.motor_func = GO_LEFT;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 5:
				param.motor_func = GO_RIGHT;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 6:
				param.motor_func = PAN_SELF_CHECK;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 7:
				param.motor_func = PAN_STOP;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 8:
				param.motor_func = LEFT_UP;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 9:
				param.motor_func = LEFT_DOWN;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 10:
				param.motor_func = RIGHT_UP;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 11:
				param.motor_func = RIGHT_DOWN;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 12:
				printf("please input the pan_pos:");
				ret = scanf("%d", &param.goal_pan_pos);
				if (ret != 1)
					printf("scanf erro %d\n", ret);
				getchar();
				printf("please input the tilt_pos:");
				ret = scanf("%d", &param.goal_tilt_pos);
				if (ret != 1)
					printf("scanf erro %d\n", ret);
				getchar();
				param.motor_func = GO_POS;
				ioctl(fd, TY_MOTOR_IOCTL_SET, &param);
				break;
			case 13:
				ioctl(fd, TY_MOTOR_IOCTL_STOP, NULL);
				break;
			case 14:
				printf("please input the pan_speed:");
				ret = scanf("%d", &param.goal_pan_speed);
				if (ret != 1)
					printf("scanf pan speed erro %d\n", ret);
				getchar();
				printf("now pan speed is %d\n", param.goal_pan_speed);
				printf("please input the tilt_speed:");
				ret = scanf("%d", &param.goal_tilt_speed);
				if (ret != 1)
					printf("scanf tilt speed erro %d\n", ret);
				getchar();
				printf("now tilt speed is %d\n", param.goal_tilt_speed);
				break;
			case 15:
				gparam.state_cmd= GET_PAN_POS;
				ioctl(fd, TY_MOTOR_IOCTL_GET, &gparam);
				printf("pan pos is %d \n",gparam.state_value);

				gparam.state_cmd= GET_TILT_POS;
				ioctl(fd, TY_MOTOR_IOCTL_GET, &gparam);
				printf("tilt pos is %d \n",gparam.state_value);
				break;				
			case 16:
			default:
				flag = 1;
				break;
		}
	}

	close(fd);
	
	return;
}


int main(int argc, char** argv)
{
	int *p = NULL;
	
	signal(SIGINT, signal_handle);

    printf("begin___\n");

	ptz_func_example();

    printf("end___\n");

	return 0;
}






