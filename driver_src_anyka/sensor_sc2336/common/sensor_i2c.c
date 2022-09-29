#include <linux/i2c.h>
#include "sensor_i2c.h"

/*define i2c write & read function type*/
typedef int (*i2c_write_type)(const struct i2c_client *client, int reg, int value);
typedef int (*i2c_read_type)(const struct i2c_client *client, int reg);

/*
 * i2c_write_reg2_value1 -
 * register 2 bytes and value 1 byte to write
 *
 * @client[IN]:			pointer to i2c client
 * @reg[IN]:			register
 * @value[IN]:			value to write
 *
 * RETURN:	0-success, others-fail
 */
static int i2c_write_reg2_value1(const struct i2c_client *client, int reg, int value)
{
	unsigned char msg[3];

	msg[0] = reg >> 8;
	msg[1] = reg & 0xff;
	msg[2] = value;

	return i2c_master_send(client, msg, 3);
}

/*
 * i2c_read_reg2_value1 -
 * register 2 bytes and value 1 byte to read
 *
 * @client[IN]:			pointer to i2c client
 * @reg[IN]:			register
 *
 * RETURN:	value to the register
 */
static int i2c_read_reg2_value1(const struct i2c_client *client, int reg)
{
	int ret;
	unsigned char msg[2];
	unsigned char data;

	msg[0] = reg >> 8;
	msg[1] = reg & 0xff;

	ret = i2c_master_send(client, msg, 2);
	if (ret < 0)
		return ret;

	ret = i2c_master_recv(client, &data, 1);
	if (ret < 0)
		return ret;

	return data;
}

/*
 * i2c_write_reg2_value2 -
 * register 2 bytes and value 2 byte to write
 *
 * @client[IN]:			pointer to i2c client
 * @reg[IN]:			register
 * @value[IN]:			value to write
 *
 * RETURN:	0-success, others-fail
 */
static int i2c_write_reg2_value2(const struct i2c_client *client, int reg, int value)
{
	unsigned char msg[4];

	msg[0] = reg >> 8;    //high 8bit first send
	msg[1] = reg & 0xff;  //low 8bit second send
	msg[2] = value >> 8; //high 8bit firt send
	msg[3] = value & 0xff;   //low 8bit second send

	return i2c_master_send(client, msg, 4);
}

/*
 * i2c_read_reg2_value2 -
 * register 2 bytes and value 2 byte to read
 *
 * @client[IN]:			pointer to i2c client
 * @reg[IN]:			register
 *
 * RETURN:	value to the register
 */
static int i2c_read_reg2_value2(const struct i2c_client *client, int reg)
{
	int ret;
	unsigned char msg[4];
	unsigned char buf[2];

	msg[0] = reg >> 8;    //high 8bit first send
	msg[1] = reg & 0xff;  //low 8bit second send

	ret = i2c_master_send(client, msg, 2);
	if (ret < 0)
		return ret;

	ret = i2c_master_recv(client, buf, 2);
	if (ret < 0)
		return ret;

	return (buf[0] << 8) | buf[1];
}

/*
 * i2c_write_reg1_value1 -
 * register 1 bytes and value 1 byte to write
 *
 * @client[IN]:			pointer to i2c client
 * @reg[IN]:			register
 * @value[IN]:			value to write
 *
 * RETURN:	0-success, others-fail
 */
static int i2c_write_reg1_value1(const struct i2c_client *client, int reg, int value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

/*
 * i2c_read_reg1_value1 -
 * register 1 bytes and value 1 byte to read
 *
 * @client[IN]:			pointer to i2c client
 * @reg[IN]:			register
 *
 * RETURN:	value to the register
 */
static int i2c_read_reg1_value1(const struct i2c_client *client, int reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

/*
 * i2c_write_reg1_value2 -
 * register 1 bytes and value 2 byte to write
 *
 * @client[IN]:			pointer to i2c client
 * @reg[IN]:			register
 * @value[IN]:			value to write
 *
 * RETURN:	0-success, others-fail
 */
static int i2c_write_reg1_value2(const struct i2c_client *client, int reg, int value)
{
	return i2c_smbus_write_word_data(client, reg, value);
}

/*
 * i2c_read_reg1_value2 -
 * register 1 bytes and value 2 byte to read
 *
 * @client[IN]:			pointer to i2c client
 * @reg[IN]:			register
 *
 * RETURN:	value to the register
 */
static int i2c_read_reg1_value2(const struct i2c_client *client, int reg)
{
	return i2c_smbus_read_word_data(client, reg);
}

/*all i2c write functions to a array*/
static i2c_write_type i2c_write_table[2][2] = {
	{
		i2c_write_reg1_value1,
		i2c_write_reg1_value2,
	},
	{
		i2c_write_reg2_value1,
		i2c_write_reg2_value2,
	},
};

/*all i2c read functions to a array*/
static i2c_read_type i2c_read_table[2][2] = {
	{
		i2c_read_reg1_value1,
		i2c_read_reg1_value2,
	},
	{
		i2c_read_reg2_value1,
		i2c_read_reg2_value2,
	},
};

/*
 * i2c_write -
 * i2c write entry
 *
 * @trans:			transfor paramters
 */
int i2c_write(struct i2c_transfer_struct *trans)
{
	int reg_bytes;
	int value_bytes;
	i2c_write_type write;

	if (!trans)
		return -1;

	reg_bytes = trans->reg_bytes;
	value_bytes = trans->value_bytes;

	if (reg_bytes < 1 ||
			reg_bytes > 2 ||
			value_bytes < 1 ||
			value_bytes > 2)
		return -1;

	write = i2c_write_table[reg_bytes - 1][value_bytes - 1];

	return write(trans->client, trans->reg, trans->value);
}

/*
 * i2c_read -
 * i2c read entry
 *
 * @trans:			transfor paramters
 */
int i2c_read(struct i2c_transfer_struct *trans)
{
	int reg_bytes;
	int value_bytes;
	i2c_read_type read;

	if (!trans)
		return -1;

	reg_bytes = trans->reg_bytes;
	value_bytes = trans->value_bytes;

	if (reg_bytes < 1 ||
			reg_bytes > 2 ||
			value_bytes < 1 ||
			value_bytes > 2)
		return -1;

	read = i2c_read_table[reg_bytes - 1][value_bytes - 1];

	return read(trans->client, trans->reg);
}
