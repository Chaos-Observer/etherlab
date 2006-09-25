/*
 * $RCSfile: el10xx.c,v $
 * $Revision: 1.2 $
 * $Date: 2006/02/15 13:43:49 $
 *
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  xPCI1710_digio
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "get_string.h"

#define CARD_ID                         (ssGetSFcnParam(S,0))
#define MODULE      ((uint_T)mxGetScalar(ssGetSFcnParam(S,1)))
#define A_DIR       ((uint_T)mxGetScalar(ssGetSFcnParam(S,2)))
#define B_DIR       ((uint_T)mxGetScalar(ssGetSFcnParam(S,3)))
#define PARAM_COUNT                                       4

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
    uint_T dataType[] = {  0, /*i Dummy, DTYPE starts at 1 */
        SS_DOUBLE, SS_SINGLE, 
        SS_INT8, SS_UINT8, 
        SS_INT16, SS_UINT16,
        SS_INT32, SS_UINT32, 
        SS_BOOLEAN, 
        DYNAMICALLY_TYPED
    };
    

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
    if (A_DIR != 1 || B_DIR != 1) {
        if (!ssSetNumInputPorts(S,0)) return;
        ssSetInputPortWidth(   S, 0, 1);
        ssSetInputPortDataType(S, 0, DYNAMICALLY_TYPED);
        ssSetInputPortDirectFeedThrough(S, 0, 1);
    }

    /*
     * Set Outputs
     */
    if (!ssSetNumOutputPorts(S, 1)) return;
    ssSetOutputPortWidth(   S, 0, 1);
    ssSetOutputPortDataType(S, 0, SS_UINT8);

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, 1);
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
    const char *card_id = getString(S,CARD_ID);
    int32_T module = MODULE-1;
    int32_T clock_source = CLOCK_SRC;
    int32_T divisor = DIVISOR;

    if (!ssWriteRTWStrParam(S, "CardId", card_id))
        return;
    if (!ssWriteRTWScalarParam(S, "Module", &module, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ClockSrc", &clock_source, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Divisor", &divisor, SS_INT32))
        return;
    if (!ssWriteRTWWorkVect(S, "PWork", 1, "PrivData", 1))
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
