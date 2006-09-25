/*
 * $RCSfile: el31xx.c,v $
 * $Revision: 1.2 $
 * $Date: 2006/02/15 13:45:03 $
 *
 * SFunction to implement Beckhoff's series EL316x of EtherCAT Analog Input 
 * Terminals
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */


#define S_FUNCTION_NAME  el31xx
#define S_FUNCTION_LEVEL 2

#include "math.h"
#include "simstruc.h"
#include "ethercat_ss_funcs.h"

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

struct el31xxx_dev {
    real_T rawFullScale;
    real_T rawOffset;
    int_T sign;
    uint_T width;
    char_T *input_pdo;
    char_T *status_pdo;
};

struct el31xxx_dev el3102_dev = {10.0, 0.0,   1, 2, 
    "Beckhoff_EL3102_Input", "Beckhoff_EL3102_Status" };
struct el31xxx_dev el3104_dev = {10.0, 0.0,   1, 4, 
    "Beckhoff_EL3104_Input", "Beckhoff_EL3104_Status"};
struct el31xxx_dev el3108_dev = {10.0, 0.0,   1, 8, 
    "Beckhoff_EL3108_Input", "Beckhoff_EL3108_Status" };

struct el31xxx_dev el3112_dev = {0.02, 0.0,   0, 2, 
    "Beckhoff_EL3112_Input", "Beckhoff_EL3112_Status"};
struct el31xxx_dev el3114_dev = {0.02, 0.0,   0, 4, 
    "Beckhoff_EL3114_Input", "Beckhoff_EL3114_Status"};

struct el31xxx_dev el3122_dev = {0.16, 0.004, 0, 2, 
    "Beckhoff_EL3122_Input", "Beckhoff_EL3122_Status"};
struct el31xxx_dev el3124_dev = {0.16, 0.004, 0, 4, 
    "Beckhoff_EL3124_Input", "Beckhoff_EL3124_Status"};

struct el31xxx_dev el3142_dev = {0.02, 0.0,   0, 2, 
    "Beckhoff_EL3142_Input", "Beckhoff_EL3142_Status"};
struct el31xxx_dev el3144_dev = {0.02, 0.0,   0, 4, 
    "Beckhoff_EL3144_Input", "Beckhoff_EL3144_Status"};
struct el31xxx_dev el3148_dev = {0.02, 0.0,   0, 8, 
    "Beckhoff_EL3148_Input", "Beckhoff_EL3148_Status"};

struct el31xxx_dev el3152_dev = {0.16, 0.004, 0, 2, 
    "Beckhoff_EL3152_Input", "Beckhoff_EL3152_Status"};
struct el31xxx_dev el3154_dev = {0.16, 0.004, 0, 4, 
    "Beckhoff_EL3154_Input", "Beckhoff_EL3154_Status"};
struct el31xxx_dev el3158_dev = {0.16, 0.004, 0, 8, 
    "Beckhoff_EL3158_Input", "Beckhoff_EL3158_Status"};

struct el31xxx_dev el3162_dev = {10.0, 0.0,   0, 2,
    "Beckhoff_EL3162_Input", "Beckhoff_EL3162_Status"};
struct el31xxx_dev el3164_dev = {10.0, 0.0,   0, 4,
    "Beckhoff_EL3164_Input", "Beckhoff_EL3164_Status"};
struct el31xxx_dev el3168_dev = {10.0, 0.0,   0, 8,
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
    struct el31xxx_dev *device;
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
    struct el31xxx_dev *device;
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
        (struct el31xxx_dev *)model->priv_data;

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
    ssSetNumContStates(S, devInstance->filter == 1 ? device->width : 0);
    ssSetNumDiscStates(S, devInstance->filter == 2 ? device->width : 0);
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

/* Return the integration constant (k) for the following filter such that
 * it has a low pass frequency of w.
 * 
 *                           +---+                                      
 *                +---+      | 1 |                                       
 * ------->O----->| k |----->| - |------+----->                         
 *         ^      +---+      | s |      |                               
 *         |                 +---+      |                               
 *         |                            |                               
 *         +----------------------------+                               
 *
 * This is simply w itself
 * */
real_T cont_convert(SimStruct *S, real_T w, real_T Ts)
{
    return w;
}

/* Return the input weight (k) for the following discrete filter with sample
 * time Ts such that it has an equivalent low pass frequency of a continuous
 * filter.
 * 
 *  U                            +-----+           Y                    
 *   n            +---+          |     |            n                    
 * ------->p----->| k |----->p-->|  -1 |---+-----+----->                
 *        -|      +---+     +|   | z   |   |     |                       
 *         |                 |   +-----+   |     |                      
 *         |                 |             |     |                      
 *         |                 +-------------+     |                      
 *         |                                     |                      
 *         |                                     |                      
 *         +-------------------------------------+                      
 *
 * The characteristic equation of this diagram
 *  Y(n+1)  = k*U(n) + (1-k)*Y(n)
 *
 * A continuous low pass filter has the property that a dirac delta decays
 * to (1 - 2/pi) after time t where t = 1/(2*pi*f) = 1/w where f is the 
 * low pass frequency
 *
 * The aim is now to find k for a discrete filter as above that will let a 
 * dirac delta 
 *      U(n) = {1: n=0; 0: n!=0}
 * decay to Y(N) = k(1-2/pi) in sample N = t/Ts = 1/(w*Ts)
 *
 *                          N         1/w*Ts
 * Y(N) = k(1-2/pi) = k(1-k)  = k(1-k)
 *
 * k = 1 - (1-2/pi)^(w*Ts)
 * */
real_T disc_convert(SimStruct *S, real_T w, real_T Ts)
{
    real_T k1,k,dk,f,df,N;
    real_T a = (1 - 2/M_PI);
    int_T i = 100;

    return 1.0 - pow( a, w*Ts);

    /*
     * This has to be solved iteratively
     * First estimate of k: k = 1/N
     * Now solve:
     *   f(k) = k(1-k)^N - k(1 - 2/pi)
     *
     * using Newton: k = k - f(k)/f'(k)
     *
     *  where:
     *   f'(k) = (1-k)^N - k*N*(1-k)^(N-1) - (1 - 2/pi)
     *
     */
    N = 1.0/(w*Ts);

    k = w*Ts;           /* First estimate of k
                           The exact value is emperically found to be
                           allways be larger than this first estimate */

    if (k >= 1)
        return 1.0;

    do {
        k1 = k;
        f = k*pow( 1-k, N) - k*(1 - 2/M_PI);
        df = pow( 1-k, N) - k*(N)*pow( 1-k, N-1) - (1 - 2/M_PI);
        dk = -f/df;
        k =  (k+dk >= 1.0) ? (1.0+k)/2.0 : k+dk;
    } while (fabs(k-k1) > 1.0e-6 && --i);
    if (i == 0)
        ssSetErrorStatus(S, "Could not find correct filter time constant. "
                "Choose a value larger than the sample time.");

    return k;
}

#define MDL_SET_WORK_WIDTHS
static void mdlSetWorkWidths(SimStruct *S)
{
    int_T dims, param, i;
    int_T scaleDim  = mxGetNumberOfElements( ssGetSFcnParam(S,SCALE_IDX));
    int_T offsetDim = mxGetNumberOfElements( ssGetSFcnParam(S,OFFSET_IDX));
    int_T omegaDim = mxGetNumberOfElements( ssGetSFcnParam(S,OMEGA_IDX));
    struct el31xx *devInstance = (struct el31xx *)ssGetUserData(S);
    struct el31xxx_dev *device = devInstance->device;
    real_T Ts = TSAMPLE;
    real_T (*convert)(SimStruct *S, real_T f, real_T Ts);
    real_T *m, *c, *w;
    int_T paramCount = 0;
    ssParamRec p = {
            NULL,               /* *name */
            1,                  /* nDimensions */
            &dims,              /* *dimensions */
            SS_DOUBLE,          /* dataTypeId */
            0,                  /* complexSignal */
            NULL,               /* *data */
            NULL,               /* dataAttributes */
            1,                  /* nDlgParamIndices */
            &param,             /* *dlgParamIndices */
            RTPARAM_NOT_TRANSFORMED, /* transformed */
            0                   /* outputAsMatrix */
        
    };
    if (devInstance->scale && devInstance->filter) {
        paramCount = 3;
    } else if (devInstance->scale) {
        paramCount = 2;
    } else if (devInstance->filter) {
        paramCount = 1;
    } else {
        return;
    }

    ssSetNumRunTimeParams(S, paramCount);
    paramCount = 0;

    /* Only when picking the third choice of Data Type
     * does Output scaling take place */
    if (devInstance->scale) {

        if (scaleDim != 1 && device->width != scaleDim) {
            ssSetErrorStatus(S,"Dimensions of Gain does not match output width");
        }
        if (offsetDim != 1 && device->width != offsetDim) {
            ssSetErrorStatus(S,"Dimensions of Offset does not match output width");
        }
        if (omegaDim != 1 && device->width != omegaDim) {
            ssSetErrorStatus(S,"Dimensions of LPF Frequncy does not match output width");
        }

        p.name = "FullScale";
        dims = scaleDim;
        param = SCALE_IDX;
        p.data = m = mxCalloc(dims,sizeof(real_T));
        mexMakeMemoryPersistent(m);
        for( i = 0; i < dims; i++ )
            m[i] = (mxGetPr(ssGetSFcnParam(S,param)))[i];
        ssSetRunTimeParamInfo(S, paramCount++, &p);

        p.name = "Offset";
        dims = offsetDim;
        param = OFFSET_IDX;
        p.data = c = mxCalloc(dims,sizeof(real_T));
        mexMakeMemoryPersistent(c);
        for( i = 0; i < dims; i++ )
            c[i] = (mxGetPr(ssGetSFcnParam(S,param)))[i];
        ssSetRunTimeParamInfo(S, paramCount++, &p);
    }

    if (devInstance->filter) {
        if (devInstance->filter == 1) { /* Continuous */
            p.name = "Omega";
            convert = cont_convert;
        } else {                        /* Discrete */
            p.name = "InputWeight";
            convert = disc_convert;
        }
        dims = omegaDim;
        param = OMEGA_IDX;
        p.data = w = mxCalloc(dims,sizeof(real_T));
        mexMakeMemoryPersistent(w);
        for( i = 0; i < dims; i++ ) {
            w[i] = convert(S, (mxGetPr(ssGetSFcnParam(S,param)))[i], Ts);
            if (w[i] == 0.0) 
                ssSetErrorStatus(S,"Time Constant for LPF too small");
        }
        ssSetRunTimeParamInfo(S, paramCount++, &p);
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
    struct el31xx *devInstance = (struct el31xx *)ssGetUserData(S);
    struct el31xxx_dev *device = devInstance->device;
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
