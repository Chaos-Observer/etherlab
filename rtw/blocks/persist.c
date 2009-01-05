/*
* $Id$
*
* This SFunction implements the persistent block.
*
* Copyright (c) 2008, Richard Hacker
* License: GPL
*
*/

#define S_FUNCTION_NAME  persist
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"

#include <stdint.h>

#define TSAMPLE      mxGetScalar(ssGetSFcnParam(S,0))
#define PARAM_COUNT                               1

struct persist {
    uint_T width;
};

/*====================*
 * S-function methods *
 *====================*/

/* Function: mdlInitializeSizes =============================================
 * Abstract:
 *    The sizes information is used by Simulink to determine the S-function
 *    block's characteristics (number of inputs, outputs, states, etc.).
 */
static void mdlInitializeSizes(SimStruct *S)
{
    int_T i;

    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }

    for( i = 0; i < PARAM_COUNT; i++)
        ssSetSFcnParamTunable(S, i, SS_PRM_NOT_TUNABLE);

    /* Input port */
    if (!ssSetNumInputPorts(S, 1))
        return;
    ssSetInputPortWidth(S, 0, DYNAMICALLY_SIZED);
    ssSetInputPortDataType(S, 0, DYNAMICALLY_TYPED);
    ssSetInputPortDirectFeedThrough(S, 0, 1);

    /* Output port */
    if (!ssSetNumOutputPorts(S, 1))
        return;
    ssSetOutputPortWidth(S, 0, DYNAMICALLY_SIZED);
    ssSetOutputPortDataType(S, 0, DYNAMICALLY_TYPED);

    ssSetNumSampleTimes(S, 1);
    /*
    ssSetNumIWork(S, DYNAMICALLY_SIZED);
    ssSetNumRWork(S, DYNAMICALLY_SIZED);
    */

    ssSetOptions(S,
            SS_OPTION_WORKS_WITH_CODE_REUSE
            | SS_OPTION_RUNTIME_EXCEPTION_FREE_CODE
            | SS_OPTION_CALL_TERMINATE_ON_EXIT);
}

/* Function: mdlInitializeSampleTimes =======================================
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
    ssSetInputPortWidth(S, 0, width);
    ssSetOutputPortWidth(S, 0, width);
}

#define MDL_SET_OUTPUT_PORT_WIDTH
static void mdlSetOutputPortWidth(SimStruct *S, int_T port, int_T width)
{
}

#define MDL_SET_INPUT_PORT_DATA_TYPE
static void mdlSetInputPortDataType(SimStruct *S, int_T port, DTypeId id)
{
    ssSetInputPortDataType(S, 0, id);
    ssSetOutputPortDataType(S, 0, id);
}

#define MDL_SET_OUTPUT_PORT_DATA_TYPE
static void mdlSetOutputPortDataType(SimStruct *S, int_T port, DTypeId id)
{
}

#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
    ssSetNumRunTimeParams(S, 2);
    {
        int_T width = 1;
        ssParamRec p = {
            .name = "Initialised",
            .nDimensions = 1,
            .dimensions = &width,
            .dataTypeId = SS_BOOLEAN,
            .complexSignal = 0,
            .data = mxCalloc(1, sizeof(boolean_T)),
            .dataAttributes = NULL,
            .nDlgParamIndices = 0,
            .dlgParamIndices = NULL,
            .transformed = RTPARAM_TRANSFORMED,
            .outputAsMatrix = 0,
        };

        mexMakeMemoryPersistent(p.data);

        ssSetRunTimeParamInfo(S, 0, &p);
    }

    {
        int_T width = ssGetInputPortWidth(S, 0);
        ssParamRec p = {
            .name = "InitialValue",
            .nDimensions = 1,
            .dimensions = &width,
            .dataTypeId = ssGetInputPortDataType(S,0),
            .complexSignal = 0,
            .data = mxCalloc(ssGetInputPortWidth(S,0), sizeof(real_T)),
            .dataAttributes = NULL,
            .nDlgParamIndices = 0,
            .dlgParamIndices = NULL,
            .transformed = RTPARAM_TRANSFORMED,
            .outputAsMatrix = 0,
        };

        mexMakeMemoryPersistent(p.data);

        ssSetRunTimeParamInfo(S, 1, &p);

/*
        switch (p.dataTypeId) {
            case SS_DOUBLE:
            case SS_SINGLE:
                ssSetNumIWork(S, 0);
                ssSetNumRWork(S, p.dimensions[0]);
                break;

            default:
                ssSetNumIWork(S, p.dimensions[0]);
                ssSetNumRWork(S, 0);

        }
*/
    }

    return;
}

/* Function: mdlOutputs =====================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector, ssGetY(S).
 */
static void mdlOutputs(SimStruct *S, int_T tid)
{
}

/* Function: mdlTerminate ===================================================
 * Abstract:
 *    In this function, you should perform any actions that are necessary
 *    at the termination of a simulation.  For example, if memory was
 *    allocated in mdlStart, this is the place to free it.
 */
static void mdlTerminate(SimStruct *S)
{
    int_T i;

    for (i = 0; i < ssGetNumRunTimeParams(S); i++) {
        ssParamRec *p = ssGetRunTimeParamInfo(S, i);
        mxFree(p->data);
    }
}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
    /*
    int_T single_shot = SINGLESHOT;

    if (!ssWriteRTWScalarParam(S, "SingleShot", &single_shot, SS_INT32))
        return;
    switch (ssGetInputPortDataType(S,0)) {
        case SS_DOUBLE:
        case SS_SINGLE:
            if (!ssWriteRTWWorkVect(S, "RWork", 1, "TriggerCounter", WIDTH))
                return;
            break;
        default:
            break;
            }
        */
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
