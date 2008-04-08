/*
 * $RCSfile: el10xx.c,v $
 * $Revision$
 * $Date$
 *
 * SFunction to implement Beckhoff's series EL10xx of EtherCAT Digital Input 
 * Terminals
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  el10xx
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "ethercat_ss_funcs.h"

#define PARAM_COUNT 6
#define MASTER   ((uint_T)mxGetScalar(ssGetSFcnParam(S,0)))
#define ADDR                         (ssGetSFcnParam(S,1)) 
#define DEVICE_STR                   (ssGetSFcnParam(S,2))
#define OP_TYPE  ((uint_T)mxGetScalar(ssGetSFcnParam(S,3)))
#define OP_DTYPE ((uint_T)mxGetScalar(ssGetSFcnParam(S,4)))
#define TSAMPLE          (mxGetScalar(ssGetSFcnParam(S,5)))

struct el10xx_dev {
    int_T       width;              /* Number of input channels */
    char_T      *device_model;
    char_T      *input_pdo;         /* Name for input pdo */
};

struct el10xx_dev el1002 = {2,"Beckhoff_EL1002", "Beckhoff_EL1002_PDO_Inputs"};
struct el10xx_dev el1004 = {4,"Beckhoff_EL1004", "Beckhoff_EL1004_PDO_Inputs"};
struct el10xx_dev el1004v2 = {4,"Beckhoff_EL1004", "Beckhoff_EL1004v2_PDO_Inputs"};
struct el10xx_dev el1008 = {8,"Beckhoff_EL1008", "Beckhoff_EL1008_PDO_Inputs"};
struct el10xx_dev el1012 = {2,"Beckhoff_EL1012", "Beckhoff_EL1012_PDO_Inputs"};
struct el10xx_dev el1014 = {4,"Beckhoff_EL1014", "Beckhoff_EL1014_PDO_Inputs"};
struct el10xx_dev el1018 = {8,"Beckhoff_EL1018", "Beckhoff_EL1018_PDO_Inputs"};

struct supportedDevice supportedDevices[] = {
        {"EL1002", &el1002},
        {"EL1004", &el1004},
        {"EL1004v2", &el1004v2},
        {"EL1008", &el1008},
        {"EL1012", &el1012},
        {"EL1014", &el1014},
        {"EL1018", &el1018},
        {"",0}};    

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
    uint_T op_dtype;
    const struct supportedDevice *dev;
    struct el10xx_dev *devInstance;
    
    /* See sfuntmpl_doc.c for more details on the macros below */
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    dev = getDevice(S,supportedDevices,DEVICE_STR);
    if (ssGetErrorStatus(S))
        return;
    devInstance = (struct el10xx_dev *)dev->priv_data;

    ssSetUserData(S,devInstance);
    
    switch (OP_DTYPE) {
        case 1:         /* Same as input */
            op_dtype = (OP_TYPE == 1) ? SS_UINT8 : SS_BOOLEAN;
            break;
        case 2:         /* Double */
            op_dtype = SS_DOUBLE;
            break;
        case 3:         /* Single */
            op_dtype = SS_SINGLE;
            break;
        case 4:         /* Inherited */
            op_dtype = DYNAMICALLY_TYPED;
            break;
        default:
            ssSetErrorStatus(S, "Invalid output data type selected");
            return;
    }

    /*
     * Set Inputs
     */
    if (!ssSetNumInputPorts(S,0)) return;

    /*
     * Set Outputs
     */
    if (OP_TYPE == 3) {
        if (!ssSetNumOutputPorts(S, devInstance->width)) return;
    } else {
        if (!ssSetNumOutputPorts(S, 1)) return;
    }

    for( i = 0; i < ssGetNumOutputPorts(S); i++) {
        /* Only by "Vector Input" have CHANNELS port width, else 1 */
        ssSetOutputPortWidth(   S, i, 
                OP_TYPE == 2 ? devInstance->width : 1);
        ssSetOutputPortDataType(S, i, op_dtype);
    }

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
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
    int32_T master = MASTER;
    const char *addr = getString(S,ADDR);
    int32_T op_type = OP_TYPE;
    struct el10xx_dev *devInstance = (struct el10xx_dev *)ssGetUserData(S);

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddr", addr))
        return;
    if (!ssWriteRTWStrParam(S, "DeviceModel", devInstance->device_model))
        return;
    if (!ssWriteRTWStrParam(S, "Input_PDO", devInstance->input_pdo))
        return;
    if (!ssWriteRTWScalarParam(S, "Width", &devInstance->width, SS_INT32))
        return;
    if (!ssWriteRTWScalarParam(S, "OpType", &op_type, SS_INT32))
        return;
    if (!ssWriteRTWWorkVect(S, "PWork", 1, "InputByte", 1))
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
