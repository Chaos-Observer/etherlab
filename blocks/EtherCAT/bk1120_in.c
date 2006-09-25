/*
 * $RCSfile: el31xx.c,v $
 * $Revision: 1.2 $
 * $Date: 2006/02/15 13:45:03 $
 *
 * SFunction to implement Beckhoff's EK1120 EtherCAT to K-Bus 
 * Terminals
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  bk1120_in
#define S_FUNCTION_LEVEL 2

#include "math.h"
#include "simstruc.h"
#include "ethercat_ss_funcs.h"

#define MASTER     ((uint_T)mxGetScalar(ssGetSFcnParam(S,0)))
#define SLAVE                          (ssGetSFcnParam(S,1))
#define ADDR       ((uint_T)mxGetScalar(ssGetSFcnParam(S,2)))
#define IP_TYPE    ((uint_T)mxGetScalar(ssGetSFcnParam(S,3)))
#define WIDTH      ((uint_T)mxGetScalar(ssGetSFcnParam(S,4)))
#define SWAP       ((uint_T)mxGetScalar(ssGetSFcnParam(S,5)))
#define OP_DTYPE   ((uint_T)mxGetScalar(ssGetSFcnParam(S,6)))
#define OP_SCALE   ((uint_T)mxGetScalar(ssGetSFcnParam(S,7)))
#define SCALE_IDX                                        8
#define OFFSET_IDX                                       9
#define FILTER     ((uint_T)mxGetScalar(ssGetSFcnParam(S,10)))
#define OMEGA_IDX                                        11   
#define TSAMPLE            (mxGetScalar(ssGetSFcnParam(S,12)))
#define PARAM_COUNT                                      13

struct ek1120 {
    uint_T status;
    uint_T raw;
    uint_T scale;
    int_T op_type;
    int_T filter;       /* 0: No filter
                         * 1: Continuous filter
                         * 2: Discrete filter
                         */
};
    

/*====================*
 * S-function methods *
 *====================*/

uint_T dataType[ndtypes] = {  0, /*i Dummy, DTYPE starts at 1 */
    SS_INT8, SS_UINT8, 
    SS_INT16, SS_UINT16,
    SS_INT32, SS_UINT32, 
    SS_BOOLEAN, 
    SS_DOUBLE, SS_SINGLE, 
    DYNAMICALLY_TYPED
};
real_T input_range[NDTYPES] = {0.0,
    1<<7, 1<<8, 
    1<<15, 1<<16, 
    1<<31, 1<<32, 
    1, 
    1, 1, 
    0};
/* Number of data types that is accounted for */
int_T ndtypes = sizeof(dataType)/sizeof(dataType[0]);

/* Function: mdlInitializeSizes ===============================================
 * Abstract:
 *    The sizes information is used by Simulink to determine the S-function
 *    block's characteristics (number of inputs, outputs, states, etc.).
 */
static void mdlInitializeSizes(SimStruct *S)
{
    int_T i, width;
    struct ek1120 *devInstance;
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    if (OP_DTYPE >= ndtypes)
        /* This can only happen if the data types in the simulink
         * block is changed without updating the data type structures
         * above */
        ssSetErrorStatus(S, "Invalid output data type selected");
        return;
    }

    if (!(devInstance = (struct ek1120 *)calloc(1,sizeof(struct ek1120)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }
    ssSetUserData(S,devInstance);

    devInstance->op_type = dataType[OP_DTYPE];
    devInstance->scale = SCALE;
    devInstance->width = WIDTH;

    if (!ssSetNumInputPorts(S, 0)) return;

    if (!ssSetNumOutputPorts(S, 1)) return;
    ssSetOutputPortWidth(S, 0, devInstance->width);
    ssSetOutputPortDataType(S, 0, devInstance->op_type);

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, DYNAMICALLY_SIZED);
    ssSetNumDiscStates(S, DYNAMICALLY_SIZED);
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

#include "ss_analog_in_funcs.h"

#define MDL_SET_OUTPUT_PORT_DATA_TYPE
static void mdlSetOutputPortDataType(SimStruct *S, int_T portIdx, int_T dtype)
{
    struct ek1120 *devInstance = (struct ek1120 *)ssGetUserData(S);
    int_T dtype_idx;

    for ( dtype_idx = 0; dtype_idx < ndtypes; dtype_idx++) {
        if ()
            break;
    }

    if (dtype_idx == ndtypes)
        ssSetErrorStatus(S, 
    ssSetOutputPortDataType(S, portIdx, dtype);
    devInstance->input_range = input_range[dtype_idx];

}
#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
    struct ek1120 *devInstance = (struct ek1120 *)ssGetUserData(S);
    int_T paramCount = 0;

    paramCount += devInstance->scale ? 2 : 0;
    paramCount += devInstance->filter ? 1 : 0;

    ssSetNumRunTimeParams(S, paramCount);

    /* Misuse paramCount as a counter variable now */
    paramCount = 0;

    /* Only when picking the third choice of Data Type
     * does Output scaling take place */
    if (devInstance->scale) {
        /* Defined in ss_analog_in_funcs.h */
        set_scaling(S, 0, SCALE_IDX, OFFSET_IDX, paramCount);
        paramCount += 2;
    }

    if (devInstance->filter) {
        /* Defined in ss_analog_in_funcs.h */
        set_filter(S, 0, OMEGA_IDX, paramCount++);
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
    ssParamRec *p;
    struct ek1120 *devInstance = (struct ek1120 *)ssGetUserData(S);

    if (!devInstance)
        return;

    /* If output scaling is used, we malloc()'ed some data space */
    if (devInstance->scale) {

        p = ssGetRunTimeParamInfo(S,0);
        mxFree(p->data);

        p = ssGetRunTimeParamInfo(S,1);
        mxFree(p->data);

        if (devInstance->filter) {
            p = ssGetRunTimeParamInfo(S,2);
            mxFree(p->data);
        }
    }

    free(devInstance);

}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
    struct ek1120 *devInstance = (struct ek1120 *)ssGetUserData(S);
    int32_T master = MASTER;
    int32_T image_offset = ADDR;
    int32_T swap = SWAP;
    const char *addr = getString(S,SLAVE);

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddr", addr))
        return;
    if (!ssWriteRTWScalarParam(S, "ImageOffset", image_offset, SS_INT32))
        return;
    /* IP_TYPE */
    if (!ssWriteRTWScalarParam(S, "Width", 
                &device->width, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Swap", swap, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "OpType", 
                &devInstance->op_type, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ScaleOp", 
                &devInstance->scale, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "RawFullScale", 
                &device->rawFullScale, SS_DOUBLE))
        return;
    if (!ssWriteRTWScalarParam(S, "RawOffset", 
                &device->rawOffset, SS_DOUBLE))
        return;
    if (!ssWriteRTWScalarParam(S, "Filter", 
                &devInstance->filter, SS_INT32))
        return;
    if (!ssWriteRTWWorkVect(S, "PWork", 1, "ImageOffset", device->width))
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
