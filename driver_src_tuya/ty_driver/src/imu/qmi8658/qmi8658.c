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

#include "qmi8658.h"
#include "ty_imu.h"

#define  DEVICE_NAME	"qmi8658"
#define  I2C_RETRY_CNT	3

static qmi8658_data *qmi8658;

struct imu_data{
	short acc[3];
	short gyro[3];
};
volatile static struct imu_data imu_data;

static int qmi8658_write_reg(u8 reg, u8 data)
{
	u8 i = 0;
    u8 buf[2] = {reg, data};
	struct i2c_client *client = qmi8658->client;

    struct i2c_msg msg = {
        .addr  = client->addr,
        .flags = 0,
        .len   = 2,
        .buf   = buf,
    };

	while (i++ < I2C_RETRY_CNT) {
		if (1 == i2c_transfer(client->adapter, &msg, 1))
			break;

		mdelay(10);
	}

	if (i == I2C_RETRY_CNT+1) {
		dev_err(&client->dev, "qmi8658 write rigster error!\n");
		return -1;
	}

    return 0;
}

static int qmi8658_read_reg(u8 reg, u8 *buf, u8 len)
{
	u8 i = 0;
	struct i2c_client *client = qmi8658->client;

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
		dev_err(&client->dev, "qmi8658 read rigster error!\n");
		return -1;
	}

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
//	qmi8658 register configuration
//
/////////////////////////////////////////////////////////////////////////////
/* Configure accelerometer parameters: range, frequency, filter */
static int qmi8658_config_acc(enum qmi8658_AccRange range,
							  enum qmi8658_AccOdr odr,
							  enum qmi8658_LpfConfig lpfEnable,
							  enum qmi8658_StConfig stEnable)
{
	int err = -1;
	u8 ctrl_data = 0;

	// set odr & range
	if (stEnable == Qmi8658St_Enable)
		ctrl_data = (u8)range | (u8)odr | 0x80;
	else
		ctrl_data = (u8)range | (u8)odr;
	err = qmi8658_write_reg(Qmi8658Register_Ctrl2, ctrl_data);

	// set LPF & HPF
	ctrl_data = 0;
	err |= qmi8658_read_reg(Qmi8658Register_Ctrl5, &ctrl_data, 1);
	ctrl_data &= 0xf0;
	if (lpfEnable == Qmi8658Lpf_Enable) {
		ctrl_data |= A_LSP_MODE_3;
		ctrl_data |= 0x01;
	} else {
		ctrl_data &= ~0x01;
	}
	err |= qmi8658_write_reg(Qmi8658Register_Ctrl5, ctrl_data);

	if (err)
		qmi8658_log("qmi8658_config_acc err:%d", err);

	return err;
}

/* Configure gyroscope parameters: range, frequency, filter */
static int qmi8658_config_gyro(enum qmi8658_GyrRange range,
							   enum qmi8658_GyrOdr odr,
							   enum qmi8658_LpfConfig lpfEnable,
							   enum qmi8658_StConfig stEnable)
{
	int err = -1;
	u8 ctrl_data = 0; 

	// set odr & range
	if (stEnable == Qmi8658St_Enable)
		ctrl_data = (u8)range | (u8)odr | 0x80;
	else
		ctrl_data = (u8)range | (u8)odr;
	err = qmi8658_write_reg(Qmi8658Register_Ctrl3, ctrl_data);

	// set LPF & HPF
	ctrl_data = 0;
	err |= qmi8658_read_reg(Qmi8658Register_Ctrl5, &ctrl_data, 1);
	ctrl_data &= 0x0f;
	if (lpfEnable == Qmi8658Lpf_Enable) {
		ctrl_data |= G_LSP_MODE_3;
		ctrl_data |= 0x10;
	} else {
		ctrl_data &= ~0x10;
	}
	err |= qmi8658_write_reg(Qmi8658Register_Ctrl5, ctrl_data);

	if (err)
		qmi8658_log("qmi8658_config_gyro err:%d", err);

	return err;
}

/* Read acceleration raw data */
static int qmi8658_read_acc(volatile short acc_xyz[3])
{
	u8 status0 = 0;
	u8 buf_reg[6] = {0};

	qmi8658_read_reg(Qmi8658Register_Status0, &status0, 1);
	if (status0 & QMI8658_ACCDATA_READY)
		qmi8658_read_reg(Qmi8658Register_Ax_L, buf_reg, 6);
	else
		return -1;

	imu_data.acc[0] = (buf_reg[1] << 8) | (buf_reg[0]);
	imu_data.acc[1] = (buf_reg[3] << 8) | (buf_reg[2]);
	imu_data.acc[2] = (buf_reg[5] << 8) | (buf_reg[4]);

	return 0;
}

/* Read gyroscope raw data */
static int qmi8658_read_gyro(volatile short gyro_xyz[3])
{
	u8 status0 = 0;
	u8 buf_reg[6] = {0};

	qmi8658_read_reg(Qmi8658Register_Status0, &status0, 1);
	if (status0 & QMI8658_GYRODATA_READY)
		qmi8658_read_reg(Qmi8658Register_Gx_L, buf_reg, 6);
	else
		return -1;

	gyro_xyz[0] = (buf_reg[1] << 8) | (buf_reg[0]);
	gyro_xyz[1] = (buf_reg[3] << 8) | (buf_reg[2]);
	gyro_xyz[2] = (buf_reg[5] << 8) | (buf_reg[4]);

	return 0;
}

/* Read raw data of acceleration and gyroscope */
static int qmi8658_read_acc_gyro(volatile short acc_xyz[3],
								volatile short gyro_xyz[3])
{
	u8 status0 = 0;
	u8 buf_reg[12] = {0};

	qmi8658_read_reg(Qmi8658Register_Status0, &status0, 1);
	if (status0 & QMI8658_ACCGYRO_READY)
		qmi8658_read_reg(Qmi8658Register_Ax_L, buf_reg, 12);
	else
		return -1;

	acc_xyz[0] = (buf_reg[1] << 8) | (buf_reg[0]);
	acc_xyz[1] = (buf_reg[3] << 8) | (buf_reg[2]);
	acc_xyz[2] = (buf_reg[5] << 8) | (buf_reg[4]);

	gyro_xyz[0] = (buf_reg[7] << 8) | (buf_reg[6]);
	gyro_xyz[1] = (buf_reg[9] << 8) | (buf_reg[8]);
	gyro_xyz[2] = (buf_reg[11] << 8) | (buf_reg[10]);

	return 0;
}

/* Get qmi8658 version number and other information */
static int qmi8658_check_id(void)
{
	int err = 0;
	u8 chip_id = 0;
	u8 revision_id = 0;
	u8 firmware_id[3];

	err = qmi8658_read_reg(Qmi8658Register_WhoAmI, &chip_id, 1);
	if (err || chip_id != QMI8658_CHIP_ID) {
		return -1;
	}

	qmi8658_read_reg(Qmi8658Register_Revision, &revision_id, 1);
	qmi8658_read_reg(Qmi8658Register_firmware_id, firmware_id, 3);

	qmi8658_log("chip id=0x%x, revision id=0x%x, firmware id=[%d.%d.%d]",
		chip_id, revision_id, firmware_id[0], firmware_id[1], firmware_id[2]);

	return 0;
}

static void qmi8658_soft_reset(void)
{
	qmi8658_write_reg(Qmi8658Register_Reset, 0xb0);
	mdelay(10);
}

/* 0: all disable, 1: enable acceleration, 2: enable gyroscope, 3: all enable */
static void qmi8658_enableSensors(unsigned char enableFlags)
{
	qmi8658_write_reg(Qmi8658Register_Ctrl7, enableFlags);
	qmi8658->cfg.enSensors = enableFlags & QMI8658_ACCGYR_ENABLE;
	mdelay(1);
}

static int qmi8658_config_reg(void)
{
	int err = -1;

	qmi8658_write_reg(Qmi8658Register_Ctrl1, 0x60 | QMI8658_INT2_ENABLE | QMI8658_INT1_ENABLE);

	qmi8658_enableSensors(QMI8658_DISABLE_ALL);

	err = qmi8658_config_acc(Qmi8658AccRange_16g, Qmi8658AccOdr_500Hz,
							Qmi8658Lpf_Enable, Qmi8658St_Disable);
	err |= qmi8658_config_gyro(Qmi8658GyrRange_2048dps, Qmi8658GyrOdr_500Hz,
							Qmi8658Lpf_Enable, Qmi8658St_Disable);

	qmi8658_enableSensors(QMI8658_ACCGYR_ENABLE);

	return err;
}

static int qmi8658_init(void)
{
	if (qmi8658_check_id())
		return -1;

	qmi8658_soft_reset();

	return qmi8658_config_reg();
}

/////////////////////////////////////////////////////////////////////////////
//
//	qmi8658 attributes
//
/////////////////////////////////////////////////////////////////////////////
/* Delayed work queue callback function */
static void qmi8658_input_func(struct work_struct *work)
{
	u8 enSensors = qmi8658->cfg.enSensors;

	if (enSensors == QMI8658_ACC_ENABLE) {
		qmi8658_read_acc(imu_data.acc);
		input_report_abs(qmi8658->input_dev, ABS_X, imu_data.acc[0]);
		input_report_abs(qmi8658->input_dev, ABS_Y, imu_data.acc[1]);
		input_report_abs(qmi8658->input_dev, ABS_Z, imu_data.acc[2]);
	} else if (enSensors == QMI8658_GYR_ENABLE) {
		qmi8658_read_gyro(imu_data.gyro);
		input_report_abs(qmi8658->input_dev, ABS_RX, imu_data.gyro[0]);
		input_report_abs(qmi8658->input_dev, ABS_RY, imu_data.gyro[1]);
		input_report_abs(qmi8658->input_dev, ABS_RZ, imu_data.gyro[2]);
	} else if (enSensors == QMI8658_ACCGYR_ENABLE) {
		qmi8658_read_acc_gyro(imu_data.acc, imu_data.gyro);
		input_report_abs(qmi8658->input_dev, ABS_X, imu_data.acc[0]);
		input_report_abs(qmi8658->input_dev, ABS_Y, imu_data.acc[1]);
		input_report_abs(qmi8658->input_dev, ABS_Z, imu_data.acc[2]);
		input_report_abs(qmi8658->input_dev, ABS_RX, imu_data.gyro[0]);
		input_report_abs(qmi8658->input_dev, ABS_RY, imu_data.gyro[1]);
		input_report_abs(qmi8658->input_dev, ABS_RZ, imu_data.gyro[2]);
	}
	input_sync(qmi8658->input_dev);

	schedule_delayed_work(&qmi8658->work, msecs_to_jiffies(qmi8658->delay));
}

static int qmi8658_open(struct inode *inode, struct file *file)
{
	if (!atomic_read(&qmi8658->open_flag)) {
		dev_info(&qmi8658->client->dev, "start qmi8658 schedule work\n");
		atomic_set(&qmi8658->open_flag, 1);
		schedule_delayed_work(&qmi8658->work, msecs_to_jiffies(qmi8658->delay));
		return 0;
	}

	return -1;
}

static int qmi8658_release(struct inode *inode, struct file *file)
{
	if (atomic_read(&qmi8658->open_flag)) {
		dev_info(&qmi8658->client->dev, "stop qmi8658 schedule work\n");
		cancel_delayed_work(&qmi8658->work);
		atomic_set(&qmi8658->open_flag, 0);
	}

	return 0;
}

static long qmi8658_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case IMU_SET_DELAY:
		if (copy_from_user(&qmi8658->delay, argp, sizeof(qmi8658->delay)))
			return -EFAULT;
		break;

	case IMU_GET_DELAY:
		if (copy_to_user(argp, &qmi8658->delay, sizeof(qmi8658->delay)))
			return -EFAULT;
		break;

	case IMU_SET_ENABLE:
		if (copy_from_user(&qmi8658->cfg.enSensors, argp, sizeof(qmi8658->cfg.enSensors)))
			return -EFAULT;
		break;

	case IMU_GET_ENABLE:
		if (copy_to_user(argp, &qmi8658->cfg.enSensors, sizeof(qmi8658->cfg.enSensors)))
			return -EFAULT;
		break;

	default:
		dev_err(&qmi8658->client->dev, "ioctl cmd invalid\n");
		return -EINVAL;
		break;
	}

	return 0;
}

static const struct file_operations qmi8658_fops = {
    .owner          = THIS_MODULE,
    .open           = qmi8658_open,
    .release        = qmi8658_release,
    .unlocked_ioctl = qmi8658_ioctl,
};

static ssize_t qmi8658_enable_show(struct device *dev,
                       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d (0:all disable, 1:acc enable, 2:gyro enable, 3:all enable)\n",
			qmi8658->cfg.enSensors);
}

static ssize_t qmi8658_enable_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
	int error;
	u8 old_enSensors = qmi8658->cfg.enSensors;
	unsigned long data;

	error = kstrtoul(buf, 10, &data);
	data &= QMI8658_ACCGYR_ENABLE;
	if ((u8)data != old_enSensors)
		qmi8658_enableSensors((u8)data);

	return count;
}

static ssize_t qmi8658_delay_show(struct device *dev,
                       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%dms\n", qmi8658->delay);
}

static ssize_t qmi8658_delay_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
	int error;
	unsigned long data;

	error = kstrtoul(buf, 10, &data);
	qmi8658->delay = (int)data;

	return count;
}

static ssize_t qmi8658_run_show(struct device *dev,
                       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", qmi8658->run_flag);
}

static ssize_t qmi8658_run_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
	int error;
	unsigned long data;

	error = kstrtoul(buf, 10, &data);

	if (qmi8658->run_flag != (int)data) {
		if ((int)data == 1 && !atomic_read(&qmi8658->open_flag)) {
			dev_info(&qmi8658->client->dev, "start qmi8658 schedule work\n");
			atomic_set(&qmi8658->open_flag, 1);
			schedule_delayed_work(&qmi8658->work,
					msecs_to_jiffies(qmi8658->delay));
		} else if ((int)data == 0 && atomic_read(&qmi8658->open_flag)) {
			dev_info(&qmi8658->client->dev, "stop qmi8658 schedule work\n");
			cancel_delayed_work(&qmi8658->work);
			atomic_set(&qmi8658->open_flag, 0);
		}

		qmi8658->run_flag = (int)data;
	}

	return count;
}
static DEVICE_ATTR(enable, 0644, qmi8658_enable_show, qmi8658_enable_store);
static DEVICE_ATTR(delay, 0644, qmi8658_delay_show, qmi8658_delay_store);
static DEVICE_ATTR(run, 0644, qmi8658_run_show, qmi8658_run_store);

static struct attribute *qmi8658_attrs[] = {
    &dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_run.attr,
    NULL
};

static const struct attribute_group qmi8658_attrs_group = {
	.name   = "qmi8658",
    .attrs = qmi8658_attrs,
};

static const struct attribute_group *qmi8658_attr_groups[] = {
    &qmi8658_attrs_group,
    NULL
};

static struct miscdevice qmi8658_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "imu0",
	.fops = &qmi8658_fops,
	.groups = qmi8658_attr_groups,
};

static int qmi8658_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
	dev_info(&client->dev, "%s enter\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "qmi8658 check_functionality failed\n");
		return -ENODEV;
	}

	qmi8658 = devm_kzalloc(&client->dev, sizeof(*qmi8658), GFP_KERNEL);
	if (!qmi8658)
		return -ENOMEM;

	qmi8658->delay = 1000;
	qmi8658->client = client;
	atomic_set(&qmi8658->open_flag, 0);
	i2c_set_clientdata(client, qmi8658);

	INIT_DELAYED_WORK(&qmi8658->work, qmi8658_input_func);

    /* Initialize the qmi8658 sensor */
    if (qmi8658_init() != 0) {
        dev_err(&client->dev, "qmi8658 init failed\n");
        return -1;
    }

	/* Declare input device */
	qmi8658->input_dev = devm_input_allocate_device(&client->dev);
	if (!qmi8658->input_dev) {
		dev_err(&client->dev, "qmi8658 allocate input failed\n");
		return -ENOMEM;
	}
	/* Setup input device */
	set_bit(EV_ABS, qmi8658->input_dev->evbit);

	input_set_abs_params(qmi8658->input_dev, ABS_X, ABSMIN_ACC, ABSMAX_ACC, 0, 0);
	input_set_abs_params(qmi8658->input_dev, ABS_Y, ABSMIN_ACC, ABSMAX_ACC, 0, 0);
	input_set_abs_params(qmi8658->input_dev, ABS_Z, ABSMIN_ACC, ABSMAX_ACC, 0, 0);

	input_set_abs_params(qmi8658->input_dev, ABS_RX, ABSMIN_GYRO, ABSMAX_GYRO, 0, 0);
	input_set_abs_params(qmi8658->input_dev, ABS_RY, ABSMIN_GYRO, ABSMAX_GYRO, 0, 0);
	input_set_abs_params(qmi8658->input_dev, ABS_RZ, ABSMIN_GYRO, ABSMAX_GYRO, 0, 0);

	qmi8658->input_dev->name = "qmi8658";

	/* Register */
	if (input_register_device(qmi8658->input_dev)) {
		dev_err(&client->dev, "qmi8658 register input failed\n");
		return -ENODEV;
	}

	/* Register as a misc device */
    misc_register(&qmi8658_device);

    return 0;
}

static int qmi8658_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "%s enter\n", __func__);

	misc_deregister(&qmi8658_device);
	input_unregister_device(qmi8658->input_dev);
	cancel_delayed_work(&qmi8658->work);

    return 0;
}

static const struct i2c_device_id qmi8658_id[] = {
    { DEVICE_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, qmi8658_id);

#ifdef CONFIG_OF
static const struct of_device_id qmi8658_of_match[] = {
    { .compatible = "QST,qmi8658", },
    {  }
};
MODULE_DEVICE_TABLE(of, qmi8658_of_match);
#endif

static struct i2c_driver qmi8658_driver = {
    .driver = {
        .owner          = THIS_MODULE,
        .name           = DEVICE_NAME,
        .of_match_table = of_match_ptr(qmi8658_of_match),
    },
    .id_table = qmi8658_id,
    .probe    = qmi8658_probe,
    .remove   = qmi8658_remove,
};

static int qmi8658_match(struct i2c_client *client)
{
	int err = 0;
	u8 chip_id = 0;

	qmi8658 = kzalloc(sizeof(*qmi8658), GFP_KERNEL);
	if (!qmi8658)
		return -ENOMEM;

	qmi8658->client = client;
	client->addr = 0x6a;

	err = qmi8658_read_reg(Qmi8658Register_WhoAmI, &chip_id, 1);
	kfree(qmi8658);
	if (err || chip_id != QMI8658_CHIP_ID)
		return 0;

	dev_info(&client->dev, "qmi8658 chip id:0x%x\n", chip_id);

	return 1;
}

struct imu_dev qmi8658_dev = {
	.driver = &qmi8658_driver,
	.match = qmi8658_match,
	.probe = qmi8658_probe,
	.remove = qmi8658_remove,
};
