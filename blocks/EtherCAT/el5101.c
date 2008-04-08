/*
 * $RCSfile: el5101.c,v $
 * $Revision$
 * $Date$
 *
 * SFunction to implement Beckhoff's input coupler EL5101 Incremental Encoder
 * Terminal
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  el5101
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "ethercat_ss_funcs.h"

#define MASTER      ((uint_T)mxGetScalar(ssGetSFcnParam(S,0)))
#define INDEX                           (ssGetSFcnParam(S,1))

/* PARAMETERS */
#define RELOAD      ((uint_T)mxGetScalar(ssGetSFcnParam(S,2)))
#define INDEX_RESET ((uint_T)mxGetScalar(ssGetSFcnParam(S,3)))
#define FWDCOUNT    ((uint_T)mxGetScalar(ssGetSFcnParam(S,4)))
#define GATE        ((uint_T)mxGetScalar(ssGetSFcnParam(S,5)))
#define FREQWIN     ((uint_T)mxGetScalar(ssGetSFcnParam(S,6)))
#define RELOADVALUE ((uint_T)mxGetScalar(ssGetSFcnParam(S,7)))

/* INPUTS */
#define CONTROL_IN  ((uint_T)mxGetScalar(ssGetSFcnParam(S,8)))
#define PRESET_IN   ((uint_T)mxGetScalar(ssGetSFcnParam(S,9)))

/* OUTPUTS */
#define OP_DTYPE    ((uint_T)mxGetScalar(ssGetSFcnParam(S,10)))
#define VALUE_OUT   ((uint_T)mxGetScalar(ssGetSFcnParam(S,11)))
#define LATCH_OUT   ((uint_T)mxGetScalar(ssGetSFcnParam(S,12)))
#define FREQ_OUT    ((uint_T)mxGetScalar(ssGetSFcnParam(S,13)))
#define PERIOD_OUT  ((uint_T)mxGetScalar(ssGetSFcnParam(S,14)))
#define WINDOW_OUT  ((uint_T)mxGetScalar(ssGetSFcnParam(S,15)))
#define STATUS_OUT  ((uint_T)mxGetScalar(ssGetSFcnParam(S,16)))

#define TSAMPLE             (mxGetScalar(ssGetSFcnParam(S,17)))
#define PARAM_COUNT                                       18

struct el5101 {
    uint_T reload;
    uint_T index_reset;
    uint_T fwdcount;
    uint_T gate;
    uint_T freqwin;
    uint_T reloadvalue;
    
    uint_T inports;
    int_T control_port;
    int_T preset_port;
    
    uint_T outports;
    uint_T raw;
    int_T value_port;
    int_T latch_port;
    int_T freq_port;
    int_T period_port;
    int_T window_port;
    uint_T status_port;
};
    

/*====================*
 * S-function methods *
 *====================*/

/* Function: mdlInitializeSizes ===============================================
 * Abstract:
 *    The sizes information is used by Simulink to determine the S-function
 *    block's characteristics (number of inputs, outputs, states, etc.).
 */
static void mdlInitializeSizes(SimStruct *S)
{
    struct el5101 *devInstance;
    uint_T i;
    uint_T op_dtype;
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    if (!(devInstance = (struct el5101 *)calloc(1,sizeof(struct el5101)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }
    ssSetUserData(S,devInstance);

    /* 
     * Set Parameters
     */
    devInstance->reload      = RELOAD;
    devInstance->index_reset = INDEX_RESET;
    devInstance->fwdcount    = FWDCOUNT;
    devInstance->gate        = GATE;
    devInstance->freqwin     = FREQWIN;
    devInstance->reloadvalue = RELOADVALUE;
    
    
    /* Reset output ports */
    devInstance->outports = 0;
    devInstance->value_port  = -1;
    devInstance->latch_port  = -1;
    devInstance->freq_port   = -1;
    devInstance->period_port = -1;
    devInstance->window_port = -1;
    devInstance->status_port = -1;
    
    switch (OP_DTYPE) {
        case 1:         /* Raw */
            devInstance->raw = 1;
            break;
        case 2:         /* Double */
            devInstance->raw = 0;
            break;
        default:
            ssSetErrorStatus(S, "Invalid output type selected");
            return;
    }
    
    /*
     * Set Inputs and Outputs
     */

    /* 
     * Reset input ports 
     * -1 means that it is not assigned 
     */
    devInstance->inports = 0;
    devInstance->control_port = -1;
    devInstance->preset_port  = -1;
    if (CONTROL_IN) {
        devInstance->control_port = devInstance->inports++;
    }
    if (PRESET_IN) {
        devInstance->preset_port = devInstance->inports++;
    }
    if (!ssSetNumInputPorts(S,devInstance->inports)) return;
    for (i = 0; i < devInstance->inports; i++) {
        ssSetInputPortWidth(S,i,1);
        ssSetInputPortDataType(S, i, DYNAMICALLY_TYPED);
    }
    
    /* 
     * Reset output ports 
     * -1 means that it is not assigned 
     */
    devInstance->outports = 0;
    devInstance->value_port  = -1;
    devInstance->latch_port  = -1;
    devInstance->freq_port   = -1;
    devInstance->period_port = -1;
    devInstance->window_port = -1;
    devInstance->status_port = -1;
    
    /* Value output */
    if (VALUE_OUT) {
        devInstance->value_port = devInstance->outports++;
    }
    /* Latch output */
    if (LATCH_OUT) {
        devInstance->latch_port = devInstance->outports++;
    }
    /* Frequency output */
    if (FREQ_OUT) {
        devInstance->freq_port = devInstance->outports++;
    }
    /* Period output */
    if (PERIOD_OUT) {
        devInstance->period_port = devInstance->outports++;
    }
    /* Window output */
    if (WINDOW_OUT) {
        devInstance->window_port = devInstance->outports++;
    }
    /* Status output */
    if (STATUS_OUT) {
        devInstance->status_port = devInstance->outports++;
    }
    if (!(ssSetNumOutputPorts(S,devInstance->outports)))
        return;
    for (i = 0; i < devInstance->outports; i++) {
        ssSetOutputPortWidth(S,i,1);
        ssSetOutputPortDataType(S, i, 
            devInstance->raw ? SS_UINT16 : DYNAMICALLY_TYPED);
    }
    if (devInstance->raw) {
        /* Frequency output */
        if (devInstance->freq_port != -1)
            ssSetOutputPortDataType(S, 
                devInstance->freq_port, SS_UINT32);
        /* Status output */
        if (devInstance->status_port != -1)
            ssSetOutputPortDataType(S, 
                devInstance->status_port, SS_UINT8);
    }
    
    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, devInstance->inports + devInstance->outports);
    ssSetNumModes(S, 0);
    ssSetNumNonsampledZCs(S, 0);

    ssSetOptions(S, 
            SS_OPTION_WORKS_WITH_CODE_REUSE | 
            SS_OPTION_RUNTIME_EXCEPTION_FREE_CODE);
}

/* Function: mdlInitializeSampleTimes =========================================
 * Abstract:
 *    This function is used to specify the sample time(s) for your
 *    S-function. You must register the same number of sample times as
 *    specified in ssSetNumSampleTimes.
 */
static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, TSAMPLE);
    ssSetOffsetTime(S, 0, 0.0);
}

/* Function: mdlOutputs =======================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector, ssGetY(S).
 */
static void mdlOutputs(SimStruct *S, int_T tid)
{
}

/* Function: mdlTerminate =====================================================
 * Abstract:
 *    In this function, you should perform any actions that are necessary
 *    at the termination of a simulation.  For example, if memory was
 *    allocated in mdlStart, this is the place to free it.
 */
static void mdlTerminate(SimStruct *S)
{
    ssParamRec *p;
    int_T i;
    struct el5101 *devInstance = (struct el5101 *)ssGetUserData(S);

    if (!devInstance)
        return;

    free(devInstance);
}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
    struct el5101 *devInstance = (struct el5101 *)ssGetUserData(S);
    int32_T master = MASTER;
    const char *addr = getString(S,INDEX);

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddr", addr))
        return;

    if (!ssWriteRTWScalarParam(S, "Reload", &devInstance->reload, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "IndexReset", &devInstance->index_reset, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "FwdCount", &devInstance->fwdcount, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Gate", &devInstance->gate, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "FreqWin", &devInstance->freqwin, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ReloadValue", &devInstance->reloadvalue, SS_UINT32))
        return;
    
    if (!ssWriteRTWScalarParam(S, "InPorts", &devInstance->inports, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ControlPort", &devInstance->control_port, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "PresetPort", &devInstance->preset_port, SS_INT32))
        return;

    if (!ssWriteRTWScalarParam(S, "OutPorts", &devInstance->outports, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ValuePort", &devInstance->value_port, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "LatchPort", &devInstance->latch_port, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "FreqPort", &devInstance->freq_port, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "PeriodPort", &devInstance->period_port, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "WindowPort", &devInstance->window_port, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "StatusPort", &devInstance->status_port, SS_INT32))
        return;

    if (devInstance->inports && devInstance->outports) {
        if (!ssWriteRTWWorkVect(S, "PWork", 2, 
                    "InPtr", devInstance->inports, 
                    "OutPtr", devInstance->outports)) 
            return;
    } else if (devInstance->inports) {
        if (!ssWriteRTWWorkVect(S, "PWork", 1, 
                    "InPtr", devInstance->inports))
            return;
    } else if (devInstance->outports) {
        if (!ssWriteRTWWorkVect(S, "PWork", 1, 
                    "OutPtr", devInstance->outports)) 
            return;
    }

}



/*======================================================*
 * See sfuntmpl_doc.c for the optional S-function methods *
 *======================================================*/

/*=============================*
 * Required S-function trailer *
 *=============================*/

#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
