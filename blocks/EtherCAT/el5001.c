/*
 * $RCSfile: el5001.c,v $
 * $Revision$
 * $Date$
 *
 * SFunction to implement Beckhoff's SSI input coupler EL5001.
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  el5001
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "ethercat_ss_funcs.h"
#include "ss_analog_in_funcs.c"

#define MASTER     ((uint_T)mxGetScalar(ssGetSFcnParam(S,0)))
#define INDEX                          (ssGetSFcnParam(S,1))
#define SCALE_IDX                                        2
#define OFFSET_IDX                                       3
#define OP_DTYPE   ((uint_T)mxGetScalar(ssGetSFcnParam(S,4)))
#define SSI_CODE   ((uint_T)mxGetScalar(ssGetSFcnParam(S,5)))
#define BAUDRATE   ((uint_T)mxGetScalar(ssGetSFcnParam(S,6)))
#define FRAMETYPE  ((uint_T)mxGetScalar(ssGetSFcnParam(S,7)))
#define FRAMESIZE  ((uint_T)mxGetScalar(ssGetSFcnParam(S,8)))
#define DATALEN    ((uint_T)mxGetScalar(ssGetSFcnParam(S,9)))
#define FRAME_ERR  ((uint_T)mxGetScalar(ssGetSFcnParam(S,10)))
#define INHIBIT    ((uint_T)mxGetScalar(ssGetSFcnParam(S,11)))
#define INH_TIME   ((uint_T)mxGetScalar(ssGetSFcnParam(S,12)))
#define STATUS     ((uint_T)mxGetScalar(ssGetSFcnParam(S,13)))
#define FILTER     ((uint_T)mxGetScalar(ssGetSFcnParam(S,14)))
#define OMEGA_IDX                                        15
#define TSAMPLE            (mxGetScalar(ssGetSFcnParam(S,16)))
#define PARAM_COUNT                                      17

struct el5001 {
    uint_T status;
    uint_T raw;
    uint_T scale;
    int_T op_type;
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
    struct el5001 *devInstance;
    uint_T i;
    uint_T op_dtype;
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    if (!(devInstance = (struct el5001 *)calloc(1,sizeof(struct el5001)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }
    ssSetUserData(S,devInstance);

    devInstance->status = STATUS;
    if (FILTER)
        devInstance->filter = (TSAMPLE == 0) ? 1 : 2;

    switch (OP_DTYPE) {
        case 1:         /* Raw */
            devInstance->raw = 1;
            op_dtype = SS_UINT32;
            break;
        case 2:         /* Double */
            op_dtype = SS_DOUBLE;
            break;
        case 3:         /* Double with scale and offset */
            op_dtype = SS_DOUBLE;
            devInstance->scale = 1;
            break;
        default:
            ssSetErrorStatus(S, "Invalid output data type selected");
            return;
    }

    /*
     * Set Inputs and Outputs
     */
    if (!ssSetNumInputPorts(S,0)) return;

    if (!ssSetNumOutputPorts(S,devInstance->status ? 2 : 1)) return;
    ssSetOutputPortWidth(S,0,1);
    ssSetOutputPortDataType(S,0,op_dtype);
    if (devInstance->status) {
        ssSetOutputPortWidth(S,1,1);
        ssSetOutputPortDataType(S,1,SS_UINT8);
    }

    ssSetNumSampleTimes(S, 1);
    if (devInstance->filter) {
        if (TSAMPLE) {
            ssSetNumDiscStates(S, DYNAMICALLY_SIZED);
        } else {
            ssSetNumContStates(S, DYNAMICALLY_SIZED);
        }
    } else {
        ssSetNumContStates(S, 0);
        ssSetNumDiscStates(S, 0);
    }
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, devInstance->status ? 2 : 1);
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
    struct el5001 *devInstance = (struct el5001 *)ssGetUserData(S);

    devInstance->paramCount = 0;
    if (devInstance->scale)
        devInstance->paramCount += 2;
    if (devInstance->filter)
        devInstance->paramCount++;

    ssSetNumRunTimeParams(S, devInstance->paramCount);

    /* Only when picking the third choice of Data Type
     * does Output scaling take place */
    devInstance->paramCount = 0;
    if (devInstance->scale) {
        set_scaling(S, 1, SCALE_IDX, OFFSET_IDX, 
                devInstance->paramCount);
        if (ssGetErrorStatus(S))
            return;
        devInstance->paramCount += 2;
    }

    if (devInstance->filter) {
        set_filter(S, 1, OMEGA_IDX, devInstance->paramCount);
        if (ssGetErrorStatus(S))
            return;
        devInstance->paramCount += 1;
    }
}

#define MDL_DERIVATIVES
static void mdlDerivatives(SimStruct *S)
{
    /* Required, otherwise Simulink complains if the filter is chosen
     * while in continuous sample time */
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
    struct el5001 *devInstance = (struct el5001 *)ssGetUserData(S);

    if (!devInstance)
        return;

    /* If output scaling is used, we malloc()'ed some data space */
    for (i = 0; i < devInstance->paramCount; i++) {
        p = ssGetRunTimeParamInfo(S,i);
        free(p->data);
    }
        
    free(devInstance);
}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
    struct el5001 *devInstance = (struct el5001 *)ssGetUserData(S);
    int32_T master = MASTER;
    const char *addr = getString(S,INDEX);
    int32_T frame_error = FRAME_ERR ? 1 : 0;
    int32_T power_fail = 1;
    int32_T ssi_code = (SSI_CODE == 1) ? 0 : 1;
    int32_T baud = BAUDRATE;
    int32_T frame_type = FRAMETYPE - 1;
    int32_T frame_size = FRAMESIZE;
    int32_T data_len = DATALEN;
    int32_T enable_inhibit = INHIBIT ? 1 : 0;
    int32_T inh_time = INH_TIME;
    real_T inputmax;

    switch (FRAMETYPE) {
        case 1: /* Multiturn 25bits */
            inputmax = 1U<<(25+1);      /* 25 Significant bits */
            frame_type = 0;
            break;
        case 2: /* Singleturn 13bits */
            inputmax = 1U<<(13+1);      /* 13 Significant bits */
            frame_type = 1;
            break;
        case 3: /* Variable */
            inputmax = (real_T)(~0U>>(32-frame_size)) + 1.0;
            frame_type = 2;
            break;
        default:
            ssSetErrorStatus(S, "Invalid setting for Frame Type");
    }

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddr", addr))
        return;

    /* Configuration for the SSI */
    if (!ssWriteRTWScalarParam(S, "FrameError", &frame_error, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "PowerFail", &power_fail, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "EnableInhibit", &enable_inhibit, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "SSICode", &ssi_code, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "BaudRate", &baud, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "FrameType", &frame_type, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "FrameSize", &frame_size, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "DataLength", &data_len, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "InhibitTime", &inh_time, SS_INT32))
        return;

    /* Management data to write inline code for the block */
    if (!ssWriteRTWScalarParam(S, "RawOp", &devInstance->raw, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ScaleOp", &devInstance->scale, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "StatusOp", &devInstance->status, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Filter", &devInstance->filter, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "InputMax", &inputmax, SS_DOUBLE))
        return;
    if (devInstance->status) { 
        if (!ssWriteRTWWorkVect(S, "PWork", 2, 
                    "SSI_Long", 1, 
                    "StatusByte", 1))
                return;
    } else {
        if (!ssWriteRTWWorkVect(S, "PWork", 1, 
                    "SSI_Long", 1))
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
