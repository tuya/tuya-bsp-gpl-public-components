#ifndef __AK_PCM_OS_DEP_COMMON_H__
#define __AK_PCM_OS_DEP_COMMON_H__

#define AK_MSEC_PER_SEC_ULL     (1000ULL)
#define AK_USEC_PER_MSEC_ULL    (1000ULL)

unsigned long long get_current_microsecond(void);

#endif //__AK_PCM_OS_DEP_COMMON_H__