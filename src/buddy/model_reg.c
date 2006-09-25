#include <stdint.h>

#include "defines.h"
#include CAPI_HEADER

#include "msr_reg.h"
#include "msr_main.h"
#include "msr_lists.h"

#include "rtmodel.h"

static int reg_signals(void)
{
	rtwCAPI_ModelMappingInfo* mmi = &(rtmGetDataMapInfo(rtM).mmi);
	const rtwCAPI_Signals* signals;
	unsigned int sigIdx, dimIdx, dimArrayIdx, dataTypeIdx; 
	const rtwCAPI_DimensionMap*    dimMap;
	const rtwCAPI_DataTypeMap*     dTypeMap;
	const uint_T*                  dimArray;
	const char *cTypeName;


	const char *path, *name;
	unsigned int rnum, cnum;
	unsigned int dataType, orientation, dataSize;
        uintptr_t offset;
	
	signals = rtwCAPI_GetSignals(mmi);
	if (signals == NULL) {
		fprintf(stderr, "Could not get Model Parameters Ptr\n");
		return -1;
	}


	dimMap   = rtwCAPI_GetDimensionMap(mmi);
	dimArray = rtwCAPI_GetDimensionArray(mmi);
	dTypeMap = rtwCAPI_GetDataTypeMap(mmi);
	if (!dimMap || !dimArray || !dTypeMap) {
		fprintf(stderr, "One of dimMap, dimArray or dTypeMap is NULL %p %p %p\n", dimMap, dimArray, dTypeMap);
		return -1;
	}

	for (sigIdx = 0; sigIdx < rtwCAPI_GetNumSignals(mmi); sigIdx++) {
		path = rtwCAPI_GetSignalBlockPath(signals, sigIdx);
		name = rtwCAPI_GetSignalName(signals, sigIdx);

		offset = (uintptr_t)rtwCAPI_GetDataAddress(
				rtwCAPI_GetDataAddressMap(mmi),
				rtwCAPI_GetSignalAddrIdx(signals, sigIdx)
				) - (uintptr_t)&rtB;


		dimIdx = rtwCAPI_GetSignalDimensionIdx(signals, sigIdx);

		if (rtwCAPI_GetNumDims(dimMap, dimIdx) != 2) {
			fprintf(stderr, "Cannot handle array dimensions != 2\n");
			return -1;
		}
		dimArrayIdx = rtwCAPI_GetDimArrayIndex(dimMap, dimIdx);
		orientation = rtwCAPI_GetOrientation(dimMap, dimIdx);
		if (orientation == rtwCAPI_MATRIX_COL_MAJOR_ND) {
			fprintf(stderr, "Cannot handle n Dimensional arrays\n");
			return -1;
		}

		rnum = dimArray[dimArrayIdx];
		cnum = dimArray[dimArrayIdx+1];

		dataTypeIdx = rtwCAPI_GetSignalDataTypeIdx(signals, sigIdx);
		cTypeName = rtwCAPI_GetDataTypeCName(dTypeMap, dataTypeIdx);
		dataType = rtwCAPI_GetDataTypeSLId(dTypeMap, dataTypeIdx);
		dataSize = rtwCAPI_GetDataTypeSize(dTypeMap, dataTypeIdx);

                if (!msr_reg_rtw_signal(path, name, cTypeName, offset, rnum,
                        cnum, dataType, orientation, dataSize))
                    return -1;

	}

	return 0;
}

#ifdef rtP
static int reg_params(void *mdl_rtP)
{
	rtwCAPI_ModelMappingInfo* mmi = &(rtmGetDataMapInfo(rtM).mmi);
	const rtwCAPI_BlockParameters* blockParams;
	unsigned int paramIdx, dimIdx, dimArrayIdx, dataTypeIdx; 
	const rtwCAPI_DimensionMap*    dimMap;
	const rtwCAPI_DataTypeMap*     dTypeMap;
	const uint_T*                  dimArray;
	const char *cTypeName;


	const char *path, *name;
	ptrdiff_t offset;
	unsigned int rnum, cnum;
	unsigned int dataType, orientation, dataSize;

	blockParams = rtwCAPI_GetBlockParameters(mmi);
	if (blockParams == NULL) {
		fprintf(stderr, "Could not get Model Parameters Ptr\n");
		return -1;
	}

	dimMap   = rtwCAPI_GetDimensionMap(mmi);
	dimArray = rtwCAPI_GetDimensionArray(mmi);
	dTypeMap = rtwCAPI_GetDataTypeMap(mmi);
	if (!dimMap || !dimArray || !dTypeMap) {
		fprintf(stderr, "One of dimMap, dimArray or dTypeMap is NULL %p %p %p\n", dimMap, dimArray, dTypeMap);
		return -1;
	}


	for (paramIdx = 0; paramIdx < rtwCAPI_GetNumBlockParameters(mmi); 
			paramIdx++){
		path = rtwCAPI_GetBlockParameterBlockPath(blockParams, paramIdx);
		name = rtwCAPI_GetBlockParameterName(blockParams, paramIdx);
		offset = (ptrdiff_t)rtwCAPI_GetDataAddress(
                        rtwCAPI_GetDataAddressMap(mmi),
                        rtwCAPI_GetBlockParameterAddrIdx(blockParams, paramIdx)
                        ) - (ptrdiff_t)&rtP;

		dimIdx = rtwCAPI_GetBlockParameterDimensionIdx(blockParams, 
                        paramIdx);

		if (rtwCAPI_GetNumDims(dimMap, dimIdx) != 2) {
			fprintf(stderr, "Cannot handle array dimensions != 2\n");
			return -1;
		}
		dimArrayIdx = rtwCAPI_GetDimArrayIndex(dimMap, dimIdx);
		orientation = rtwCAPI_GetOrientation(dimMap, dimIdx);
		if (orientation == rtwCAPI_MATRIX_COL_MAJOR_ND) {
			fprintf(stderr, "Cannot handle n Dimensional arrays\n");
			return -1;
		}

		rnum = dimArray[dimArrayIdx];
		cnum = dimArray[dimArrayIdx+1];

		dataTypeIdx = rtwCAPI_GetBlockParameterDataTypeIdx(blockParams,
                        paramIdx);
		cTypeName = rtwCAPI_GetDataTypeCName(dTypeMap, dataTypeIdx);
		dataType = rtwCAPI_GetDataTypeSLId(dTypeMap, dataTypeIdx);
		dataSize = rtwCAPI_GetDataTypeSize(dTypeMap, dataTypeIdx);

		if (!msr_reg_rtw_param( path, name, cTypeName, 
                            (void *)((uintptr_t)mdl_rtP + offset), rnum, cnum, 
                            dataType, orientation, dataSize))
                    return -1;
	}

	return 0;
}
#endif

int model_init(void *mdl_rtP)
{
	printf("Initialising model\n");

	CAPI_INIT(rtM);

        /*
	if (reg_meta()) {
		fprintf(stderr, "Could not register parameters with msr software\n");
		goto out_reg_params;
	}
        */

#ifdef rtP
	if (reg_params(mdl_rtP)) {
		fprintf(stderr, "Could not register parameters with msr software\n");
		goto out_reg_params;
	}
#endif

	if (reg_signals()) {
		fprintf(stderr, "Could not register signals with msr software\n");
		goto out_reg_signals;
	}

        return 0;

out_reg_signals:
#ifdef rtP
out_reg_params:
#endif
	return -1;
}
