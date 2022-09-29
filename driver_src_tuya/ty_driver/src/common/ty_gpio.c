
#include <ty_gpio.h>


INT ty_used_gpio_init(ty_used_gpio_t *used_pins, BYTE pin_num)
{
	BYTE 	i 	= 0;
    INT 	ret	= 0;

	for (i = 0; i < pin_num; i++) {
		if (-1 == used_pins->gpio){		//不使用
			used_pins++;
			continue;
		}
		ret |= gpio_request(used_pins->gpio, used_pins->pin_name);

		if (INPUT_PIN == used_pins->direction) {		//input
			gpio_direction_input(used_pins->gpio);
		}else {
			gpio_direction_output(used_pins->gpio, used_pins->init_val);
		}
		used_pins++;
	}

    return ret;	
}

INT ty_used_gpio_deinit(ty_used_gpio_t *used_pins, BYTE pin_num)
{
	BYTE 	i 	= 0;

	for (i = 0; i < pin_num; i++) {
		if (-1 == used_pins->gpio){		//不使用
			used_pins++;
			continue;
		}		
		gpio_free(used_pins->gpio);
		used_pins++;
	}

    return 0;
}
