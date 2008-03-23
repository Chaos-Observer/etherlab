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

#include "model.h"
#include "mdl_wrapper.h"

#ifndef MODEL
# error "must define MODEL"
#endif

#ifndef NUMST
# error "must define number of sample times, NUMST"
#endif

/** Perform calculation step of base period
 *
 * This function is called to perform a calculation step for the model's
 * base period.
 *
 * Return:
 *      Pointer to error string in case of an error.
 */
const char *
rt_OneStepMain(double time)
{
    return MdlStep(0, time);
} /* end rtOneStepMain */

const char *
rt_OneStepTid(unsigned int tid, double time)
{
    return MdlStep(tid, time);
} /* end rtOneStepTid */

void
mdl_set_error_msg(const char *msg)
{
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
    const char *error_msg;
    unsigned int i __attribute__((unused));

    error_msg = MdlInit();

    if (error_msg)
        return error_msg;

    rtw_model.mdl_rtB = &cvt;
    rtw_model.rtB_size = sizeof(cvt);

    rtw_model.mdl_rtP = &param;
    rtw_model.rtP_size = sizeof(param);

    /* Some variables concerning sample times passed to the model */
    rtw_model.numst = NUMST;
    rtw_model.base_period = BASEPERIOD;
    rtw_model.subtask_decimation = NUMST ? subtask_decimation : NULL;

    rtw_model.decimation = DECIMATION > 1 ? DECIMATION : 1;
    rtw_model.max_overrun = OVERRUNMAX;
    rtw_model.buffer_time = BUFFER_TIME > 0 ? BUFFER_TIME*1e6 : 0;
    rtw_model.stack_size = STACKSIZE;

    rtw_model.symbols = NULL;
    rtw_model.symbol_len = 0;

    /* Register model callbacks */
    rtw_model.rt_OneStepMain = rt_OneStepMain;
    rtw_model.rt_OneStepTid = rt_OneStepTid;
    rtw_model.set_error_msg = mdl_set_error_msg;

    rtw_model.modelVersion = STR(MODELVERSION);
    rtw_model.modelName = STR(MODEL);

    return NULL;

} /* end main */
