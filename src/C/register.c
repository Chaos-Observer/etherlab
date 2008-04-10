/******************************************************************************
 *
 * $Id$
 *
 * This file is used to capsulate the C model so that it can
 * be called from the kernel module rt_module.c.
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

#include <linux/string.h> // memcpy(), etc.

#include <include/application.h>
#include <include/rt_app.h>
#include <include/etl_data_info.h>
#include <include/etl_application_description.h>

#ifndef MODEL
# error "must define MODEL"
#endif

#ifndef NUMST
# error "must define number of sample times, NUMST"
#endif

static const char* rt_OneStepMain(void);
static const char* rt_OneStepTid(unsigned int);
static void        mdl_set_error_msg(const char*);
static const char* get_signal_info(struct signal_info *si);
static const char* get_param_info(struct signal_info *si);

static struct task_stats task_stats[NUMST];

// Cannot declare static, as it is still required by rt_mdl_register.c
struct rt_app rt_app = {
    .app_rtB = &signal,
    .rtB_size = sizeof(signal),

    .app_rtP = &param,
    .rtP_size = sizeof(param),

    /* Some variables concerning sample times passed to the model */
    .num_st = NUMST,
    .num_tasks = NUMST,

    .rt_OneStepMain = rt_OneStepMain,
    .rt_OneStepTid = rt_OneStepTid,
    .get_signal_info = get_signal_info,
    .get_param_info = get_param_info,

    .task_period = task_period,

    .decimation = DECIMATION > 1 ? DECIMATION : 1,
    .max_overrun = OVERRUNMAX,
    .buffer_time = BUFFER_TIME > 0 ? BUFFER_TIME*1000000 : 1000000,
    .stack_size = STACKSIZE,
    .task_stats = task_stats,

    .payload_files = NULL,

    /* Register model callbacks */
    .set_error_msg = mdl_set_error_msg,
    .appVersion = STR(MODELVERSION),
    .appName = STR(MODEL),
};

/** Perform calculation step of base period
 *
 * This function is called to perform a calculation step for the model's
 * base period.
 *
 * Return:
 *      Pointer to error string in case of an error.
 */
static const char *
rt_OneStepMain(void)
{
    return TaskStep(0);
} /* end rtOneStepMain */

static const char *
rt_OneStepTid(unsigned int tid)
{
    return TaskStep(tid);
} /* end rtOneStepTid */

static void
mdl_set_error_msg(const char *msg)
{
}

static const char* 
get_signal_info(struct signal_info *si)
{
    if (si->index >= rt_app.signal_count) {
        return "Signal index exceeded.";
    }

    si->data_type = capi_signals[si->index].data_type;
    si->st_index  = capi_signals[si->index].st_index;
    si->offset = capi_signals[si->index].address - (void*)&signal;
    memcpy(si->dim, capi_signals[si->index].dim, sizeof(si->dim));
    strncpy(si->name, capi_signals[si->index].name, sizeof(si->name));
    strncpy(si->alias, capi_signals[si->index].alias, sizeof(si->alias));
    strncpy(si->path, capi_signals[si->index].path, si->path_buf_len);

    return NULL;
}

static const char* 
get_param_info(struct signal_info *si)
{
    if (si->index >= rt_app.param_count) {
        return "Signal index exceeded.";
    }

    si->data_type = capi_parameters[si->index].data_type;
    si->st_index  = capi_parameters[si->index].st_index;
    si->offset = capi_parameters[si->index].address - (void*)&param;
    memcpy(si->dim, capi_parameters[si->index].dim, sizeof(si->dim));
    strncpy(si->name, capi_parameters[si->index].name, sizeof(si->name));
    strncpy(si->alias, capi_parameters[si->index].alias, sizeof(si->alias));
    strncpy(si->path, capi_parameters[si->index].path, si->path_buf_len);

    return NULL;
}

/* Function: mdl_stop ========================================================
 *
 * Abstact:
 *      Cleanup model to free memory
 */
void 
app_stop(void)
{
    AppExit();
}

/* Function: main ============================================================
 *
 * Abstract:
 *      Execute model on a generic target such as a workstation.
 */
const char *
app_start(void)
{
    const char *error_msg = NULL;
    size_t len;
    unsigned int idx;

    // Count the number of signals and parameters, and also find the 
    // length of the longest path
    for (idx = 0; capi_signals[idx].address; idx++) {
        len = strlen(capi_signals[idx].path);
        if (len > rt_app.variable_path_len) {
            rt_app.variable_path_len = len;
        }
    }
    rt_app.signal_count = idx;

    for (idx = 0; capi_parameters[idx].address; idx++) {
        len = strlen(capi_parameters[idx].path);
        if (len > rt_app.variable_path_len) {
            rt_app.variable_path_len = len;
        }
    }
    rt_app.param_count = idx;

    error_msg = AppInit();

    return error_msg;

} /* end main */
