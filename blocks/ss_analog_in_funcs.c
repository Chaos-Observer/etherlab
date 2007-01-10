/* This file should be included in analog input S-Functions and it
 * provides functionality to set an output filter and to scale the output */

#include "math.h"

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
    real_T a = (1 - 2/M_PI);

    return 1.0 - pow( a, w*Ts);
}

static void set_scaling(SimStruct *S, int_T width, 
        int_T gain_idx, int_T offset_idx, int_T param_idx)
{
    int_T i;
    real_T *g, *o;
    int_T gain_dims = mxGetNumberOfElements( ssGetSFcnParam(S,gain_idx));
    int_T offset_dims = mxGetNumberOfElements( ssGetSFcnParam(S,offset_idx));
    ssParamRec p = {
            NULL,               /* *name - gets filled in later*/
            1,                  /* nDimensions */
            NULL,               /* *dimensions */
            SS_DOUBLE,          /* dataTypeId */
            0,                  /* complexSignal */
            NULL,               /* *data - filled in later */
            NULL,               /* dataAttributes */
            0,                  /* nDlgParamIndices - filled in later */
            NULL,               /* *dlgParamIndices - filled in later */
            RTPARAM_TRANSFORMED, /* transformed */
            0                   /* outputAsMatrix */
        
    };

    if (gain_dims != 1 && gain_dims != width) {
        ssSetErrorStatus(S,
                "Dimensions of Gain does not match output width");
        return;
    }
    if (offset_dims != 1 && offset_dims != width) {
        ssSetErrorStatus(S,
                "Dimensions of Offset does not match output width");
        return;
    }

    p.name = "FullScale";
    p.dimensions = &gain_dims;
    p.data = g = malloc(gain_dims*sizeof(real_T));
    if (!g) {
        ssSetErrorStatus(S,"Could not allocate memory");
        return;
    }
    p.nDlgParamIndices = 1;
    p.dlgParamIndices = &gain_idx;
    for( i = 0; i < gain_dims; i++ )
        g[i] = (mxGetPr(ssGetSFcnParam(S,gain_idx)))[i];
    ssSetRunTimeParamInfo(S, param_idx++, &p);

    p.name = "Offset";
    p.dimensions = &offset_dims;
    p.data = o = malloc(offset_dims*sizeof(real_T));
    if (!o) {
        ssSetErrorStatus(S,"Could not allocate memory");
        free(g);
        return;
    }
    p.nDlgParamIndices = 1;
    p.dlgParamIndices = &offset_idx;
    for( i = 0; i < offset_dims; i++ )
        o[i] = (mxGetPr(ssGetSFcnParam(S,offset_idx)))[i];
    ssSetRunTimeParamInfo(S, param_idx++, &p);
}

static void set_filter(SimStruct *S, int_T width, int_T omega_idx, 
        int_T param_idx)
{
    int_T i;
    real_T *w;
    int_T ndims = mxGetNumberOfElements( ssGetSFcnParam(S,omega_idx));
    real_T Ts = ssGetSampleTime(S,0);
    real_T (*convert)(SimStruct *S, real_T f, real_T Ts);
    ssParamRec p = {
            NULL,               /* *name - gets filled in later*/
            1,                  /* nDimensions */
            &ndims,             /* *dimensions */
            SS_DOUBLE,          /* dataTypeId */
            0,                  /* complexSignal */
            NULL,               /* *data - filled in later */
            NULL,               /* dataAttributes */
            1,                  /* nDlgParamIndices */
            &omega_idx,         /* *dlgParamIndices */
            RTPARAM_TRANSFORMED, /* transformed */
            0                   /* outputAsMatrix */
        
    };

    if (ndims != 1 && ndims != width) {
        ssSetErrorStatus(S,
                "Dimensions of LPF Frequncy does not match output width");
        return;
    }

    if (Ts) {           /* Discrete */
        p.name = "InputWeight";
        convert = disc_convert;
        ssSetNumDiscStates(S, width);
    } else {            /* Continuous */
        p.name = "Omega";
        convert = cont_convert;
        ssSetNumContStates(S, width);
    }
    p.data = w = malloc(ndims*sizeof(real_T));
    if (!w) {
        ssSetErrorStatus(S,"Could not allocate memory");
        return;
    }
    for( i = 0; i < ndims; i++ ) {
        w[i] = convert(S, (mxGetPr(ssGetSFcnParam(S,omega_idx)))[i], Ts);
        if (w[i] == 0.0) {
            ssSetErrorStatus(S,"Time Constant for LPF too small");
            free(w);
            return;
        }
    }
    ssSetRunTimeParamInfo(S, param_idx, &p);
}
