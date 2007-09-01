/******************************************************************************
 *
 *           $RCSfile: rtai_wrapper.c,v $
 *           $Revision: 1.3 $
 *           $Author: rich $
 *           $Date: 2006/02/04 11:07:15 $
 *           $State: Exp $
 *
 *           Autor: Richard Hacker
 *
 *           (C) Copyright IgH 2005
 *           Ingenieurgemeinschaft IgH
 *           Heinz-Baecker Str. 34
 *           D-45356 Essen
 *           Tel.: +49 201/36014-0
 *           Fax.: +49 201/36014-14
 *           E-mail: info@igh-essen.com
 *
 * 	This file is used to capsulate the RTW model so that it can
 * 	be called from the kernel module rt_module.c.
 *
 *           $Log: rtai_wrapper.c,v $
 *           Revision 1.3  2006/02/04 11:07:15  rich
 *           call MdlTerminate on error in init()
 *
 *           Revision 1.2  2006/02/02 09:07:33  rich
 *           Removed supurfluous #include <stdio.h>
 *
 *           Revision 1.1  2006/01/23 14:50:56  rich
 *           Initial revision
 *
 *
 *****************************************************************************/ 

#include "mdl_wrapper.h"
#include "mdl_taskinfo.h"
#include "mdl_time.h"
#include "msf.h"

#include "rtmodel.h"
#include "rt_sim.h"
#include "defines.h"


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

extern void MdlInitializeSizes(void);
extern void MdlInitializeSampleTimes(void);
extern void MdlStart(void);
extern void MdlOutputs(int_T tid);
extern void MdlUpdate(int_T tid);
extern void MdlTerminate(void);

double world_time[NUMST];

#if NCSTATES > 0
  extern void rt_ODECreateIntegrationData(RTWSolverInfo *si);
  extern void rt_ODEUpdateContinuousStates(RTWSolverInfo *si);

# define rt_CreateIntegrationData(S) \
    rt_ODECreateIntegrationData(rtmGetRTWSolverInfo(S));
# define rt_UpdateContinuousStates(S) \
    rt_ODEUpdateContinuousStates(rtmGetRTWSolverInfo(S));
# else
# define rt_CreateIntegrationData(S)  \
      rtsiSetSolverName(rtmGetRTWSolverInfo(S),"FixedStepDiscrete");
# define rt_UpdateContinuousStates(S) \
      rtmSetT(S, rtsiGetSolverStopTime(rtmGetRTWSolverInfo(S)));
#endif


#if !defined(MULTITASKING)  /* SINGLETASKING */

/* Function: rtOneStep ========================================================
 *
 * Abstract:
 *      Perform one step of the model. This function is modeled such that
 *      it could be called from an interrupt service routine (ISR) with minor
 *      modifications.
 */
const char *
rt_OneStep(double time)
{
    RT_MODEL *S = rtw_model.rt_model;
    real_T tnext;

    world_time[0] = time;

    tnext = rt_SimGetNextSampleHit();
    rtsiSetSolverStopTime(rtmGetRTWSolverInfo(S),tnext);

    MdlOutputs(0);

    MdlUpdate(0);
    rt_SimUpdateDiscreteTaskSampleHits(rtmGetNumSampleTimes(S),
                                       rtmGetTimingData(S),
                                       rtmGetSampleHitPtr(S),
                                       rtmGetTPtr(S));

    if (rtmGetSampleTime(S,0) == CONTINUOUS_SAMPLE_TIME) {
        rt_UpdateContinuousStates(S);
    }

    return rtmGetErrorStatusFlag(S);
} /* end rtOneStep */

double *
etl_get_world_time_ptr(unsigned int tid)
{
    return &world_time[0];
}

#else /* MULTITASKING */

# if TID01EQ == 1
#  define FIRST_TID 1
# else
#  define FIRST_TID 0
# endif

/* Function: rtOneStep ========================================================
 *
 * Abstract:
 *      Perform one step of the model. This function is modeled such that
 *      it could be called from an interrupt service routine (ISR) with minor
 *      modifications.
 *
 *      This routine is modeled for use in a multitasking environment and
 *	therefore needs to be fully re-entrant when it is called from an
 *	interrupt service routine.
 *
 * Note:
 *      Error checking is provided which will only be used if this routine
 *      is attached to an interrupt.
 *
 */
const char *
rt_OneStepMain(double time)
{
    RT_MODEL *S = rtw_model.rt_model;
    real_T tnext;

    world_time[FIRST_TID] = time;

    /*******************************************
     * Step the model for the base sample time *
     *******************************************/

    tnext = rt_SimUpdateDiscreteEvents(
                rtmGetNumSampleTimes(S),
                rtmGetTimingData(S),
                rtmGetSampleHitPtr(S),
                rtmGetPerTaskSampleHitsPtr(S));

    rtsiSetSolverStopTime(rtmGetRTWSolverInfo(S),tnext);

    MdlOutputs(FIRST_TID);

    MdlUpdate(FIRST_TID);

    if (rtmGetSampleTime(S,0) == CONTINUOUS_SAMPLE_TIME) {
        rt_UpdateContinuousStates(S);
    } else {
        rt_SimUpdateDiscreteTaskTime(rtmGetTPtr(S), 
                                     rtmGetTimingData(S), 0);
    }

#if FIRST_TID == 1
    rt_SimUpdateDiscreteTaskTime(rtmGetTPtr(S), 
                                 rtmGetTimingData(S),1);
#endif

    return rtmGetErrorStatusFlag(S);
} /* end rtOneStepMain */

const char *
rt_OneStepTid(uint_T tid, double time)
{
    RT_MODEL *S = rtw_model.rt_model;

#if FIRST_TID == 1
    tid += FIRST_TID;
#endif

    world_time[tid] = time;

/*
    if (rtmGetSampleHitArray(S)[tid]) {
        rtmSetErrorStatusFlag(S, "Sample hit was not ready");
    }
*/


    MdlOutputs(tid);

    MdlUpdate(tid);

    rt_SimUpdateDiscreteTaskTime(rtmGetTPtr(S), 
                                 rtmGetTimingData(S),tid);

    return rtmGetErrorStatusFlag(S);

} /* end rtOneStepTid */

double *
etl_get_world_time_ptr(unsigned int tid)
{
    if (tid == 0) 
        tid = FIRST_TID;

    return &world_time[tid];
}

#endif /* MULTITASKING */

void
mdl_set_error_msg(const char *msg)
{
    RT_MODEL *S = rtw_model.rt_model;

    rtmSetErrorStatusFlag(S, msg);
}

/* Function: mdl_stop ========================================================
 *
 * Abstact:
 *      Cleanup model to free memory
 */
void 
mdl_stop(void)
{
    MdlTerminate();
}

/* Function: mdl_get_sample_time_multiplier ==================================
 *
 * Abstact:
 *      Get the multiplier of the base sample time for sample index idx
 */
unsigned int
mdl_get_sample_time_multiplier(unsigned int idx)
{
    RT_MODEL  *S = rtw_model.rt_model;
    unsigned int tid0 = TID01EQ ? 1 : 0;

    idx += TID01EQ ? 1 : 0;
    return (unsigned int)
        (rtmGetSampleTime(S, idx)/rtmGetSampleTime(S, tid0)
         + 0.5);
}

/* Function: main ============================================================
 *
 * Abstract:
 *      Execute model on a generic target such as a workstation.
 */
const char *
mdl_start(void)
{
    RT_MODEL  *S;
    const char *errmsg;

    /************************
     * Initialize the model *
     ************************/
    rtw_model.rt_model = S = MODEL();

    if (rtmGetErrorStatus(S) != NULL) {
	    return rtmGetErrorStatus(S);
    }

    MdlInitializeSizes();
    MdlInitializeSampleTimes();
    
    if ((errmsg = rt_SimInitTimingEngine(
                    rtmGetNumSampleTimes(S),
                    rtmGetStepSize(S),
                    rtmGetSampleTimePtr(S),
                    rtmGetOffsetTimePtr(S),
                    rtmGetSampleHitPtr(S),
                    rtmGetSampleTimeTaskIDPtr(S),
                    rtmGetTStart(S),
                    &rtmGetSimTimeStep(S),
                    &rtmGetTimingData(S)))) {
        return errmsg;
    }
    rt_CreateIntegrationData(S);

    MdlStart();
    if ((errmsg = rtmGetErrorStatus(S))) {
        MdlTerminate();
        return errmsg;
    }


    rtw_model.mdl_rtB = &rtB;
    rtw_model.rtB_size = sizeof(rtB);

#ifdef rtP
    rtw_model.mdl_rtP = &rtP;
    rtw_model.rtP_size = sizeof(rtP);
#else
    rtw_model.mdl_rtP = NULL;
    rtw_model.rtP_size = 0;
#endif

    /* Some variables concerning sample times passed to the model */
#ifdef MULTITASKING
    rtw_model.numst = TID01EQ ? NUMST-1 : NUMST;
    rtw_model.rt_OneStepMain = rt_OneStepMain;
    rtw_model.rt_OneStepTid = rt_OneStepTid;
#else
    rtw_model.numst = 1;
    rtw_model.rt_OneStepMain = rt_OneStep;
    rtw_model.rt_OneStepTid = NULL;
#endif

    rtw_model.base_period = (unsigned long)(rtmGetStepSize(S)*1.0e6 + 0.5);
    rtw_model.get_sample_time_multiplier = mdl_get_sample_time_multiplier;

    rtw_model.downsample = DOWNSAMPLE > 1 ? DOWNSAMPLE : 1;
    rtw_model.max_overrun = OVERRUNMAX;
    rtw_model.buffer_time = BUFFER_TIME > 0 ? BUFFER_TIME*1e6 : 0;
    rtw_model.stack_size = STACKSIZE;

    rtw_model.symbols = model_symbols;
    rtw_model.symbol_len = model_symbol_len;

    /* Register model callbacks */
    rtw_model.set_error_msg = mdl_set_error_msg;
    rtw_model.modelVersion = STR(MODELVERSION);
    rtw_model.modelName = STR(MODEL);

    return NULL;

} /* end main */
