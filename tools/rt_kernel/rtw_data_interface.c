/***********************************************************************
 *
 * $Id$
 *
 * This file implements fuctionality to interact with the signals and
 * parameters provided by Matlab/Simulink's Real-Time Workshop (TM)
 *
 * Due to the fact that internal data types and structures are used, this
 * file has to be compiled together with the model. It cannot be part
 * of the rt_kernel.
 * 
 * Copyright (C) 2008  Richard Hacker
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***********************************************************************/


#include "include/etl_data_info.h"
#include "include/rtw_data_interface.h"

/* capi.h is generated by the makefile */
#include "capi.h"
#include "rtmodel.h"

#ifndef max
#define max(x1,x2) ((x1) > (x2) ? (x1) : (x2))
#endif

#ifndef min
#define min(x1,x2) ((x2) > (x1) ? (x1) : (x2))
#endif

rtwCAPI_ModelMappingInfo* mmi;
const rtwCAPI_DimensionMap*    dimMap;
const rtwCAPI_DataTypeMap*     dTypeMap;
const uint_T*                  dimArray;
const rtwCAPI_Signals* signals;
const rtwCAPI_BlockParameters* blockParams;
unsigned int maxSignalIdx;
unsigned int maxParameterIdx;
void ** dataAddressMap;

// Length of model name
size_t model_name_len;

const char* get_signal_info(struct signal_info *si)
{
    unsigned int dimIdx, dimArrayIdx, dataTypeIdx; 
    const char *path;
    const char *name;
    size_t path_len;

    if (si->index >= maxSignalIdx) {
        return "Exceeded signal index array.";
    }

    path = rtwCAPI_GetSignalBlockPath(signals, si->index);
    path_len = strlen(path);

    /* Check whether RTW still composes the path as
     * <model-name>/<path-to-signal> */
    if (path_len <= model_name_len || path[model_name_len] != '/') {
        return "RTW unexpectedly changed path string composition.";
    }

    // Skip the model name and subsequent '/'
    path += model_name_len + 1;
    path_len -= model_name_len + 1;

    // Separate the RTW path into a path and a name section. The name is
    // the string after the last '/'
    name = strrchr(path, '/');
    if (name < path) {
        /* There is no path, only a name */
        name = path;
        path_len = 0;
    }
    else {
        // Copy the last '/' as well. It will become the string terminator
        // '\0' later on.
        path_len = name - path;
        name++;
    }

    path_len = min(si->path_len, path_len);

    strncpy(si->alias, rtwCAPI_GetSignalName(signals, si->index),
            sizeof(si->alias));
    strncpy(si->path, path, path_len);
    strncpy(si->name, name, sizeof(si->name));

    // Make sure the strings are terminated
    si->alias[sizeof(si->alias)-1] = '\0';
    si->name[sizeof(si->name)-1] = '\0';
    si->path[path_len] = '\0';

    si->offset =
        (void*)rtwCAPI_GetDataAddress( dataAddressMap, 
                rtwCAPI_GetSignalAddrIdx(signals, si->index))
        - (void*)&rtB;

    dimIdx = rtwCAPI_GetSignalDimensionIdx(signals, si->index);

    if (rtwCAPI_GetNumDims(dimMap, dimIdx) != 2) {
        return "Cannot handle array dimensions != 2";
    }
    dimArrayIdx = rtwCAPI_GetDimArrayIndex(dimMap, dimIdx);
    // dimArray[dimArrayIdx]   = number of rows
    // dimArray[dimArrayIdx+1] = numbol or columns

    si->dim[0] = si->dim[1] = 0;
    if (min(dimArray[dimArrayIdx],dimArray[dimArrayIdx+1]) == 1) {
        si->dim[0] = max(dimArray[dimArrayIdx],dimArray[dimArrayIdx+1]);
    }
    else {
        switch (rtwCAPI_GetOrientation(dimMap, dimIdx)) {
            case rtwCAPI_MATRIX_COL_MAJOR:
                if (min(dimArray[dimArrayIdx],dimArray[dimArrayIdx+1]) == 1) {
                } else {
                    si->dim[0] = dimArray[dimArrayIdx];
                    si->dim[1] = dimArray[dimArrayIdx+1];
                }
                break;
            case rtwCAPI_MATRIX_ROW_MAJOR:
                si->dim[0] = dimArray[dimArrayIdx+1];
                si->dim[1] = dimArray[dimArrayIdx];
                break;
            default:
                return "Unknown RTW data orientation encountered.";
        }
    }

    dataTypeIdx = rtwCAPI_GetSignalDataTypeIdx(signals, si->index);
    switch (rtwCAPI_GetDataTypeSLId(dTypeMap, dataTypeIdx)) {
        case SS_DOUBLE:
            si->data_type = si_double_T;
            break;
        case SS_SINGLE:
            si->data_type = si_single_T;
            break;
        case SS_INT8:
            si->data_type = si_sint8_T;
            break;
        case SS_UINT8:
            si->data_type = si_uint8_T;
            break;
        case SS_INT16:
            si->data_type = si_sint16_T;
            break;
        case SS_UINT16:
            si->data_type = si_uint16_T;
            break;
        case SS_INT32:
            si->data_type = si_sint32_T;
            break;
        case SS_UINT32:
            si->data_type = si_uint32_T;
            break;
        case SS_BOOLEAN:
            si->data_type = si_boolean_T;
            break;
        default:
            return "Unknown RTW data type encountered.";
    }

    si->st_index = rtwCAPI_GetSignalSampleTimeIdx(signals, si->index);
    si->st_index = si->st_index - (TID01EQ && si->st_index);

    // Check that the data type is compatable
    if (si_data_width[si->data_type] !=
            rtwCAPI_GetDataTypeSize(dTypeMap, dataTypeIdx)) {
        return "Incompatable data type size encountered.";
    }
    if (rtwCAPI_GetDataIsComplex(dTypeMap, dataTypeIdx)) {
        return "Cannot interact with complex signals yet.";
    }
    if (rtwCAPI_GetDataIsPointer(dTypeMap, dataTypeIdx)) {
        return "Cannot interact with pointer signals.";
    }

    return NULL;
}

const char* get_param_info(struct signal_info* si)
{
    unsigned int dimIdx, dimArrayIdx, dataTypeIdx; 
    const char *path, *name;
    size_t path_len;

    if (si->index >= maxParameterIdx) {
        return "Exceeded parameter index array.";
    }

    path = rtwCAPI_GetBlockParameterBlockPath(blockParams, si->index);
    name = rtwCAPI_GetBlockParameterName(blockParams, si->index);
    path_len = strlen(path);

    /* Check whether RTW still composes the path as
     * <model-name>/<path-to-signal> */
    if (path_len <= model_name_len || path[model_name_len] != '/') {
        return "RTW unexpectedly changed path string composition.";
    }

    // Skip the model name
    path += model_name_len + 1;
    path_len = min(si->path_len, path_len - model_name_len - 1);

    si->alias[0] = '\0';
    strncpy(si->path, path, path_len);
    strncpy(si->name, name, sizeof(si->name));

    // Make sure the strings are terminated
    si->name[sizeof(si->name)-1] = '\0';
    si->path[path_len] = '\0';

    si->offset =
        (void*)rtwCAPI_GetDataAddress( dataAddressMap, 
                rtwCAPI_GetBlockParameterAddrIdx(blockParams, si->index))
        - (void*)&rtP;

    dimIdx = rtwCAPI_GetBlockParameterDimensionIdx(blockParams, si->index);

    if (rtwCAPI_GetNumDims(dimMap, dimIdx) != 2) {
        return "Cannot handle array dimensions != 2";
    }
    dimArrayIdx = rtwCAPI_GetDimArrayIndex(dimMap, dimIdx);

    si->dim[0] = si->dim[1] = 0;
    if (min(dimArray[dimArrayIdx],dimArray[dimArrayIdx+1]) == 1) {
        si->dim[0] = max(dimArray[dimArrayIdx],dimArray[dimArrayIdx+1]);
    }
    else {
        switch (rtwCAPI_GetOrientation(dimMap, dimIdx)) {
            case rtwCAPI_MATRIX_COL_MAJOR:
                if (min(dimArray[dimArrayIdx],dimArray[dimArrayIdx+1]) == 1) {
                } else {
                    si->dim[0] = dimArray[dimArrayIdx];
                    si->dim[1] = dimArray[dimArrayIdx+1];
                }
                break;
            case rtwCAPI_MATRIX_ROW_MAJOR:
                si->dim[0] = dimArray[dimArrayIdx+1];
                si->dim[1] = dimArray[dimArrayIdx];
                break;
            default:
                return "Unknown RTW data orientation encountered.";
        }
    }

    dataTypeIdx = rtwCAPI_GetBlockParameterDataTypeIdx(blockParams, si->index);
    switch (rtwCAPI_GetDataTypeSLId(dTypeMap, dataTypeIdx)) {
        case SS_DOUBLE:
            si->data_type = si_double_T;
            break;
        case SS_SINGLE:
            si->data_type = si_single_T;
            break;
        case SS_INT8:
            si->data_type = si_sint8_T;
            break;
        case SS_UINT8:
            si->data_type = si_uint8_T;
            break;
        case SS_INT16:
            si->data_type = si_sint16_T;
            break;
        case SS_UINT16:
            si->data_type = si_uint16_T;
            break;
        case SS_INT32:
            si->data_type = si_sint32_T;
            break;
        case SS_UINT32:
            si->data_type = si_uint32_T;
            break;
        case SS_BOOLEAN:
            si->data_type = si_boolean_T;
            break;
        default:
            return "Unknown RTW data type encountered.";
    }

    // Sample time index is irrelevant for parameters
    si->st_index = ~0U;

    // Check that the data type is compatable
    if (si_data_width[si->data_type] !=
            rtwCAPI_GetDataTypeSize(dTypeMap, dataTypeIdx)) {
        return "Incompatable data type size encountered.";
    }
    if (rtwCAPI_GetDataIsComplex(dTypeMap, dataTypeIdx)) {
        return "Cannot interact with complex signals yet.";
    }
    if (rtwCAPI_GetDataIsPointer(dTypeMap, dataTypeIdx)) {
        return "Cannot interact with pointer signals.";
    }

    return NULL;
}

const char* rtw_capi_init(
        RT_MODEL *rtM,
        unsigned int *max_signals,
        unsigned int *max_parameters,
        unsigned int *max_path_len
        )
{
    unsigned int i;
    size_t path_len;

    model_name_len = strlen(STR(MODEL));

    mmi = &(rtmGetDataMapInfo(rtM).mmi);
    dimMap   = rtwCAPI_GetDimensionMap(mmi);
    dimArray = rtwCAPI_GetDimensionArray(mmi);
    dTypeMap = rtwCAPI_GetDataTypeMap(mmi);
    dataAddressMap = rtwCAPI_GetDataAddressMap(mmi);
    if (!dimMap || !dimArray || !dTypeMap) {
        return "One of dimMap, dimArray or dTypeMap is NULL.";
    }

    signals = rtwCAPI_GetSignals(mmi);
    if (signals == NULL) {
        return "Could not find RTW Signals structure.";
    }
    *max_signals = maxSignalIdx = rtwCAPI_GetNumSignals(mmi);

#ifdef rtP
    blockParams = rtwCAPI_GetBlockParameters(mmi);
    if (blockParams == NULL) {
        return "Could not find RTW Parameters structure.";
    }
    maxParameterIdx = rtwCAPI_GetNumBlockParameters(mmi);
#else
    maxParameterIdx = 0;
#endif
    *max_parameters = maxParameterIdx;

    // Find out the length of the longest path. This is necessary for the
    // user space buddy to allocate enough space.
    for (i = 0; i < maxSignalIdx; i++) {
        path_len = strlen(rtwCAPI_GetSignalBlockPath(signals, i));
        if (*max_path_len < path_len) {
            *max_path_len = path_len;
        }
    }
    for (i = 0; i < maxParameterIdx; i++) {
        path_len = strlen(
                rtwCAPI_GetBlockParameterBlockPath(blockParams, i));
        if (*max_path_len < path_len) {
            *max_path_len = path_len;
        }
    }
    return NULL;
}
