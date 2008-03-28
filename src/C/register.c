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

#include "include/model.h"
#include "include/rt_model.h"
#include "include/etl_data_info.h"
#include "include/etl_mdf.h"

#ifndef MODEL
# error "must define MODEL"
#endif

#ifndef NUMST
# error "must define number of sample times, NUMST"
#endif

static const char* rt_OneStepMain(void);
static const char* rt_OneStepTid(unsigned int);
static void        mdl_set_error_msg(const char*);
static const char* get_signal_info(struct signal_info *si, const char** path);
static const char* get_param_info(struct signal_info *si, const char** path);

static unsigned int task_period[NUMST] = { TASK_DECIMATIONS };
static struct task_stats task_stats[NUMST];

static int maxSignalIdx;
static int maxParameterIdx;

// Cannot declare static, as it is still required by rt_mdl_register.c
struct rt_model rt_model = {
    .mdl_rtB = &cvt,
    .rtB_size = sizeof(cvt),

    .mdl_rtP = &param,
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
    .buffer_time = BUFFER_TIME > 0 ? BUFFER_TIME*1e6 : 0,
    .stack_size = STACKSIZE,
    .task_stats = task_stats,

    .payload_files = NULL,

    /* Register model callbacks */
    .set_error_msg = mdl_set_error_msg,
    .modelVersion = STR(MODELVERSION),
    .modelName = STR(MODEL),
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
    return MdlStep(0);
} /* end rtOneStepMain */

static const char *
rt_OneStepTid(unsigned int tid)
{
    return MdlStep(tid);
} /* end rtOneStepTid */

static void
mdl_set_error_msg(const char *msg)
{
}

static const char* 
get_signal_info(struct signal_info *si, const char** path)
{
    if (si->index >= maxSignalIdx) {
        return "Signal index exceeded.";
    }
    memcpy(si, &capi_signals[si->index], sizeof(*si));
    *path = &capi_signals[si->index].path[0];

    return NULL;
}

static const char* 
get_param_info(struct signal_info *si, const char** path)
{
    if (si->index >= maxParameterIdx) {
        return "Signal index exceeded.";
    }
    memcpy(si, &capi_parameters[si->index], sizeof(*si));
    *path = &capi_parameters[si->index].path[0];

    return NULL;
}

/* Function: mdl_stop ========================================================
 *
 * Abstact:
 *      Cleanup model to free memory
 */
void 
mdl_stop(void)
{
    MdlExit();
}

/* Function: main ============================================================
 *
 * Abstract:
 *      Execute model on a generic target such as a workstation.
 */
const char *
mdl_start(void)
{
    const char *error_msg = NULL;
    unsigned int i;

    // At the moment, task_period only contains decimations. Convert that
    // to microseconds here
    for (i = 0; i < NUMST; i++)
        task_period[i] *= BASEPERIOD;

    for (maxSignalIdx = 0; capi_signals[maxSignalIdx].address;
            maxSignalIdx++) {
        if (strlen(capi_signals[maxSignalIdx].path) 
                > rt_model.variable_path_len) {
            rt_model.variable_path_len =
                strlen(capi_signals[maxSignalIdx].path);
        }
    }
    rt_model.signal_count = maxSignalIdx;

    for (maxParameterIdx = 0; capi_parameters[maxParameterIdx].address;
            maxParameterIdx++) {
        if (strlen(capi_parameters[maxParameterIdx].path) 
                > rt_model.variable_path_len) {
            rt_model.variable_path_len =
                strlen(capi_parameters[maxParameterIdx].path);
        }
    }
    rt_model.param_count = maxParameterIdx;

    error_msg = MdlInit();

    return error_msg;

} /* end main */
