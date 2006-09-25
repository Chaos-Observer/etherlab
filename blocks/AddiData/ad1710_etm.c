/*
 * $RCSfile: el10xx.c,v $
 * $Revision: 1.2 $
 * $Date: 2006/02/15 13:43:49 $
 *
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  ad1710_etm
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"

#define PARAM_COUNT 5
#define BOARD        ((uint_T)mxGetScalar(ssGetSFcnParam(S,0)))
#define MODULE       ((uint_T)mxGetScalar(ssGetSFcnParam(S,1)))
#define CLOCK        ((uint_T)mxGetScalar(ssGetSFcnParam(S,2)))
#define TIMING_UNIT  ((uint_T)mxGetScalar(ssGetSFcnParam(S,3)))
#define TIMING_BASE  ((uint_T)mxGetScalar(ssGetSFcnParam(S,4)))

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
    uint_T i;

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
    if (!ssSetNumInputPorts(S,0)) return;

    /*
     * Set Outputs
     */
   if (!ssSetNumOutputPorts(S, 1)) return;
   ssSetOutputPortWidth(   S, 0, 2);
   ssSetOutputPortDataType(S, 0, SS_DOUBLE);

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    ssSetNumRWork(S, 1);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, 0);
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
    ssSetSampleTime(S, 0, 0.0);
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
    int32_T board = BOARD;
    int32_T module = MODULE;
    const char *clock_src;
    int32_T timing_unit = TIMING_UNIT-1;
    int32_T timing_base = TIMING_BASE;

    switch (CLOCK) {
        case 1:
            clock_src = "APCI1710_30MHZ";
            break;
        case 2:
            clock_src = "APCI1710_33MHZ";
            break;
        case 3:
            clock_src = "APCI1710_40MHZ";
            break;
    }

    if (!ssWriteRTWScalarParam(S, "Board", &board, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Module", &module, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "ClockSrc", clock_src))
        return;
    if (!ssWriteRTWScalarParam(S, "TimingUnit", &timing_unit, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "TimingBase", &timing_base, SS_INT32))
        return;
    if (!ssWriteRTWWorkVect(S, "RWork", 1, "RealTime", 1))
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
