#ifndef __SENSOR_I2C_H__
#define __SENSOR_I2C_H__

/*
 * struct i2c_transfer_struct -
 * i2c write & read struct
 *
 * @client:			pointer to i2c client
 * @reg_bytes:		byte number of register
 * @value_bytes:	byte number of value
 * @reg:			register
 * @value:			value to the register
 */
struct i2c_transfer_struct {
	const struct i2c_client *client;
	int reg_bytes;
	int value_bytes;
	int reg;
	int value;
};

/*
 * i2c_write -
 * i2c write entry
 *
 * @trans:			transfor paramters
 */
int i2c_write(struct i2c_transfer_struct *trans);

/*
 * i2c_read -
 * i2c read entry
 *
 * @trans:			transfor paramters
 */
int i2c_read(struct i2c_transfer_struct *trans);
#endif
