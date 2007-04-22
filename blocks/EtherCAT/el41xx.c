/*
 * $RCSfile: el41xx.c,v $
 * $Revision: 1.3 $
 * $Date: 2006/07/24 22:43:46 $
 *
 * SFunction to implement Beckhoff's series EL41xx of EtherCAT Analog Output 
 * Terminals
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  el41xx
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "ethercat_ss_funcs.h"

#define PARAM_COUNT 6
#define MASTER     ((uint_T)mxGetScalar(ssGetSFcnParam(S,0)))
#define INDEX                          (ssGetSFcnParam(S,1))
#define MODEL_STR                      (ssGetSFcnParam(S,2))
#define OP_TYPE    ((uint_T)mxGetScalar(ssGetSFcnParam(S,3)))
#define STATUS     ((uint_T)mxGetScalar(ssGetSFcnParam(S,4)))
#define TSAMPLE            (mxGetScalar(ssGetSFcnParam(S,5)))

struct el41xx_dev {
    real_T rawFullScale;
    real_T rawOffset;
    int_T sign;
    uint_T max_width;
    char_T *device_model;
    char_T *output_pdo;
    char_T *status_pdo;
};

struct el41xx_dev el4102_dev = {10.0, 0.0,   0, 2, 
    "Beckhoff_EL4102", "Beckhoff_EL4102_PDO_Output", "Beckhoff_EL4102_PDO_Status"};
struct el41xx_dev el4104_dev = {10.0, 0.0,   0, 4, 
    "Beckhoff_EL4104", "Beckhoff_EL4104_PDO_Output", "Beckhoff_EL4104_PDO_Status"};
struct el41xx_dev el4108_dev = {10.0, 0.0,   0, 8, 
    "Beckhoff_EL4108", "Beckhoff_EL4108_PDO_Output", "Beckhoff_EL4108_PDO_Status"};

struct el41xx_dev el4112_dev = {0.02, 0.0,   0, 2, 
    "Beckhoff_EL4112", "Beckhoff_EL4112_PDO_Output", "Beckhoff_EL4112_PDO_Status"};
struct el41xx_dev el4114_dev = {0.02, 0.0,   0, 4, 
    "Beckhoff_EL4114", "Beckhoff_EL4114_PDO_Output", "Beckhoff_EL4114_PDO_Status"};
struct el41xx_dev el4118_dev = {0.02, 0.0,   0, 8, 
    "Beckhoff_EL4118", "Beckhoff_EL4118_PDO_Output", "Beckhoff_EL4118_PDO_Status"};

struct el41xx_dev el4122_dev = {0.16, 0.004, 0, 2, 
    "Beckhoff_EL4122", "Beckhoff_EL4122_PDO_Output", "Beckhoff_EL4122_PDO_Status"};
struct el41xx_dev el4124_dev = {0.16, 0.004, 0, 4, 
    "Beckhoff_EL4124", "Beckhoff_EL4124_PDO_Output", "Beckhoff_EL4124_PDO_Status"};
struct el41xx_dev el4128_dev = {0.16, 0.004, 0, 8, 
    "Beckhoff_EL4128", "Beckhoff_EL4128_PDO_Output", "Beckhoff_EL4128_PDO_Status"};

struct el41xx_dev el4132_dev = {10.0, 0.0,   1, 2, 
    "Beckhoff_EL4132", "Beckhoff_EL4132_PDO_Output", "Beckhoff_EL4132_PDO_Status"};
struct el41xx_dev el4134_dev = {10.0, 0.0,   1, 4, 
    "Beckhoff_EL4134", "Beckhoff_EL4134_PDO_Output", "Beckhoff_EL4134_PDO_Status"};
struct el41xx_dev el4138_dev = {10.0, 0.0,   1, 8, 
    "Beckhoff_EL4138", "Beckhoff_EL4138_PDO_Output", "Beckhoff_EL4138_PDO_Status"};

struct supportedDevice supportedDevices[] = {
    {"EL4102", &el4102_dev},
    {"EL4104", &el4104_dev},
    {"EL4108", &el4108_dev},

    {"EL4112", &el4112_dev},
    {"EL4114", &el4114_dev},
    {"EL4118", &el4118_dev},

    {"EL4122", &el4122_dev},
    {"EL4124", &el4124_dev},
    {"EL4128", &el4128_dev},

    {"EL4132", &el4132_dev},
    {"EL4134", &el4134_dev},
    {"EL4138", &el4138_dev},

    {}
};

struct el41xx {
    const struct supportedDevice *model;
    uint_T vector;
    uint_T status;
    uint_T scale;
    uint_T width;
    uint_T raw;
    struct el41xx_dev *device;
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
    struct el41xx *devInstance;
    struct el41xx_dev *device;
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

    if (!(devInstance = (struct el41xx *)calloc(1,sizeof(struct el41xx)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }
    ssSetUserData(S,devInstance);

    devInstance->model = model;
    devInstance->device = device =
        (struct el41xx_dev *)model->priv_data;

    /*
     * Set Inputs and Outputs
     */
    switch (OP_TYPE) {
        case 1:         
            /* Selected Vector output */
            devInstance->vector = 1;

            if (!ssSetNumInputPorts(S, 1)) return;
            ssSetInputPortWidth(S, 0, DYNAMICALLY_SIZED);
            ssSetInputPortDataType(S, 0, DYNAMICALLY_TYPED);
            ssSetInputPortDirectFeedThrough(S, 0, 1);
            ssSetInputPortRequiredContiguous(S,0,0);

            /* Check for Status output */
            if (STATUS) {
                devInstance->status = 1;
                if (!ssSetNumOutputPorts(S, 1)) return;

                ssSetOutputPortWidth(S, 0, DYNAMICALLY_SIZED);
                ssSetOutputPortDataType(S, 0, SS_UINT8);
            }

            break;

        case 2:         /* Separate outputs */
            if (!ssSetNumInputPorts(S, device->max_width)) return;
            if (!ssSetNumOutputPorts(S, 0)) return;

            for( i = 0; i < device->max_width; i++) {
                ssSetInputPortWidth(S, i, 1);
                ssSetInputPortDataType(S, i, DYNAMICALLY_TYPED);
                ssSetInputPortDirectFeedThrough(S, i, 1);
                ssSetInputPortRequiredContiguous(S,i,0);
            }

            devInstance->width = device->max_width;
            break;

        default:
            ssSetErrorStatus(S, "Selected Output Type is not supported");
            return;
    }

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, DYNAMICALLY_SIZED);
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

#define MDL_SET_INPUT_PORT_WIDTH
static void mdlSetInputPortWidth(SimStruct *S, int_T port, int_T width)
{
    struct el41xx *devInstance = (struct el41xx *)ssGetUserData(S);
    
    if (!devInstance->width) {
        if (width > devInstance->device->max_width) {
            ssSetErrorStatus(S, 
                    "Input vector to Terminal exceeds available channels");
            return;
        }
        devInstance->width = width;
    } else if (devInstance->width != width) {
        ssSetErrorStatus(S,"Input port of block does not match the "
                "vector size set by the Status port");
        return;
    }
    ssSetInputPortWidth(S, port, width);
}

#define MDL_SET_INPUT_PORT_DATA_TYPE
static void mdlSetInputPortDataType(SimStruct *S, int_T port, DTypeId id)
{
    struct el41xx *devInstance = (struct el41xx *)ssGetUserData(S);
    uint_T i;

    /* Port 0 sets the policy for all other input ports */
    if (port == 0) {
        if (id != SS_DOUBLE && 
                id != (devInstance->device->sign ? SS_INT16 : SS_UINT16)) {
            ssSetErrorStatus(S, 
                    devInstance->device->sign 
                    ? "Output only accepts Double or int16_T"
                    : "Output only accepts Double or uint16_T");
            return;
        }

        for (i = 0; i < ssGetNumInputPorts(S); i++)
            ssSetInputPortDataType(S,i,id);
    }
    return;
}

#define MDL_SET_OUTPUT_PORT_WIDTH
static void mdlSetOutputPortWidth(SimStruct *S, int_T port, int_T width)
{
    struct el41xx *devInstance = (struct el41xx *)ssGetUserData(S);
    
    if (!devInstance->width) {
        if (width > devInstance->device->max_width) {
            ssSetErrorStatus(S, "Too many outputs from block");
            return;
        }
        devInstance->width = width;
    } else if (devInstance->width != width) {
        ssSetErrorStatus(S,"Status port of block does not match the "
                "vector size set by the Input port");
        return;
    }
    ssSetOutputPortWidth(S, port, width);
}

#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
    struct el41xx *devInstance = (struct el41xx *)ssGetUserData(S);
    ssSetNumPWork(S, 
            devInstance->status ? devInstance->width*2 : devInstance->width);
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
    struct el41xx *devInstance = (struct el41xx *)ssGetUserData(S);
    struct el41xx_dev *device = devInstance->device;
    int32_T master = MASTER;
    const char *addr = getString(S,INDEX);

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddr", addr))
        return;
    if (!ssWriteRTWStrParam(S, "DeviceModel", 
                devInstance->device->device_model))
        return;
    if (!ssWriteRTWStrParam(S, "Output_PDO", 
                devInstance->device->output_pdo))
        return;
    if (!ssWriteRTWScalarParam(S, "Width", 
                &devInstance->width, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Signed", 
                &device->sign, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "VectorOp", 
                &devInstance->vector, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "RawOp", 
                &devInstance->raw, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "RawFullScale", 
                &device->rawFullScale, SS_DOUBLE))
        return;
    if (!ssWriteRTWScalarParam(S, "RawOffset", 
                &device->rawOffset, SS_DOUBLE))
        return;
    if (!ssWriteRTWScalarParam(S, "StatusOp", 
                &devInstance->status, SS_INT32))
        return;
    if (devInstance->status) { 
        if (!ssWriteRTWStrParam(S, "Status_PDO", 
                    devInstance->device->status_pdo))
            return;
        if (!ssWriteRTWWorkVect(S, "PWork", 2, "DA_Word", devInstance->width,
                    "StatusByte", devInstance->width))
                return;
    } else {
        if (!ssWriteRTWWorkVect(S, "PWork", 1, "DA_Word", devInstance->width))
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
