/*
 *
 * SFunction to register output variables
 *
 * Copyright (c) 2007, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  etherlab_in
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "get_string.h"

#define ID                             (ssGetSFcnParam(S,0)) 
#define SRC_DTYPE  ((uint_T)mxGetScalar(ssGetSFcnParam(S,1)))
#define LEN        ((uint_T)mxGetScalar(ssGetSFcnParam(S,2)))
#define SCALE_IDX                                        3
#define OFFSET_IDX                                       4
#define OUT_DTYPE  ((uint_T)mxGetScalar(ssGetSFcnParam(S,5)))
#define TSAMPLE            (mxGetScalar(ssGetSFcnParam(S,6)))
#define PARAM_COUNT                                      7


struct {
    uint_T type;
    uint_T len;
    char_T *ctype;
} ss_dtype_properties[] = {
    {0,}, 
    {SS_BOOLEAN, sizeof(boolean_T), "boolean_T"},
    {SS_INT8,  sizeof(int8_T),      "int8_T"  }, 
    {SS_UINT8, sizeof(uint8_T),     "uint8_T" }, 
    {SS_INT16,  sizeof(int16_T),    "int16_T" }, 
    {SS_UINT16, sizeof(uint16_T),   "uint16_T"}, 
    {SS_INT32,  sizeof(int32_T),    "int32_T" }, 
    {SS_UINT32, sizeof(uint32_T),   "uint32_T"}, 
    {SS_SINGLE, sizeof(real32_T),   "real32_T"}, 
    {SS_DOUBLE, sizeof(real_T),     "real_T"  }, 
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
    uint_T i;


    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    if (SRC_DTYPE == 0 || SRC_DTYPE > 9) {
        ssSetErrorStatus(S, "Unknown source data type");
        return;
    }
    
    if (!ssSetNumInputPorts(S, 0)) return;

    if (!ssSetNumOutputPorts(S, 1)) return;
    ssSetOutputPortWidth(S, 0, LEN);
    switch (OUT_DTYPE) {
        case 1:         /* Same as source */
            ssSetOutputPortDataType(S,0,
                    ss_dtype_properties[SRC_DTYPE].type);
            break;
        case 2:         /* Double */
            ssSetOutputPortDataType(S,0,SS_DOUBLE);
            break;
        case 3:         /* Double with scale and offset */
            ssSetOutputPortDataType(S,0,SS_DOUBLE);
            break;
        default:
            ssSetErrorStatus(S, "Invalid output type selected");
            return;
    }

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    if (OUT_DTYPE != 1)
        ssSetNumPWork(S, 
                LEN * ss_dtype_properties[SRC_DTYPE].len / sizeof(void*) + 1);
    else
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
    uint32_T len = LEN;

    if (!ssWriteRTWStrParam(S, "VarName", id))
        return;
    if (!ssWriteRTWStrParam(S, "CTypeName", 
                ss_dtype_properties[SRC_DTYPE].ctype))
        return;
    if (!ssWriteRTWScalarParam(S, "VectorLen", &len, SS_UINT32))
        return;
    if (OUT_DTYPE != 1) {
        if (!ssWriteRTWWorkVect(S, "PWork", 1, 
                    "Buffer", 
                    (LEN * ss_dtype_properties[SRC_DTYPE].len 
                     / sizeof(void*) + 1)));
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
