/*
 * $RCSfile: el10xx.c,v $
 * $Revision: 1.2 $
 * $Date: 2006/02/15 13:43:49 $
 *
 * SFunction to implement Beckhoff's BK1120 EtherCAT to K-Bus 
 * Terminals
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  bk1120_out
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "get_string.h"

#define MASTER     ((uint_T)mxGetScalar(ssGetSFcnParam(S,0)))
#define SLAVE                          (ssGetSFcnParam(S,1))
#define KBUS_OFFSET ((uint_T)mxGetScalar(ssGetSFcnParam(S,2)))
#define OP_TYPE    ((uint_T)mxGetScalar(ssGetSFcnParam(S,3)))
#define SWAP       ((uint_T)mxGetScalar(ssGetSFcnParam(S,4)))
#define TSAMPLE            (mxGetScalar(ssGetSFcnParam(S,5)))
#define PARAM_COUNT                                      6

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
    int_T i, channels;
    
    /* See sfuntmpl_doc.c for more details on the macros below */
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    /*
     * Set Inputs
     */
    if (!ssSetNumInputPorts(S,1)) return;
    ssSetInputPortWidth(S, 0, DYNAMICALLY_SIZED);
    ssSetInputPortDataType(S, 0, DYNAMICALLY_TYPED);
    ssSetInputPortDirectFeedThrough(S, 0, 1);
    ssSetInputPortRequiredContiguous(S,0,0);

    /*
     * Set Outputs
     */
    if (!ssSetNumOutputPorts(S, 0)) return;

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, 1);  /* Used in RTW to store output byte address */
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
}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
    int32_T master = MASTER;
    const char *addr = getString(S,SLAVE);
    int32_T image_offset = KBUS_OFFSET;
    int32_T op_type = OP_TYPE;
    int32_T swap    = SWAP;

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddr", addr))
        return;
    if (!ssWriteRTWScalarParam(S, "ImageOffset", &image_offset, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "OutputDataFormat", &op_type, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Swap", &swap, SS_INT32))
        return;
    if (!ssWriteRTWWorkVect(S, "PWork", 1, "OutputAddr", 1))
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
