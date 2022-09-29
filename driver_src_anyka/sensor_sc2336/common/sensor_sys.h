#ifndef __SENSOR__SYS_H
#define __SENSOR__SYS_H

/*define sensors ID & interface functions*/
typedef int (*sensor_id_cb)(void);
typedef char * (*sensor_if_cb)(void);

/*
 * sensor_sys_init -
 * sensor sys init entry
 *
 * @id_cb:			callback of read sensor id
 * @if_cb:			callback of get sensor interface
 */
int sensor_sys_init(sensor_id_cb id_cb, sensor_if_cb if_cb);

/*
 * sensor_sys_exit -
 * sensor sys exit entry
 */
void sensor_sys_exit(void);

#endif
