/*****************************************************************************
 *
 * $Id$
 *
 * SFunction to implement Moog Servodrive Controllers (MSD)
 *
 * Copyright (c) 2007, Florian Pose
 *
 ****************************************************************************/

#define S_FUNCTION_NAME  moog_msd
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "ethercat_ss_funcs.h"
#include "ss_analog_in_funcs.c"

#define PARAM_COUNT 3
#define MASTER     ((uint_T)mxGetScalar(ssGetSFcnParam(S, 0)))
#define INDEX                          (ssGetSFcnParam(S, 1))
#define TSAMPLE            (mxGetScalar(ssGetSFcnParam(S, 2)))

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
    int_T i, width;
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }

    for (i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S, i, SS_PRM_NOT_TUNABLE);

    /*
     * Set Inputs and Outputs
     */
    if (!ssSetNumInputPorts(S, 2)) return;
    if (!ssSetNumOutputPorts(S, 2)) return;

    /* control word */
    ssSetInputPortDataType(S, 0, SS_UINT16);
    ssSetInputPortWidth(S, 0, 1);
    ssSetInputPortDirectFeedThrough(S, 0, 0);

    /* target velocity */
    ssSetInputPortDataType(S, 1, SS_INT32);
    ssSetInputPortWidth(S, 1, 1);
    ssSetInputPortDirectFeedThrough(S, 1, 0);

    /* status word */
    ssSetOutputPortDataType(S, 0, SS_UINT16);
    ssSetOutputPortWidth(S, 0, 1);

    /* current velocity */
    ssSetOutputPortDataType(S, 1, SS_INT32);
    ssSetOutputPortWidth(S, 1, 1);

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, 4);
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

#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
    ssSetNumRunTimeParams(S, 0);
}

/* Function: mdlOutputs =======================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector, ssGetY(S).
 */
static void mdlOutputs(SimStruct *S, int_T tid)
{
}

#define MDL_DERIVATIVES
static void mdlDerivatives(SimStruct *S)
{
    /* Required, otherwise Simulink complains if the filter is chosen
     * while in continuous sample time */
}

/* Function: mdlTerminate =====================================================
 * Abstract:
 *    In this function, you should perform any actions that are necessary
 *    at the termination of a simulation.  For example, if memory was
 *    allocated in mdlStart, this is the place to free it.
 */
static void mdlTerminate(SimStruct *S)
{
}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
    int32_T master = MASTER;
    const char *addr = getString(S, INDEX);

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddr", addr))
        return;
    if (!ssWriteRTWWorkVect(S, "PWork", 4,
                "ControlWord", 1,
                "TargetVelocity", 1,
                "StatusWord", 1,
                "CurrentVelocity", 1
                ))
        return;
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

