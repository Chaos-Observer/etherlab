/*
* $Id$
*
* This SFunction propagates width of port 1 to other ports
*
* Copyright (c) 2017, Richard Hacker
* License: GPLv3+
*
*/

#define S_FUNCTION_NAME  propagate_width
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"



#define PORTS      (int_T)(mxGetScalar(ssGetSFcnParam(S,0)))
#define WIDTH      (int_T)(mxGetScalar(ssGetSFcnParam(S,1)))
#define PARAM_COUNT                                     2

/*====================*
 * S-function methods *
 *====================*/

/* Function: mdlInitializeSizes =============================================
 * Abstract:
 *    The sizes information is used by Simulink to determine the S-function
 *    block's characteristics (number of inputs, outputs, states, etc.).
 */
static void mdlInitializeSizes(SimStruct *S)
{
    int_T i;
    int_T width = WIDTH;

    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }

    for( i = 0; i < PARAM_COUNT; i++)
        ssSetSFcnParamTunable(S, i, SS_PRM_NOT_TUNABLE);

    /* Process input ports */
    if (!ssSetNumInputPorts(S, PORTS))
        return;
    if (!ssSetNumOutputPorts(S, 0))
        return;

    /* Input port */
    for (i = 0; i < ssGetNumInputPorts(S); ++i) {
        ssSetInputPortWidth(S, i, width);
        ssSetInputPortDataType(S, i, DYNAMICALLY_TYPED);
    }

    ssSetNumSampleTimes(S, PORT_BASED_SAMPLE_TIMES);

    ssSetOptions(S,
            SS_OPTION_WORKS_WITH_CODE_REUSE
            | SS_OPTION_RUNTIME_EXCEPTION_FREE_CODE
            | SS_OPTION_CALL_TERMINATE_ON_EXIT);
}

/* Function: mdlInitializeSampleTimes =======================================
 * Abstract:
 *    This function is used to specify the sample time(s) for your
 *    S-function. You must register the same number of sample times as
 *    specified in ssSetNumSampleTimes.
 */
static void mdlInitializeSampleTimes(SimStruct *S)
{
    /*ssSetSampleTime(S, 0, TSAMPLE);*/
}

/* Function: mdlOutputs =====================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector, ssGetY(S).
 */
static void mdlOutputs(SimStruct *S, int_T tid)
{
}

/* Function: mdlTerminate ===================================================
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
