/**
 * @file
 * Header file for task statistics
 *
 * The following functions are defined in @c <model>.c, the RTW product of a 
 * Simulink model. These functions enable calling code that sets various
 * variables defined in @c <model>.c directly out of the main RTAI thread.
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
