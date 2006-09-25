/*
 * $RCSfile: ethercat_ss_funcs.h,v $
 * $Revision: 1.2 $
 * $Date: 2006/02/02 16:53:32 $
 *
 * Header function to get a string parameter
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */

#include "simstruc.h"

/* Returns a string given an mxArray.
 * Don't need to mxFree, because this string is not permanent */
const char *getString(
        SimStruct *S, 
        const mxArray *modelParam)
{
    uint_T strLen;
    char *str;

    /* Check whether the device type is supported */
    if (!mxIsChar(modelParam)) {
        ssSetErrorStatus(S, "Format string is not a character array");
        return NULL;
    }
    
    strLen = mxGetM(modelParam)*mxGetN(modelParam) + 1;
    
    if (!(str = mxCalloc(strLen, sizeof(mxChar)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return NULL;
    }

    if (mxGetString(modelParam, str, strLen)) {
        ssSetErrorStatus(S, "Could not convert mxArray to string");
        return NULL;
    }

    return str;
}
