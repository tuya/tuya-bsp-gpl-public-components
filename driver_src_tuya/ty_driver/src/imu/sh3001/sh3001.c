/******************************************************************
* Senodia 6D IMU(Gyroscope+Accelerometer) SH3001 driver, By Senodia AE@20220505;
*
*
* 1. unsigned char SH3001_init(void) : SH3001 initialize function
* 2. void SH3001_GetImuData( ... ): SH3001 read gyro and accel data function
* 3. float SH3001_GetTempData(void)：SH3001 read termperature function
* 4. void SH3001_pre_INT_config(void): Before using other INT config functions, need to call the function
*
*   For example: set accelerometer data ready INT
*   SH3001_pre_INT_config();
*
*	SH3001_INT_Config(	SH3001_INT0_LEVEL_HIGH,
*                       SH3001_INT_NO_LATCH,
*                       SH3001_INT1_LEVEL_HIGH,
*                       SH3001_INT_CLEAR_STATUS,
*                       SH3001_INT1_NORMAL,
*                       SH3001_INT0_NORMAL,
*                       3);
*
*   SH3001_INT_Enable(  SH3001_INT_ACC_READY,
*                       SH3001_INT_ENABLE,
*                       SH3001_INT_MAP_INT);
*
* 5. void SH3001_INT_Flat_Config( unsigned char flatTimeTH, unsigned char flatTanHeta2)：
*	If using the function, need to config SH3001_INT_Orient_Config( ... ) function.
*
* 6. delay_ms(x): delay x millisecond.
*
* 7. New version SH3001: SH3001_CHIP_ID1(0xDF) = 0x61;
*	SH3001_CompInit(... ): initialize compensation coefficients;
*	SH3001_GetImuCompData( ... ): SH3001 read gyro and accel compensation data function
*
*
******************************************************************/
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/ioctl.h>

#include "sh3001.h"
#include "ty_imu.h"



struct imu_data{
	short acc[3];
	short gyro[3];
};
static struct imu_data imu_data;


static sh3001_data *sh3001;


static int SH3001_read(u8 reg, u8 len, u8 *buf)
{
	u8 i = 0;
	struct i2c_client *client = sh3001->client;

    struct i2c_msg msg[2] = {
        [0] = {
            .addr  = client->addr,
            .flags = 0,
            .len   = 1,
            .buf   = &reg,
        },
        [1] = {
            .addr  = client->addr,
            .flags = I2C_M_RD,
            .len   = len,
            .buf   = buf,
        },
    };

	while (i++ < I2C_RETRY_CNT) {
		if (2 == i2c_transfer(client->adapter, msg, 2))
			break;

		mdelay(10);
	}

	if (i == I2C_RETRY_CNT+1) {
		dev_err(&client->dev, "sh3001 read rigster error!\n");
		return (SH3001_FALSE);
	}

	return (SH3001_TRUE);
}

static int SH3001_write(u8 reg, u8 len, u8 *buf)
{
	u8 i = 0;
	struct i2c_msg msg;
    u8 data[MAX_DATA_SIZE] = {0};
	struct i2c_client *client = sh3001->client;

	data[0] = reg;
	memcpy(&data[1], buf, len);

	msg.addr  = client->addr;
	msg.flags = 0;
	msg.len   = len + 1;
	msg.buf   = data;

	while (i++ < I2C_RETRY_CNT) {
		if (1 == i2c_transfer(client->adapter, &msg, 1))
			break;

		mdelay(10);
	}

	if (i == I2C_RETRY_CNT+1) {
		dev_err(&client->dev, "sh3001 write rigster error!\n");
		return (SH3001_FALSE);
	}

	return (SH3001_TRUE);
}

void delay_ms(unsigned short mSecond)
{
	mdelay(mSecond);
}

/******************************************************************
* Description:	Local Variable		
******************************************************************/
static compCoefType compCoef;
static unsigned char storeAccODR;


/******************************************************************
* Description:	Local Function Prototypes		
******************************************************************/
static void SH3001_ModuleReset_MCC(void);
static void SH3001_ModuleReset_MCD(void);
static void SH3001_ModuleReset_MCF(void);
static void SH3001_ModuleReset(void);

static void SH3001_Acc_Config(	unsigned char accODR, 
                                unsigned char accRange, 
                                unsigned char accCutOffRreq,
                                unsigned char accFilterEnble);

static void SH3001_Gyro_Config(	unsigned char gyroODR, 
                                unsigned char gyroRangeX, 
                                unsigned char gyroRangeY,
                                unsigned char gyroRangeZ,
                                unsigned char gyroCutOffRreq,
                                unsigned char gyroFilterEnble);
												
static void SH3001_Temp_Config(	unsigned char tempODR, 
                                unsigned char tempEnable);																									

static void SH3001_CompInit(compCoefType *compCoef);



/******************************************************************
* Description: reset MCC internal modules;
*
* Parameters: void	
*  						
* return:	void
*																
******************************************************************/
static void SH3001_ModuleReset_MCC(void)
{    
	unsigned char regAddr[8] = {0xC0, 0xD3, 0xD3, 0xD5, 0xD4, 0xBB, 0xB9, 0xBA};
	unsigned char regDataA[8] = {0x38, 0xC6, 0xC1, 0x02, 0x0C, 0x18, 0x18, 0x18};
	unsigned char regDataB[8] = {0x3D, 0xC2, 0xC2, 0x00, 0x04, 0x00, 0x00, 0x00};
    
    //Drive Start
    SH3001_write(regAddr[0], 1, &regDataA[0]);
    SH3001_write(regAddr[1], 1, &regDataA[1]);
    delay_ms(100);
    SH3001_write(regAddr[0], 1, &regDataB[0]);
    SH3001_write(regAddr[1], 1, &regDataB[1]); 
    delay_ms(50);
    
    //ADC Reset
    SH3001_write(regAddr[2], 1, &regDataA[2]);
    SH3001_write(regAddr[3], 1, &regDataA[3]);
    delay_ms(1);
    SH3001_write(regAddr[2], 1, &regDataB[2]);
    delay_ms(1);
    SH3001_write(regAddr[3], 1, &regDataB[3]);
    delay_ms(50);
    
    //CVA Reset
    SH3001_write(regAddr[4], 1, &regDataA[4]);
    delay_ms(10);
    SH3001_write(regAddr[4], 1, &regDataB[4]);
    
    delay_ms(1);
    
    SH3001_write(regAddr[5], 1, &regDataA[5]);
    delay_ms(10);
    SH3001_write(regAddr[6], 1, &regDataA[6]);
    delay_ms(10);
    SH3001_write(regAddr[7], 1, &regDataA[7]);
    delay_ms(10);
    SH3001_write(regAddr[5], 1, &regDataB[5]);
    delay_ms(10);
    SH3001_write(regAddr[6], 1, &regDataB[6]);
    delay_ms(10);
    SH3001_write(regAddr[7], 1, &regDataB[7]);
    delay_ms(10);     
}


/******************************************************************
* Description: reset MCD internal modules;
*
* Parameters: void	
*  						
* return:	void
*																
******************************************************************/
static void SH3001_ModuleReset_MCD(void)
{    
	unsigned char regAddr[8] = {0xC0, 0xD3, 0xD3, 0xD5, 0xD4, 0xBB, 0xB9, 0xBA};
	unsigned char regDataA[8] = {0x38, 0xD6, 0xD1, 0x02, 0x08, 0x18, 0x18, 0x18};
	unsigned char regDataB[8] = {0x3D, 0xD2, 0xD2, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    //Drive Start
    SH3001_write(regAddr[0], 1, &regDataA[0]);
    SH3001_write(regAddr[1], 1, &regDataA[1]);
    delay_ms(100);
    SH3001_write(regAddr[0], 1, &regDataB[0]);
    SH3001_write(regAddr[1], 1, &regDataB[1]); 
    delay_ms(50);
    
    //ADC Reset
    SH3001_write(regAddr[2], 1, &regDataA[2]);
    SH3001_write(regAddr[3], 1, &regDataA[3]);
    delay_ms(1);
    SH3001_write(regAddr[2], 1, &regDataB[2]);
    delay_ms(1);
    SH3001_write(regAddr[3], 1, &regDataB[3]);
    delay_ms(50);
    
    //CVA Reset
    SH3001_write(regAddr[4], 1, &regDataA[4]);
    delay_ms(10);
    SH3001_write(regAddr[4], 1, &regDataB[4]);
    
    delay_ms(1);
    
    SH3001_write(regAddr[5], 1, &regDataA[5]);
    delay_ms(10);
    SH3001_write(regAddr[6], 1, &regDataA[6]);
    delay_ms(10);
    SH3001_write(regAddr[7], 1, &regDataA[7]);
    delay_ms(10);
    SH3001_write(regAddr[5], 1, &regDataB[5]);
    delay_ms(10);
    SH3001_write(regAddr[6], 1, &regDataB[6]);
    delay_ms(10);
    SH3001_write(regAddr[7], 1, &regDataB[7]);
    delay_ms(10);     
}

/******************************************************************
* Description: reset MCF internal modules;
*
* Parameters: void	
*  						
* return:	void
*																
******************************************************************/
static void SH3001_ModuleReset_MCF(void)
{    
	unsigned char regAddr[8] = {0xC0, 0xD3, 0xD3, 0xD5, 0xD4, 0xBB, 0xB9, 0xBA};
	unsigned char regDataA[8] = {0x38, 0x16, 0x11, 0x02, 0x08, 0x18, 0x18, 0x18};
	unsigned char regDataB[8] = {0x3E, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    //Drive Start
    SH3001_write(regAddr[0], 1, &regDataA[0]);
    SH3001_write(regAddr[1], 1, &regDataA[1]);
    delay_ms(100);
    SH3001_write(regAddr[0], 1, &regDataB[0]);
    SH3001_write(regAddr[1], 1, &regDataB[1]); 
    delay_ms(50);
    
    //ADC Reset
    SH3001_write(regAddr[2], 1, &regDataA[2]);
    SH3001_write(regAddr[3], 1, &regDataA[3]);
    delay_ms(1);
    SH3001_write(regAddr[2], 1, &regDataB[2]);
    delay_ms(1);
    SH3001_write(regAddr[3], 1, &regDataB[3]);
    delay_ms(50);
    
    //CVA Reset
    SH3001_write(regAddr[4], 1, &regDataA[4]);
    delay_ms(10);
    SH3001_write(regAddr[4], 1, &regDataB[4]);
    
    delay_ms(1);
    
    SH3001_write(regAddr[5], 1, &regDataA[5]);
    delay_ms(10);
    SH3001_write(regAddr[6], 1, &regDataA[6]);
    delay_ms(10);
    SH3001_write(regAddr[7], 1, &regDataA[7]);
    delay_ms(10);
    SH3001_write(regAddr[5], 1, &regDataB[5]);
    delay_ms(10);
    SH3001_write(regAddr[6], 1, &regDataB[6]);
    delay_ms(10);
    SH3001_write(regAddr[7], 1, &regDataB[7]);
    delay_ms(10);     
}

	
/******************************************************************
* Description: reset chip internal modules;
*
* Parameters: void	
*  						
* return:	void
*																
******************************************************************/
static void SH3001_ModuleReset(void)
{
    unsigned char regData = 0;	
    
    SH3001_read(SH3001_CHIP_VERSION, 1, &regData);	
       
    if(regData == SH3001_CHIP_VERSION_MCC)
    {    
        SH3001_ModuleReset_MCC();
    }
    else if(regData == SH3001_CHIP_VERSION_MCD)
    {    
        SH3001_ModuleReset_MCD();
    }
    else if(regData == SH3001_CHIP_VERSION_MCF)
    {
        SH3001_ModuleReset_MCF();
    }
    else
    {
        SH3001_ModuleReset_MCD();
    }     
}	




/******************************************************************
* Description:	1.Switch SH3001 power mode;
*               2.Normal mode: 1.65mA; Sleep mode: 162uA; Acc normal mode:393uA;
*
* Parameters:   powerMode
*               SH3001_NORMAL_MODE
*               SH3001_SLEEP_MODE
*               SH3001_POWERDOWN_MODE
*				SH3001_ACC_NORMAL_MODE
*
* return:	SH3001_FALSE or SH3001_TRUE
*
******************************************************************/
unsigned char SH3001_SwitchPowerMode(unsigned char powerMode)
{
	unsigned char regAddr[10] = {0xCF, 0x22, 0x2F, 0xCB, 0xCE, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7};
	unsigned char regData[10] = {0};
	unsigned char i = 0;
	unsigned char accODR = SH3001_ODR_1000HZ;	
		
	if((powerMode != SH3001_NORMAL_MODE)
		&& (powerMode != SH3001_SLEEP_MODE)
		&& (powerMode != SH3001_POWERDOWN_MODE)
		&& (powerMode != SH3001_ACC_NORMAL_MODE))
	{
		return (SH3001_FALSE);
	}		
		
		
	for(i=0; i<10; i++)
	{
		SH3001_read(regAddr[i], 1, &regData[i]);	
	}
	
	switch(powerMode)
	{
	case SH3001_NORMAL_MODE:
		// restore accODR
		SH3001_write(SH3001_ACC_CONF1, 1, &storeAccODR);
		
		regData[0] = (regData[0] & 0xF8);
		regData[1] = (regData[1] & 0x7F);
		regData[2] = (regData[2] & 0xF7);
		regData[3] = (regData[3] & 0xF7);
		regData[4] = (regData[4] & 0xFE);
		regData[5] = (regData[5] & 0xFC) | 0x02;
		regData[6] = (regData[6] & 0x9F);
		regData[7] = (regData[7] & 0xF9);
		for(i=0; i<8; i++)
		{
			SH3001_write(regAddr[i], 1, &regData[i]);	
		}
		
		regData[7] = (regData[7] & 0x87);
		regData[8] = (regData[8] & 0x1F);
		regData[9] = (regData[9] & 0x03);
		for(i=7; i<10; i++)
		{
			SH3001_write(regAddr[i], 1, &regData[i]);	
		}		
		SH3001_ModuleReset();
		return (SH3001_TRUE);
	
	case SH3001_SLEEP_MODE:
		// store current acc ODR
		SH3001_read(SH3001_ACC_CONF1, 1, &storeAccODR);
		// set acc ODR=1000Hz
		SH3001_write(SH3001_ACC_CONF1, 1, &accODR);
	
		regData[0] = (regData[0] & 0xF8) | 0x07;
		regData[1] = (regData[1] & 0x7F) | 0x80;
		regData[2] = (regData[2] & 0xF7) | 0x08;
		regData[3] = (regData[3] & 0xF7) | 0x08;
		regData[4] = (regData[4] & 0xFE);
		regData[5] = (regData[5] & 0xFC) | 0x01;
		regData[6] = (regData[6] & 0x9F);
		regData[7] = (regData[7] & 0xF9) | 0x06;
		for(i=0; i<8; i++)
		{
			SH3001_write(regAddr[i], 1, &regData[i]);	
		}
		
		regData[7] = (regData[7] & 0x87);
		regData[8] = (regData[8] & 0x1F);
		regData[9] = (regData[9] & 0x03);
		for(i=7; i<10; i++)
		{
			SH3001_write(regAddr[i], 1, &regData[i]);	
		}
		return (SH3001_TRUE);
		
	case SH3001_POWERDOWN_MODE:
		regData[0] = (regData[0] & 0xF8);
		regData[1] = (regData[1] & 0x7F) | 0x80;
		regData[2] = (regData[2] & 0xF7) | 0x08;
		regData[3] = (regData[3] & 0xF7) | 0x08;
		regData[4] = (regData[4] & 0xFE);
		regData[5] = (regData[5] & 0xFC) | 0x01;
		regData[6] = (regData[6] & 0x9F) | 0x60;
		regData[7] = (regData[7] & 0xF9) | 0x06;
		for(i=0; i<8; i++)
		{
			SH3001_write(regAddr[i], 1, &regData[i]);	
		}
		
		regData[7] = (regData[7] & 0x87);
		regData[8] = (regData[8] & 0x1F);
		regData[9] = (regData[9] & 0x03);
		for(i=7; i<10; i++)
		{
			SH3001_write(regAddr[i], 1, &regData[i]);	
		}
		return (SH3001_TRUE);
		
	case SH3001_ACC_NORMAL_MODE:
		regData[0] = (regData[0] & 0xF8);
		regData[1] = (regData[1] & 0x7F);
		regData[2] = (regData[2] & 0xF7);
		regData[3] = (regData[3] & 0xF7);
		regData[4] = (regData[4] | 0x01);
		regData[5] = (regData[5] & 0xFC) | 0x01;
		regData[6] = (regData[6] & 0x9F);
		regData[7] = (regData[7] & 0xF9) | 0x06;
		for(i=0; i<8; i++)
		{
			SH3001_write(regAddr[i], 1, &regData[i]);	
		}
		
		regData[7] = (regData[7] & 0x87) | 0x78;
		regData[8] = (regData[8] & 0x1F) | 0xE0;
		regData[9] = (regData[9] & 0x03) | 0xFC;
		for(i=7; i<10; i++)
		{
			SH3001_write(regAddr[i], 1, &regData[i]);	
		}
		return (SH3001_TRUE);		
	
	default:
		return (SH3001_FALSE);
	}
}	


/******************************************************************
* Description:	1.set accelerometer parameters;
*               2.accCutOffRreq = accODR * 0.40 or accODR * 0.25 or accODR * 0.11 or accODR * 0.04;
*
* Parameters: 	accODR              accRange                accCutOffFreq       accFilterEnble
*               SH3001_ODR_1000HZ	SH3001_ACC_RANGE_16G	SH3001_ACC_ODRX040	SH3001_ACC_FILTER_EN
*               SH3001_ODR_500HZ	SH3001_ACC_RANGE_8G		SH3001_ACC_ODRX025	SH3001_ACC_FILTER_DIS
*               SH3001_ODR_250HZ	SH3001_ACC_RANGE_4G		SH3001_ACC_ODRX011
*               SH3001_ODR_125HZ	SH3001_ACC_RANGE_2G		SH3001_ACC_ODRX004
*               SH3001_ODR_63HZ
*               SH3001_ODR_31HZ
*               SH3001_ODR_16HZ
*               SH3001_ODR_2000HZ
*               SH3001_ODR_4000HZ
*               SH3001_ODR_8000HZ
*  
* return:	void
*
******************************************************************/
static void SH3001_Acc_Config(unsigned char accODR, 
                              unsigned char accRange, 
                              unsigned char accCutOffFreq,
                              unsigned char accFilterEnble)
{
    unsigned char regData = 0;	
	
	// enable acc digital filter
	SH3001_read(SH3001_ACC_CONF0, 1, &regData);
	regData |= 0x01;
	SH3001_write(SH3001_ACC_CONF0, 1, &regData);
	
	// set acc ODR
	storeAccODR = accODR;
	SH3001_write(SH3001_ACC_CONF1, 1, &accODR);
	
	// set acc Range
	SH3001_write(SH3001_ACC_CONF2, 1, &accRange);
		
	// bypass acc low pass filter or not
	SH3001_read(SH3001_ACC_CONF3, 1, &regData);
	regData &= 0x17;
	regData |= (accCutOffFreq | accFilterEnble);
	SH3001_write(SH3001_ACC_CONF3, 1, &regData);
}


/******************************************************************
* Description:	1.set gyroscope parameters;
*               2.gyroCutOffRreq is at Page 32 of SH3001 datasheet.
*
* Parameters:   gyroODR             gyroRangeX,Y,Z          gyroCutOffFreq      gyroFilterEnble
*               SH3001_ODR_1000HZ	SH3001_GYRO_RANGE_125	SH3001_GYRO_ODRX00	SH3001_GYRO_FILTER_EN
*               SH3001_ODR_500HZ	SH3001_GYRO_RANGE_250	SH3001_GYRO_ODRX01	SH3001_GYRO_FILTER_DIS
*               SH3001_ODR_250HZ	SH3001_GYRO_RANGE_500	SH3001_GYRO_ODRX02
*               SH3001_ODR_125HZ	SH3001_GYRO_RANGE_1000  SH3001_GYRO_ODRX03
*               SH3001_ODR_63HZ		SH3001_GYRO_RANGE_2000
*               SH3001_ODR_31HZ
*               SH3001_ODR_16HZ
*               SH3001_ODR_2000HZ
*               SH3001_ODR_4000HZ
*               SH3001_ODR_8000HZ
*               SH3001_ODR_16000HZ
*               SH3001_ODR_32000HZ
*  	 
* return:	void
*
******************************************************************/
static void SH3001_Gyro_Config(	unsigned char gyroODR, 
                                unsigned char gyroRangeX, 
                                unsigned char gyroRangeY,
                                unsigned char gyroRangeZ,
                                unsigned char gyroCutOffFreq,
                                unsigned char gyroFilterEnble)
{
    unsigned char regData = 0;	
	
	// enable gyro digital filter
	SH3001_read(SH3001_GYRO_CONF0, 1, &regData);
	regData |= 0x01;
	SH3001_write(SH3001_GYRO_CONF0, 1, &regData);
	
	// set gyro ODR
	SH3001_write(SH3001_GYRO_CONF1, 1, &gyroODR);
	
	// set gyro X\Y\Z range
	SH3001_write(SH3001_GYRO_CONF3, 1, &gyroRangeX);
	SH3001_write(SH3001_GYRO_CONF4, 1, &gyroRangeY);
	SH3001_write(SH3001_GYRO_CONF5, 1, &gyroRangeZ);
		
	// bypass gyro low pass filter or not
	SH3001_read(SH3001_GYRO_CONF2, 1, &regData);
	regData &= 0xE3;
	regData |= (gyroCutOffFreq | gyroFilterEnble);
	SH3001_write(SH3001_GYRO_CONF2, 1, &regData);
}


/******************************************************************
* Description:	set temperature parameters;
*
* Parameters: 	tempODR                 tempEnable
*               SH3001_TEMP_ODR_500		SH3001_TEMP_EN
*               SH3001_TEMP_ODR_250		SH3001_TEMP_DIS
*               SH3001_TEMP_ODR_125		
*               SH3001_TEMP_ODR_63		
*
* return:	void
*  							
******************************************************************/
static void SH3001_Temp_Config(	unsigned char tempODR, 
                                unsigned char tempEnable)
{
    unsigned char regData = 0;	
	
	// enable temperature, set ODR
	SH3001_read(SH3001_TEMP_CONF0, 1, &regData);
	regData &= 0x4F;
	regData |= (tempODR | tempEnable);
	SH3001_write(SH3001_TEMP_CONF0, 1, &regData);
}


/******************************************************************
* Description:	read temperature parameters;
*
* Parameters: 	void	
*  		
* return:	temperature data(deg);
* 
******************************************************************/
#if 0
float SH3001_GetTempData(void)
{
    unsigned char regData[2] = {0};	
    unsigned short int tempref[2] = {0};
	
	// read temperature data, unsigned 12bits;   SH3001_TEMP_CONF0..SH3001_TEMP_CONF1
	SH3001_read(SH3001_TEMP_CONF0, 2, &regData[0]);
	tempref[0] = ((unsigned short int)(regData[0] & 0x0F) << 8) | regData[1];
	
	SH3001_read(SH3001_TEMP_ZL, 2, &regData[0]);
	tempref[1] = ((unsigned short int)(regData[1] & 0x0F) << 8) | regData[0];	
	
	return ( (((float)(tempref[1] - tempref[0]))/16.0f) + 25.0f );
}
#endif

/******************************************************************
* Description:	enable or disable INT, mapping interrupt to INT pin or INT1 pin
*
* Parameters:   intType                 intEnable               intPinSel
*               SH3001_INT_LOWG         SH3001_INT_ENABLE       SH3001_INT_MAP_INT
*               SH3001_INT_HIGHG        SH3001_INT_DISABLE      SH3001_INT_MAP_INT1
*               SH3001_INT_INACT        		  
*               SH3001_INT_ACT
*               SH3001_INT_DOUBLE_TAP
*               SH3001_INT_SINGLE_TAP
*               SH3001_INT_FLAT
*               SH3001_INT_ORIENTATION
*               SH3001_INT_FIFO_GYRO
*               SH3001_INT_GYRO_READY
*               SH3001_INT_ACC_FIFO
*               SH3001_INT_ACC_READY
*               SH3001_INT_FREE_FALL
*               SH3001_INT_UP_DOWN_Z
* return:	void
* 
******************************************************************/
void SH3001_INT_Enable(	unsigned short int intType, 
                        unsigned char intEnable, 
                        unsigned char intPinSel)
{
    unsigned char regData[2] = {0};	
    unsigned short int u16IntVal = 0, u16IntTemp = 0;
	
	// Z axis change between UP to DOWN
	if((intType & 0x0040) == SH3001_INT_UP_DOWN_Z)
	{
		SH3001_read(SH3001_ORIEN_INTCONF0, 1, &regData[0]);
		regData[0] = (intEnable == SH3001_INT_ENABLE) \
                     ? (regData[0] | 0x40) : (regData[0] & 0xBF);
		SH3001_write(SH3001_ORIEN_INTCONF0, 1, &regData[0]);
	}	
	
	if((intType & 0xFF1F))
	{	
		// enable or disable INT
		SH3001_read(SH3001_INT_ENABLE0, 2, &regData[0]);
		u16IntVal = ((unsigned short int)regData[0] << 8) | regData[1];
		
		u16IntVal = (intEnable == SH3001_INT_ENABLE) \
                    ? (u16IntVal | intType) : (u16IntVal & ~intType);		
    
		regData[0] = (unsigned char)(u16IntVal >> 8);
		regData[1] = (unsigned char)(u16IntVal);		
		SH3001_write(SH3001_INT_ENABLE0, 1, &regData[0]);
		SH3001_write(SH3001_INT_ENABLE1, 1, &regData[1]);
							
		
		// mapping interrupt to INT pin or INT1 pin
		SH3001_read(SH3001_INT_PIN_MAP0, 2, &regData[0]);
		u16IntVal = ((unsigned short int)regData[0] << 8) | regData[1];
    
		u16IntTemp = (intType << 1) & 0xE03E;
		u16IntTemp = u16IntTemp | (intType & 0x0F00);
		if(intType & SH3001_INT_LOWG)
		{
			u16IntTemp |= 0x0001;
		}
	
		u16IntVal = (intPinSel == SH3001_INT_MAP_INT1) \
                    ? (u16IntVal | u16IntTemp) : (u16IntVal & ~u16IntTemp);
			
		regData[0] = (unsigned char)(u16IntVal >> 8);
		regData[1] = (unsigned char)(u16IntVal);
		SH3001_write(SH3001_INT_PIN_MAP0, 1, &regData[0]);
		SH3001_write(SH3001_INT_PIN_MAP1, 1, &regData[1]);
	}	
}


/******************************************************************
* Description:	1.config INT function
*               2.intCount is valid only when intLatch is equal to SH3001_INT_NO_LATCH;
*
* Parameters:   int0Level					intLatch             	int1Level                 intClear					
*               SH3001_INT0_LEVEL_HIGH		SH3001_INT_NO_LATCH		SH3001_INT1_LEVEL_HIGH    SH3001_INT_CLEAR_ANY
*               SH3001_INT0_LEVEL_LOW	  	SH3001_INT_LATCH		SH3001_INT1_LEVEL_LOW     SH3001_INT_CLEAR_STATUS
*
*
*               int1Mode					int0Mode				intTime
*               SH3001_INT1_NORMAL			SH3001_INT0_NORMAL		Unit: 2mS
*               SH3001_INT1_OD		  		SH3001_INT0_OD
*
* return:	void
* 
******************************************************************/
void SH3001_INT_Config(	unsigned char int0Level,
                        unsigned char intLatch,
                        unsigned char int1Level,  
                        unsigned char intClear,
                        unsigned char int1Mode,
                        unsigned char int0Mode,
                        unsigned char intTime)
{
	unsigned char regData = 0;	
	
	SH3001_read(SH3001_INT_CONF, 1, &regData);
	
	regData = (int0Level == SH3001_INT0_LEVEL_LOW) \
              ? (regData | SH3001_INT0_LEVEL_LOW) : (regData & SH3001_INT0_LEVEL_HIGH);
	
	regData = (intLatch == SH3001_INT_NO_LATCH) \
              ? (regData | SH3001_INT_NO_LATCH) : (regData & SH3001_INT_LATCH);
  
	regData = (int1Level == SH3001_INT1_LEVEL_LOW) \
              ? (regData | SH3001_INT1_LEVEL_LOW) : (regData & SH3001_INT1_LEVEL_HIGH);
	
	regData = (intClear == SH3001_INT_CLEAR_ANY) \
              ? (regData | SH3001_INT_CLEAR_ANY) : (regData & SH3001_INT_CLEAR_STATUS);
	
	regData = (int1Mode == SH3001_INT1_NORMAL) \
              ? (regData | SH3001_INT1_NORMAL) : (regData & SH3001_INT1_OD);	

	regData = (int0Mode == SH3001_INT0_NORMAL) \
              ? (regData | SH3001_INT0_NORMAL) : (regData & SH3001_INT0_OD);			
	
	SH3001_write(SH3001_INT_CONF, 1, &regData);	
	
	if(intLatch == SH3001_INT_NO_LATCH)
	{
		if(intTime != 0)
		{	
			regData = intTime;
			SH3001_write(SH3001_INT_LIMIT, 1, &regData);	
		}	
	}
}		

/******************************************************************
* Description:	1.lowG INT config;
*               2.lowGThres: x=0.5mG@2G or x=1mG@4G or x=2mG@8G or x=4mG@16G
*
* Parameters: 	lowGEnDisIntAll	            lowGThres		lowGTimsMs
*               SH3001_LOWG_ALL_INT_EN		Unit: x mG		Unit: 2mS
*               SH3001_LOWG_ALL_INT_DIS
*
* return:	void
* 
******************************************************************/
void SH3001_INT_LowG_Config(	unsigned char lowGEnDisIntAll,
                                unsigned char lowGThres,
                                unsigned char lowGTimsMs)
{
	unsigned char regData = 0;		
	
	SH3001_read(SH3001_HIGHLOW_G_INT_CONF, 1, &regData);	
	regData &= 0xFE;
	regData |= lowGEnDisIntAll;
	SH3001_write(SH3001_HIGHLOW_G_INT_CONF, 1, &regData);	
	
	regData = lowGThres; 
	SH3001_write(SH3001_LOWG_INT_THRESHOLD, 1, &regData);	
	
	regData = lowGTimsMs; 
	SH3001_write(SH3001_LOWG_INT_TIME, 1, &regData);		
}


/******************************************************************
* Description:	1.highG INT config;
*               2.highGThres: x=0.5mG@2G, x=1mG@4G, x=2mG@8G, x=4mG@16G
*
* Parameters: 	highGEnDisIntX              highGEnDisIntY              highGEnDisIntZ				
*               SH3001_HIGHG_X_INT_EN		SH3001_HIGHG_Y_INT_EN		SH3001_HIGHG_Z_INT_EN		
*               SH3001_HIGHG_X_INT_DIS		SH3001_HIGHG_Y_INT_DIS		SH3001_HIGHG_Z_INT_DIS
*			
*
*               highGEnDisIntAll			highGThres					highGTimsMs
*               SH3001_HIGHG_ALL_INT_EN		Unit: x mG					Unit: 2mS
*               SH3001_HIGHG_ALL_INT_DIS
*
* return:	void
* 
******************************************************************/
void SH3001_INT_HighG_Config(	unsigned char highGEnDisIntX,
                                unsigned char highGEnDisIntY,
                                unsigned char highGEnDisIntZ,
                                unsigned char highGEnDisIntAll,
                                unsigned char highGThres,
                                unsigned char highGTimsMs)
{
	unsigned char regData = 0;		
	
	SH3001_read(SH3001_HIGHLOW_G_INT_CONF, 1, &regData);	
	regData &= 0x0F;
	regData |= highGEnDisIntX | highGEnDisIntY | highGEnDisIntZ | highGEnDisIntAll;
	SH3001_write(SH3001_HIGHLOW_G_INT_CONF, 1, &regData);		
																			
	regData = highGThres; 
	SH3001_write(SH3001_HIGHG_INT_THRESHOLD, 1, &regData);	
			
	regData = highGTimsMs; 
	SH3001_write(SH3001_HIGHG_INT_TIME, 1, &regData);	
}


/******************************************************************
* Description:	inactivity INT config;
*
* Parameters: 	inactEnDisIntX              inactEnDisIntY				inactEnDisIntZ
*               SH3001_INACT_X_INT_EN		SH3001_INACT_Y_INT_EN		SH3001_INACT_Z_INT_EN
*               SH3001_INACT_X_INT_DIS	    SH3001_INACT_Y_INT_DIS	    SH3001_INACT_Z_INT_DIS
*								
*								
*               inactIntMode				inactLinkStatus				inactTimeS		
*               SH3001_INACT_AC_MODE		SH3001_LINK_PRE_STA			Unit: S									
*               SH3001_INACT_DC_MODE		SH3001_LINK_PRE_STA_NO
*
*
*               inactIntThres				inactG1
*               (3 Bytes)									
*
* return:	void
* 
******************************************************************/
void SH3001_INT_Inact_Config(	unsigned char inactEnDisIntX,
                                unsigned char inactEnDisIntY,
                                unsigned char inactEnDisIntZ,
                                unsigned char inactIntMode,
                                unsigned char inactLinkStatus,	
                                unsigned char inactTimeS,
                                unsigned int inactIntThres,
                                unsigned short int inactG1)
{
	unsigned char regData = 0;	
	
	SH3001_read(SH3001_ACT_INACT_INT_CONF, 1, &regData);	
	regData &= 0xF0;
	regData |= inactEnDisIntX | inactEnDisIntY | inactEnDisIntZ | inactIntMode;
	SH3001_write(SH3001_ACT_INACT_INT_CONF, 1, &regData);	
	
	regData = (unsigned char)inactIntThres;
	SH3001_write(SH3001_INACT_INT_THRESHOLDL, 1, &regData);
	regData = (unsigned char)(inactIntThres >> 8);
	SH3001_write(SH3001_INACT_INT_THRESHOLDM, 1, &regData);
	regData = (unsigned char)(inactIntThres >> 16);
	SH3001_write(SH3001_INACT_INT_THRESHOLDH, 1, &regData);	
		
	regData = inactTimeS;
	SH3001_write(SH3001_INACT_INT_TIME, 1, &regData);	
	
	regData = (unsigned char)inactG1;
	SH3001_write(SH3001_INACT_INT_1G_REFL, 1, &regData);
	regData = (unsigned char)(inactG1 >> 8);
	SH3001_write(SH3001_INACT_INT_1G_REFH, 1, &regData);	
	
	
	SH3001_read(SH3001_ACT_INACT_INT_LINK, 1, &regData);	
	regData &= 0xFE;
	regData |= inactLinkStatus;
	SH3001_write(SH3001_ACT_INACT_INT_LINK, 1, &regData);		
}	



/******************************************************************
* Description:	activity INT config;
*				actIntThres: 1 LSB is 0.24mg@+/-2G, 0.48mg@+/-4G, 0.97mg@+/-8G, 1.95mg@+/-16G
*
* Parameters: 	actEnDisIntX			actEnDisIntY			actEnDisIntZ
*               SH3001_ACT_X_INT_EN		SH3001_ACT_Y_INT_EN		SH3001_ACT_Z_INT_EN
*               SH3001_ACT_X_INT_DIS	SH3001_ACT_Y_INT_DIS	SH3001_ACT_Z_INT_DIS
*								
*								
*               actIntMode				actTimeNum		actIntThres		actLinkStatus						
*               SH3001_ACT_AC_MODE		(1 Byte)		(1 Byte)		SH3001_LINK_PRE_STA			
*               SH3001_ACT_DC_MODE										SH3001_LINK_PRE_STA_NO
*
* return:	void
* 
******************************************************************/
void SH3001_INT_Act_Config(	unsigned char actEnDisIntX,
                            unsigned char actEnDisIntY,
                            unsigned char actEnDisIntZ,
                            unsigned char actIntMode,
							unsigned char actTimeNum,
                            unsigned char actIntThres,
                            unsigned char actLinkStatus)
{
	unsigned char regData = 0;	
	
	SH3001_read(SH3001_ACT_INACT_INT_CONF, 1, &regData);	
	regData &= 0x0F;
	regData |= actEnDisIntX | actEnDisIntY | actEnDisIntZ | actIntMode;
	SH3001_write(SH3001_ACT_INACT_INT_CONF, 1, &regData);	
	
	regData = actIntThres;
	SH3001_write(SH3001_ACT_INT_THRESHOLD, 1, &regData);

	regData = actTimeNum;
	SH3001_write(SH3001_ACT_INT_TIME, 1, &regData);
	
	SH3001_read(SH3001_ACT_INACT_INT_LINK, 1, &regData);	
	regData &= 0xFE;
	regData |= actLinkStatus;
	SH3001_write(SH3001_ACT_INACT_INT_LINK, 1, &regData);		
}	



/******************************************************************
* Description:	1.tap INT config;
*               2.tapWaitTimeWindowMs is more than tapWaitTimeMs;
*
* Parameters: 	tapEnDisIntX				tapEnDisIntY				tapEnDisIntZ
*               SH3001_TAP_X_INT_EN			SH3001_TAP_Y_INT_EN			SH3001_TAP_Z_INT_EN
*               SH3001_TAP_X_INT_DIS		SH3001_TAP_Y_INT_DIS		SH3001_TAP_Z_INT_DIS
*								
*								
*               tapIntThres		tapTimeMs	    tapWaitTimeMs	    tapWaitTimeWindowMs
*               (1 Byte)		Unit: mS		Unit: mS			Unit: mS
*								
* return:	void
* 
******************************************************************/
void SH3001_INT_Tap_Config(	unsigned char tapEnDisIntX,
                            unsigned char tapEnDisIntY,
                            unsigned char tapEnDisIntZ,
                            unsigned char tapIntThres,
                            unsigned char tapTimeMs,
                            unsigned char tapWaitTimeMs,
                            unsigned char tapWaitTimeWindowMs)
{
	unsigned char regData = 0;		
	
	SH3001_read(SH3001_ACT_INACT_INT_LINK, 1, &regData);	
	regData &= 0xF1;
	regData |= tapEnDisIntX | tapEnDisIntY | tapEnDisIntZ;
	SH3001_write(SH3001_ACT_INACT_INT_LINK, 1, &regData);																			
																			
	regData = tapIntThres; 
	SH3001_write(SH3001_TAP_INT_THRESHOLD, 1, &regData);
	
	regData = tapTimeMs; 
	SH3001_write(SH3001_TAP_INT_DURATION, 1, &regData);	
	
	regData = tapWaitTimeMs; 
	SH3001_write(SH3001_TAP_INT_LATENCY, 1, &regData);	

	regData = tapWaitTimeWindowMs; 
	SH3001_write(SH3001_DTAP_INT_WINDOW, 1, &regData);	
}


/******************************************************************
* Description:	flat INT time threshold, unit: mS;
*
* Parameters: 	flatTimeTH					flatTanHeta2
*               SH3001_FLAT_TIME_500MS		0 ~ 63
*               SH3001_FLAT_TIME_1000MS
*               SH3001_FLAT_TIME_2000MS
*  		
* return:	void
* 
******************************************************************/
void SH3001_INT_Flat_Config(	unsigned char flatTimeTH, unsigned char flatTanHeta2)
{
	unsigned char regData = 0;	
	
	//SH3001_read(SH3001_FLAT_INT_CONF, 1, &regData);	
	regData =  (flatTimeTH & 0xC0) | (flatTanHeta2 & 0x3F);
	SH3001_write(SH3001_FLAT_INT_CONF, 1, &regData);		
}	


/******************************************************************
* Description:	orientation config;
*
* Parameters: 	orientBlockMode				orientMode					orientTheta
*               SH3001_ORIENT_BLOCK_MODE0	SH3001_ORIENT_SYMM			(1 Byte)
*               SH3001_ORIENT_BLOCK_MODE1	SH3001_ORIENT_HIGH_ASYMM
*               SH3001_ORIENT_BLOCK_MODE2	SH3001_ORIENT_LOW_ASYMM
*               SH3001_ORIENT_BLOCK_MODE3
*
*               orientG1point5				orientSlope					orientHyst
*               (2 Bytes)					(2 Bytes)					(2 Bytes)
*  		
* return:	void
* 
******************************************************************/
void SH3001_INT_Orient_Config(	unsigned char 	orientBlockMode,
                                unsigned char 	orientMode,
                                unsigned char	orientTheta,
                                unsigned short	orientG1point5,
                                unsigned short 	orientSlope,
                                unsigned short 	orientHyst)
{
	unsigned char regData[2] = {0};	
	
	SH3001_read(SH3001_ORIEN_INTCONF0, 1, &regData[0]);
	regData[0] |= (regData[0] & 0xC0) | (orientTheta & 0x3F); 
	SH3001_write(SH3001_ORIEN_INTCONF0, 1, &regData[0]);

	SH3001_read(SH3001_ORIEN_INTCONF1, 1, &regData[0]);
	regData[0] &= 0xF0;
	regData[0] |= (orientBlockMode & 0x0C) | (orientMode & 0x03);
	SH3001_write(SH3001_ORIEN_INTCONF1, 1, &regData[0]);
	
	regData[0] = (unsigned char)orientG1point5;
	regData[1] = (unsigned char)(orientG1point5 >> 8);
	SH3001_write(SH3001_ORIEN_INT_LOW, 1, &regData[0]);
	SH3001_write(SH3001_ORIEN_INT_HIGH, 1, &regData[1]);
	
	regData[0] = (unsigned char)orientSlope;
	regData[1] = (unsigned char)(orientSlope >> 8);
	SH3001_write(SH3001_ORIEN_INT_SLOPE_LOW, 1, &regData[0]);	
	SH3001_write(SH3001_ORIEN_INT_SLOPE_HIGH, 1, &regData[1]);
	
	regData[0] = (unsigned char)orientHyst;
	regData[1] = (unsigned char)(orientHyst >> 8);
	SH3001_write(SH3001_ORIEN_INT_HYST_LOW, 1, &regData[0]);	
	SH3001_write(SH3001_ORIEN_INT_HYST_HIGH, 1, &regData[1]);	
}


/******************************************************************
* Description:	1.freeFall INT config;
*               2.freeFallThres: x=0.5mG@2G or x=1mG@4G or x=2mG@8G or x=4mG@16G
*
* Parameters: 	freeFallThres	freeFallTimsMs
*               Unit: x mG		Unit: 2mS
*
* return:	void
* 
******************************************************************/
void SH3001_INT_FreeFall_Config(	unsigned char freeFallThres,
                                    unsigned char freeFallTimsMs)
{
	unsigned char regData = 0;
	
	regData = freeFallThres; 
	SH3001_write(SH3001_FREEFALL_INT_THRES, 1, &regData);	
	
	regData = freeFallTimsMs; 
	SH3001_write(SH3001_FREEFALL_INT_TIME, 1, &regData);																						
}



/******************************************************************
* Description:	1.read INT status0
*
* Parameters: 	void
*
* return:	unsigned short
*           bit 11: Acc Data Ready Interrupt status							
*           bit 10: Acc FIFO Watermark Interrupt status
*           bit 9: Gyro Ready Interrupt status
*           bit 8: Gyro FIFO Watermark Interrupt status
*           bit 7: Free-fall Interrupt status
*           bit 6: Orientation Interrupt status
*           bit 5: Flat Interrupt status
*           bit 4: Tap Interrupt status
*           bit 3: Single Tap Interrupt status
*           bit 2: Double Tap Interrupt status
*           bit 1: Activity Interrupt status
*           bit 0: Inactivity Interrupt status
*               0: Not Active
*               1: Active
*
******************************************************************/
unsigned short SH3001_INT_Read_Status0(void)
{
	unsigned char regData[2] = {0};	
	
	SH3001_read(SH3001_INT_STA0, 2, &regData[0]);	

	return( ((unsigned short)(regData[1] & 0x0F) << 8) | regData[0]);
}


/******************************************************************
* Description:	1.read INT status2
*
* Parameters: 	void
*
* return:	unsigned char
*           bit 7: Sign of acceleration that trigger High-G Interrupt
*               0: Positive
*               1: Negative
*           bit 6: Whether High-G Interrupt is triggered by X axis
*           bit 5: Whether High-G Interrupt is triggered by Y axis
*           bit 4: Whether High-G Interrupt is triggered by Z axis
*               0: No
*               1: Yes
*           bit 3: Reserved
*           bit 2: Reserved
*           bit 1: Reserved
*           bit 0: Low-G Interrupt status
*               0: Not Active
*               1: Active
*
******************************************************************/
unsigned char SH3001_INT_Read_Status2(void)
{
	unsigned char regData = 0;	
	
	SH3001_read(SH3001_INT_STA2, 1, &regData);	

	return( (regData & 0xF1));
}


/******************************************************************
* Description:	1.read INT status3
*
* Parameters: 	void
*
* return:	unsigned char
*           bit 7: Sign of acceleration that trigger Activity or Inactivity Interrupt
*               0: Positive
*               1: Negative
*           bit 6: Whether Activity or Inactivity Interrupt is triggered by X axis
*           bit 5: Whether Activity or Inactivity Interrupt is triggered by Y axis
*           bit 4: Whether Activity or Inactivity Interrupt is triggered by Z axis
*               0: No
*               1: Yes
*			bit 3: Sign of acceleration that trigger Single or Double Tap Interrup
*               0: Positive
*               1: Negative
*			bit 2: Whether Single or Double Tap Interrupt is triggered by X axis
*			bit 1: Whether Single or Double Tap Interrupt is triggered by Y axis
*			bit 0: Whether Single or Double Tap Interrupt is triggered by Z axis
*               0: No
*               1: Yes
******************************************************************/
unsigned char SH3001_INT_Read_Status3(void)
{
	unsigned char regData = 0;	
	
	SH3001_read(SH3001_INT_STA3, 1, &regData);	

	return(regData);
}



/******************************************************************
* Description:	1.read INT status4
*
* Parameters: 	void
*
* return:	unsigned char
*           bit 7: Reserved
*           bit 6: Reserved
*           bit 5: Reserved
*           bit 4: Reserved
*           bit 3: Reserved
*           bit 2: Orientation Interrupt Value of Z-axis
*               0: Upward
*               1: Downward
*           bit 1..0: Orientation Interrupt Value of X and Y-axis
*               00: Landscape left
*               01: Landscape right
*               10: Portrait upside down
*               11: Portrait upright
*
******************************************************************/
unsigned char SH3001_INT_Read_Status4(void)
{
	unsigned char regData = 0;	
	
	SH3001_read(SH3001_INT_STA4, 1, &regData);	

	return(regData & 0x07);
}


/******************************************************************
* Description:	Before running other INT config functions, need to call SH3001_pre_INT_config();
*
* Parameters: 	void
*
* return:	void
*
*
******************************************************************/
void SH3001_pre_INT_config(void)
{	
  unsigned char regData = 0;

	regData = 0x20;
	SH3001_write(SH3001_ORIEN_INTCONF0, 1, &regData);

	regData = 0x0A;
	SH3001_write(SH3001_ORIEN_INTCONF1, 1, &regData);
	
	regData = 0x01;
	SH3001_write(SH3001_ORIEN_INT_HYST_HIGH, 1, &regData);		
}





/******************************************************************
* Description:	reset FIFO controller;
*
* Parameters: 	fifoMode
*               SH3001_FIFO_MODE_DIS
*               SH3001_FIFO_MODE_FIFO
*               SH3001_FIFO_MODE_STREAM
*               SH3001_FIFO_MODE_TRIGGER
*
* return:	void
* 
******************************************************************/
void SH3001_FIFO_Reset(unsigned char fifoMode)
{
	unsigned char regData = 0;		
	
	SH3001_read(SH3001_FIFO_CONF0, 1, &regData);
	regData |= 0x80;
	SH3001_write(SH3001_FIFO_CONF0, 1, &regData);
	regData &= 0x7F;
	SH3001_write(SH3001_FIFO_CONF0, 1, &regData);
	
	regData = fifoMode & 0x03;
	SH3001_write(SH3001_FIFO_CONF0, 1, &regData);
}


/******************************************************************
* Description:	1.FIFO down sample frequency config;
*               2.fifoAccFreq:	accODR*1/2, *1/4, *1/8, *1/16, *1/32, *1/64, *1/128, *1/256
*               3.fifoGyroFreq:	gyroODR*1/2, *1/4, *1/8, *1/16, *1/32, *1/64, *1/128, *1/256
*
* Parameters: 	fifoAccDownSampleEnDis		fifoAccFreq/fifoGyroFreq	fifoGyroDownSampleEnDis
*               SH3001_FIFO_ACC_DOWNS_EN	SH3001_FIFO_FREQ_X1_2		SH3001_FIFO_GYRO_DOWNS_EN
*               SH3001_FIFO_ACC_DOWNS_DIS	SH3001_FIFO_FREQ_X1_4		SH3001_FIFO_GYRO_DOWNS_DIS
*											SH3001_FIFO_FREQ_X1_8
*											SH3001_FIFO_FREQ_X1_16
*											SH3001_FIFO_FREQ_X1_32
*											SH3001_FIFO_FREQ_X1_64
*											SH3001_FIFO_FREQ_X1_128
*											SH3001_FIFO_FREQ_X1_256
* return:	void
* 
******************************************************************/
void SH3001_FIFO_Freq_Config(	unsigned char fifoAccDownSampleEnDis,
                                unsigned char fifoAccFreq,
                                unsigned char fifoGyroDownSampleEnDis,
                                unsigned char fifoGyroFreq)
{
	unsigned char regData = 0;		
	
	regData |= fifoAccDownSampleEnDis | fifoGyroDownSampleEnDis;
	regData |= (fifoAccFreq << 4) | fifoGyroFreq;
	SH3001_write(SH3001_FIFO_CONF4, 1, &regData);
}


/******************************************************************
* Description:	1.data type config in FIFO;
*               2.fifoWaterMarkLevel is less than or equal to 1024;
*
* Parameters: 	fifoMode				    fifoWaterMarkLevel
*               SH3001_FIFO_EXT_Z_EN	    <=1024
*               SH3001_FIFO_EXT_Y_EN
*               SH3001_FIFO_EXT_X_EN							
*               SH3001_FIFO_TEMPERATURE_EN							
*               SH3001_FIFO_GYRO_Z_EN							
*               SH3001_FIFO_GYRO_Y_EN						
*               SH3001_FIFO_GYRO_X_EN
*               SH3001_FIFO_ACC_Z_EN
*               SH3001_FIFO_ACC_Y_EN
*               SH3001_FIFO_ACC_X_EN
*               SH3001_FIFO_ALL_DIS
*															
* return:	void
* 
******************************************************************/
void SH3001_FIFO_Data_Config(	unsigned short fifoMode,
                                unsigned short fifoWaterMarkLevel)
{
	unsigned char regData = 0;
	
	if(fifoWaterMarkLevel > 1024)
	{
		fifoWaterMarkLevel = 1024;
	}
	
	SH3001_read(SH3001_FIFO_CONF2, 1, &regData);
	regData = (regData & 0xC8) \
              | ((unsigned char)(fifoMode >> 8) & 0x30) \
              | (((unsigned char)(fifoWaterMarkLevel >> 8)) & 0x07);
	SH3001_write(SH3001_FIFO_CONF2, 1, &regData);

	regData = (unsigned char)fifoMode;
	SH3001_write(SH3001_FIFO_CONF3, 1, &regData);
	
	regData = (unsigned char)fifoWaterMarkLevel;
	SH3001_write(SH3001_FIFO_CONF1, 1, &regData);
}



/******************************************************************
* Description:	1. read Fifo status and fifo entries count
*
* Parameters: 	*fifoEntriesCount, store fifo entries count, less than or equal to 1024;
*
*															
* return:	unsigned char
*           bit 7: 0
*           bit 6: 0
*           bit 5: Whether FIFO Watermark has been reached
*           bit 4: Whether FIFO is full
*           bit 3: Whether FIFO is empty
*               0: No
*               1: Yes
*           bit 2: 0
*           bit 1: 0
*           bit 0: 0
*
*
******************************************************************/
unsigned char SH3001_FIFO_Read_Status(unsigned short int *fifoEntriesCount)
{
	unsigned char regData[2] = {0};		
	
	SH3001_read(SH3001_FIFO_STA0, 2, &regData[0]);
	*fifoEntriesCount = ((unsigned short int)(regData[1] & 0x07) << 8) | regData[0];
	
	return (regData[1] & 0x38);
}


/******************************************************************
* Description:	1. read Fifo data
*
* Parameters: 	*fifoReadData		fifoDataLength
*               data				data length
*															
* return:	void
*
*
******************************************************************/
void SH3001_FIFO_Read_Data(unsigned char *fifoReadData, unsigned short int fifoDataLength)
{
	if(fifoDataLength > 1024)
	{
		fifoDataLength = 1024;
	}
	
	while(fifoDataLength--)
	{
		SH3001_read(SH3001_FIFO_DATA, 1, fifoReadData);
		fifoReadData++;
	}	
}	


/******************************************************************
* Description:	reset Mater I2C;
*
* Parameters: 	void
*															
* return:	void
* 
******************************************************************/
void SH3001_MI2C_Reset(void)
{
	unsigned char regData = 0;		
	
	SH3001_read(SH3001_MI2C_CONF0, 1, &regData);
	regData |= 0x80;
	SH3001_write(SH3001_MI2C_CONF0, 1, &regData);
	regData &= 0x7F;
	SH3001_write(SH3001_MI2C_CONF0, 1, &regData);	
	
}

/******************************************************************
* Description:	1.master I2C config;
*               2.master I2C clock frequency is (1MHz/(6+3*mi2cFreq));
*               3.mi2cSlaveAddr: slave device address, 7 bits;
*
*
* Parameters: 	mi2cReadMode					mi2cODR							mi2cFreq
*               SH3001_MI2C_READ_MODE_AUTO		SH3001_MI2C_READ_ODR_200HZ		<=15
*               SH3001_MI2C_READ_MODE_MANUAL	SH3001_MI2C_READ_ODR_100HZ
*												SH3001_MI2C_READ_ODR_50HZ		
*											    SH3001_MI2C_READ_ODR_25HZ																							
* return:	void
* 
******************************************************************/
void SH3001_MI2C_Bus_Config(	unsigned char mi2cReadMode,
                                unsigned char mi2cODR,
                                unsigned char mi2cFreq)
{
	unsigned char regData = 0;		
	
	if(mi2cFreq > 15)
	{
		mi2cFreq = 15;
	}
	
	SH3001_read(SH3001_MI2C_CONF1, 1, &regData);
	regData = (regData &0xC0) | (mi2cODR & 0x30) | mi2cFreq;
	SH3001_write(SH3001_MI2C_CONF1, 1, &regData);	
	
	SH3001_read(SH3001_MI2C_CONF0, 1, &regData);
	regData = (regData & 0xBF) | mi2cReadMode;
	SH3001_write(SH3001_MI2C_CONF0, 1, &regData);		
}



/******************************************************************
* Description:	1.master I2C address and command config;
*               2.mi2cSlaveAddr: slave device address, 7 bits;
*
* Parameters: 	mi2cSlaveAddr		mi2cSlaveCmd		mi2cReadMode
*               (1 Byte)			(1 Byte)			SH3001_MI2C_READ_MODE_AUTO
*                                                       SH3001_MI2C_READ_MODE_MANUAL
*																						
* return:	void
* 
******************************************************************/
void SH3001_MI2C_Cmd_Config(	unsigned char mi2cSlaveAddr,
                                unsigned char mi2cSlaveCmd,
                                unsigned char mi2cReadMode)
{
	unsigned char regData = 0;	

	SH3001_read(SH3001_MI2C_CONF0, 1, &regData);
	regData = (regData & 0xBF) | mi2cReadMode;
	SH3001_write(SH3001_MI2C_CONF0, 1, &regData);
	
	regData = mi2cSlaveAddr << 1;
	SH3001_write(SH3001_MI2C_CMD0, 1, &regData);
	SH3001_write(SH3001_MI2C_CMD1, 1, &mi2cSlaveCmd);																																							
}


/******************************************************************
* Description:	1.master I2C write data fucntion;
*               2.mi2cWriteData: write data;
*
* Parameters: 	mi2cWriteData
*																						
* return:	SH3001_TRUE or SH3001_FALSE
* 
******************************************************************/
unsigned char SH3001_MI2C_Write(unsigned char mi2cWriteData)
{
	unsigned char regData = 0;	
	unsigned char i = 0;

	SH3001_write(SH3001_MI2C_WR, 1, &mi2cWriteData);
		
	//Master I2C enable, write-operation
	SH3001_read(SH3001_MI2C_CONF0, 1, &regData);
	regData = (regData & 0xFC) | 0x02;
	SH3001_write(SH3001_MI2C_CONF0, 1, &regData);
	
	// wait write-operation to end
	while(i++ < 20)
	{	
		SH3001_read(SH3001_MI2C_CONF0, 1, &regData);
		if(regData & 0x30)
			break;
	}
		
	if((regData & 0x30) == SH3001_MI2C_SUCCESS)
	{
		return (SH3001_TRUE);	
	}	
	else
	{
		return (SH3001_FALSE);	
	}		
}																

/******************************************************************
* Description:	1.master I2C read data fucntion;
*               2.*mi2cReadData: read data;
*
* Parameters: 	*mi2cReadData
*																						
* return:	SH3001_TRUE or SH3001_FALSE
* 
******************************************************************/
unsigned char SH3001_MI2C_Read(unsigned char *mi2cReadData)
{
	unsigned char regData = 0;			
	unsigned char i = 0;
		
	
	SH3001_read(SH3001_MI2C_CONF0, 1, &regData);
	if((regData & 0x40) == 0)
	{	
		//Master I2C enable, read-operation
		regData |= 0x03;
		SH3001_write(SH3001_MI2C_CONF0, 1, &regData);
	}
	
	// wait read-operation to end
	while(i++ < 20)
	{	
		SH3001_read(SH3001_MI2C_CONF0, 1, &regData);
		if(regData & 0x30)
			break;
	}	

	SH3001_read(SH3001_MI2C_CONF0, 1, &regData);
	if((regData & 0x30) == SH3001_MI2C_SUCCESS)
	{
		SH3001_read(SH3001_MI2C_RD, 1, &regData);
		*mi2cReadData = regData;
		
		return (SH3001_TRUE);	
	}	
	else
	{
		return (SH3001_FALSE);	
	}		
}	





/******************************************************************
* Description:	1.SPI interface config;  Default: SH3001_SPI_4_WIRE
*
* Parameters: 	spiInterfaceMode
*               SH3001_SPI_3_WIRE
*               SH3001_SPI_4_WIRE
*
* return:	void
* 
******************************************************************/
void SH3001_SPI_Config(	unsigned char spiInterfaceMode)
{
	unsigned char regData = spiInterfaceMode;		
	
	SH3001_write(SH3001_SPI_CONF, 1, &regData);
} 






/******************************************************************
* Description:	read some compensation coefficients
*
* Parameters: 	compCoefType
*
* return:	void
* 
******************************************************************/
static void SH3001_CompInit(compCoefType *compCoef)
{				
	unsigned char coefData[2] = {0};
	
	// Acc Cross
	SH3001_read(0x81, 2, coefData);	
	compCoef->cYX = (signed char)coefData[0];
	compCoef->cZX = (signed char)coefData[1];
	SH3001_read(0x91, 2, coefData);	
	compCoef->cXY = (signed char)coefData[0];
	compCoef->cZY = (signed char)coefData[1];
	SH3001_read(0xA1, 2, coefData);	
	compCoef->cXZ = (signed char)coefData[0];
	compCoef->cYZ = (signed char)coefData[1];			
	
	// Gyro Zero
	SH3001_read(0x60, 1, coefData);	
	compCoef->jX = (signed char)coefData[0];
	SH3001_read(0x68, 1, coefData);	
	compCoef->jY = (signed char)coefData[0];
	SH3001_read(0x70, 1, coefData);	
	compCoef->jZ = (signed char)coefData[0];
	
	SH3001_read(SH3001_GYRO_CONF3, 1, coefData);
	coefData[0] = coefData[0] & 0x07;
	compCoef->xMulti = ((coefData[0] < 2) || (coefData[0] >= 7)) ? 1 : (1 << (6 - coefData[0]));	
	SH3001_read(SH3001_GYRO_CONF4, 1, coefData);
	coefData[0] = coefData[0] & 0x07;
	compCoef->yMulti = ((coefData[0] < 2) || (coefData[0] >= 7)) ? 1 : (1 << (6 - coefData[0]));	
	SH3001_read(SH3001_GYRO_CONF5, 1, coefData);
	coefData[0] = coefData[0] & 0x07;
	compCoef->zMulti = ((coefData[0] < 2) || (coefData[0] >= 7)) ? 1 : (1 << (6 - coefData[0]));	
			
	SH3001_read(0x2E, 1, coefData);		
	compCoef->paramP0 = coefData[0] & 0x1F;		
}





/******************************************************************
* Description: 	SH3001 Acc self-test function
*
* Parameters:	void
*																						
* return:	SH3001_TRUE or SH3001_FALSE
*
******************************************************************/
unsigned char SH3001_AccSelfTest(void)
{					
	unsigned char regAddr[15] = {0x25, 0x64, 0x65, 0x66, 0x67, 0x6C, 0x6D, 0x6E, \
								0x6F, 0x74, 0x75, 0x76, 0x77, 0xCB, 0xCC};
	unsigned char regInit[15] = {0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, \
								0x00, 0x00, 0x04, 0x00, 0x00, 0x02, 0xFC};
	unsigned char regBack[15] = {0};
	unsigned char accTemp[6] = {0};
	signed int accSum[3] = {0};
	signed short accAvg1[3] = {0}, accAvg2[3] = {0};
	unsigned char i = 0;
	signed short accRange[3] = {0};
	
	
	for(i=0; i<15; i++)
	{
		SH3001_read(regAddr[i], 1, &regBack[i]);
	}

	for(i=0; i<15; i++)
	{
		SH3001_write(regAddr[i], 1, &regInit[i]);
	}
	
	delay_ms(200);
	
	i = 0;
	while(i++ < 20)
	{	
		SH3001_read(SH3001_ACC_XL, 6, accTemp);
		accSum[0] += ((short)accTemp[1] << 8) | accTemp[0];
		accSum[1] += ((short)accTemp[3] << 8) | accTemp[2];
		accSum[2] += ((short)accTemp[5] << 8) | accTemp[4];			
	}
	accAvg1[0] = accSum[0]/20;
	accAvg1[1] = accSum[1]/20;
	accAvg1[2] = accSum[2]/20;
		
	
	accTemp[0] = 0x03;
	SH3001_write(0xCB, 1, &accTemp[0]);
	delay_ms(500);
	
	accSum[0] = 0;
	accSum[1] = 0;
	accSum[2] = 0;
	i = 0;
	while(i++ < 20)
	{	
		SH3001_read(SH3001_ACC_XL, 6, accTemp);
		accSum[0] += ((short)accTemp[1] << 8) | accTemp[0];
		accSum[1] += ((short)accTemp[3] << 8) | accTemp[2];
		accSum[2] += ((short)accTemp[5] << 8) | accTemp[4];			
	}
	accAvg2[0] = accSum[0]/20;
	accAvg2[1] = accSum[1]/20;
	accAvg2[2] = accSum[2]/20;

		
	for(i=0; i<15; i++)
	{
		SH3001_write(regAddr[i], 1, &regBack[i]);		
	}
	
	accRange[0] = accAvg2[0] - accAvg1[0];
	accRange[1] = accAvg2[1] - accAvg1[1];
	accRange[2] = accAvg2[2] - accAvg1[2];
	
	if(((accRange[0] > 1700) && (accRange[0] < 3000))
		&& ((accRange[1] > 2000) && (accRange[1] < 3500))
		&& ((accRange[2] > 2500) && (accRange[2] < 4200)))
	{
		return(SH3001_TRUE);
	}	
	else
	{
		return(SH3001_FALSE);
	}	
}


/******************************************************************
* Description: 	SH3001 Gyro self-test function
*
* Parameters:	void
*																						
* return:	SH3001_TRUE or SH3001_FALSE
*
******************************************************************/
unsigned char SH3001_GyroSelfTest(void)
{					
	unsigned char regAddr[17] = {0x80, 0x90, 0xA0, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, \
								0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xCE, 0xDC};
	unsigned char regInit[17] = {0x05, 0x05, 0x05, 0x0C, 0x0C, 0x0C, 0x00, 0x00, \
								0x00, 0x40, 0x40, 0x40, 0x08, 0x08, 0x08, 0x90, 0xE0};
	unsigned char regBack[17] = {0};
	unsigned char gyroTemp[6] = {0};
	signed int quadSum[3] = {0};
	signed short quadAvg1[3] = {0}, quadAvg2[3] = {0};
	unsigned char i = 0;
	signed short gyroRange[3] = {0};
		
	for(i=0; i<17; i++)
	{
		SH3001_read(regAddr[i], 1, &regBack[i]);	
	}

	for(i=0; i<17; i++)
	{
		SH3001_write(regAddr[i], 1, &regInit[i]);		
	}
	
	delay_ms(200);
	
	i = 0;
	while(i++ < 20)
	{	
		SH3001_read(0x1A, 6, gyroTemp);
		quadSum[0] += ((short)gyroTemp[3] << 8) | gyroTemp[0];
		quadSum[1] += ((short)gyroTemp[1] << 8) | gyroTemp[4];
		quadSum[2] += ((short)gyroTemp[5] << 8) | gyroTemp[2];
	}
	quadAvg1[0] = quadSum[0]/20;
	quadAvg1[1] = quadSum[1]/20;
	quadAvg1[2] = quadSum[2]/20;
	
	gyroTemp[0] = 0x10;
	SH3001_write(0xB9, 1, &gyroTemp[0]);
	SH3001_write(0xBA, 1, &gyroTemp[0]);
	SH3001_write(0xBB, 1, &gyroTemp[0]);
	delay_ms(200);
	
	quadSum[0] = 0;
	quadSum[1] = 0;
	quadSum[2] = 0;
	i = 0;
	while(i++ < 20)
	{	
		SH3001_read(0x1A, 6, gyroTemp);
		quadSum[0] += ((short)gyroTemp[3] << 8) | gyroTemp[0];
		quadSum[1] += ((short)gyroTemp[1] << 8) | gyroTemp[4];
		quadSum[2] += ((short)gyroTemp[5] << 8) | gyroTemp[2];		
	}
	quadAvg2[0] = quadSum[0]/20;
	quadAvg2[1] = quadSum[1]/20;
	quadAvg2[2] = quadSum[2]/20;
	
	for(i=0; i<17; i++)
	{
		SH3001_write(regAddr[i], 1, &regBack[i]);		
	}
	
	gyroRange[0] = quadAvg1[0] - quadAvg2[0];
	gyroRange[1] = quadAvg1[1] - quadAvg2[1];
	gyroRange[2] = quadAvg1[2] - quadAvg2[2];
		
	
	if(((gyroRange[0] > 0) && (gyroRange[0] < 2000))
		&& ((gyroRange[1] > 500) && (gyroRange[1] < 2800))
		&& ((gyroRange[2] > 1000) && (gyroRange[2] < 3800)))
	{
		return(SH3001_TRUE);
	}	
	else
	{
		return(SH3001_FALSE);
	}		
}


/******************************************************************
* Description: 	SH3001 initialization function
*
* Parameters:	void
*																						
* return:	SH3001_TRUE or SH3001_FALSE
*
******************************************************************/
unsigned char SH3001_init(void) 
{
    unsigned char regData = 0;
	unsigned char i = 0;
	
	// SH3001 chipID = 0x61;	
	while((regData != 0x61) && (i++ < 3))
	{
		SH3001_read(SH3001_CHIP_ID, 1, &regData);	
		if((i == 3) && (regData != 0x61))
		{
			printk("SH3001 init fail 0x%x\n", regData);
			return SH3001_FALSE;
		}	
	}
		
	// reset internal module
	SH3001_ModuleReset();
			
	// 500Hz, 16G, cut off Freq(BW)=500*0.25Hz=125Hz, enable filter;
    SH3001_Acc_Config(SH3001_ODR_500HZ, 
                      SH3001_ACC_RANGE_16G,		
                      SH3001_ACC_ODRX025,
                      SH3001_ACC_FILTER_EN);
	
	// 500Hz, X\Y\Z 2000deg/s, cut off Freq(BW)=181Hz, enable filter;
	SH3001_Gyro_Config(	SH3001_ODR_500HZ, 
                        SH3001_GYRO_RANGE_2000, 
                        SH3001_GYRO_RANGE_2000,
                        SH3001_GYRO_RANGE_2000,
                        SH3001_GYRO_ODRX00,
                        SH3001_GYRO_FILTER_EN);	
	
	// temperature ODR is 63Hz, enable temperature measurement
	SH3001_Temp_Config(SH3001_TEMP_ODR_63, SH3001_TEMP_EN);
	
	SH3001_read(SH3001_CHIP_ID1, 1, &regData);
	if(regData == 0x61)
	{	
		// read compensation coefficient
		SH3001_CompInit(&compCoef);
	}
					
	return SH3001_TRUE;
}

/******************************************************************
* Description: 	Read SH3001 accelerometer data
*
* Parameters:	accData[3]: acc X,Y,Z;
*																						
* return:	void
* 
******************************************************************/
void SH3001_GetImuAccData( short accData[3] )
{
	unsigned char regData[6]={0};	
		
	SH3001_read(SH3001_ACC_XL, 6, regData);
	accData[0] = ((short)regData[1] << 8) | regData[0];
	accData[1] = ((short)regData[3] << 8) | regData[2];
	accData[2] = ((short)regData[5] << 8) | regData[4];		


	SENODIADBG("sh3001 acc raw data: x = %d, y = %d, z = %d",
			accData[0], accData[1], accData[2]);
}

/******************************************************************
* Description: 	Read SH3001 gyroscope data
*
* Parameters:	gyroData[3]: gyro X,Y,Z;
*																						
* return:	void
* 
******************************************************************/
void SH3001_GetImuGyroData( short gyroData[3] )
{
	unsigned char regData[6]={0};	
		
	SH3001_read(SH3001_GYRO_XL, 6, regData);
	gyroData[0] = ((short)regData[1] << 8) | regData[0];
	gyroData[1] = ((short)regData[3] << 8) | regData[2];
	gyroData[2] = ((short)regData[5] << 8) | regData[4];		

	SENODIADBG("sh3001 gyro raw data: rx = %d, ry = %d, rz = %d",
			gyroData[0], gyroData[1], gyroData[2]);
}

/******************************************************************
* Description: 	Read SH3001 gyroscope and accelerometer data
*
* Parameters:	accData[3]: acc X,Y,Z;  gyroData[3]: gyro X,Y,Z;
*																						
* return:	void
* 
******************************************************************/
void SH3001_GetImuData( short accData[3], short gyroData[3] )
{
	unsigned char regData[12]={0};	
		
	SH3001_read(SH3001_ACC_XL, 12, regData);
	accData[0] = ((short)regData[1] << 8) | regData[0];
	accData[1] = ((short)regData[3] << 8) | regData[2];
	accData[2] = ((short)regData[5] << 8) | regData[4];		
	
	gyroData[0] = ((short)regData[7] << 8) | regData[6];
	gyroData[1] = ((short)regData[9] << 8) | regData[8];
	gyroData[2] = ((short)regData[11] << 8) | regData[10];

	SENODIADBG("sh3001 raw data: %d %d %d %d %d %d",
		accData[0], accData[1], accData[2], gyroData[0], gyroData[1], gyroData[2]);
}

#if 0
/******************************************************************
* Description: 	1.Read SH3001 gyroscope and accelerometer data after compensation
*				2.Only When the chipID is 0x61 at register 0xDF, can use SH3001_GetImuCompData() to get sensor data;
*
* Parameters:	accData[3]: acc X,Y,Z;  gyroData[3]: gyro X,Y,Z;
*            	 
*																						
* return:	void
* 
******************************************************************/
void SH3001_GetImuCompData(short accData[3], short gyroData[3])
{
	unsigned char regData[15]={0};		
	unsigned char paramP;
	int accTemp[3], gyroTemp[3];
					
	SH3001_read(SH3001_ACC_XL, 15, regData);
	accData[0] = ((short)regData[1] << 8) | regData[0];
	accData[1] = ((short)regData[3] << 8) | regData[2];
	accData[2] = ((short)regData[5] << 8) | regData[4];			
	gyroData[0] = ((short)regData[7] << 8) | regData[6];
	gyroData[1] = ((short)regData[9] << 8) | regData[8];
	gyroData[2] = ((short)regData[11] << 8) | regData[10];	
	paramP = regData[14] & 0x1F;
		
	// Acc Cross
	accTemp[0] = (int)(	accData[0] + \
						accData[1] * ((float)compCoef.cXY/1024.0f) + \
						accData[2] * ((float)compCoef.cXZ/1024.0f) );
				 
	accTemp[1] = (int)( accData[0] * ((float)compCoef.cYX/1024.0f) + \
						accData[1] + \
						accData[2] * ((float)compCoef.cYZ/1024.0f) );
				 
	accTemp[2] = (int)( accData[0] * ((float)compCoef.cZX/1024.0f) + \
						accData[1] * ((float)compCoef.cZY/1024.0f) + \
						accData[2] );	
	
	if(accTemp[0] > 32767)
		accTemp[0] = 32767;
	else if(accTemp[0] < -32768)
		accTemp[0] = -32768;
	if(accTemp[1] > 32767)
		accTemp[1] = 32767;
	else if(accTemp[1] < -32768)
		accTemp[1] = -32768;	
	if(accTemp[2] > 32767)
		accTemp[2] = 32767;
	else if(accTemp[2] < -32768)
		accTemp[2] = -32768;			 
	accData[0] = (short)accTemp[0];
	accData[1] = (short)accTemp[1];
	accData[2] = (short)accTemp[2];		

	gyroTemp[0] = gyroData[0] - (paramP - compCoef.paramP0) * compCoef.jX * compCoef.xMulti;
	gyroTemp[1] = gyroData[1] - (paramP - compCoef.paramP0) * compCoef.jY * compCoef.yMulti;
	gyroTemp[2] = gyroData[2] - (paramP - compCoef.paramP0) * compCoef.jZ * compCoef.zMulti;
	if(gyroTemp[0] > 32767)
		gyroTemp[0] = 32767;
	else if(gyroTemp[0] < -32768)
		gyroTemp[0] = -32768;
	if(gyroTemp[1] > 32767)
		gyroTemp[1] = 32767;
	else if(gyroTemp[1] < -32768)
		gyroTemp[1] = -32768;
	if(gyroTemp[2] > 32767)
		gyroTemp[2] = 32767;
	else if(gyroTemp[2] < -32768)
		gyroTemp[2] = -32768;
	
	// Gyro Zero			 				 		
	gyroData[0] = (short)gyroTemp[0];
	gyroData[1] = (short)gyroTemp[1];
	gyroData[2] = (short)gyroTemp[2];							 

	printk("%d %d %d %d %d %d\n", accData[0], accData[1], accData[2], gyroData[0], gyroData[1], gyroData[2]);
}
#endif

/******************************************************************
*
* Description: 	Linux adaptation sh3001 function
*
******************************************************************/
/* Delayed work queue callback function */
static void sh3001_input_func(struct work_struct *work)
{
	u8 enSensors = sh3001->enSensors;

	if (enSensors == SH3001_ACC_ENABLE) {
		SH3001_GetImuAccData(imu_data.acc);
		input_report_abs(sh3001->input_dev, ABS_X, imu_data.acc[0]);
		input_report_abs(sh3001->input_dev, ABS_Y, imu_data.acc[1]);
		input_report_abs(sh3001->input_dev, ABS_Z, imu_data.acc[2]);
	} else if (enSensors == SH3001_GYR_ENABLE) {
		SH3001_GetImuGyroData(imu_data.gyro);
		input_report_abs(sh3001->input_dev, ABS_RX, imu_data.gyro[0]);
		input_report_abs(sh3001->input_dev, ABS_RY, imu_data.gyro[1]);
		input_report_abs(sh3001->input_dev, ABS_RZ, imu_data.gyro[2]);
	} else if (enSensors == SH3001_ACCGYR_ENABLE) {
		SH3001_GetImuData(imu_data.acc, imu_data.gyro);
		input_report_abs(sh3001->input_dev, ABS_X, imu_data.acc[0]);
		input_report_abs(sh3001->input_dev, ABS_Y, imu_data.acc[1]);
		input_report_abs(sh3001->input_dev, ABS_Z, imu_data.acc[2]);
		input_report_abs(sh3001->input_dev, ABS_RX, imu_data.gyro[0]);
		input_report_abs(sh3001->input_dev, ABS_RY, imu_data.gyro[1]);
		input_report_abs(sh3001->input_dev, ABS_RZ, imu_data.gyro[2]);
	}
	input_sync(sh3001->input_dev);

	schedule_delayed_work(&sh3001->work, msecs_to_jiffies(sh3001->delay));
}

static int sh3001_open(struct inode *inode, struct file *file)
{
	if (!atomic_read(&sh3001->open_flag)) {
		dev_info(&sh3001->client->dev, "start sh3001 schedule work\n");
		atomic_set(&sh3001->open_flag, 1);
		schedule_delayed_work(&sh3001->work, msecs_to_jiffies(sh3001->delay));
		return 0;
	}

	return -1;
}

static int sh3001_release(struct inode *inode, struct file *file)
{
	if (atomic_read(&sh3001->open_flag)) {
		dev_info(&sh3001->client->dev, "stop sh3001 schedule work\n");
		cancel_delayed_work(&sh3001->work);
		atomic_set(&sh3001->open_flag, 0);
	}

	return 0;
}

static long sh3001_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case IMU_SET_DELAY:
		if (copy_from_user(&sh3001->delay, argp, sizeof(sh3001->delay)))
			return -EFAULT;
		break;

	case IMU_GET_DELAY:
		if (copy_to_user(argp, &sh3001->delay, sizeof(sh3001->delay)))
			return -EFAULT;
		break;

	case IMU_SET_ENABLE:
		if (copy_from_user(&sh3001->enSensors, argp, sizeof(sh3001->enSensors)))
			return -EFAULT;
		break;

	case IMU_GET_ENABLE:
		if (copy_to_user(argp, &sh3001->enSensors, sizeof(sh3001->enSensors)))
			return -EFAULT;
		break;

	default:
		dev_err(&sh3001->client->dev, "ioctl cmd invalid\n");
		return -EINVAL;
		break;
	}

	return 0;
}

static const struct file_operations sh3001_fops = {
    .owner          = THIS_MODULE,
    .open           = sh3001_open,
    .release        = sh3001_release,
    .unlocked_ioctl = sh3001_ioctl,
};

static ssize_t sh3001_enable_show(struct device *dev,
                       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d (0:all disable, 1:acc enable, 2:gyro enable, 3:all enable)\n",
			sh3001->enSensors);
}

static ssize_t sh3001_enable_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
	int error;
	unsigned long data;

	error = kstrtoul(buf, 10, &data);
	sh3001->enSensors = (int)(data&SH3001_ACCGYR_ENABLE);

	return count;
}

static ssize_t sh3001_delay_show(struct device *dev,
                       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%dms\n", sh3001->delay);
}

static ssize_t sh3001_delay_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
	int error;
	unsigned long data;

	error = kstrtoul(buf, 10, &data);
	sh3001->delay = (int)data;

	return count;
}

static ssize_t sh3001_run_show(struct device *dev,
                       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sh3001->run_flag);
}

static ssize_t sh3001_run_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
	int error;
	unsigned long data;

	error = kstrtoul(buf, 10, &data);

	if (sh3001->run_flag != (int)data) {
		if ((int)data == 1 && !atomic_read(&sh3001->open_flag)) {
			dev_info(&sh3001->client->dev, "start sh3001 schedule work\n");
			atomic_set(&sh3001->open_flag, 1);
			schedule_delayed_work(&sh3001->work,
					msecs_to_jiffies(sh3001->delay));
		} else if ((int)data == 0 && atomic_read(&sh3001->open_flag)) {
			dev_info(&sh3001->client->dev, "stop sh3001 schedule work\n");
			cancel_delayed_work(&sh3001->work);
			atomic_set(&sh3001->open_flag, 0);
		}

		sh3001->run_flag = (int)data;
	}

	return count;
}
static DEVICE_ATTR(enable, 0644, sh3001_enable_show, sh3001_enable_store);
static DEVICE_ATTR(delay, 0644, sh3001_delay_show, sh3001_delay_store);
static DEVICE_ATTR(run, 0644, sh3001_run_show, sh3001_run_store);

static struct attribute *sh3001_attrs[] = {
    &dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_run.attr,
    NULL
};

static const struct attribute_group sh3001_attrs_group = {
	.name   = DEVICE_NAME,
    .attrs = sh3001_attrs,
};

static const struct attribute_group *sh3001_attr_groups[] = {
    &sh3001_attrs_group,
    NULL
};

static struct miscdevice sh3001_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "imu0",
	.fops = &sh3001_fops,
	.groups = sh3001_attr_groups,
};

static int sh3001_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
	dev_info(&client->dev, "%s enter\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "sh3001 check_functionality failed\n");
		return -ENODEV;
	}

	sh3001 = devm_kzalloc(&client->dev, sizeof(*sh3001), GFP_KERNEL);
	if (!sh3001)
		return -ENOMEM;

	sh3001->delay = 1000;
	sh3001->enSensors = SH3001_ACCGYR_ENABLE;
	sh3001->client = client;
	atomic_set(&sh3001->open_flag, 0);
	i2c_set_clientdata(client, sh3001);

	INIT_DELAYED_WORK(&sh3001->work, sh3001_input_func);

	/* Initialize the sh3001 sensor */
	if (SH3001_init() != 0) {
		dev_err(&client->dev, "sh3001 init failed\n");
		return -1;
	}

	/* Declare input device */
	sh3001->input_dev = devm_input_allocate_device(&client->dev);
	if (!sh3001->input_dev) {
		dev_err(&client->dev, "sh3001 allocate input failed\n");
		return -ENOMEM;
	}
	/* Setup input device */
	set_bit(EV_ABS, sh3001->input_dev->evbit);

	input_set_abs_params(sh3001->input_dev, ABS_X, ABSMIN_ACC, ABSMAX_ACC, 0, 0);
	input_set_abs_params(sh3001->input_dev, ABS_Y, ABSMIN_ACC, ABSMAX_ACC, 0, 0);
	input_set_abs_params(sh3001->input_dev, ABS_Z, ABSMIN_ACC, ABSMAX_ACC, 0, 0);

	input_set_abs_params(sh3001->input_dev, ABS_RX, ABSMIN_GYRO, ABSMAX_GYRO, 0, 0);
	input_set_abs_params(sh3001->input_dev, ABS_RY, ABSMIN_GYRO, ABSMAX_GYRO, 0, 0);
	input_set_abs_params(sh3001->input_dev, ABS_RZ, ABSMIN_GYRO, ABSMAX_GYRO, 0, 0);

	sh3001->input_dev->name = "sh3001";

	/* Register */
	if (input_register_device(sh3001->input_dev)) {
		dev_err(&client->dev, "sh3001 register input failed\n");
		return -ENODEV;
	}

	/* Register as a misc device */
	misc_register(&sh3001_device);

	return 0;
}

static int sh3001_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "%s enter\n", __func__);

	misc_deregister(&sh3001_device);
	input_unregister_device(sh3001->input_dev);
	cancel_delayed_work(&sh3001->work);

    return 0;
}

static const struct i2c_device_id sh3001_id[] = {
    { DEVICE_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, sh3001_id);

#ifdef CONFIG_OF
static const struct of_device_id sh3001_of_match[] = {
    { .compatible = "Senodia,sh3001", },
    {  }
};
MODULE_DEVICE_TABLE(of, sh3001_of_match);
#endif

static struct i2c_driver sh3001_driver = {
    .driver = {
        .owner          = THIS_MODULE,
        .name           = DEVICE_NAME,
        .of_match_table = of_match_ptr(sh3001_of_match),
    },
    .id_table = sh3001_id,
    .probe    = sh3001_probe,
    .remove   = sh3001_remove,
};

static int sh3001_match(struct i2c_client *client)
{
	int err = 0;
	u8 chip_id = 0;

	sh3001 = kzalloc(sizeof(*sh3001), GFP_KERNEL);
	if (!sh3001)
		return -ENOMEM;

	sh3001->client = client;
	client->addr = SH3001_ADDRESS;

	err = SH3001_read(SH3001_CHIP_ID, 1, &chip_id);
	kfree(sh3001);
	if (err || chip_id != IMU_CHIP_ID)
		return 0;

	dev_info(&client->dev, "sh3001 chip id:0x%x\n", chip_id);

	return 1;
}

struct imu_dev sh3001_dev = {
	.driver = &sh3001_driver,
	.match = sh3001_match,
	.probe = sh3001_probe,
	.remove = sh3001_remove,
};
