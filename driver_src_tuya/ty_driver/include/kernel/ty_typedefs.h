#ifndef _TY_TYPEDEFS_H_
#define _TY_TYPEDEFS_H_

#include <linux/printk.h>

#define DEBUG_INFO	1

#define MS_ERROR(fmt, args...) 		printk("\033[1;31m""Error:"fmt"\033[0m", ##args)	//red
#ifdef DEBUG_INFO
#define MS_DEBUG(fmt, args...) 		printk("\033[1;32m"fmt"\033[0m", ##args)	//green
#else
#define MS_DEBUG(fmt, args...)
#endif
#define MS_INFO(fmt, args...) 		printk("\033[1;32m""INFO:"fmt"\033[0m", ##args)	//green
#define MS_WARNING(fmt, args...) 	printk("\033[1;33m""WARNING:"fmt"\033[0m", ##args)	//yellow

#define NOP() __NOP()

#ifndef CHAR
#define CHAR  char
#endif

#ifndef BYTE
#define BYTE  unsigned char
#endif

#ifndef SHORT
#define SHORT short int
#endif

#ifndef WORD
#define WORD unsigned short
#endif

#ifndef LONG
#define LONG long
#endif

#ifndef DWORD
#define DWORD unsigned long
#endif

#ifndef INT
#define INT int
#endif

#ifndef UINT
#define UINT unsigned int
#endif

#ifndef BOOL
#define BOOL BYTE
#endif

#ifndef TRUE
#define TRUE (BOOL)1
#endif

#ifndef FALSE
#define FALSE (BOOL)0
#endif

#define INFINITE 0xFFFFFFFF

#endif
