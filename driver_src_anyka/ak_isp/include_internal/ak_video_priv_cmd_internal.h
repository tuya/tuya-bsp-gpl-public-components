#ifndef __AK_VIDEO_PRIV_CMD_INTERNAL_H__
#define __AK_VIDEO_PRIV_CMD_INTERNAL_H__

struct isp_timing_info;

typedef int (*SET_ISP_MISC)(void *arg, struct isp_timing_info *isp_timing);

/*
 * priv_cmd_internal
 * @GET_INTERFACE:	get interface that sensor output
 * @GET_IO_LEVEL:	get io level that sensor support
 * @GET_MIPI_LANES:	mipi is active. get mipi lanes
 * @GET_MIPI_MHZ:	mipi is active. get mipi MBPS
 * @GET_RESET_GPIO:	get reset pin for sensor
 * @GET_PWDN_GPIO:	get power_down pin for sesnor
 * @GET AE_FASE_DEFAULT:	get default ae config for the sensor
 * @GET_SCAN_METHOD:get scanning method for sensor
 * @SET_ISP_MISC_CALLBACK:	set callback for misc timing
 * @SET_FPS_DIRECT:	set sensor change to fps now
 * @SET_MASTER:		dual is active. set sensor to master
 * @SET_SLAVER:		dual is active. set sensor to slaver
 */
enum priv_cmd_internal {
	GET_INTERFACE = 0x2000,
	GET_IO_LEVEL,
	GET_MIPI_LANES,
	GET_MIPI_MHZ,
	GET_RESET_GPIO,
	GET_PWDN_GPIO,
	GET_AE_FAST_DEFAULT,
	GET_SCAN_METHOD,		//scaning method

	SET_ISP_MISC_CALLBACK = 0x3000,
	SET_FPS_DIRECT,
	SET_MASTER,/*for dual sensor*/
	SET_SLAVER,/*for dual sensor*/
};

enum interface {
	DVP_INTERFACE	= 0,
	MIPI_INTERFACE,
	DVP_INTERFACE_BT656,
	DVP_INTERFACE_BT1120,
};

enum io_level {
	IO_LEVEL_3V3 = 0,
	IO_LEVEL_2V5,
	IO_LEVEL_1V8,
};

enum scan_method {
	SCAN_METHOD_INTERLACED = 0,	//interlaced
	SCAN_METHOD_PROGRESSIVE,	//progressive
};

struct isp_timing_info {
	int oneline;
	int fsden;
	int hblank;
	int fsdnum;
};

struct isp_timing_cb_info {
	void *isp_timing_arg;
	SET_ISP_MISC set_isp_timing;
};
#endif
