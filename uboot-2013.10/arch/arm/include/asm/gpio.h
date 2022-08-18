#include <asm-generic/gpio.h>
#ifdef CONFIG_H3_B_CODE
#include <asm/arch-ak39ev33x/ak_gpio.h>
#else
#ifdef CONFIG_37_E_CODE
#include <asm/arch-ak37e/ak_gpio.h>
#else
#include <asm/arch-ak37d/ak_gpio.h>
#endif
#endif


