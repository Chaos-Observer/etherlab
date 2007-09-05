#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/encoding.h>

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error LIBXML_TREE_ENABLED or LIBXML_OUTPUT_ENABLED not set for libxml2
#endif

#include "defines.h"
#include "capi.h"
#include "rtmodel.h"

#define CHECK_ERR(condition, position, msg) { \
    if (condition) { \
        fprintf(stderr, "%s error: %s\n", position, msg); \
        exit(1); \
    } \
}

const char *encoding = "ISO-8859-1";
const char *err_no_mem = "No memory";
uint32_t numDataTypeMaps = 0;
uint32_t numDims = 0;

xmlChar *my_convert(const char *str)
{
    const xmlCharEncodingHandlerPtr handler = 
        xmlGetCharEncodingHandler(XML_CHAR_ENCODING_8859_1);
    static xmlChar *buf = NULL;
    static ssize_t buf_len = 0;
    ssize_t new_buf_len;
    int tmp;
    int ret;

    return NULL;

    new_buf_len = 2*strlen(str)+1;
    if (buf_len < new_buf_len) {
        buf_len = new_buf_len > 1000 ? new_buf_len : 1000;
        buf = xmlRealloc(buf, new_buf_len);
        CHECK_ERR(!buf, "xmlRealloc", err_no_mem);
    }
    ret = handler->input(buf, &buf_len, (const xmlChar *)str, &tmp);
    CHECK_ERR(ret == -1, "xmlCharEncodingHandler", err_no_mem);
    CHECK_ERR(ret == -2, "xmlCharEncodingHandler", "Transcoding failed");
    return buf;
}

/* Creates a structure to describe block signals */
void dump_rtBlockSignals(xmlNodePtr root_node)
{
    char buf[10];
    unsigned sigIdx;
    xmlNodePtr signals_node, signal_node, path_node, name_node;
    rtwCAPI_ModelMappingInfo* mmi = &(rtmGetDataMapInfo(rtM).mmi);
    const rtwCAPI_Signals* bio = rtwCAPI_GetSignals(mmi);
    xmlChar *utf8name;

    signals_node = xmlNewChild(root_node, NULL, BAD_CAST "signals", NULL);
    CHECK_ERR(!signals_node, "xmlNewChild", err_no_mem);

    snprintf(buf, sizeof(buf), "%u", rtwCAPI_GetNumSignals(mmi));
    CHECK_ERR( !xmlNewProp(signals_node, BAD_CAST "count", BAD_CAST buf),
            "xmlNewProp", err_no_mem);

    for (sigIdx = 0; sigIdx < rtwCAPI_GetNumSignals(mmi); sigIdx++) {
        unsigned int sysNum;
        const char *blockPath;
        const char *signalName;
        unsigned int portNumber;
        unsigned int dataTypeIndex;
        unsigned int dimIndex;
        unsigned int fxpIndex;
        unsigned int sampleTimeIndex;
        uintptr_t addr;

        sysNum          = rtwCAPI_GetSignalSysNum(bio, sigIdx); 
        blockPath       = rtwCAPI_GetSignalBlockPath(bio, sigIdx);
        signalName      = rtwCAPI_GetSignalName(bio, sigIdx);
        portNumber      = rtwCAPI_GetSignalPortNumber(bio, sigIdx);
        dataTypeIndex   = rtwCAPI_GetSignalDataTypeIdx(bio, sigIdx);
        dimIndex        = rtwCAPI_GetSignalDimensionIdx(bio, sigIdx);
        fxpIndex        = rtwCAPI_GetSignalFixPtIdx(bio, sigIdx);
        sampleTimeIndex = rtwCAPI_GetSignalSampleTimeIdx(bio, sigIdx);

        numDims = dimIndex > numDims ? dimIndex : numDims;
        numDataTypeMaps = 
            dataTypeIndex > numDataTypeMaps ? dataTypeIndex : numDataTypeMaps;

        addr = (uintptr_t)rtwCAPI_GetDataAddress(
				rtwCAPI_GetDataAddressMap(mmi),
                        rtwCAPI_GetSignalAddrIdx(bio, sigIdx)
                        ) - (uintptr_t)&rtB;

        signal_node = xmlNewChild(signals_node, NULL, BAD_CAST "signal", NULL);
        CHECK_ERR(!signal_node, "xmlNewChild", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", sigIdx);
        CHECK_ERR( 
                !xmlNewProp(signal_node, BAD_CAST "index", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", sysNum);
        CHECK_ERR( 
                !xmlNewProp(signal_node, BAD_CAST "sysNum", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", portNumber);
        CHECK_ERR( 
                !xmlNewProp(signal_node, BAD_CAST "portNumber", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", dataTypeIndex);
        CHECK_ERR( 
                !xmlNewProp(signal_node, BAD_CAST "dataTypeIndex", 
                    BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", dimIndex);
        CHECK_ERR( 
                !xmlNewProp(signal_node, BAD_CAST "dimIndex", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", fxpIndex);
        CHECK_ERR( 
                !xmlNewProp(signal_node, BAD_CAST "fxpIndex", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", sampleTimeIndex);
        CHECK_ERR( 
                !xmlNewProp(signal_node, BAD_CAST "sampleTimeIndex", 
                    BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", addr);
        CHECK_ERR( 
                !xmlNewProp(signal_node, BAD_CAST "addrOffset", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        utf8name = my_convert(blockPath);
        path_node = xmlNewChild(signal_node, NULL, BAD_CAST "path", 
                BAD_CAST blockPath);
        CHECK_ERR(!path_node, "xmlNewChild", err_no_mem);

        if (strlen(signalName)) {
            utf8name = my_convert(signalName);
            name_node = xmlNewTextChild(signal_node, NULL, BAD_CAST "name",  
                    BAD_CAST signalName);
            CHECK_ERR(!name_node, "xmlNewChild", err_no_mem);
        }
    }
}

/* Creates a structure to describe block signals */
void dump_rtBlockParameters(xmlNodePtr root_node)
{
    char buf[10];
    unsigned paramIdx;
    xmlNodePtr parameters_node, parameter_node;
    rtwCAPI_ModelMappingInfo* mmi = &(rtmGetDataMapInfo(rtM).mmi);
    const rtwCAPI_BlockParameters* prm = rtwCAPI_GetBlockParameters(mmi);
    char *path = NULL;
    size_t max_path_len = 0;
    xmlChar *utf8name;

    parameters_node = xmlNewChild(root_node, NULL, BAD_CAST "parameters", NULL);
    CHECK_ERR(!parameters_node, "xmlNewChild", err_no_mem);

    snprintf(buf, sizeof(buf), "%u", rtwCAPI_GetNumBlockParameters(mmi));
    CHECK_ERR( !xmlNewProp(parameters_node, BAD_CAST "count", BAD_CAST buf),
            "xmlNewProp", err_no_mem);

    for (paramIdx = 0; paramIdx < rtwCAPI_GetNumBlockParameters(mmi); 
            paramIdx++) {
        const char *blockPath;
        const char *paramName;
        unsigned int dataTypeIndex;
        unsigned int dimIndex;
        unsigned int fxpIndex;
        uintptr_t addr;
        size_t path_len;

        blockPath = rtwCAPI_GetBlockParameterBlockPath(prm, paramIdx);
        paramName = rtwCAPI_GetBlockParameterName(prm, paramIdx);
        dataTypeIndex = rtwCAPI_GetBlockParameterDataTypeIdx(prm, paramIdx);
        dimIndex = rtwCAPI_GetBlockParameterDimensionIdx(prm, paramIdx);
        fxpIndex = rtwCAPI_GetBlockParameterFixPtIdx(prm, paramIdx);

        path_len = strlen(blockPath) + strlen(paramName) + 2;
        if (max_path_len < path_len) {
            max_path_len = path_len;
            path = realloc(path, max_path_len);
            CHECK_ERR(!path, "realloc", err_no_mem);
        }

        snprintf(path, max_path_len, "%s/%s", blockPath, paramName);

        numDims = dimIndex > numDims ? dimIndex : numDims;
        numDataTypeMaps = 
            dataTypeIndex > numDataTypeMaps ? dataTypeIndex : numDataTypeMaps;

        addr = (uintptr_t)rtwCAPI_GetDataAddress(
                rtwCAPI_GetDataAddressMap(mmi),
                rtwCAPI_GetSignalAddrIdx(prm, paramIdx)
                ) - (uintptr_t)&rtP;

        utf8name = my_convert(path);
        parameter_node = xmlNewChild(parameters_node, NULL, 
                BAD_CAST "parameter", BAD_CAST path);
        CHECK_ERR(!parameter_node, "xmlNewChild", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", paramIdx);
        CHECK_ERR( 
                !xmlNewProp(parameter_node, BAD_CAST "index", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", dataTypeIndex);
        CHECK_ERR( 
                !xmlNewProp(parameter_node, BAD_CAST "dataTypeIndex", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", dimIndex);
        CHECK_ERR( 
                !xmlNewProp(parameter_node, BAD_CAST "dimIndex", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", fxpIndex);
        CHECK_ERR( 
                !xmlNewProp(parameter_node, BAD_CAST "fxpIndex", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", addr);
        CHECK_ERR( 
                !xmlNewProp(parameter_node, BAD_CAST "addrOffset", BAD_CAST buf),
                "xmlNewProp", err_no_mem);
    }
    free(path);
}

void dump_dataTypeMap(xmlNodePtr root_node)
{
    char buf[10];
    unsigned idx;
    xmlNodePtr dataTypeMaps_node, dataTypeMap_node;

    rtwCAPI_ModelMappingInfo* mmi = &(rtmGetDataMapInfo(rtM).mmi);
    const rtwCAPI_DataTypeMap* dTypeMap = rtwCAPI_GetDataTypeMap(mmi);

    dataTypeMaps_node = xmlNewChild(root_node, NULL, BAD_CAST "datatypes", NULL);
    CHECK_ERR(!dataTypeMaps_node, "xmlNewChild", err_no_mem);

    snprintf(buf, sizeof(buf), "%u", numDataTypeMaps+1);
    CHECK_ERR( !xmlNewProp(dataTypeMaps_node, BAD_CAST "count", BAD_CAST buf),
            "xmlNewProp", err_no_mem);

    for (idx = 0; idx <= numDataTypeMaps; idx++) {
        const char_T *cDataName;   /* C language data type name                    */
        const char_T *mwDataName;  /* MathWorks data type, typedef in rtwtypes.h   */
        uint16_t      numElements; /* number of elements, 0 for non-structure data */
        uint16_t      elemMapIndex;/* index into the ElementMap, gives Bus Info    */
        uint16_t      dataSize;    /* data size in Bytes                           */
        uint8_t       slDataId;    /* enumerated data type from simstruc_types.h   */
        unsigned int  isComplex; /* is the data type complex (1=Complex, 0=Real) */
        unsigned int  isPointer; /* is data accessed Via Pointer (1=yes, 0= no)  */

        cDataName = rtwCAPI_GetDataTypeCName(dTypeMap, idx);
        mwDataName = rtwCAPI_GetDataTypeMWName(dTypeMap, idx);
        numElements = rtwCAPI_GetDataTypeNumElements(dTypeMap, idx);
        elemMapIndex = rtwCAPI_GetDataTypeElemMapIndex(dTypeMap,idx);
        slDataId = rtwCAPI_GetDataTypeSLId(dTypeMap, idx);
        dataSize = rtwCAPI_GetDataTypeSize(dTypeMap, idx);
        isComplex = rtwCAPI_GetDataIsComplex(dTypeMap, idx);
        isPointer = rtwCAPI_GetDataIsPointer(dTypeMap, idx);

        dataTypeMap_node = xmlNewChild(dataTypeMaps_node, NULL, 
                BAD_CAST "datatype", NULL);
        CHECK_ERR(!dataTypeMap_node, "xmlNewChild", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", idx);
        CHECK_ERR( 
                !xmlNewProp(dataTypeMap_node, BAD_CAST "index", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        CHECK_ERR( 
                !xmlNewProp(dataTypeMap_node, BAD_CAST "cdataname",
                    BAD_CAST cDataName),
                "xmlNewProp", err_no_mem);

        CHECK_ERR( 
                !xmlNewProp(dataTypeMap_node, BAD_CAST "mwDataName",
                    BAD_CAST mwDataName),
                "xmlNewProp", err_no_mem);

//        snprintf(buf, sizeof(buf), "%u", numElements);
//        CHECK_ERR( 
//                !xmlNewProp(dataTypeMap_node, BAD_CAST "numElements", BAD_CAST buf),
//                "xmlNewProp", err_no_mem);

//        snprintf(buf, sizeof(buf), "%u", elemMapIndex);
//        CHECK_ERR( 
//                !xmlNewProp(dataTypeMap_node, BAD_CAST "elemMapIndex", BAD_CAST buf),
//                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", dataSize);
        CHECK_ERR( 
                !xmlNewProp(dataTypeMap_node, BAD_CAST "dataSize", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", isComplex);
        CHECK_ERR( 
                !xmlNewProp(dataTypeMap_node, BAD_CAST "isComplex", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", isPointer);
        CHECK_ERR( 
                !xmlNewProp(dataTypeMap_node, BAD_CAST "isPointer", BAD_CAST buf),
                "xmlNewProp", err_no_mem);
    }
}

void dump_dimensionMap(xmlNodePtr root_node)
{
    char buf[10];
    unsigned idx;
    xmlNodePtr dimensionMaps_node, dimensionMap_node, dimension_node;

    rtwCAPI_ModelMappingInfo* mmi = &(rtmGetDataMapInfo(rtM).mmi);
    const rtwCAPI_DimensionMap* dimMap = rtwCAPI_GetDimensionMap(mmi);
    const uint_T *dimensionArray = rtwCAPI_GetDimensionArray(mmi);

    dimensionMaps_node = xmlNewChild(root_node, NULL, 
            BAD_CAST "dimensionMaps", NULL);
    CHECK_ERR(!dimensionMaps_node, "xmlNewChild", err_no_mem);

    snprintf(buf, sizeof(buf), "%u", numDims+1);
    CHECK_ERR( !xmlNewProp(dimensionMaps_node, BAD_CAST "count", BAD_CAST buf),
            "xmlNewProp", err_no_mem);

    for (idx = 0; idx <= numDims; idx++) {
        rtwCAPI_Orientation orientation;
        uint_T      dimArrayIndex; /* index into dimension array */
        uint8_T     numDims;       /* number of dimensions       */
        const char *orientationName = "unknown";
        unsigned int jdx;

        orientation = rtwCAPI_GetOrientation(dimMap, idx);
        dimArrayIndex = rtwCAPI_GetDimArrayIndex(dimMap, idx);
        numDims = rtwCAPI_GetNumDims(dimMap, idx);

        dimensionMap_node = xmlNewChild(dimensionMaps_node, NULL, 
                BAD_CAST "dimensionMap", NULL);
        CHECK_ERR(!dimensionMap_node, "xmlNewChild", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", idx);
        CHECK_ERR( 
                !xmlNewProp(dimensionMap_node, BAD_CAST "index", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        switch (orientation) {
            case rtwCAPI_SCALAR:
                orientationName = "scalar";
                break;
            case rtwCAPI_VECTOR:
                orientationName = "vector";
                break;
            case rtwCAPI_MATRIX_ROW_MAJOR:
                orientationName = "matrix_row_major";
                break;
            case rtwCAPI_MATRIX_COL_MAJOR:
                orientationName = "matrix_row_major";
                break;
            case rtwCAPI_MATRIX_COL_MAJOR_ND:
                orientationName = "matrix_row_major_nd";
                break;
        }
        CHECK_ERR( 
                !xmlNewProp(dimensionMap_node, BAD_CAST "orientation", 
                    BAD_CAST orientationName),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", numDims);
        CHECK_ERR( 
                !xmlNewProp(dimensionMap_node, BAD_CAST "numDims", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        for (jdx = 0; jdx < numDims; jdx++) {
            dimension_node = xmlNewChild(dimensionMap_node, NULL, 
                    BAD_CAST "dimension", NULL);
            CHECK_ERR(!dimension_node, "xmlNewChild", err_no_mem);

            snprintf(buf, sizeof(buf), "%u", jdx);
            CHECK_ERR( 
                    !xmlNewProp(dimension_node, BAD_CAST "index", BAD_CAST buf),
                    "xmlNewProp", err_no_mem);

            snprintf(buf, sizeof(buf), "%u", dimensionArray[dimArrayIndex+jdx]);
            CHECK_ERR( 
                    !xmlNewProp(dimension_node, BAD_CAST "value", BAD_CAST buf),
                    "xmlNewProp", err_no_mem);
        }

    }
}

void dump_sampleTime(xmlNodePtr root_node)
{
    char buf[20];
    unsigned idx;
    xmlNodePtr sampletimes_node, sampletime_node;

    rtwCAPI_ModelMappingInfo* mmi = &(rtmGetDataMapInfo(rtM).mmi);
    const rtwCAPI_SampleTimeMap* sampleTimeMap = rtwCAPI_GetSampleTimeMap(mmi);

    sampletimes_node = xmlNewChild(root_node, NULL, 
            BAD_CAST "sampleTimes", NULL);
    CHECK_ERR(!sampletimes_node, "xmlNewChild", err_no_mem);

    snprintf(buf, sizeof(buf), "%u", NUMST);
    CHECK_ERR( !xmlNewProp(sampletimes_node, BAD_CAST "count", BAD_CAST buf),
            "xmlNewProp", err_no_mem);

    snprintf(buf, sizeof(buf), "%u", TID01EQ);
    CHECK_ERR( !xmlNewProp(sampletimes_node, BAD_CAST "tid01eq", BAD_CAST buf),
            "xmlNewProp", err_no_mem);

    for (idx = 0; idx < NUMST; idx++) {
        real_T  *samplePeriodPtr;  /* pointer to sample time period value       */
        real_T  *sampleOffsetPtr;  /* pointer to sample time Offset value       */
        int8_T    tid;              /* task identifier  */
        uint8_T   samplingMode;     /* 1 = FrameBased, 0 = SampleBased */

        samplePeriodPtr = (real_T*)rtwCAPI_GetSamplePeriodPtr(sampleTimeMap, idx);
        sampleOffsetPtr = (real_T*)rtwCAPI_GetSampleOffsetPtr(sampleTimeMap, idx);
        tid = rtwCAPI_GetSampleTimeTID(sampleTimeMap, idx);
        samplingMode = rtwCAPI_GetSamplingMode(sampleTimeMap, idx);

        sampletime_node = xmlNewChild(sampletimes_node, NULL, 
                BAD_CAST "sampleTime", NULL);
        CHECK_ERR(!sampletime_node, "xmlNewChild", err_no_mem);

//        snprintf(buf, sizeof(buf), "%u", idx);
//        CHECK_ERR( 
//                !xmlNewProp(sampletime_node, BAD_CAST "index", BAD_CAST buf),
//                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", tid);
        CHECK_ERR( 
                !xmlNewProp(sampletime_node, BAD_CAST "tid", BAD_CAST buf),
                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%g", *samplePeriodPtr);
        CHECK_ERR( 
                !xmlNewProp(sampletime_node, BAD_CAST "samplePeriod", 
                    BAD_CAST buf),
                "xmlNewProp", err_no_mem);

//        snprintf(buf, sizeof(buf), "%g", sampleOffset);
//        CHECK_ERR( 
//                !xmlNewProp(sampletime_node, BAD_CAST "sampleOffset", 
//                    BAD_CAST buf),
//                "xmlNewProp", err_no_mem);

        snprintf(buf, sizeof(buf), "%u", samplingMode);
        CHECK_ERR( 
                !xmlNewProp(sampletime_node, BAD_CAST "samplingMode", 
                    BAD_CAST buf),
                "xmlNewProp", err_no_mem);

    }
}

int main(int argc, char **argv)
{
    xmlDocPtr doc = NULL;       /* document pointer */
    xmlNodePtr root_node;

#include "capi_init.c"

    LIBXML_TEST_VERSION;

    /* 
     * Creates a new document, a node and set it as a root node
     */
    doc = xmlNewDoc(BAD_CAST "1.0");
    CHECK_ERR( !doc, "xmlNewDoc", err_no_mem);

    root_node = xmlNewNode(NULL, BAD_CAST "modeldescription");
    CHECK_ERR( !root_node, "xmlNewNode", err_no_mem);

    xmlDocSetRootElement(doc, root_node);
    CHECK_ERR(
            !xmlNewProp(root_node, BAD_CAST "modelname", BAD_CAST STR(MODEL)),
            "xmlNewProp", err_no_mem);

    dump_rtBlockSignals(root_node);
    dump_rtBlockParameters(root_node);
    dump_dataTypeMap(root_node);
    dump_dimensionMap(root_node);
    dump_sampleTime(root_node);

    xmlSaveFormatFileEnc(argc > 1 ? argv[1] : "-", doc, "UTF-8", 1);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return 0;
}
