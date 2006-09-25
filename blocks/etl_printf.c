/*
 * sfuntmpl_basic.c: Basic 'C' template for a level 2 S-function.
 *
 *  -------------------------------------------------------------------------
 *  | See matlabroot/simulink/src/sfuntmpl_doc.c for a more detailed template |
 *  -------------------------------------------------------------------------
 *
 * Copyright 1990-2002 The MathWorks, Inc.
 * $Revision: 1.1 $
 */


/*
 * You must specify the S_FUNCTION_NAME as the name of your S-function
 * (i.e. replace sfuntmpl_basic with the name of your S-function).
 */

#define S_FUNCTION_NAME  rtai_printf
#define S_FUNCTION_LEVEL 2

/*
 * Need to include simstruc.h for the definition of the SimStruct and
 * its associated macro definitions.
 */
#include "simstruc.h"


/* 
 * Error buffer. Write errors here, susequently calling ssSetErrorStatus
 * with this error message
 */
char errbuf[100];

/* Error handling
 * --------------
 *
 * You should use the following technique to report errors encountered within
 * an S-function:
 *
 *       ssSetErrorStatus(S,"Error encountered due to ...");
 *       return;
 *
 * Note that the 2nd argument to ssSetErrorStatus must be persistent memory.
 * It cannot be a local variable. For example the following will cause
 * unpredictable errors:
 *
 *      mdlOutputs()
 *      {
 *         char msg[256];         {ILLEGAL: to fix use "static char msg[256];"}
 *         sprintf(msg,"Error due to %s", string);
 *         ssSetErrorStatus(S,msg);
 *         return;
 *      }
 *
 * See matlabroot/simulink/src/sfuntmpl_doc.c for more details.
 */

static int_T count_parameters(char_T *s)
{
    int_T c = 0;

    while (*s) {
        if (*s == '%') {
        	if (*(s+1) == '%') { /* Note: no bufLen+1 violation here! */
        		/* Found a %%, don't count it here */
        		s++;
        	} else {
        		c++;
        	}
        }
        s++;
    }

    return c;
}


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
    /* See sfuntmpl_doc.c for more details on the macros below */
    const mxArray *f;
    char *buf;
    uint_T i;
    uint_T strLen, param_count; 
    
    ssSetNumSFcnParams(S, 2);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    ssSetSFcnParamTunable(S,0,SS_PRM_NOT_TUNABLE);
    ssSetSFcnParamTunable(S,1,SS_PRM_NOT_TUNABLE);

    /* First parameter is the format string for printf() */
    f = ssGetSFcnParam(S, 0);

    if (!mxIsChar(f)) {
        ssSetErrorStatus(S, "Format string is not a character array");
        return;
    }
    
    strLen = mxGetM(f)*mxGetN(f) + 1;
    
    if (!(buf = mxCalloc(strLen, sizeof(mxChar)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }

    if (mxGetString(f, buf, strLen)) {
            ssSetErrorStatus(S, "Could not convert mxArray to string");
            return;
    }

    param_count = count_parameters(buf);

    if (mxGetPr(ssGetSFcnParam(S,1))[0]) {
            if (!ssSetNumInputPorts(S, 2)) return;
	    ssSetInputPortWidth(   S, 0, DYNAMICALLY_SIZED);
	    ssSetInputPortDataType(S, 0, DYNAMICALLY_TYPED);

	    ssSetInputPortWidth(   S, 1, DYNAMICALLY_SIZED);
	    ssSetInputPortDataType(S, 1, DYNAMICALLY_TYPED);
    } else {
            if (!ssSetNumInputPorts(S, param_count+1)) return;
	    for (i = 0; i <= param_count; i++) { /* param_count+1 loops! */
	        ssSetInputPortWidth(   S, i, DYNAMICALLY_SIZED);
                ssSetInputPortDataType(S, i, DYNAMICALLY_TYPED);
            }
    }

    /*
     * Set direct feedthrough flag (1=yes, 0=no).
     * A port has direct feedthrough if the input is used in either
     * the mdlOutputs or mdlGetTimeOfNextVarHit functions.
     * See matlabroot/simulink/src/sfuntmpl_directfeed.txt.
     */
    ssSetInputPortDirectFeedThrough(S, 0, 1);

    if (!ssSetNumOutputPorts(S, 0)) return;

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);
    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, DYNAMICALLY_SIZED);
    ssSetNumPWork(S, 2); /* 0: pointer to memory storing format string
                          * 1: pointer to input conversion function
                          */
    ssSetNumModes(S, 0);
    ssSetNumNonsampledZCs(S, 0);

    ssSetOptions(S, 
            SS_OPTION_WORKS_WITH_CODE_REUSE | 
            SS_OPTION_RUNTIME_EXCEPTION_FREE_CODE);

    mxFree(buf);
}

#define MDL_SET_INPUT_PORT_WIDTH   /* Change to #undef to remove function */
#if defined(MDL_SET_INPUT_PORT_WIDTH)
static void mdlSetInputPortWidth(SimStruct *S, int_T port, int_T width)
{
    const mxArray *f;
    char *buf;
    uint_T strLen, param_count; 

    if (port == 0) {
	ssSetInputPortWidth(S, port, width);
	return;
    }

    /* port > 1 hereafter */

    if (!ssGetInputPortConnected(S,port)) {
	ssSetInputPortWidth(S, port, ssGetInputPortWidth(S,0));
	return;
    }

    f = ssGetSFcnParam(S, 0);

    strLen = mxGetM(f)*mxGetN(f) + 1;
    
    if (!(buf = mxCalloc(strLen, sizeof(mxChar)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }

    mxGetString(f, buf, strLen);

    param_count = count_parameters(buf);

    if (ssGetInputPortWidth(S, 0) == 1) {
	if (mxGetPr(ssGetSFcnParam(S,1))[0] && param_count != width) {
            snprintf(errbuf, sizeof(errbuf),
                    "Not enough parameter inputs on input %i: have %i, "
                    "expected %i from format string", width, param_count);
            ssSetErrorStatus(S, errbuf);
            return;
        } else if (!mxGetPr(ssGetSFcnParam(S,1))[0] && width != 1) {
            snprintf(errbuf, sizeof(errbuf),
                    "Parameter inputs are not vectorized, but have %i "
                    "inputs on input %i", width, port);
            ssSetErrorStatus(S, errbuf);
            return;
	}
    } else {
	if (mxGetPr(ssGetSFcnParam(S,1))[0]) {
	    ssSetErrorStatus(S, "Cannot have vectorized parameter port when "
			    "input port 1 is vectorized"); 
	}
	if (width != 1 && width != ssGetInputPortWidth(S, 0)) {
	    snprintf(errbuf, sizeof(errbuf),
		    "Incorrect vector size for input port %i: expected %i, "
		    "but %i are connected",
		    port+1, ssGetInputPortWidth(S, 0), width);
	    ssSetErrorStatus(S, errbuf);
	}
    }

    ssSetInputPortWidth(S, port, width);

    mxFree(buf);
}
#endif /* MDL_SET_INPUT_PORT_WIDTH */


/* Function: mdlInitializeSampleTimes =========================================
 * Abstract:
 *    This function is used to specify the sample time(s) for your
 *    S-function. You must register the same number of sample times as
 *    specified in ssSetNumSampleTimes.
 */
static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, INHERITED_SAMPLE_TIME);
    ssSetOffsetTime(S, 0, 0.0);
}


#define MDL_SET_WORK_WIDTHS
#if defined(MDL_SET_WORK_WIDTHS) && defined(MATLAB_MEX_FILE)
static void mdlSetWorkWidths(SimStruct *S)
{
    ssSetNumIWork(S,ssGetInputPortWidth(S,0));
}
#endif /* MDL_SET_WORK_WIDTHS */


#define MDL_INITIALIZE_CONDITIONS   /* Change to #undef to remove function */
#if defined(MDL_INITIALIZE_CONDITIONS)
  /* Function: mdlInitializeConditions ========================================
   * Abstract:
   *    In this function, you should initialize the continuous and discrete
   *    states for your S-function block.  The initial states are placed
   *    in the state vector, ssGetContStates(S) or ssGetRealDiscStates(S).
   *    You can also perform any other initialization activities that your
   *    S-function may require. Note, this routine will be called at the
   *    start of simulation and if it is present in an enabled subsystem
   *    configured to reset states, it will be call when the enabled subsystem
   *    restarts execution to reset the states.
   */
  static void mdlInitializeConditions(SimStruct *S)
  {
      int_T i;
      
      for( i = 0; i < ssGetNumIWork(S); i++ ) 
          ssSetIWorkValue(S, i, 0);

  }
#endif /* MDL_INITIALIZE_CONDITIONS */


/*
 * Here follows various converstion functions to make a real_T
 * for every data type
 */
static int_T get_real_trig( InputPtrsType u, uint_T idx )
{
    return *((InputRealPtrsType)u)[idx] > 0.0;
}
static int_T get_real32_trig( InputPtrsType u, uint_T idx )
{
    return *((InputReal32PtrsType)u)[idx] > 0.0;
}
static int_T get_int8_trig( InputPtrsType u, uint_T idx )
{
    return *((InputInt8PtrsType)u)[idx] > 0;
}
static int_T get_uint8_trig( InputPtrsType u, uint_T idx )
{
    return *((InputUInt8PtrsType)u)[idx] > 0;
}
static int_T get_int16_trig( InputPtrsType u, uint_T idx )
{
    return *((InputInt16PtrsType)u)[idx] > 0;
}
static int_T get_uint16_trig( InputPtrsType u, uint_T idx )
{
    return *((InputUInt16PtrsType)u)[idx] > 0;
}
static int_T get_int32_trig( InputPtrsType u, uint_T idx )
{
    return *((InputInt32PtrsType)u)[idx] > 0;
}
static int_T get_uint32_trig( InputPtrsType u, uint_T idx )
{
    return *((InputUInt32PtrsType)u)[idx] > 0;
}
static int_T get_boolean_trig( InputPtrsType u, uint_T idx )
{
    return *((InputBooleanPtrsType)u)[idx] > 0;
}
static int_T get_void_trig( InputPtrsType u, uint_T idx )
{
    return 0;
}


#define MDL_START  /* Change to #undef to remove function */
#if defined(MDL_START) 
  /* Function: mdlStart =======================================================
   * Abstract:
   *    This function is called once at start of model execution. If you
   *    have states that should be initialized once, this is the place
   *    to do it.
   */
  static void mdlStart(SimStruct *S)
  {
    DTypeId uType = ssGetInputPortDataType(S,0);
    const mxArray *f;
    char *buf;
    uint_T i;
    uint_T strLen, param_count; 

    f = ssGetSFcnParam(S, 0);
    strLen = mxGetM(f)*mxGetN(f) + 1;
    if (!(buf = mxCalloc(strLen, sizeof(mxChar)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }
    mxGetString(f, buf, strLen);
    mexMakeMemoryPersistent(buf);

    ssSetPWorkValue(S,0,buf);

    switch (uType) {
        case SS_DOUBLE:
            ssSetPWorkValue(S, 1, get_real_trig);
            break;
        case SS_SINGLE:
            ssSetPWorkValue(S, 1, get_real32_trig);
            break;
        case SS_INT8:
            ssSetPWorkValue(S, 1, get_int8_trig);
            break;
        case SS_UINT8:
            ssSetPWorkValue(S, 1, get_uint8_trig);
            break;
        case SS_INT16:
            ssSetPWorkValue(S, 1, get_int16_trig);
            break;
        case SS_UINT16:
            ssSetPWorkValue(S, 1, get_uint16_trig);
            break;
        case SS_INT32:
            ssSetPWorkValue(S, 1, get_int32_trig);
            break;
        case SS_UINT32:
            ssSetPWorkValue(S, 1, get_uint32_trig);
            break;
        case SS_BOOLEAN:
            ssSetPWorkValue(S, 1, get_boolean_trig);
            break;
        default:
            ssSetPWorkValue(S, 1, get_void_trig);
            break;
    }
  }
#endif /*  MDL_START */



/* Function: mdlOutputs =======================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector, ssGetY(S).
 */
static void mdlOutputs(SimStruct *S, int_T tid)
{
}

#define MDL_UPDATE  /* Change to #undef to remove function */
#if defined(MDL_UPDATE)
  /* Function: mdlUpdate ======================================================
   * Abstract:
   *    This function is called once for every major integration time step.
   *    Discrete states are typically updated here, but this function is useful
   *    for performing any tasks that should only take place once per
   *    integration step.
   */
  static void mdlUpdate(SimStruct *S, int_T tid)
  {
    InputPtrsType u = ssGetInputPortSignalPtrs(S,0);
    int_T trig;
    uint_T i;
    char *format_str = ssGetPWorkValue(S,0);
    int_T (*get_trig)(InputPtrsType,uint_T) = ssGetPWorkValue(S,1);

    for( i = 0; i < ssGetNumIWork(S); i++) {
        trig = get_trig(u,i);

        /* Check whether trigger has changed */
        if (trig == ssGetIWorkValue(S, i))
            continue;

        /* OK, it has changed. Print if it went true */
        if (trig)
            printf("%s\n", format_str);

        /* Remember it */
        ssSetIWorkValue(S, i, trig);
    }
  }
#endif /* MDL_UPDATE */



#undef MDL_DERIVATIVES  /* Change to #undef to remove function */
#if defined(MDL_DERIVATIVES)
  /* Function: mdlDerivatives =================================================
   * Abstract:
   *    In this function, you compute the S-function block's derivatives.
   *    The derivatives are placed in the derivative vector, ssGetdX(S).
   */
  static void mdlDerivatives(SimStruct *S)
  {
  }
#endif /* MDL_DERIVATIVES */



/* Function: mdlTerminate =====================================================
 * Abstract:
 *    In this function, you should perform any actions that are necessary
 *    at the termination of a simulation.  For example, if memory was
 *    allocated in mdlStart, this is the place to free it.
 */
static void mdlTerminate(SimStruct *S)
{
    mxFree(ssGetPWorkValue(S,0));
}

#define MDL_RTW
#if defined(MDL_RTW) && defined(MATLAB_MEX_FILE)
static void mdlRTW(SimStruct *S)
{
    if (!ssWriteRTWStrParam(S, "FormatStr", ssGetPWorkValue(S,0)))
        return;
    /*
    if (!ssWriteRTWWorkVect(S, "IWork", 1,"oldStates",6))
        return;
        */
}
#endif



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
