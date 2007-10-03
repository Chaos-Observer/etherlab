/*
 *
 * SFunction to register output variables
 *
 * Copyright (c) 2007, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  etherlab_out
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "get_string.h"

#define ID                             (ssGetSFcnParam(S,0)) 
#define DTYPE      ((uint_T)mxGetScalar(ssGetSFcnParam(S,1)))
#define TSAMPLE            (mxGetScalar(ssGetSFcnParam(S,2)))
#define PARAM_COUNT                                      3


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

    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    if (DTYPE == 0 || DTYPE >= 13) {
        ssSetErrorStatus(S, "Unknown data type");
        return;
    }
    
    if (!ssSetNumInputPorts(S, 1)) return;
    ssSetInputPortWidth(S, 0, DYNAMICALLY_SIZED);
    ssSetInputPortDataType(S, 0, DYNAMICALLY_TYPED);
    ssSetInputPortDirectFeedThrough(S, 0, 1);
    ssSetInputPortRequiredContiguous(S,0,0);

    if (!ssSetNumOutputPorts(S, 0)) return;

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
 *    block. Generally outputs are placed in the input vector, ssGetY(S).
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
    const char *id      = getString(S,ID);
    uint32_T dtype = DTYPE;
    char *ctype_name[] = { "doesnt exist",
        "default",
        "boolean_T",
        "int8_T" , "uint8_T",
        "int16_T", "uint16_T",
        "int32_T", "uint32_T",
        "sint64_T", "uint64_T",
        "real32_T", "real_T",
    };
    DTypeId default_type = ssGetInputPortDataType(S,0);

    if (dtype == 1) {
        switch (default_type) {
            case SS_DOUBLE:
                dtype = 12;
                break;
            case SS_SINGLE:
                dtype = 11;
                break;
            case SS_INT8:
                dtype = 3;
                break;
            case SS_UINT8:
                dtype = 4;
                break;
            case SS_INT16:
                dtype = 5;
                break;
            case SS_UINT16:
                dtype = 6;
                break;
            case SS_INT32:
                dtype = 7;
                break;
            case SS_UINT32:
                dtype = 8;
                break;
            case SS_BOOLEAN:
                dtype = 2;
                break;
            default:
                ssSetErrorStatus(S, "Unknown data type");
                return;
        }
    }

    if (!ssWriteRTWStrParam(S, "VarName", id))
        return;
    if (!ssWriteRTWStrParam(S, "CTypeName", ctype_name[dtype]))
        return;
    if (!ssWriteRTWWorkVect(S, "PWork", 1, "Addr", 1))
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
