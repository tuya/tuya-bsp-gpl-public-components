#ifndef __AKPCM_COMMON_H__
#define __AKPCM_COMMON_H__

typedef    signed short           T_S16;      /* signed 16 bit integer */

/*
 * covert a mono buffer to stereo format
 * mono samples shall be located at first half of buf
 */
static inline void mono_to_stereo(void *dest, const char *src, int buf_size)
{
	int i;

	for (i=buf_size/4; i>0; i--) {
		((T_S16*)dest)[2*i-1] = ((T_S16*)src)[i-1];
		((T_S16*)dest)[2*i-2] = ((T_S16*)src)[i-1];
	}
}

/*
 * convert a stereo buffer to mono format
 * mono samples will be put into first half of buf
 */
static inline void stereo_to_mono(void *dest, const char *src, int buf_size)
{
	int i;

	for (i=0; i<buf_size/4; i++) {
		((T_S16*)dest)[i] = ((T_S16*)src)[2*i];
	}
}

#endif //__AKPCM_COMMON_H__
