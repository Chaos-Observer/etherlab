/******************************************************************************
 *
 * $Id$
 *
 * Here structures are defined that are used to register a Real-Time Task
 * with rt_kernel;
 *
 * Copyright (C) 2008  Richard Hacker
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/ 

#include <include/taskstats.h>      // struct task_stats
#include "module_payload.h"

struct signal_info;

/** Structure describing the Real-Time Model
 */
struct rt_app {
	/** Block IO
	 * This part is filled in during app_init()
	 */
	void *app_rtB;		/* Block IO struct */
	unsigned int rtB_size;
        unsigned int rtB_count; /* Number of BlockIO slices to be saved
                                   to BlockIO buffer */

	void *app_rtP;		/* Model parameters struct */
	unsigned int rtP_size;
	unsigned int *task_period;	/* Array of task times in microsec */
	unsigned int sample_period;	/* Photo period in microsec */
        unsigned int buffer_time;       /* Time in microsec of the BlockIO 
                                         * buffer */
	void *app_privdata;
	unsigned int max_overrun;	/* Maximum overrun count */
	unsigned long stack_size;	/* RTAI Stack size */

        unsigned int num_tasks;         /* Number of application tasks */
        unsigned int num_st;            /* Number of application 
                                           sample times */

	void *pend_rtP;         /* Pointer to a new set of parameters */

        unsigned int decimation;  /* The downsampling rate at which 
                                     photos are taken */

        int app_id;

        /* Variables used to pass model symbol to the buddy */
        const struct payload_files *payload_files;

        unsigned int signal_count;
        unsigned int param_count;
        unsigned int variable_path_len;

        struct task_stats *task_stats;

        const char *(*rt_OneStepMain)(void);
        const char *(*rt_OneStepTid)(unsigned int);
        void        (*set_error_msg)(const char *);
        const char *(*get_signal_info)(struct signal_info*);
        const char *(*get_param_info)(struct signal_info*);

        const char *appVersion;
        const char *appName;
};

extern struct rt_app rt_app;
extern struct task_stats task_stats[];
extern unsigned int task_period[];

/* Revision number that will be used to check whether the structure
 * definitions have changed */
#define REVISION "$Revision$"

const char *app_start(void);
const char *app_info_init(void* priv_data);
void app_stop(void);
