/* This file should be included in analog input S-Functions and it
 * provides functionality to set an output filter and to scale the output */

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

static void set_scaling(SimStruct *S, int_T output_port, 
        int_T gain_idx, int_T offset_idx, int_T param_idx)
{
    int_T i;
    real_T *g, *o;
    int_T gain_dims = mxGetNumberOfElements( ssGetSFcnParam(S,gain_idx));
    int_T offset_dims = mxGetNumberOfElements( ssGetSFcnParam(S,offset_idx));
    ssParamRec p = {
            NULL,               /* *name - gets filled in later*/
            1,                  /* nDimensions */
            &ndims,             /* *dimensions */
            SS_DOUBLE,          /* dataTypeId */
            0,                  /* complexSignal */
            NULL,               /* *data - filled in later */
            NULL,               /* dataAttributes */
            0,                  /* nDlgParamIndices - filled in later */
            NULL,               /* *dlgParamIndices - filled in later */
            RTPARAM_TRANSFORMED, /* transformed */
            0                   /* outputAsMatrix */
        
    };

    if (gain_dims != 1 && 
            gain_dims != ssGetOutputPortWidth(S,output_port)) {
        ssSetErrorStatus(S,
                "Dimensions of Gain does not match output width");
        return;
    }
    if (offset_dims != 1 && 
            offset_dims != ssGetOutputPortWidth(S,output_port)) {
        ssSetErrorStatus(S,
                "Dimensions of Offset does not match output width");
        return;
    }

    p.name = "FullScale";
    p.data = g = mxCalloc(gain_dims,sizeof(real_T));
    p.nDlgParamIndices = 1;
    p.dlgParamIndices = &gain_idx;
    mexMakeMemoryPersistent(g);
    for( i = 0; i < gain_dims; i++ )
        g[i] = (mxGetPr(ssGetSFcnParam(S,gain_idx)))[i];
    ssSetRunTimeParamInfo(S, param_idx++, &p);

    p.name = "Offset";
    p.data = o = mxCalloc(offset_dims,sizeof(real_T));
    p.nDlgParamIndices = 1;
    p.dlgParamIndices = &offset_idx;
    mexMakeMemoryPersistent(g);
    for( i = 0; i < offset_dims; i++ )
        o[i] = (mxGetPr(ssGetSFcnParam(S,offset_idx)))[i];
    ssSetRunTimeParamInfo(S, param_idx++, &p);
}

static void set_filter(SimStruct *S, int_T output_port, int_T omega_idx, 
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

    if (ndims != 1 && ndims != ssGetOutputPortWidth(S,output_port)) {
        ssSetErrorStatus(S,
                "Dimensions of LPF Frequncy does not match output width");
        return;
    }

    if (Ts) { /* Continuous */
        p.name = "Omega";
        convert = cont_convert;
        ssSetNumContStates(S, ndims);
        ssSetNumDiscStates(S, 0);
    } else {                        /* Discrete */
        p.name = "InputWeight";
        convert = disc_convert;
        ssSetNumContStates(S, 0);
        ssSetNumDiscStates(S, ndims);
    }
    p.data = w = mxCalloc(ndims,sizeof(real_T));
    mexMakeMemoryPersistent(w);
    for( i = 0; i < ndims; i++ ) {
        w[i] = convert(S, (mxGetPr(ssGetSFcnParam(S,omega_idx)))[i], Ts);
        if (w[i] == 0.0) 
            ssSetErrorStatus(S,"Time Constant for LPF too small");
    }
    ssSetRunTimeParamInfo(S, param_idx, &p);
}
