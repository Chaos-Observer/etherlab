/******************************************************************************
 *
 * $Id: /local/rtw-trunk/src/C/copyright.txt 301 2008-03-22T14:43:03.529462Z rich  $
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

#include <linux/limits.h>       // PATH_MAX
#include <stddef.h>             // size_t
#include "include/mdl_taskstats.h"      // struct task_stats
#include "module_payload.h"

struct signal_info;

/** Structure describing the Real-Time Model
 */
struct rt_model {
	/** Block IO
	 * This part is filled in during mdl_init()
	 */
	void *mdl_rtB;		/* Block IO struct */
	unsigned int rtB_size;
        unsigned int rtB_count; /* Number of BlockIO slices to be saved
                                   to BlockIO buffer */

	void *mdl_rtP;		/* Model parameters struct */
	unsigned int rtP_size;
	unsigned int *task_period;	/* Array of task times in microsec */
	unsigned int sample_period;	/* Photo period in microsec */
        unsigned int buffer_time;       /* Time in microsec of the BlockIO 
                                         * buffer */
	void *rtw_model;
	unsigned int max_overrun;	/* Maximum overrun count */
	unsigned long stack_size;	/* RTAI Stack size */

        unsigned int num_tasks;         /* Number of model tasks */
        unsigned int num_st;            /* Number of model sample times */

	void *pend_rtP;         /* Pointer to a new set of parameters */

        unsigned int decimation;  /* The downsampling rate at which photos are taken */

        int model_id;

        /* Variables used to pass model symbol to the buddy */
        const struct payload_files *payload_files;

        size_t signal_count;
        size_t param_count;
        size_t variable_path_len;

        struct task_stats *task_stats;

        const char *(*rt_OneStepMain)(void);
        const char *(*rt_OneStepTid)(unsigned int);
        void        (*set_error_msg)(const char *);
        const char *(*get_signal_info)(struct signal_info*, const char**);
        const char *(*get_param_info)(struct signal_info*, const char**);

        const char *modelVersion;
        const char *modelName;
};

extern struct rt_model rt_model;

/* Revision number that will be used to check whether the structure
 * definitions have changed */
#define REVISION "$Revision$"

const char *mdl_start(void);
void mdl_stop(void);
