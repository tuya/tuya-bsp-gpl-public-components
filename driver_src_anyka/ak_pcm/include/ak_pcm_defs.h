#ifndef __AKPCM_DEFS_H__
#define __AKPCM_DEFS_H__

#include "../../include/ak_pcm.h"
#include "soundcard.h"

#define ak_pcm_debug(fmt, arg...) \
	pr_debug("[%llu]:[%s:%d]: "fmt" \n", ktime_to_ms(ktime_get_boottime()), __func__, __LINE__, ##arg)
#define ak_pcm_func(fmt, arg...) \
	pr_info("[%llu]:[%s]: "fmt" \n", ktime_to_ms(ktime_get_boottime()), __func__, ##arg)
#define ak_pcm_info(fmt, arg...) \
	pr_info("[%llu]:[%s:%d]: "fmt" \n", ktime_to_ms(ktime_get_boottime()), __func__, __LINE__, ##arg)
#define ak_pcm_warn(fmt, arg...) \
	pr_warn("[%llu]:[%s:%d] warn! "fmt" \n\n", ktime_to_ms(ktime_get_boottime()), __func__, __LINE__, ##arg)
#define ak_pcm_err(fmt, arg...) \
	pr_err("[%llu]:[%s:%d] error! "fmt" \n", ktime_to_ms(ktime_get_boottime()), __func__, __LINE__, ##arg)

#define ak_pcm_assert(exp) \
	((exp)? 1 : ak_pcm_info("assert"))

#define USE_FORMATS             (AKPCM_FMTBIT_U8 | AKPCM_SMPL_BIT_U16)
#define USE_RATE                (AKPCM_RATE_CONTINUOUS | AKPCM_RATE_ALL)
#define USE_RATE_MIN            8000
#define USE_RATE_MAX            48000
#define PLAY_PERIOD_BYTES_MIN   512
#define PLAY_PERIOD_BYTES_MAX   65536
#define PLAY_PERIODS_MIN        4
#define PLAY_PERIODS_MAX        1024
#define DELAYS_FOR_CLOSE_DAC    (HZ*30)
#define CAPT_PERIOD_BYTES_MIN   512
#define CAPT_PERIOD_BYTES_MAX   32768
#define CAPT_PERIODS_MIN        4
#define CAPT_PERIODS_MAX        512
#define MAX_TIMESTAMP_CNT       80

/* GAIN channels*/
#define MIXER_VOL_HP            0
#define MIXER_VOL_LI            1
#define MIXER_VOL_MIC           2
#define MIXER_VOL_END           2

/* GAIN  default value */
#define DEFAULT_HPVOL           4
#define DEFAULT_LINEINVOL       3
#define DEFAULT_MICVOL          3

/* Mixer */
#define MIXER_OUT               0
#define MIXER_IN                1
#define MIXER_NUM               2

/* devices for playback */
#define PLAYDEV_HP              (AKPCM_PLAYDEV_HP)
#define PLAYDEV_MSK             (AKPCM_PLAYDEV_HP)

/* devices for capture */
#define CPTRDEV_MIC             (AKPCM_CPTRDEV_MIC)
#define CPTRDEV_LI              (AKPCM_CPTRDEV_LI)
#define CPTRDEV_MSK             (AKPCM_CPTRDEV_MIC | AKPCM_CPTRDEV_LI)

/* bit[0]-opened; bit[1]-prepared , bit[2]-stream; bit[3]-DMA; bit[4] -pausing */
enum run_flag_bit {
	STATUS_OPENED = 0x00,
	STATUS_PREPARED,
	STATUS_START_STREAM,
	STATUS_WORKING,
	STATUS_PAUSING,
};

enum akpcm_device {
	AK_PCM_DEV_PLAYBACK,
	AK_PCM_DEV_CAPTURE,
	AK_PCM_DEV_LOOPBACK,
	AK_PCM_DEV_MAX,
};

struct akpcm_runtime {
	struct cdev cdev;
	struct device *dev;
	struct akpcm_pars cfg;
	unsigned int hw_ptr;  /* hardware ptr*/
	unsigned int app_ptr; /* application ptr*/
	spinlock_t ptr_lock;  /* lock to protect hw_ptr & app_ptr */
	struct mutex io_lock; /* mutex to pretect ioctl */

	/* -- SW parameters -- */
	unsigned int buffer_bytes;
	unsigned int boundary;

	/* -- DMA -- */
	unsigned char *dma_area; /* DMA area(vaddr) */
	dma_addr_t dma_addr; 	 /* physical bus address */

	unsigned long long ts;

	/* cache buff */
	unsigned char *cache_buff; /* DMA area(vaddr) */

	/* runtime timestamp */
	unsigned long long timestamp[MAX_TIMESTAMP_CNT];

	/* the completion for playback or capture */
	struct completion rt_completion;

	/* wait_queue_head_t member */
	wait_queue_head_t wq;

	unsigned long actual_rate;   	/* actual rate in Hz */
	unsigned int  hp_gain_cur;		/* current value for headphone gain */
	unsigned int  linein_gain_cur;	/* current value for linein gain */
	unsigned int  mic_gain_cur;		/* current value for mic gain */

	/* bit[0]-opened; bit[1]-prepared , bit[2]-stream; bit[3]-DMA; bit[4] -pausing */
	unsigned long runflag;

	long remain_app_bytes;

	/* the mixer_source for MIXER_OUT and MIXER_IN */
	int mixer_source;

	int notify_threshold;
	int snd_dev;

	unsigned int last_transfer_count;
	unsigned int last_isr_count;
	unsigned int log_print;
	unsigned int io_trace_in;
	unsigned int io_trace_out;
};

struct akpcm {
	struct device *dev;

	struct akpcm_runtime *play_rt;
	struct akpcm_runtime *cptr_rt;
	struct akpcm_runtime *cptr_aux_rt;
	struct akpcm_runtime *loopback_rt;

	SND_PARAM param;
	struct device_node *np;
};

static inline int is_runtime_opened(struct akpcm_runtime *rt) {
	return test_bit(STATUS_OPENED, &(rt->runflag));
}

static inline int is_runtime_ready(struct akpcm_runtime *rt) {
	return test_bit(STATUS_PREPARED, &(rt->runflag));
}

static inline int is_runtime_stream(struct akpcm_runtime *rt) {
	return test_bit(STATUS_START_STREAM, &(rt->runflag));
}

static inline int is_runtime_working(struct akpcm_runtime *rt) {
	return test_bit(STATUS_WORKING, &(rt->runflag));
}

static inline int is_runtime_pausing(struct akpcm_runtime *rt) {
	return test_bit(STATUS_PAUSING, &(rt->runflag));
}

#endif//__AKPCM_DEFS_H__
