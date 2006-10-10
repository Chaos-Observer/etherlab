#include <linux/limits.h>       // PATH_MAX

struct rtw_model {
	/** Block IO
	 * This part is filled in during mdl_init()
	 */
	void *mdl_rtB;		/* Block IO struct */
	unsigned int rtB_size;
        unsigned int rtB_count; /* Number of BlockIO slices to be saved
                                   to BlockIO buffer */

	void *mdl_rtP;		/* Model parameters struct */
	unsigned int rtP_size;
	unsigned int base_period;	/* Ticker period in microsec */
	unsigned int sample_period;	/* Photo period in microsec */
        unsigned int buffer_time;       /* Time in microsec of the BlockIO 
                                         * buffer */
	void *rt_model;
	unsigned int max_overrun;	/* Maximum overrun count */
	unsigned long stack_size;	/* RTAI Stack size */

        unsigned int numst;             /* Number of model sample times */
        unsigned int (*get_sample_time_multiplier)(unsigned int);
                                        /* Get the sample time in microsec
                                         * for the specified index. Note that
                                         * if tid01eq, then use sample time
                                         * of index 1 for tid 0 and 1 */

	void *pend_rtP;         /* Pointer to a new set of parameters */
	volatile int new_rtP;   /* True if a new set needs to be loaded */

        unsigned int downsample_cnt;    /* When ==0, a photo is taken */
        unsigned int downsample;  /* The downsampling rate at which photos are taken */

        int model_id;

        const char *(*rt_OneStepMain)(void);
        const char *(*rt_OneStepTid)(unsigned int);
        void (*set_error_msg)(const char *);
        void (*init_time)(double);
        void (*set_time)(double);
        int (*take_photo)(void);
        void (*exec_stats)(
                double exec_time, 
                double jitter, 
                unsigned int overrun
                );
        const char *modelVersion;
        const char *modelName;
        char so_path[PATH_MAX]; /* Path where the shared object is
                                   located for parameter and process io */
};

extern struct rtw_model rtw_model;

/* Revision number that will be used to check whether the structure
 * definitions have changed */
#define REVISION "$Revision"

void set_time(double now);
void init_time(double now);
const char *mdl_start(void);
void mdl_stop(void);
int mdl_takephoto(void);