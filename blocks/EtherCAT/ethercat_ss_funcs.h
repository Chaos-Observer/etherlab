/*
 * $RCSfile: ethercat_ss_funcs.h,v $
 * $Revision: 1.2 $
 * $Date: 2006/02/02 16:53:32 $
 *
 * Header function for Ethercat SFunctions.
 *
 * To use, declare a list of supported devices as:
 *
 * struct device_properties my_device1 = { put properties here }
 * struct device_properties my_device1 = { put properties here }
 *
 * struct supportedDevice supportedDevices[] = {
 *      {"EL3122",&my_device1},
 *      {"EL3124",&my_device2},
 *      ...             <-- List all devices and channel count here
 *      {},
 * };
 *
 * and then call getDevice(S, supportedDevices, ModelString)
 * Terminals
 *
 * Copyright (c) 2006, Richard Hacker
 * License: GPL
 */

#include "simstruc.h"
#include "get_string.h"

struct supportedDevice {
    char *name; 
    void *priv_data;
};

/* Returns the pointer to the supported device if the name is found
 * otherwise sets error
 */
const struct supportedDevice* getDevice(
        SimStruct *S,
        const struct supportedDevice *supportedDevices, /* Null terminated
                                                         * list of devices
                                                         * supported by this
                                                         * driver */
        const mxArray *modelParam)      /* MxArray of model string */
{
    uint_T i;
    const char *tmpModelName;

    if (!(tmpModelName = getString(S, modelParam)))
        return NULL;
    
    for ( i = 0; supportedDevices[i].name; i++) {
        if (strcmp(supportedDevices[i].name, tmpModelName) == 0) {
            break;
        }
    }
    if (!supportedDevices[i].name) {
        ssSetErrorStatus(S, "Could not find selected name in list of supported devices");
        return NULL;
    }

    return &supportedDevices[i];
} 
