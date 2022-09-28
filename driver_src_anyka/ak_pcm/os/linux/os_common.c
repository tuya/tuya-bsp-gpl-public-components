
#include "pcm_port.h"
#include "os_common.h"

unsigned long long get_current_microsecond(void)
{
	struct timespec ts;
	unsigned long long now;

	ktime_get_ts(&ts);
	now = ts.tv_sec * AK_MSEC_PER_SEC_ULL * AK_USEC_PER_MSEC_ULL + ts.tv_nsec / NSEC_PER_USEC;

	return now;
}