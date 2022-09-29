#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <ty_typedefs.h>


static int hdmi_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	/*
	u8 cmd = 0;
	unsigned char read_buf[33];
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client,cmd,33,read_buf);
	if(ret > 0){
		printk("i2c read success:%d\n",ret);
	}else{
		printk("i2c read failed\n");
	}
	*/
	printk("hdmi_probe success\n");
	
	return 0;
}

static int hdmi_remove(struct i2c_client *client)
{
	return 0;
}


static const struct i2c_device_id hdmi_id_table[] = {
	{"tuya,hdmi",0},
	{}
};

static const struct of_device_id hdmi_of_match[] = {
	{
	.compatible = "tuya,hdmi"
	},
	{
	},
};

static struct i2c_driver hdmi_driver = {
	.driver = {
		.name = "hdmi",
		.owner = THIS_MODULE,
		.of_match_table = hdmi_of_match,
	},
	.probe = hdmi_probe,
	.remove = hdmi_remove,
	.id_table = hdmi_id_table,
};


int ty_hdmi_init(void)
{
	MS_INFO("%s module is installed\n", __func__);
	return i2c_add_driver(&hdmi_driver);	
}

void ty_hdmi_exit(void)
{
	i2c_del_driver(&hdmi_driver);
}

