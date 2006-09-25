/*
 * $RCSfile: el10xx.c,v $
 * $Revision: 1.2 $
 * $Date: 2006/02/15 13:43:49 $
 *
 * SFunction to implement Beckhoff's series EL200x of EtherCAT Digital Output 
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
#define DTYPE    ((uint_T)mxGetScalar(ssGetSFcnParam(S,4)))
#define TSAMPLE          (mxGetScalar(ssGetSFcnParam(S,5)))

struct supportedDevice supportedDevices[] = {
        {"EL1002", (void *)2},
        {"EL1004", (void *)4},
        {"EL1008", (void *)8},
        {"EL1012", (void *)2},
        {"EL1014", (void *)4},
        {"EL1018", (void *)8},
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
    int_T i, channels;
    uint_T dataType[] = {  0, /*i Dummy, DTYPE starts at 1 */
        SS_INT8, SS_UINT8, 
        SS_INT16, SS_UINT16,
        SS_INT32, SS_UINT32, 
        SS_BOOLEAN, 
        SS_DOUBLE, SS_SINGLE, 
        DYNAMICALLY_TYPED
    };
    const struct supportedDevice *dev;
    
    /* See sfuntmpl_doc.c for more details on the macros below */
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for( i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S,i,SS_PRM_NOT_TUNABLE);

    dev = getDevice(S,supportedDevices,DEVICE_STR);
    channels = (int_T)dev->priv_data;
    ssSetUserData(S,(void *)dev);
    
    if (!channels) {
        ssSetErrorStatus(S,"Selected device is not supported");
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
        if (!ssSetNumOutputPorts(S, channels)) return;
    } else {
        if (!ssSetNumOutputPorts(S, 1)) return;
    }

    for( i = 0; i < ssGetNumOutputPorts(S); i++) {
        /* Only by "Vector Input" have CHANNELS port width, else 1 */
        ssSetOutputPortWidth(   S, i, OP_TYPE == 2 ? channels : 1);
        ssSetOutputPortDataType(S, i, 
                OP_TYPE == 1 ? SS_UINT8 : dataType[DTYPE]);
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
            /* SS_OPTION_PLACE_ASAP | */
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
    struct supportedDevice *dev = (struct supportedDevice *)ssGetUserData(S);
    int_T channum = (int_T)dev->priv_data;

    if (!ssWriteRTWScalarParam(S, "MasterId", &master, SS_INT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddr", addr))
        return;
    if (!ssWriteRTWStrParam(S, "DeviceModel", dev->name))
        return;
    if (!ssWriteRTWScalarParam(S, "ChanNum", &channum, SS_INT32))
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
