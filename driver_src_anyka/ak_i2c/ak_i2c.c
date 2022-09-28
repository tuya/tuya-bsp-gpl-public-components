/*
  * AKxx I2C Controller 
  * /drivers/i2c/busses/ak_i2c.c
  *
  * Copyright (C) 2018 Anyka(Guangzhou) Microelectronics Technology Co., Ltd.
  *
  * Author: Feilong Dong <dong_feilong@anyka.com>
            Zhipeng Zhang <zhang_zhipeng@anyka.com>
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/semaphore.h>
#include <linux/proc_fs.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <mach/irqs.h>
#include <mach/map.h>

#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>
#include <linux/gpio.h>

/* poll mode  and  interrupt mode  define. */
//#define AK_I2C_INTERRUPT_MODE
#define AK_I2C_POLL_MODE

#define AK_I2C_NACKEN		(1 << 17)

#define AK_I2C_READ		1
#define AK_I2C_WRITE		0

/* control and data register description*/
#define AK_I2C_CTRL		0x00
#if defined(CONFIG_MACH_AK37D)
#define AK_I2C_CMD1		    0x10
#define AK_I2C_CMD2		    0x14
#define AK_I2C_CMD3		    0x18
#define AK_I2C_CMD4		    0x1C
#define AK_I2C_DATA0		0x20
#define AK_I2C_DATA1		0x24
#define AK_I2C_DATA2		0x28
#define AK_I2C_DATA3		0x2C

/* control bit */
#define INT_PEND_FLAG		(1 << 4) /*SundanceH3D platform,Control register bit */
#define AK_I2C_INTEN		(1 << 5)
#define AK_I2C_TXDIV_512	(1 << 6)
#define AK_I2C_CLR_DELAY	(0x3 << 7)
#define AK_I2C_SDA_DELAY	(0x2 << 7)
#define AK_I2C_TRX_BYTE     (9)
#define AK_I2C_TXRXSEL	    (1 << 13)
#define AK_I2C_START		(1 << 14)
#define AK_I2C_ACKEN		(1 << 15)
#define AK_I2C_NOSTOP		(1 << 16)
#define AK_TX_CLK_DIV		(0xf)

/* command bit */
#define	AK_I2C_CMD_EN			(1 << 18)
#define AK_I2C_START_BIT	    (1 << 17)

#elif defined(CONFIG_MACH_AK39EV330)
#define AK_I2C_DELAY	    0x04
#define AK_I2C_CMD1	        0x08
#define AK_I2C_CMD2	        0x0c
#define AK_I2C_DATA0		0x10
#define AK_I2C_DATA1		0x14
#define AK_I2C_DATA2		0x18
#define AK_I2C_DATA3		0x1C

/* control bit */
#define INT_PEND_FLAG		(1 << 13)/*SundanceH3-B platform,Control register bit */
#define AK_I2C_INTEN		(1 << 14)
#define AK_I2C_CLR_DELAY	(0x3 << 15)
#define AK_I2C_SDA_DELAY	(0x2 << 15)
#define AK_I2C_TRX_BYTE	    (17)
#define AK_I2C_TXRXSEL	    (1 << 22)
#define AK_I2C_START		(1 << 23)
#define AK_I2C_ACKEN		(1 << 24)
#define AK_I2C_NOSTOP		(1 << 25)
#define AK_I2C_LASTACK		(1 << 26)
#define AK_I2C_ACK			(1 << 27)

/* command bit */
#define	AK_I2C_CMD_EN			(1 << 9)
#define AK_I2C_START_BIT	    (1 << 8)

#elif defined(CONFIG_MACH_AK37E)
#define AK_I2C_SLAVE_CTRL          (0x04)
#define AK_I2C_DELAY               (0x08)
#define AK_I2C_STATUS              (0x0c)
#define AK_I2C_INTR_ENBLE          (0x10)
#define AK_I2C_M_CMD1_2            (0x14)
#define AK_I2C_CMD1                 AK_I2C_M_CMD1_2
#define AK_I2C_S_CMD1_2            (0x14)
#define AK_I2C_M_CMD3_4            (0x18)
#define AK_I2C_S_CMD3_4            (0x18)
#define AK_I2C_SLAVE_TX1           (0x1C)
#define AK_I2C_SLAVE_TX2           (0x20)
#define AK_I2C_DATA0               (0x24)
#define AK_I2C_DATA1               (0x28)
#define AK_I2C_DATA2               (0x2C)
#define AK_I2C_DATA3               (0x30)

#define INT_PEND_FLAG 	(1 << 13)/*Note:Sundance4 platform,status register bit */
#define AK_I2C_ACK			(1 << 14)

/* control bit */
#define AK_I2C_INTEN		(1 << 1)
#define AK_I2C_CLR_DELAY	(0x3 << 13)
#define AK_I2C_SDA_DELAY	(0x2 << 13)
#define AK_I2C_TRX_BYTE	    (15)
#define AK_I2C_TXRXSEL	    (1 << 20)
#define AK_I2C_START		(1 << 21)
#define AK_I2C_ACKEN		(1 << 22)
#define AK_I2C_NOSTOP		(1 << 23)
#define AK_I2C_LASTACK		(1 << 24)

/* command bit */
#define	AK_I2C_CMD_EN			(1 << 9)
#define AK_I2C_START_BIT	    (1 << 8)
#endif

#define IIC_MAX_WR_BYTE 16

/* i2c controller state */
enum ak_i2c_state {
	STATE_READ,
	STATE_WRITE
};

enum read_data_addr {
	READ_ADDR,
	READ_DATA
};

struct ak_i2c_dev {
	struct device *dev;
	void __iomem *regs;
	struct clk *clk;
    /*distinguish difference i2c controller. */
	int		id;
	unsigned int irq;
	struct i2c_adapter adap;
	struct proc_dir_entry* i2c_proc;
	struct completion completion;
	
	spinlock_t lock;

	struct i2c_msg *msg;
	unsigned int msg_num;
	unsigned int msg_idx;
	unsigned int msg_ptr;

	enum ak_i2c_state	state;
	enum read_data_addr	read_value;

	u32 bus_clk_rate;
	u32 sda_delay;
	/*
	 * The interval between the end of data transmitting/receiving and 
	 * the start of next data transmitting/receiving
	 * T = (interval+1)*Ttwi_sclk/2
	 * here, let interval_scale = interval_reg + 1;
	 * 1 <= interval_scale <=4096
	 */
	int interval_scale; 
};

static struct ak_i2c_dev *i2c0_dev_global;
static struct ak_i2c_dev *i2c1_dev_global;
static struct ak_i2c_dev *i2c2_dev_global;

static struct semaphore xfer_sem;
static void debug_dump(struct ak_i2c_dev *i2c_dev)
{
	pr_info("dump AK_I2C_DATA\n");
	pr_info("%x %x %x %x\n",
		__raw_readl(i2c_dev->regs + AK_I2C_DATA0),
		__raw_readl(i2c_dev->regs + AK_I2C_DATA1),
		__raw_readl(i2c_dev->regs + AK_I2C_DATA2),
		__raw_readl(i2c_dev->regs + AK_I2C_DATA3));

	return;
}
/* ~~~~~~~~~~~ poll mode ~~~~~~~~~~~~~~~~~~~~~~~~~ */

#ifdef AK_I2C_POLL_MODE
#define MAX_POLL_TIMES	200000
/*
 * Poll the i2c status register until the specified bit is set.
 * Return 1 if transfer finished.
 */
static int ak_poll_status(struct ak_i2c_dev *i2c_dev)
{
    unsigned long reg;
	volatile long poll_times = MAX_POLL_TIMES;
	unsigned int reg_off;
	#if defined(CONFIG_MACH_AK37E)
		reg_off  = AK_I2C_STATUS;
	#elif defined(CONFIG_MACH_AK39EV330) || defined(CONFIG_MACH_AK37D)
		reg_off  = AK_I2C_CTRL;
		#endif
	do {
		reg = __raw_readl(i2c_dev->regs + reg_off);
	} while (!(reg & INT_PEND_FLAG) && poll_times--);

	if(poll_times ==  -1)
		pr_info("%s timeout\n",__func__);

	/* 
	 * clear INT_PEND_FLAG
	 *  (1) 
	 *		for 37E: Status Register of TWI (Address: 0x000C) bit[13]
	 *		trans_finish_int
	 *		Interrupt request due to the completion of data transfer in either master mode or slave mode: a read access returns the interrupt status
	 *		1: Interrupt triggered
	 *		0: No interrupt
	 *	
	 *		write 1 to clear
	 *  (2)
	 *		for 37d & 330:Configuration Register of TWI (Address: 0x0000) bit[4]
	 *		int_pend_flag
	 *		This bit being read as 1 indicates that the transmit/ receive operation is finished. 
	 *		To start another operation, this bit should be cleared as 0.
	 *		
	 *		write 0 to clear. SEE wait_xfer_write and _xfer_read
	 * 		
	 */
	#if defined(CONFIG_MACH_AK37E)
    __raw_writel(INT_PEND_FLAG,i2c_dev->regs + reg_off); 
	#endif
    
	return 1;
}
/* _xfer_read operate maxbyte is 16 Bytes*/
static int _xfer_read(struct ak_i2c_dev *i2c_dev, unsigned char *buf, int length, int is_last_data)
{
	int i,j, ctrl_value;
	unsigned long ret;
	unsigned long reg_value;
	unsigned char *p = buf;
	int idx = 0;
	int read_need_poll = 0;

	pr_debug("enter %s, length=%d,is_last_data=%d\n",__func__, length,is_last_data);

	if(length > IIC_MAX_WR_BYTE)
		length = IIC_MAX_WR_BYTE;

	ret = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
	ret |= (AK_I2C_START | AK_I2C_ACKEN | AK_I2C_TXRXSEL); 

#ifdef CONFIG_MACH_AK37D
	ret &= ~((0xf) << AK_I2C_TRX_BYTE);
	ret |= ((length - 1) << AK_I2C_TRX_BYTE);
#endif
#if defined(CONFIG_MACH_AK39EV330) || defined(CONFIG_MACH_AK37E)
	ret &= ~((0x1f) << AK_I2C_TRX_BYTE);
	ret |= ((length)<< AK_I2C_TRX_BYTE);

	ret |= AK_I2C_LASTACK;// lastack
	if(is_last_data ==0){
		ret |= (AK_I2C_NOSTOP);  	//nostop=1;
		ret &= ~(AK_I2C_LASTACK);  	//lastack=0;
	}
	else{
		ret &=~(AK_I2C_NOSTOP);  	//nostop=0;
		ret |= (AK_I2C_LASTACK);  	//lastack=1;
	}
#endif
#if defined(CONFIG_MACH_AK39EV330) || defined(CONFIG_MACH_AK37D)
	ret &= ~(INT_PEND_FLAG);
#endif
	__raw_writel(ret, i2c_dev->regs + AK_I2C_CTRL);
	read_need_poll = 1;

	for (i = 0; i < length / 16; i++) {
		if (read_need_poll && !ak_poll_status(i2c_dev)) {
			read_need_poll = 0;
			dev_err(&i2c_dev->adap.dev, "RXRDY timeout\n");
			return -ETIMEDOUT;
		}
		read_need_poll = 0;
		for (j = 0; j < 4; j++) {
			reg_value = __raw_readl(i2c_dev->regs + AK_I2C_DATA0 + j * 4);			
			*(unsigned long *)p = reg_value;
			p +=4;
            			
		}
	}

	idx = 0;
	for (; idx < (length % 16) / 4; idx++) {
    	if (read_need_poll && !ak_poll_status(i2c_dev)) {
			read_need_poll = 0;
		    dev_err(&i2c_dev->adap.dev, "RXRDY timeout\n");
		    return -ETIMEDOUT;
	    }
        read_need_poll = 0;
		reg_value = __raw_readl(i2c_dev->regs + AK_I2C_DATA0 + idx * 4);
		*(unsigned long *)p = reg_value;
		p +=4;
	}

	if (length % 4) {
		if (read_need_poll && !ak_poll_status(i2c_dev)) {
			read_need_poll = 0;
			dev_err(&i2c_dev->adap.dev, "RXRDY timeout\n");
			return -ETIMEDOUT;
		}
		read_need_poll = 0;
		reg_value = __raw_readl(i2c_dev->regs + AK_I2C_DATA0 + idx *4);
	
		
		for (i = 0; i < length % 4; i++) {
			*p++ = (reg_value >> (i * 8)) & 0xFF;
		}
	}
	/*
	 * read operation start with : start_condition ---> write i2c slave addr --> read ....
	 * so, the ack bit store the ack/nack status of "write ii2 slave addr" operation
	 */
#if defined(CONFIG_MACH_AK39EV330) || defined(CONFIG_MACH_AK37E)
#ifdef CONFIG_MACH_AK39EV330
	ret = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
#endif
#ifdef CONFIG_MACH_AK37E
	ret = __raw_readl(i2c_dev->regs + AK_I2C_STATUS);
#endif
    if( ret & AK_I2C_ACK )
	{
		pr_info("%s: Write i2c addr,NACK\n",__func__);
		return -ETIMEDOUT;
	}
#endif
	return 0;
}


static int xfer_read(struct ak_i2c_dev *i2c_dev, unsigned char *buf, int length)
{
	int ret;
	unsigned int r_num;
	unsigned int frac;
	unsigned int index;
	unsigned char *p = buf;
	
#ifdef CONFIG_MACH_AK37D
	if(length > IIC_MAX_WR_BYTE)
		length = IIC_MAX_WR_BYTE;
#endif
	pr_debug("enter %s, length=%d\n",__func__, length);
	r_num = length / IIC_MAX_WR_BYTE;
	frac = length % IIC_MAX_WR_BYTE;
	
	for(index = 0; index < r_num; index++){
		ret = _xfer_read(i2c_dev,p,IIC_MAX_WR_BYTE,0);
		p += IIC_MAX_WR_BYTE;
	}
	if(frac){
		ret = _xfer_read(i2c_dev, p, frac, 1);
		p += frac;
	}
	return ret;
}


static int wait_xfer_write(struct ak_i2c_dev *i2c_dev, int length,int is_last_data)
{
	unsigned long ret;   
	ret = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
	ret |= (AK_I2C_START);
#if defined(CONFIG_MACH_AK39EV330)|| defined(CONFIG_MACH_AK37D)
	ret &= ~(AK_I2C_TXRXSEL | INT_PEND_FLAG);
#endif
#ifdef CONFIG_MACH_AK37D
	ret &= ~((0xf) << AK_I2C_TRX_BYTE);
	ret |= ((length - 1)<< AK_I2C_TRX_BYTE);
#endif
#if defined(CONFIG_MACH_AK39EV330)|| defined(CONFIG_MACH_AK37E)
	ret &= ~((0x1f) << AK_I2C_TRX_BYTE);
	ret |= ((length)<< AK_I2C_TRX_BYTE);
	if(is_last_data == 0){
		ret |= (AK_I2C_NOSTOP);  	//nostop=1;
	}
	else{
		ret &=~(AK_I2C_NOSTOP);  	//nostop=0;
	}
#endif
#ifdef CONFIG_MACH_AK37E
    ret &= ~(AK_I2C_TXRXSEL);
#endif
	__raw_writel(ret, i2c_dev->regs + AK_I2C_CTRL);
	
	if (!ak_poll_status(i2c_dev)) {
		dev_info(&i2c_dev->adap.dev, "TXRDY timeout\n");
		return -ETIMEDOUT;
	}
    
#ifdef CONFIG_MACH_AK37E
	ret = __raw_readl(i2c_dev->regs + AK_I2C_STATUS);
    if( ret & AK_I2C_ACK )
	{	
		/* 
	     * write 1 to clear nack.
	     * bit[14]	ack bit
		 * 1 = NACK is responded by the slave device in the process of data transfer when master-mode I2C controller sends data to the bus
		 * 0 = no NACK is responded by the slave devices when I2C controller operates in master-transmitter mode
		 * 
		 * The operation "__raw_writel(ret,i2c_dev->regs + AK_I2C_STATUS);" 
		 * may also clear bit [13] trans_finish_int and bit[0] time_out_int:
	     */
		__raw_writel(ret,i2c_dev->regs + AK_I2C_STATUS);
		pr_info("%s: NACK\n",__func__);
		return -ECONNREFUSED;
	}
#endif
#ifdef CONFIG_MACH_AK39EV330  
	ret = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
    if( ret & AK_I2C_ACK )
	{
		/*	bit[27]
		 *	the ACK bit value received from slave.
		 *  Note：this bit cleared when start another operation (write 1 to bit[23]).
		 *	so, don't need to clear it here.
		 */
		pr_info("%s: NACK\n",__func__);
		return -ECONNREFUSED;
	}
#endif
	return 0;
}

static int xfer_write(struct ak_i2c_dev *i2c_dev, unsigned char *buf, int length)
{	
	int i;
	int ret;
	int w_num, frac;
	unsigned long reg_value;
	unsigned char *p = buf;

#ifdef CONFIG_MACH_AK37D
	if(length > IIC_MAX_WR_BYTE)
		length = IIC_MAX_WR_BYTE;
#endif

	pr_debug("enter %s, length=%d\n",__func__, length);
	w_num = length / IIC_MAX_WR_BYTE;
	frac = length % IIC_MAX_WR_BYTE;

	while(w_num--) {
		for(i = 0; i < 4; i++) {			
			reg_value = *(unsigned long *)p;

			p += 4;
			__raw_writel(reg_value, i2c_dev->regs + AK_I2C_DATA0 + i * 4);
		}
		if(frac > 0) 
			ret = wait_xfer_write(i2c_dev, IIC_MAX_WR_BYTE,0);
		else
			ret = wait_xfer_write(i2c_dev, IIC_MAX_WR_BYTE,1);
	}
    
    if(frac > 0) {
        unsigned long value = 0;
        for(i = 0; i < frac; i+=4) {
			value = *(unsigned long *)p;
			p +=4;
			__raw_writel(value, i2c_dev->regs + AK_I2C_DATA0 + i);
		}
		ret = wait_xfer_write(i2c_dev, frac,1);
	}
	
	return ret;
}

static int ak_i2c_doxfer(struct ak_i2c_dev *i2c_dev, struct i2c_msg *msgs, int num)
{
	unsigned int addr = (msgs->addr & 0x7f) << 1;
	int i, ret, rw_flag;

	for (i = 0; i < num; i++) {
		if (msgs->flags & I2C_M_RD) {
			addr |= AK_I2C_READ;
			rw_flag = 1;
		} else {
			rw_flag = 0;
		}
		if (msgs->flags & I2C_M_REV_DIR_ADDR)
			addr ^= 1;

		__raw_writel((AK_I2C_CMD_EN | AK_I2C_START_BIT)|addr, i2c_dev->regs + AK_I2C_CMD1);

		if(msgs->len && msgs->buf) {
			if (rw_flag == 1)
				ret = xfer_read(i2c_dev, msgs->buf, msgs->len);
			else
				ret = xfer_write(i2c_dev, msgs->buf, msgs->len);
			
			if (ret)
				return ret;
		}
		pr_debug( "transfer complete\n");
		msgs++;		/* next message */
	}
	return i;
}
#endif

/* ~~~~~~~~~~~ INTER MODE ~~~~~~~~~~~~~~~~~~~ */

#ifdef AK_I2C_INTERRUPT_MODE 

static void clear_int_flag(void)
{
	unsigned long tmp;

	tmp = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
	tmp &= ~INT_PEND_FLAG;
	__raw_writel(tmp, i2c_dev->regs + AK_I2C_CTRL);
}

/* irq disable functions */
static void ak_i2c_disable_irq(struct ak_i2c_dev *i2c_dev)
{
	unsigned long tmp;
	
	tmp = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
	__raw_writel(tmp & ~AK_I2C_INTEN, i2c_dev->regs + AK_I2C_CTRL);
}

static inline void ak_i2c_master_complete(struct ak_i2c_dev *i2c_dev, int ret)
{
	dev_dbg(i2c_dev->dev, "master_complete %d\n", ret);

	i2c_dev->msg_ptr = 0;
	i2c_dev->msg = NULL;
	i2c_dev->msg_num = 0;
	if (ret)
		i2c_dev->msg_idx = ret;
	
	wake_up(&i2c_dev->wait);
}

static inline void ak_i2c_stop(struct ak_i2c_dev *i2c_dev, int ret)
{
	dev_dbg(i2c_dev->dev, "STOP\n");

	ak_i2c_master_complete(i2c, ret);
	ak_i2c_disable_irq(i2c);
}

static int ak_i2c_irq_transfer(struct ak_i2c_dev *i2c_dev)
{
	unsigned int addr = (i2c_dev->msg->addr & 0x7f)<< 1 ;
	unsigned int num_char, i, length;
	unsigned long stat, regval = 0;
	unsigned char *p = i2c_dev->msg->buf;

read_next:
	if(i2c_dev->msg_idx < i2c_dev->msg_num) {
		if (i2c_dev->msg->len == 0) {
			ak_i2c_stop(i2c_dev, 0);
			return 0;
		}
		
		stat = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
		stat &= ~(0xf << 9);
		if (i2c_dev->msg->flags & I2C_M_RD) {
			addr |= AK_I2C_READ ;
			i2c_dev->state = STATE_READ ;
			stat |= AK_I2C_TXRXSEL ;
		} 
		else {
			i2c_dev->state = STATE_WRITE ;
			stat &= ~AK_I2C_TXRXSEL ;
		}
	
		if (i2c_dev->msg->flags & I2C_M_REV_DIR_ADDR)
			addr ^= 1;
		
		__raw_writel((AK_I2C_CMD_EN | AK_I2C_START_BIT)|addr, i2c_dev->regs + AK_I2C_CMD1);
		
		switch(i2c_dev->state) {
			case STATE_WRITE:			
				if (i2c_dev->msg->len > 16) {
						printk("Error, needed debug more data transmitted.\n");
						return 0;
				}
				if (i2c_dev->msg_ptr < i2c_dev->msg->len) {
					
					num_char = i2c_dev->msg->len % 16;
					
					if (num_char > 0) {
						__raw_writel(stat | ((num_char - 1) << 9), AK_I2C_CTRL);
						for(i = 0; i < num_char; i+=4) {
							regval = *(unsigned long *)p;
							p += 4;
							__raw_writel(regval, AK_I2C_DATA0 + i);
						}
						i2c_dev->msg_ptr += num_char;			
					}
					
					if(i2c_dev->msg_ptr >= i2c_dev->msg->len){
						i2c_dev->msg_ptr = 0;
						i2c_dev->msg_idx++;
						i2c_dev->msg++;
					}
					
					stat = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
					__raw_writel(stat | AK_I2C_START, i2c_dev->regs + AK_I2C_CTRL);				
				}
				break;

			case STATE_READ:
				length = i2c_dev->msg->len;
				
				if (i2c_dev->msg->len > 16) {
					printk("Error, needed debug more data transmitted.\n");
					return 0;
				}
				__raw_writel(stat | ((length - 1) << 9), i2c_dev->regs + AK_I2C_CTRL);
				
				if(i2c_dev->read_value == READ_ADDR) {
					stat = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
					__raw_writel(stat | AK_I2C_START, i2c_dev->regs + AK_I2C_CTRL);
					i2c_dev->read_value = READ_DATA;
				} else {
					if (i2c_dev->msg_ptr < i2c_dev->msg->len) {

						num_char = i2c_dev->msg->len % 16;

						if (num_char > 0) {
							for (i = 0; i < num_char; i+=4) {
								regval = __raw_readl(i2c_dev->regs + AK_I2C_DATA0 + i);
								*(unsigned long *)p = regval;
								p += 4;
							}
							i2c_dev->msg_ptr += num_char;
						}
						
						if (i2c_dev->msg_ptr >= i2c_dev->msg->len) {
							/* we need to go to the next i2c message */
							dev_dbg(i2c_dev->dev, "READ: Next Message\n");
											
							i2c_dev->msg_ptr = 0;
							i2c_dev->msg_idx++;
							i2c_dev->msg++;
							
							if(i2c_dev->msg_idx >= i2c_dev->msg_num) {
								ak_i2c_stop(i2c, 0);
								return 0;
							}
						}
						i2c_dev->read_value = READ_ADDR;
						goto read_next;
					}							
				}
				break;
		}	
	} 
	else {
		ak_i2c_stop(i2c_dev, 0);
	}
	return 0;
}

/* ak_i2c_irq
 *
 * top level IRQ servicing routine
*/
static irqreturn_t ak_i2c_irq(int irqno, void *dev_id)
{
	struct ak_i2c_dev *i2c_dev = dev_id;

	clear_int_flag();

	/* pretty much this leaves us with the fact that we've
	 * transmitted or received whatever byte we last sent */
	
	ak_i2c_irq_transfer(i2c_dev);

	return IRQ_HANDLED;
}

static int ak_i2c_doxfer(struct ak_i2c_dev *i2c_dev, struct i2c_msg *msgs, int num)
{
	unsigned long timeout, stat;
	int ret;

	spin_lock_irq(&i2c_dev->lock);
	i2c_dev->msg     = msgs;
	i2c_dev->msg_num = num;
	i2c_dev->msg_ptr = 0;
	i2c_dev->msg_idx = 0;
	i2c_dev->read_value = READ_ADDR;

	ak_i2c_irq_transfer(i2c);
	stat = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
	stat |= AK_I2C_INTEN | AK_I2C_ACKEN;
	__raw_writel(stat, i2c_dev->regs + AK_I2C_CTRL);

	spin_unlock_irq(&i2c_dev->lock);
	timeout = wait_event_timeout(i2c_dev->wait, i2c_dev->msg_num == 0, HZ * 5);

	ret = i2c_dev->msg_idx;

	/* having these next two as dev_err() makes life very
	 * noisy when doing an i2cdetect */

	if (timeout == 0)
		dev_info(i2c_dev->dev, "timeout\n");
	else if (ret != num)
		dev_info(i2c_dev->dev, "incomplete xfer (%d)\n", ret);

	return ret;
}
#endif


/* ak_i2c_xfer
 *
 * first port of call from the i2c bus code when an message needs
 * transferring across the i2c bus.
*/
static int ak_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)
{
	struct ak_i2c_dev *i2c_dev = (struct ak_i2c_dev *)adap->algo_data;
	int retry;
	int ret;

	down(&xfer_sem);
	for (retry = 0; retry < adap->retries; retry++) {
	
		ret = ak_i2c_doxfer(i2c_dev, msgs, num);
	
		if (ret != -EAGAIN)
		{
			up(&xfer_sem);
			return ret;
		}

		dev_dbg(i2c_dev->dev, "Retrying transmission (%d)\n", retry);

		udelay(100);
	}
	
	up(&xfer_sem);
	return -EREMOTEIO;
}

/* declare our i2c functionality */
static u32 ak_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_PROTOCOL_MANGLING;
}

/* i2c bus registration info */

static const struct i2c_algorithm ak_i2c_algorithm = {
	.master_xfer		= ak_i2c_xfer,
	.functionality		= ak_i2c_func,
};

/* ak_i2c_calcdivisor
 *
 * return the divisor settings for a given frequency
*/
static int ak_i2c_calcdivisor(unsigned long clkin, unsigned int wanted,
				   unsigned int *div1, unsigned int *divs)
{
	unsigned int calc_divs = clkin / wanted / 2;
	unsigned int calc_div1;

	if (calc_divs > (16*16))
		calc_div1 = 512;
	else
		calc_div1 = 16;

	calc_divs /= calc_div1;

	if (calc_divs == 0)
		calc_divs = 1;
	if (calc_divs > 16)
		calc_divs = 16;

	*divs = calc_divs;
	*div1 = calc_div1;

	return clkin / ((calc_divs * 2)* calc_div1);
}

/* ak_i2c_clockrate
 *
 * work out a divisor for the user requested frequency setting,
 * either by the requested frequency, or scanning the acceptable
 * range of frequencies until something is found
*/
static int ak_i2c_clockrate(struct ak_i2c_dev *i2c_dev, unsigned int *got)
{
	unsigned long clkin = clk_get_rate(i2c_dev->clk);
	unsigned int divs, div1;
	unsigned int target_frequency;
	u32 i2c_val, sda_delay,interval_reg;
	int freq;

	clkin /= 1000;		/* clkin now in KHz */
	
	dev_dbg(i2c_dev->dev, "pdata desired frequency %u\n", i2c_dev->bus_clk_rate);

	target_frequency = i2c_dev->bus_clk_rate;
	target_frequency /= 1000; /* Target frequency now in KHz */

#ifdef CONFIG_MACH_AK37D
	freq = ak_i2c_calcdivisor(clkin, target_frequency, &div1, &divs);
	if (freq > target_frequency) {
		dev_err(i2c_dev->dev,
			"Unable to achieve desired frequency %uKHz."	\
			" Lowest achievable %dKHz\n", target_frequency, freq);
	}
#endif

#if defined(CONFIG_MACH_AK39EV330) || defined(CONFIG_MACH_AK37E)
    divs = clkin / target_frequency / 2;
    freq = target_frequency;
#endif
	*got = freq;

	i2c_val = __raw_readl(i2c_dev->regs + AK_I2C_CTRL);
#ifdef CONFIG_MACH_AK37D
	i2c_val &= ~(AK_TX_CLK_DIV | AK_I2C_TXDIV_512);
	i2c_val |= (divs-1);

	if (div1 == 512)
		i2c_val |= AK_I2C_TXDIV_512;
#endif

#if defined(CONFIG_MACH_AK39EV330) || defined(CONFIG_MACH_AK37E)
    i2c_val &= ~0x1fff;
    i2c_val |= (divs-1);
#endif
	__raw_writel(i2c_val, i2c_dev->regs + AK_I2C_CTRL);

	if (i2c_dev->sda_delay) {
        /*clkin in KHz, i2c_dev->sda_delay in ns , actual_delay_time = 5 *sda_delay*(1/freq) */
		sda_delay = (i2c_dev->sda_delay * clkin) / 1000000;
		sda_delay = DIV_ROUND_UP(sda_delay, 5);
		if (sda_delay > 3)
			sda_delay = 3;		
	} else
		sda_delay = 3;

	#if defined(CONFIG_MACH_AK39EV330)
		i2c_val |= (sda_delay <<15);
	#elif defined(CONFIG_MACH_AK37D)
		i2c_val |= (sda_delay <<7);
	#elif defined(CONFIG_MACH_AK37E)
		i2c_val |= (sda_delay <<13);
	#endif
	pr_debug("regiter_bit sda_delay=%d, clkin=%lu, actual sda_delay=%lu\n",sda_delay, clkin,sda_delay*5*1000000/clkin);
	__raw_writel(i2c_val, i2c_dev->regs + AK_I2C_CTRL);

	/*
	 * The interval between the end of data transmitting/receiving and 
	 * the start of next data transmitting/receiving
	 * T = (interval_reg + 1)*Ttwi_sclk/2
	 * here, let interval = interval_reg + 1;
	 */
	#if defined(CONFIG_MACH_AK39EV330) || defined(CONFIG_MACH_AK37E)
		if((i2c_dev->interval_scale - 1) > 0){
			__raw_writel((i2c_dev->interval_scale - 1) & 0xfff, i2c_dev->regs + AK_I2C_DELAY);
		}
	#endif
	return 0;
}



/* ak_i2c_initialise
 *
 * initialise the controller, set the IO lines and frequency
*/
static int ak_i2c_initialise(struct ak_i2c_dev *i2c_dev)
{
	unsigned int freq;

#if defined(CONFIG_MACH_AK39EV330) || defined(CONFIG_MACH_AK37D)
	__raw_writel(AK_I2C_INTEN | AK_I2C_ACKEN, i2c_dev->regs + AK_I2C_CTRL);
#endif
#if defined(CONFIG_MACH_AK37E)
    __raw_writel(AK_I2C_ACKEN, i2c_dev->regs + AK_I2C_CTRL);
    __raw_writel(AK_I2C_INTEN, i2c_dev->regs + AK_I2C_INTR_ENBLE);
#endif
	/* we need to work out the divisors for the clock... */
	if (ak_i2c_clockrate(i2c_dev, &freq) != 0) {
		__raw_writel(0, AK_I2C_CTRL);
		dev_err(i2c_dev->dev, "cannot meet bus frequency required\n");
		return -EINVAL;
	}
	/* let i2c_dev->bus_clk_rate store the got rate*/
	i2c_dev->bus_clk_rate = freq;
	/* todo - check that the i2c lines aren't being dragged anywhere */
	dev_dbg(i2c_dev->dev, "bus frequency set to %d KHz\n", freq);

	return 0;
}

/*
 * /proc/i2c*
 */
static int i2c0_proc_show(struct seq_file *m, void *v)
{
	char buf;
	struct i2c_msg msg;
	int res, i, j;

	seq_printf(m,
			   "work-mode: host\n"
			   "clockrate: %dKHZ\n"
			   "bus-id: %d\n"
			   "i2c-devs: \n"
			   ,i2c0_dev_global->bus_clk_rate
			   ,i2c0_dev_global->adap.nr);

	seq_printf(m,"     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
	for (i = 0; i < 128; i += 16) {
		seq_printf(m,"%02x: ", i);
		for(j = 0; j < 16; j++) {

			
			msg.addr = i+j;
			msg.flags = I2C_M_TEN;
			msg.flags |= I2C_M_RD;
			msg.len = 1;
			msg.buf = &buf;
		
			res = i2c_transfer(&i2c0_dev_global->adap, &msg, 1);

			if (res < 0)
				seq_printf(m,"-- ");
			else
				seq_printf(m,"%02x ", i+j);
		}
		seq_printf(m,"\n");
	}
			   
	return 0;
}

/*
 * /proc/i2c*
 */
static int i2c1_proc_show(struct seq_file *m, void *v)
{
	char buf;
	struct i2c_msg msg;
	int res, i, j;

	seq_printf(m,
			   "work-mode: host\n"
			   "clockrate: %dKHZ\n"
			   "bus-id: %d\n"
			   "i2c-devs: \n"
			   ,i2c1_dev_global->bus_clk_rate
			   ,i2c1_dev_global->adap.nr);

	seq_printf(m,"     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
	for (i = 0; i < 128; i += 16) {
		seq_printf(m,"%02x: ", i);
		for(j = 0; j < 16; j++) {

			
			msg.addr = i+j;
			msg.flags = I2C_M_TEN;
			msg.flags |= I2C_M_RD;
			msg.len = 1;
			msg.buf = &buf;
		
			res = i2c_transfer(&i2c1_dev_global->adap, &msg, 1);

			if (res < 0)
				seq_printf(m,"-- ");
			else
				seq_printf(m,"%02x ", i+j);
		}
		seq_printf(m,"\n");
	}
			   
	return 0;
}

/*
 * /proc/i2c*
 */
static int i2c2_proc_show(struct seq_file *m, void *v)
{
	char buf;
	struct i2c_msg msg;
	int res, i, j;

	seq_printf(m,
			   "work-mode: host\n"
			   "clockrate: %dKHZ\n"
			   "bus-id: %d\n"
			   "i2c-devs: \n"
			   ,i2c2_dev_global->bus_clk_rate
			   ,i2c2_dev_global->adap.nr);

	seq_printf(m,"     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
	for (i = 0; i < 128; i += 16) {
		seq_printf(m,"%02x: ", i);
		for(j = 0; j < 16; j++) {

			
			msg.addr = i+j;
			msg.flags = I2C_M_TEN;
			msg.flags |= I2C_M_RD;
			msg.len = 1;
			msg.buf = &buf;
		
			res = i2c_transfer(&i2c2_dev_global->adap, &msg, 1);

			if (res < 0)
				seq_printf(m,"-- ");
			else
				seq_printf(m,"%02x ", i+j);
		}
		seq_printf(m,"\n");
	}
			   
	return 0;
}

static int i2c0_info_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, i2c0_proc_show, NULL);
}
static int i2c1_info_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, i2c1_proc_show, NULL);
}
static int i2c2_info_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, i2c2_proc_show, NULL);
}

#define PROC_I2C_OPS(index)					\
{											\
	.open 		= i2c##index##_info_open,		\
	.read		= seq_read,					\
	.llseek		= seq_lseek,				\
	.release	= seq_release				\
}

static const struct file_operations proc_i2c_operations[] = {
	PROC_I2C_OPS(0),
	PROC_I2C_OPS(1),
	PROC_I2C_OPS(2),
};

/* ak_i2c_probe
 *
 * called by the bus driver when a suitable device is found
*/
static int ak_i2c_probe(struct platform_device *pdev)
{
	struct ak_i2c_dev *i2c_dev;
	struct resource *mem, *irq;
	int ret;
	struct i2c_adapter *adap;
	struct device_node *np = pdev->dev.of_node;
	char proc_NodeName[5] = {0};
	pdev->id = of_alias_get_id(np, "i2c");

	i2c_dev = devm_kzalloc(&pdev->dev, sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev)
		return -ENOMEM;
	platform_set_drvdata(pdev, i2c_dev);
	i2c_dev->dev = &pdev->dev;
    i2c_dev->id = pdev->id;
	init_completion(&i2c_dev->completion);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	i2c_dev->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(i2c_dev->regs))
		return PTR_ERR(i2c_dev->regs);

	i2c_dev->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(i2c_dev->clk)) {
		dev_err(&pdev->dev, "Could not get clock\n");
		return PTR_ERR(i2c_dev->clk);
	}

	clk_prepare_enable(i2c_dev->clk);

	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency",
				   &i2c_dev->bus_clk_rate);
	if (ret < 0) {
		dev_warn(&pdev->dev,
			 "Could not read clock-frequency property\n");
		i2c_dev->bus_clk_rate = 100000;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "sda-delay",
				   &i2c_dev->sda_delay);
	if (ret < 0) {
		dev_warn(&pdev->dev,"sda-delay property is not exist\n");
	}	
	ret = of_property_read_u32(pdev->dev.of_node, "interval_scale",
				   &i2c_dev->interval_scale);
	if (ret < 0) {
		dev_warn(&pdev->dev,"interval_scale property is not exist\n");
		i2c_dev->interval_scale = 1;
	}
	pr_debug("i2c_dev->bus_clk_rate=%d\n",i2c_dev->bus_clk_rate);
	pr_debug("i2c_dev->sda_delay=%d\n",i2c_dev->sda_delay);
	pr_debug("i2c_dev->interval_scale=%d\n",i2c_dev->interval_scale);

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq) {
		dev_err(&pdev->dev, "No IRQ resource\n");
		return -ENODEV;
	}
	i2c_dev->irq = irq->start;

#ifdef AK_I2C_INTERRUPT_MODE
	ret = request_irq(i2c_dev->irq, ak_i2c_irq, IRQF_SHARED,
			  dev_name(&pdev->dev), i2c_dev);
	if (ret) {
		dev_err(&pdev->dev, "Could not request IRQ\n");
		return -ENODEV;
	}
#endif

	/* setup info block for the i2c core */
	adap = &i2c_dev->adap;
	i2c_set_adapdata(adap, i2c_dev);
	adap->owner = THIS_MODULE;
	adap->class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	strlcpy(adap->name, "ak-i2c", sizeof(adap->name));
	adap->algo = &ak_i2c_algorithm;
	adap->algo_data = i2c_dev;
	adap->retries = 2;
	adap->dev.parent = &pdev->dev;
	adap->dev.of_node = pdev->dev.of_node;
	adap->nr = of_alias_get_id(pdev->dev.of_node, "i2c");

	spin_lock_init(&i2c_dev->lock);

	/* initialise the i2c controller */

	ret = ak_i2c_initialise(i2c_dev);

	sema_init(&xfer_sem, 1);
	
	ret = i2c_add_numbered_adapter(&i2c_dev->adap);
	if (ret < 0) {
		free_irq(i2c_dev->irq, i2c_dev);
		dev_err(&pdev->dev, "failed to add bus to i2c core\n");
	}

	pr_info("%s:  AK I2C adapter\n", dev_name(&i2c_dev->adap.dev));

	/* create proc node to output state in runtime */
	sprintf(proc_NodeName,"i2c%d",adap->nr);
	if(adap->nr == 0)
		i2c0_dev_global = i2c_dev;
	else if(adap->nr == 1)
		i2c1_dev_global = i2c_dev;
	else if(adap->nr == 2)
		i2c2_dev_global = i2c_dev;
	i2c_dev->i2c_proc = proc_create(proc_NodeName, 0, NULL, &proc_i2c_operations[adap->nr]);

	return ret;
}


/* ak_i2c_remove
 *
 * called when device is removed from the bus
*/
static int ak_i2c_remove(struct platform_device *pdev)
{
	struct ak_i2c_dev *i2c_dev = platform_get_drvdata(pdev);
	if(i2c_dev->i2c_proc)
	{
		proc_remove(i2c_dev->i2c_proc);
	}
	i2c_del_adapter(&i2c_dev->adap);
	clk_disable_unprepare(i2c_dev->clk);
	//kfree(i2c_dev);
	return 0;
}

static const struct of_device_id ak_i2c_match[] = {
	{ .compatible = "anyka,ak37d-i2c0" },
	{ .compatible = "anyka,ak37d-i2c1" },	
	{ .compatible = "anyka,ak37d-i2c2" },
	{ .compatible = "anyka,ak37d-i2c3" },
	{ .compatible = "anyka,ak39ev330-i2c0" },
	{ .compatible = "anyka,ak39ev330-i2c1" },
	{ .compatible = "anyka,ak39ev330-i2c2" },
    { .compatible = "anyka,ak37e-i2c0" },
    { .compatible = "anyka,ak37e-i2c1" },
    { .compatible = "anyka,ak37e-i2c2" },
    { .compatible = "anyka,ak37e-i2c3" },
	{ }
};
MODULE_DEVICE_TABLE(of, ak_i2c_match);

static struct platform_driver ak_i2c_driver = {
	.probe = ak_i2c_probe,
	.remove = ak_i2c_remove,
	.driver = { 
		.name = "ak-i2c",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak_i2c_match),
	}
};

static int __init ak_i2c_init(void)
{
	return platform_driver_register(&ak_i2c_driver);
}

static void __exit ak_i2c_exit(void)
{
	platform_driver_unregister(&ak_i2c_driver);
}

subsys_initcall(ak_i2c_init);
module_exit(ak_i2c_exit);

MODULE_DESCRIPTION("Anyka I2C driver");
MODULE_AUTHOR("Anyka Microelectronic Ltd.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.06");
