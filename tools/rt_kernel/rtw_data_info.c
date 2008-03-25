/***********************************************************************
 *
 * $Id$
 *
 * Here functions are defined to 
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
#include "include/rtw_data_info.h"

/* capi.h is generated by the makefile */
#include "capi.h"
#include "rtmodel.h"

rtwCAPI_ModelMappingInfo* mmi;
const rtwCAPI_DimensionMap*    dimMap;
const rtwCAPI_DataTypeMap*     dTypeMap;
const uint_T*                  dimArray;
const rtwCAPI_Signals* signals;
const rtwCAPI_BlockParameters* blockParams;
unsigned int maxSignalIdx;
unsigned int maxParameterIdx;
void ** dataAddressMap;

static const char* get_signal_info(struct signal_info *si, const char **path)
{
    unsigned int dimIdx, dimArrayIdx, dataTypeIdx; 

    if (si->index >= maxSignalIdx) {
        return "Exceeded signal index array.";
    }

    // Pass path as a pointer; gets copied later
    *path = rtwCAPI_GetSignalBlockPath(signals, si->index);

    strncpy(si->name, rtwCAPI_GetSignalName(signals, si->index),
            sizeof(si->name));
    // Make sure the string is terminated
    si->name[sizeof(si->name)-1] = '\0';

    si->offset =
        (void*)rtwCAPI_GetDataAddress( dataAddressMap, 
                rtwCAPI_GetSignalAddrIdx(signals, si->index))
        - (void*)&rtB;

    dimIdx = rtwCAPI_GetSignalDimensionIdx(signals, si->index);

    if (rtwCAPI_GetNumDims(dimMap, dimIdx) != 2) {
        return "Cannot handle array dimensions != 2";
    }
    dimArrayIdx = rtwCAPI_GetDimArrayIndex(dimMap, dimIdx);
    si->rnum = dimArray[dimArrayIdx];
    si->cnum = dimArray[dimArrayIdx+1];

    switch (rtwCAPI_GetOrientation(dimMap, dimIdx)) {
        case rtwCAPI_SCALAR:
            si->orientation = si_scalar;
            break;
        case rtwCAPI_VECTOR:
            si->orientation = si_vector;
            break;
        case rtwCAPI_MATRIX_COL_MAJOR:
            si->orientation = si_matrix_col_major;
            break;
        case rtwCAPI_MATRIX_ROW_MAJOR:
            si->orientation = si_matrix_row_major;
            break;
        default:
            return "Unknown RTW data orientation encountered.";
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

static const char* get_param_info(struct signal_info* si, const char **path)
{
    unsigned int dimIdx, dimArrayIdx, dataTypeIdx; 

    if (si->index >= maxParameterIdx) {
        return "Exceeded parameter index array.";
    }

    // Pass path as a pointer; gets copied later
    *path = rtwCAPI_GetBlockParameterBlockPath(blockParams, si->index);

    strncpy(si->name, rtwCAPI_GetBlockParameterName(blockParams, si->index),
            sizeof(si->name));
    // Make sure the string is terminated
    si->name[sizeof(si->name)-1] = '\0';

    si->offset =
        (void*)rtwCAPI_GetDataAddress( dataAddressMap, 
                rtwCAPI_GetBlockParameterAddrIdx(blockParams, si->index))
        - (void*)&rtP;

    dimIdx = rtwCAPI_GetBlockParameterDimensionIdx(blockParams, si->index);

    if (rtwCAPI_GetNumDims(dimMap, dimIdx) != 2) {
        return "Cannot handle array dimensions != 2";
    }
    dimArrayIdx = rtwCAPI_GetDimArrayIndex(dimMap, dimIdx);
    si->rnum = dimArray[dimArrayIdx];
    si->cnum = dimArray[dimArrayIdx+1];

    switch (rtwCAPI_GetOrientation(dimMap, dimIdx)) {
        case rtwCAPI_SCALAR:
            si->orientation = si_scalar;
            break;
        case rtwCAPI_VECTOR:
            si->orientation = si_vector;
            break;
        case rtwCAPI_MATRIX_COL_MAJOR:
            si->orientation = si_matrix_col_major;
            break;
        case rtwCAPI_MATRIX_ROW_MAJOR:
            si->orientation = si_matrix_row_major;
            break;
        default:
            return "Unknown RTW data orientation encountered.";
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
        const char* (**si)(struct signal_info*, const char**),
        const char* (**pi)(struct signal_info*, const char**),
        unsigned int *max_signals,
        unsigned int *max_parameters,
        unsigned int *max_path_len
        )
{
    unsigned int i;
    size_t path_len;

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

    *si = get_signal_info;
    *pi = get_param_info;

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
