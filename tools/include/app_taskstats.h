/**
 * \file
 * Header file for task statistics
 */
#ifdef __KERNEL__
#include <linux/time.h>
#else
#include <sys/time.h>
#endif

struct task_stats {
    struct timeval time;
    unsigned int exec_time;
    unsigned int time_step;
    unsigned int overrun;
};
