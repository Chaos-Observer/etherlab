/*
 * $RCSfile: el31xx.c,v $
 * $Revision: 1.2 $
 * $Date: 2006/02/15 13:45:03 $
 *
 * SFunction to implement Beckhoff's series EL31xx of EtherCAT Analog Input 
 * Terminals
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  el31xx
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "ethercat_ss_funcs.h"
#include "ss_analog_in_funcs.c"

#define PARAM_COUNT 11
#define MASTER     ((uint_T)mxGetScalar(ssGetSFcnParam(S,0)))
#define INDEX                          (ssGetSFcnParam(S,1))
#define MODEL_STR                      (ssGetSFcnParam(S,2))
#define OP_TYPE    ((uint_T)mxGetScalar(ssGetSFcnParam(S,3)))
#define SCALE_IDX                                        4
#define OFFSET_IDX                                       5
#define OP_DTYPE   ((uint_T)mxGetScalar(ssGetSFcnParam(S,6)))
#define STATUS     ((uint_T)mxGetScalar(ssGetSFcnParam(S,7)))
#define FILTER     ((uint_T)mxGetScalar(ssGetSFcnParam(S,8)))
#define OMEGA_IDX                                        9   
#define TSAMPLE            (mxGetScalar(ssGetSFcnParam(S,10)))

struct el31xx_dev {
    real_T rawFullScale;
    real_T rawOffset;
    int_T sign;
    uint_T width;
    char_T *input_pdo;
    char_T *status_pdo;
};

struct el31xx_dev el3102_dev = {10.0, 0.0,   1, 2, 
    "Beckhoff_EL3102_Input", "Beckhoff_EL3102_Status" };
struct el31xx_dev el3104_dev = {10.0, 0.0,   1, 4, 
    "Beckhoff_EL3104_Input", "Beckhoff_EL3104_Status"};
struct el31xx_dev el3108_dev = {10.0, 0.0,   1, 8, 
    "Beckhoff_EL3108_Input", "Beckhoff_EL3108_Status" };

struct el31xx_dev el3112_dev = {0.02, 0.0,   0, 2, 
    "Beckhoff_EL3112_Input", "Beckhoff_EL3112_Status"};
struct el31xx_dev el3114_dev = {0.02, 0.0,   0, 4, 
    "Beckhoff_EL3114_Input", "Beckhoff_EL3114_Status"};

struct el31xx_dev el3122_dev = {0.16, 0.004, 0, 2, 
    "Beckhoff_EL3122_Input", "Beckhoff_EL3122_Status"};
struct el31xx_dev el3124_dev = {0.16, 0.004, 0, 4, 
    "Beckhoff_EL3124_Input", "Beckhoff_EL3124_Status"};

struct el31xx_dev el3142_dev = {0.02, 0.0,   0, 2, 
    "Beckhoff_EL3142_Input", "Beckhoff_EL3142_Status"};
struct el31xx_dev el3144_dev = {0.02, 0.0,   0, 4, 
    "Beckhoff_EL3144_Input", "Beckhoff_EL3144_Status"};
struct el31xx_dev el3148_dev = {0.02, 0.0,   0, 8, 
    "Beckhoff_EL3148_Input", "Beckhoff_EL3148_Status"};

struct el31xx_dev el3152_dev = {0.16, 0.004, 0, 2, 
    "Beckhoff_EL3152_Input", "Beckhoff_EL3152_Status"};
struct el31xx_dev el3154_dev = {0.16, 0.004, 0, 4, 
    "Beckhoff_EL3154_Input", "Beckhoff_EL3154_Status"};
struct el31xx_dev el3158_dev = {0.16, 0.004, 0, 8, 
    "Beckhoff_EL3158_Input", "Beckhoff_EL3158_Status"};

struct el31xx_dev el3162_dev = {10.0, 0.0,   0, 2,
    "Beckhoff_EL3162_Input", "Beckhoff_EL3162_Status"};
struct el31xx_dev el3164_dev = {10.0, 0.0,   0, 4,
    "Beckhoff_EL3164_Input", "Beckhoff_EL3164_Status"};
struct el31xx_dev el3168_dev = {10.0, 0.0,   0, 8,
    "Beckhoff_EL3168_Input", "Beckhoff_EL3168_Status"};

struct supportedDevice supportedDevices[] = {
    {"EL3102", &el3102_dev},
    {"EL3104", &el3104_dev},
    {"EL3108", &el3108_dev},

    {"EL3112", &el3112_dev},
    {"EL3114", &el3114_dev},

    {"EL3122", &el3122_dev},
    {"EL3124", &el3124_dev},

    {"EL3142", &el3142_dev},
    {"EL3144", &el3144_dev},
    {"EL3148", &el3148_dev},

    {"EL3152", &el3152_dev},
    {"EL3154", &el3154_dev},
    {"EL3158", &el3158_dev},

    {"EL3162", &el3162_dev},
    {"EL3164", &el3164_dev},
    {"EL3168", &el3168_dev},
    {}
};

struct el31xx {
    const struct supportedDevice *model;
    uint_T vector;
    uint_T status;
    uint_T raw;
    uint_T scale;
    int_T op_type;
    int_T filter;       /* 0: No filter
                         * 1: Continuous filter
                         * 2: Discrete filter
                         */
    int_T paramCount;   /* Number of runtime parameters */
    struct el31xx_dev *device;
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
    int_T i, width;
    struct el31xx *devInstance;
    struct el31xx_dev *device;
    const struct supportedDevice *model;
    uint_T op_dtype;
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    model = getDevice(S, supportedDevices, MODEL_STR);
    if (ssGetErrorStatus(S))
        return;

    if (!(devInstance = (struct el31xx *)calloc(1,sizeof(struct el31xx)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }
    ssSetUserData(S,devInstance);

    devInstance->device = device;
    devInstance->op_type = OP_DTYPE;
    if (FILTER)
        devInstance->filter = (TSAMPLE == 0) ? 1 : 2;
    devInstance->device = device =
        (struct el31xx_dev *)model->priv_data;

    switch (OP_DTYPE) {
        case 1:         /* Raw */
            devInstance->raw = 1;
            op_dtype = device->sign ? SS_INT16 : SS_UINT16;
            break;
        case 2:         /* Double */
            op_dtype = SS_DOUBLE;
            break;
        case 3:         /* Double with scale and offset */
            op_dtype = SS_DOUBLE;
            devInstance->scale = 1;
            break;
        default:
            ssSetErrorStatus(S, "Invalid output type selected");
            return;
    }

    /*
     * Set Inputs and Outputs
     */
    switch (OP_TYPE) {
        case 1:         
            /* Selected Vector output */
            devInstance->vector = 1;

            if (!ssSetNumInputPorts(S, 0)) return;

            /* Check for Status output */
            if (STATUS) {
                /* OK, 2 outputs, vectorized */
                devInstance->status = 1;
                if (!ssSetNumOutputPorts(S, 2)) return;

                ssSetOutputPortWidth(S, 0, device->width);
                ssSetOutputPortDataType(S, 0, op_dtype);
                ssSetOutputPortWidth(S, 1, device->width);
                ssSetOutputPortDataType(S, 1, SS_UINT8);
            } else {
                if (!ssSetNumOutputPorts(S, 1)) return;

                ssSetOutputPortWidth(S, 0, device->width);
                ssSetOutputPortDataType(S, 0, op_dtype);
            }

            break;

        case 2:         /* Separate outputs */
            if (!ssSetNumInputPorts(S, 0)) return;
            if (!ssSetNumOutputPorts(S, device->width))
                return;
            for( i = 0; i < device->width; i++) {
                ssSetOutputPortWidth(S, i, 1);
                ssSetOutputPortDataType(S, i, op_dtype);
            }
            break;

        default:
            ssSetErrorStatus(S, "Selected Output Type is not supported");
            return;
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
    ssSetNumPWork(S, (devInstance->status ? 2 : 1) * device->width);
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
    struct el31xx *devInstance = (struct el31xx *)ssGetUserData(S);
    struct el31xx_dev *device = devInstance->device;

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
        set_scaling(S, device->width, SCALE_IDX, OFFSET_IDX, 
                devInstance->paramCount);
        if (ssGetErrorStatus(S))
            return;
        devInstance->paramCount += 2;
    }

    if (devInstance->filter) {
        set_filter(S, device->width, OMEGA_IDX, devInstance->paramCount);
        if (ssGetErrorStatus(S))
            return;
        devInstance->paramCount += 1;
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
    struct el31xx *devInstance = (struct el31xx *)ssGetUserData(S);
    int_T i;

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
    struct el31xx *devInstance = (struct el31xx *)ssGetUserData(S);
    struct el31xx_dev *device = devInstance->device;
    int32_T master = MASTER;
    const char *addr = getString(S,INDEX);

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddr", addr))
        return;
    if (!ssWriteRTWStrParam(S, "Input_PDO", 
                devInstance->device->input_pdo))
        return;
    if (!ssWriteRTWScalarParam(S, "RawFullScale", 
                &device->rawFullScale, SS_DOUBLE))
        return;
    if (!ssWriteRTWScalarParam(S, "RawOffset", 
                &device->rawOffset, SS_DOUBLE))
        return;
    if (!ssWriteRTWScalarParam(S, "Width", 
                &device->width, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "VectorOp", 
                &devInstance->vector, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "OpType", 
                &devInstance->op_type, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "RawOp", 
                &devInstance->raw, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "ScaleOp", 
                &devInstance->scale, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "StatusOp", 
                &devInstance->status, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Signed", 
                &device->sign, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Filter", 
                &devInstance->filter, SS_INT32))
        return;
    if (devInstance->status) { 
        if (!ssWriteRTWStrParam(S, "Status_PDO", 
                    devInstance->device->status_pdo))
            return;
        if (!ssWriteRTWWorkVect(S, "PWork", 2, "AD_Word", device->width,
                    "StatusByte", device->width))
                return;
    } else {
        if (!ssWriteRTWWorkVect(S, "PWork", 1, "AD_Word", device->width))
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
