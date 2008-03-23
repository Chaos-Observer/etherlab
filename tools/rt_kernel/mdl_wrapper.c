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
#include "mdl_time.h"
#include "fio_ioctl.h"
#include "rtw_data_info.h"

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

#ifdef MULTITASKING
extern const char* rt_OneStepMain(void);
extern const char* rt_OneStepTid(uint_T);
#else
extern const char* rt_OneStep(void);
#endif
extern void mdl_set_error_msg(const char *msg);

double etl_world_time[NUMST];
unsigned int task_period[NUMST - (TID01EQ ? 1 : 0)];
struct task_stats task_stats[NUMST - (TID01EQ ? 1 : 0)];
unsigned int get_signal_info(struct signal_info* si, char **name);
unsigned int get_param_info(struct signal_info* si, char **name);

/* Instantiate and initialise rtw_model here */
struct rtw_model rtw_model = {
    .mdl_rtB = &rtB,
    .rtB_size = sizeof(rtB),

#ifdef rtP
    .mdl_rtP = &rtP,
    .rtP_size = sizeof(rtP),
#else
    .mdl_rtP = NULL,
    .rtP_size = 0,
#endif

    /* Some variables concerning sample times passed to the model */
#ifdef MULTITASKING
    .numst = TID01EQ ? NUMST-1 : NUMST,
    .rt_OneStepMain = rt_OneStepMain,
    .rt_OneStepTid = rt_OneStepTid,
#else
    .numst = 1,
    .rt_OneStepMain = rt_OneStep,
    .rt_OneStepTid = NULL,
#endif

    .task_period = task_period,

    .decimation = DECIMATION > 1 ? DECIMATION : 1,
    .max_overrun = OVERRUNMAX,
    .buffer_time = BUFFER_TIME > 0 ? BUFFER_TIME*1e6 : 0,
    .stack_size = STACKSIZE,
    .task_stats = task_stats,

    .payload_files = payload_files,

    /* Register model callbacks */
    .set_error_msg = mdl_set_error_msg,
    .modelVersion = STR(MODELVERSION),
    .modelName = STR(MODEL),
};

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
rt_OneStep()
{
    RT_MODEL *S = rtw_model.rt_model;
    real_T tnext;

    etl_world_time[0] = (double)task_stats[0].time.tv_sec 
        + (double)task_stats[0].time.tv_usec / 1.0e6;

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
rt_OneStepMain()
{
    RT_MODEL *S = rtw_model.rt_model;
    real_T tnext;

    etl_world_time[FIRST_TID] = (double)task_stats[0].time.tv_sec 
        + (double)task_stats[0].time.tv_usec / 1.0e6;
    etl_world_time[0] = etl_world_time[FIRST_TID];


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
rt_OneStepTid(uint_T tid)
{
    RT_MODEL *S = rtw_model.rt_model;
    uint_T rtw_tid = tid + FIRST_TID;

    etl_world_time[rtw_tid] = (double)task_stats[tid].time.tv_sec 
        + (double)task_stats[tid].time.tv_usec / 1.0e6;

/*
    if (rtmGetSampleHitArray(S)[rtw_tid]) {
        rtmSetErrorStatusFlag(S, "Sample hit was not ready");
    }
*/


    MdlOutputs(rtw_tid);

    MdlUpdate(rtw_tid);

    rt_SimUpdateDiscreteTaskTime(rtmGetTPtr(S), 
                                 rtmGetTimingData(S),rtw_tid);

    return rtmGetErrorStatusFlag(S);

} /* end rtOneStepTid */

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
    int i;

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

//    if ((errmsg = rtw_capi_init(S, 
//                    &rtw_model.get_signal_info,
//                    &rtw_model.get_param_info,
//                    &rtw_model.signal_count,
//                    &rtw_model.param_count,
//                    &rtw_model.variable_path_len))) {
//        return errmsg;
//    }

    for (i = 0; i < rtw_model.numst; i++) {
        task_period[i] = (unsigned int)
            (rtmGetSampleTime(S, i + (TID01EQ ? 1 : 0))*1.0e6 + 0.5);
    }

    // Cannot assign this in struct definition :(
//    rtw_model.symbol_len = model_symbols_len;

    return NULL;

} /* end main */