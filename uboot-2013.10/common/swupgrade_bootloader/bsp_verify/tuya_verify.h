/*
 * (C) Copyright 2020
 * Cheng Lei, TUYA Software Engineering, chenglei@tuya.com.
 * Tuya pack code
 */
 
#ifndef tuya_verify__h
#define tuya_verify__h
 
#ifdef __cplusplus
extern "C" 
{
#endif

int tuya_verify_data(unsigned char* buf, unsigned int buf_len,
		unsigned char* sign);

#if !defined(__UBOOT__)
int tuya_verify_file(char *file_name, char *sig_file_name);
#endif

#ifdef __cplusplus
}
#endif

#endif
