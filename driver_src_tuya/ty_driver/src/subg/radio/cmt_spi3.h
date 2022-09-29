#ifndef __CMT_SPI3_H
#define __CMT_SPI3_H

#include <linux/delay.h>
#include <ty_typedefs.h>

extern void ty_set_gpio1_in(void);
extern void ty_set_gpio2_in(void);
extern void ty_set_gpio3_in(void);
extern int ty_read_gpio1_pin(void);
extern int ty_read_gpio2_pin(void);
extern int ty_read_gpio3_pin(void);
extern void ty_set_csb_out(void);
extern void ty_set_fcsb_out(void);
extern void ty_set_sclk_out(void);
extern void ty_set_sdio_out(void);
extern void ty_set_sdio_in(void);
extern void ty_set_csb_h(void);
extern void ty_set_csb_l(void);
extern void ty_set_fcsb_h(void);
extern void ty_set_fcsb_l(void);
extern void ty_set_sclk_h(void);
extern void ty_set_sclk_l(void);
extern void ty_set_sdio_h(void);
extern void ty_set_sdio_l(void);
extern int ty_read_sdio_pin(void);
/* ************************************************************************
*  The following need to be modified by user
*  ************************************************************************ */
#define t_nSysTickCount 	1 //同步时钟，用户可以不用定义

#define CMT2300A_SetGpio1In()           ty_set_gpio1_in()
#define CMT2300A_SetGpio2In()           ty_set_gpio2_in()
#define CMT2300A_SetGpio3In()           ty_set_gpio3_in()
#define CMT2300A_ReadGpio1()            ty_read_gpio1_pin()
#define CMT2300A_ReadGpio2()            ty_read_gpio2_pin()
#define CMT2300A_ReadGpio3()            ty_read_gpio3_pin()
#define CMT2300A_DelayMs(ms)            msleep(ms)
#define CMT2300A_DelayUs(us)            udelay(us)
#define g_nSysTickCount 				t_nSysTickCount

#define cmt_spi3_csb_out()      		ty_set_csb_out()
#define cmt_spi3_fcsb_out()     		ty_set_fcsb_out()
#define cmt_spi3_sclk_out()     		ty_set_sclk_out()
#define cmt_spi3_sdio_out()     		ty_set_sdio_out()
#define cmt_spi3_sdio_in()      		ty_set_sdio_in()

#define cmt_spi3_csb_1()        		ty_set_csb_h()
#define cmt_spi3_csb_0()        		ty_set_csb_l()

#define cmt_spi3_fcsb_1()       		ty_set_fcsb_h()
#define cmt_spi3_fcsb_0()       		ty_set_fcsb_l()
		    
#define cmt_spi3_sclk_1()       		ty_set_sclk_h()
#define cmt_spi3_sclk_0()       		ty_set_sclk_l()

#define cmt_spi3_sdio_1()       		ty_set_sdio_h()
#define cmt_spi3_sdio_0()       		ty_set_sdio_l()
#define cmt_spi3_sdio_read()    		ty_read_sdio_pin()

/* ************************************************************************ */
__inline void cmt_spi3_delay(void);
__inline void cmt_spi3_delay_us(void);

void cmt_spi3_init(void);

void cmt_spi3_send(BYTE data8);
BYTE cmt_spi3_recv(void);

void cmt_spi3_write(BYTE addr, BYTE dat);
void cmt_spi3_read(BYTE addr, BYTE* p_dat);

void cmt_spi3_write_fifo(const BYTE* p_buf, WORD len);
void cmt_spi3_read_fifo(BYTE* p_buf, WORD len);

#endif
