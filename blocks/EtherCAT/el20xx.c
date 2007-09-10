/*
 * $RCSfile: el20xx.c,v $
 * $Revision: 1.2 $
 * $Date: 2006/07/19 12:04:41 $
 *
 * SFunction to implement Beckhoff's series EL200x of EtherCAT Digital Output 
 * Terminals
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  el20xx
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "ethercat_ss_funcs.h"

#define PARAM_COUNT 6
#define MASTER   ((uint_T)mxGetScalar(ssGetSFcnParam(S,0)))
#define ADDR                         (ssGetSFcnParam(S,1))
#define MODEL_STR                    (ssGetSFcnParam(S,2))
#define IP_TYPE  ((uint_T)mxGetScalar(ssGetSFcnParam(S,3)))
#define STATUS   ((uint_T)mxGetScalar(ssGetSFcnParam(S,4)))
#define TSAMPLE          (mxGetScalar(ssGetSFcnParam(S,5)))

struct el2xxx_dev {
    int_T       max_channels;       /* Number of channels */
    char_T      *device_model;      /* Device Model Number */
    char_T      *output_pdo;        /* Name for output pdo */
    char_T      *status_pdo;        /* Set if device has a status output */
};

struct el2xxx_dev el2002 = {2,"Beckhoff_EL2002", "Beckhoff_EL2002_PDO_Outputs"};
struct el2xxx_dev el2004 = {4,"Beckhoff_EL2004", "Beckhoff_EL2004_PDO_Outputs"};
struct el2xxx_dev el2008 = {8,"Beckhoff_EL2008", "Beckhoff_EL2008_PDO_Outputs"};
struct el2xxx_dev el2032 = {2,"Beckhoff_EL2032", "Beckhoff_EL2032_PDO_Outputs",
                                        "Beckhoff_EL2032_PDO_Status"};

struct el2xxx_dev el2002v2 = {2,"Beckhoff_EL2002", "Beckhoff_EL2002v2_PDO_Outputs"};
struct el2xxx_dev el2004v2 = {4,"Beckhoff_EL2004", "Beckhoff_EL2004v2_PDO_Outputs"};
struct el2xxx_dev el2008v2 = {8,"Beckhoff_EL2008", "Beckhoff_EL2008v2_PDO_Outputs"};
struct el2xxx_dev el2032v2 = {2,"Beckhoff_EL2032", "Beckhoff_EL2032v2_PDO_Outputs",
                                        "Beckhoff_EL2032v2_PDO_Status"};

struct supportedDevice supportedDevices[] = {
        {"EL2002v2", &el2002v2},
        {"EL2004v2", &el2004v2},
        {"EL2008v2", &el2008v2},
        {"EL2032v2", &el2032v2},
        {"EL2002", &el2002},
        {"EL2004", &el2004},
        {"EL2008", &el2008},
        {"EL2032", &el2032},
        {},
};    

struct el20xx {
    int_T vector;
    const struct supportedDevice *model;
    const struct el2xxx_dev *device;
    int_T width;
    int_T status;
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
    int_T i;
    const struct supportedDevice *dev;
    struct el20xx *devInstance;
    
    /* See sfuntmpl_doc.c for more details on the macros below */
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    dev = getDevice(S, supportedDevices, MODEL_STR);
    if (ssGetErrorStatus(S))
        return;

    if (!(devInstance = (struct el20xx *)calloc(1,sizeof(struct el20xx)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }
    ssSetUserData(S,devInstance);

    devInstance->model = dev;
    devInstance->device = (struct el2xxx_dev *)dev->priv_data;

    if (STATUS) {
        if (!devInstance->device->status_pdo) {
            ssSetErrorStatus(S, 
                    "Selected model does not provide a status output");
            return;
        } else if (IP_TYPE != 3) {
            devInstance->status = 1;
        } else {
            ssSetErrorStatus(S, 
                    "Status output not allowed for separate inputs");
            return;
        }
    }

    /*
     * Set Inputs
     */
    if (IP_TYPE == 3) {
        if (!ssSetNumInputPorts(S, devInstance->device->max_channels)) 
            return;
    } else {
        devInstance->vector = 1;
        if (!ssSetNumInputPorts(S, 1)) return;
    }

    for( i = 0; i < ssGetNumInputPorts(S); i++) {
        /* Only by "Vector Input" have CHANNELS port width, else 1 */
        ssSetInputPortWidth(   S, i, IP_TYPE == 2 ? DYNAMICALLY_SIZED : 1);
        ssSetInputPortDataType(S, i, DYNAMICALLY_TYPED);
        ssSetInputPortRequiredContiguous(S, i, 0);
        ssSetInputPortDirectFeedThrough(S, i, 1);
    }

    /* If vector inputs is not chosen, width is set to maximum for the
     * device */
    if (IP_TYPE != 2) {
        devInstance->width = devInstance->device->max_channels;
    }

    /*
     * Set Outputs
     */
    if (devInstance->status && devInstance->vector) {
        if (!ssSetNumOutputPorts(S,1)) return;
        ssSetOutputPortWidth(S,0,DYNAMICALLY_SIZED);
        ssSetOutputPortDataType(S,0,SS_UINT8);
    } else {
        if (!ssSetNumOutputPorts(S,0)) return;
    }

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, devInstance->status ? 2 : 1);  /* Used in RTW to store 
                                                       output byte address */
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
    struct el20xx *devInstance = (struct el20xx *)ssGetUserData(S);
    
    if (width > devInstance->device->max_channels) {
        ssSetErrorStatus(S, "Too many inputs to block");
        return;
    }
    devInstance->width = width;
    ssSetInputPortWidth(S, port, width);
}

#define MDL_SET_OUTPUT_PORT_WIDTH
static void mdlSetOutputPortWidth(SimStruct *S, int_T port, int_T width)
{
    struct el20xx *devInstance = (struct el20xx *)ssGetUserData(S);
    
    if (width > devInstance->width) {
        ssSetErrorStatus(S, "Too many inputs to block");
        return;
    }
    ssSetOutputPortWidth(S, port, width);
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
    struct el20xx *devInstance = (struct el20xx *)ssGetUserData(S);

    if (devInstance)
        free(devInstance);
}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
    int32_T master = MASTER;
    const char *addr = getString(S,ADDR);
    int32_T ip_type = IP_TYPE;
    struct el20xx *devInstance = (struct el20xx *)ssGetUserData(S);

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddr", addr))
        return;
    if (!ssWriteRTWStrParam(S, "DeviceModel", devInstance->device->device_model))
        return;
    if (!ssWriteRTWStrParam(S, "Output_PDO", devInstance->device->output_pdo))
        return;
    if (!ssWriteRTWScalarParam(S, "Width", &devInstance->width, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "IpType", &ip_type, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "StatusOp", &devInstance->status, SS_INT32))
        return;
    if (devInstance->status) {
        if (!ssWriteRTWStrParam(S, "Status_PDO",
                    devInstance->device->status_pdo))
            return;
        if (!ssWriteRTWWorkVect(S, "PWork", 2, 
                    "OutputByte", 1, 
                    "StatusByte",1))
            return;
    } else {
        if (!ssWriteRTWWorkVect(S, "PWork", 1, "OutputByte", 1))
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
