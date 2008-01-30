/*
 * Model description file.
 * 
 * This file is used to determine 
 * 
 * 
 * 
 */
#include "capi.h"

static const struct capi_signal capi_signal[] = {
   { "outputs/meter",
     "double", 10, &rtB.var1[0] - &rtB},
   { "var2",
     "double", 0, &rtB.var2 - &rtB},
};

static const struct capi_parameter capi_parameter[] = {
   { "Input/Gain1",
     "double", 10, &rtP.param1[0] - &rtP},
};
