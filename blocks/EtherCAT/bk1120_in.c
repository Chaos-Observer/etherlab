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


#define S_FUNCTION_NAME  bk1120_in
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "get_string.h"
#include "ss_analog_in_funcs.c"

#define MASTER      ((uint_T)mxGetScalar(ssGetSFcnParam(S,0)))
#define SLAVE                           (ssGetSFcnParam(S,1))
#define KBUS_OFFSET ((uint_T)mxGetScalar(ssGetSFcnParam(S,2)))
#define IP_DTYPE    ((uint_T)mxGetScalar(ssGetSFcnParam(S,3)))
#define WIDTH       ((uint_T)mxGetScalar(ssGetSFcnParam(S,4)))
#define SWAP        ((uint_T)mxGetScalar(ssGetSFcnParam(S,5)))
#define OP_DTYPE    ((uint_T)mxGetScalar(ssGetSFcnParam(S,6)))
#define SCALING     ((uint_T)mxGetScalar(ssGetSFcnParam(S,7)))
#define SCALE_IDX                                         8
#define OFFSET_IDX                                        9
#define FILTER      ((uint_T)mxGetScalar(ssGetSFcnParam(S,10)))
#define OMEGA_IDX                                         11
#define TSAMPLE             (mxGetScalar(ssGetSFcnParam(S,12)))
#define PARAM_COUNT                                       13

struct bk1120 {
    uint_T scaling;
    real_T inputMax;
    uint_T width;
    int_T ip_dtype;
    int_T filter;       /* 0: No filter
                         * 1: Continuous filter
                         * 2: Discrete filter
                         */
    int_T paramCount;   /* Number of runtime parameters */
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
    int_T i, channels;
    struct bk1120 *devInstance;
    
    /* See sfuntmpl_doc.c for more details on the macros below */
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    if (!(devInstance = (struct bk1120 *)calloc(1,sizeof(struct bk1120)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }
    ssSetUserData(S,devInstance);

    /*
     * Set Inputs
     */
    if (!ssSetNumInputPorts(S,0)) return;

    /*
     * Set Outputs
     */
    if (!ssSetNumOutputPorts(S, 1)) return;
    devInstance->width = WIDTH;
    devInstance->ip_dtype = IP_DTYPE;
    ssSetOutputPortWidth(   S, 0, devInstance->width);
    switch (OP_DTYPE) {
        case 1: 
            {
                uint_T dataType[] = {  0, /*i Dummy, DTYPE starts at 1 */
                    SS_BOOLEAN, SS_BOOLEAN, SS_BOOLEAN, SS_BOOLEAN, 
                    SS_BOOLEAN, SS_BOOLEAN, SS_BOOLEAN, SS_BOOLEAN, 
                    SS_INT8, SS_UINT8, 
                    SS_INT16, SS_UINT16,
                    SS_INT32, SS_UINT32, 
                };
                ssSetOutputPortDataType(S, 0, dataType[devInstance->ip_dtype]);
                break;
            }
        case 2:
        case 3:
            ssSetOutputPortDataType(S, 0, 
                    (OP_DTYPE == 2) ? SS_DOUBLE : SS_SINGLE);
            devInstance->scaling = SCALING;
            devInstance->filter = FILTER;
            break;
        case 4:
            ssSetOutputPortDataType(S, 0, DYNAMICALLY_TYPED);
            break;
        default:
            ssSetErrorStatus(S,"Invalid Output Data type selected.");
    }

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    if (devInstance->filter) {
        if (TSAMPLE) {
            ssSetNumDiscStates(S, DYNAMICALLY_SIZED);
            devInstance->filter = 2;
        } else {
            ssSetNumContStates(S, DYNAMICALLY_SIZED);
            devInstance->filter = 1;
        }
    }
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, 1);  /* Used in RTW to store input byte address */
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
    int_T paramCount;
    struct bk1120 *devInstance = ssGetUserData(S);

    paramCount = 0;
    if (devInstance->scaling)
        paramCount += 2;
    if (devInstance->filter)
        paramCount++;

    ssSetNumRunTimeParams(S, paramCount);
    devInstance->paramCount = paramCount;

    /* Only when picking the third choice of Data Type
     * does Output scaling take place */
    paramCount = 0;
    if (devInstance->scaling) {
        real_T dataTypeMax[] = { 0.0, 
            1.0, 1.0, 1.0, 1.0,
            1.0, 1.0, 1.0, 1.0,
            128.0, 256.0,
            32768.0, 65536.0, 
            2147483648.0, 4294967296.0,
        };

        set_scaling(S, 1, SCALE_IDX, OFFSET_IDX, paramCount);
        if (ssGetErrorStatus(S))
            return;
        paramCount += 2;

        devInstance->paramCount = paramCount;
        devInstance->scaling = 1;
        devInstance->inputMax = dataTypeMax[devInstance->ip_dtype];
    }

    if (devInstance->filter) {
        set_filter(S, 1, OMEGA_IDX, paramCount);
        if (ssGetErrorStatus(S))
            return;
        paramCount += 1;

        devInstance->paramCount = paramCount;
        devInstance->filter = 1;
    }
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
    int_T i;
    ssParamRec *p;
    struct bk1120 *devInstance = ssGetUserData(S);

    for (i = 0; i < devInstance->paramCount; i++) {
        p = ssGetRunTimeParamInfo(S,i);
        free(p->data);
    }

    free(devInstance);
}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
    int32_T master = MASTER;
    const char *addr = getString(S,SLAVE);
    int32_T image_offset = KBUS_OFFSET;
    int32_T swap    = SWAP;
    struct bk1120 *devInstance = ssGetUserData(S);

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(   S, "SlaveAddr", addr))
        return;
    if (!ssWriteRTWScalarParam(S, "ImageOffset", &image_offset, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "InputDataType", 
                &devInstance->ip_dtype, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Width", &devInstance->width, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Swap", &swap, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Scaling", &devInstance->scaling, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "InputMax", 
                &devInstance->inputMax, SS_DOUBLE))
        return;
    if (!ssWriteRTWScalarParam(S, "Filter", &devInstance->filter, SS_INT32))
        return;
    if (!ssWriteRTWWorkVect(   S, "PWork", 1, "InputAddr", 1))
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
