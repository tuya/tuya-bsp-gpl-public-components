#ifndef __TY_GPIO_H__
#define __TY_GPIO_H__

#include <linux/gpio.h>
#include <ty_typedefs.h>

#define OUTPUT_PIN			0
#define INPUT_PIN			1

#define LOW_LEVEL			0
#define HIGH_LEVEL			1

typedef struct ty_used_gpio {
	unsigned 	gpio;	
	const char 	*pin_name;
	char		direction;
	int			init_val;
}ty_used_gpio_t;

INT ty_used_gpio_init(ty_used_gpio_t *used_pins, BYTE pin_num);
INT ty_used_gpio_deinit(ty_used_gpio_t *used_pins, BYTE pin_num);

#endif