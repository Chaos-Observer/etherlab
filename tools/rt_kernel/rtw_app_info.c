/******************************************************************************
 *
 * $Id$
 *
 * Here the application info is initialised for RTW generated code.
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
 * Original copyright notice:
 *           Autor: Richard Hacker
 *
 *           (C) Copyright IgH 2008
 *           Ingenieurgemeinschaft IgH
 *           Heinz-Baecker Str. 34
 *           D-45356 Essen
 *           Tel.: +49 201/36014-0
 *           Fax.: +49 201/36014-14
 *           E-mail: info@igh-essen.com
 *
 *****************************************************************************/ 

#include <include/rt_app.h>
#include <include/mdl_time.h>
#include <include/rtw_data_interface.h>
#include <include/defines.h>

#ifndef RT
# error "must define RT"
#endif

#ifndef MODEL
# error "must define MODEL"
#endif

#ifndef NUMST
# error "must define number of sample times, NUMST"
#endif

#ifndef NCSTATES
# error "must define NCSTATES"
#endif

/*====================*
 * External functions *
 *====================*/
extern RT_MODEL *MODEL(void);

unsigned int task_period[NUMST - (TID01EQ ? 1 : 0)];
struct task_stats task_stats[NUMST - (TID01EQ ? 1 : 0)];

/* Instantiate and initialise rt_app here */
struct rt_app rt_app = {
    .app_rtB = &rtB,
    .rtB_size = sizeof(rtB),

#ifdef rtP
    .app_rtP = &rtP,
    .rtP_size = sizeof(rtP),
#else
    .app_rtP = NULL,
    .rtP_size = 0,
#endif

    /* Some variables concerning sample times passed to the model */
    .num_st = TID01EQ ? NUMST-1 : NUMST,
#ifdef MULTITASKING
    .num_tasks = TID01EQ ? NUMST-1 : NUMST,
#else
    .num_tasks = 1,
#endif

    .get_signal_info = get_signal_info,
#ifdef rtP
    .get_param_info = get_param_info,
#endif

    .task_period = task_period,

    .decimation = DECIMATION > 1 ? DECIMATION : 1,
    .max_overrun = OVERRUNMAX,
    .buffer_time = BUFFER_TIME > 0 ? BUFFER_TIME*1e6 : 0,
    .stack_size = STACKSIZE,
    .task_stats = task_stats,

    .payload_files = payload_files,

    .appVersion = STR(MODELVERSION),
    .appName = STR(MODEL),
};

const char*
app_info_init(void* priv_data)
{
    RT_MODEL* S = (RT_MODEL*)priv_data;
    const char* errmsg;

    /************************
     * Initialize the model *
     ************************/
    rt_app.app_privdata = priv_data;

    if ((errmsg = rtw_capi_init(S, 
                    &rt_app.signal_count,
                    &rt_app.param_count,
                    &rt_app.variable_path_len))) {
        return errmsg;
    }

    return NULL;

}
