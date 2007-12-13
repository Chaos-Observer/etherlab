/*****************************************************************************
 *
 * $Id$
 *
 * SFunction to implement an arbitrary EtherCAT slave.
 *
 * Copyright (c) 2007, Florian Pose
 * License: GPL
 *
 ****************************************************************************/

#define S_FUNCTION_NAME  generic_ethercat
#define S_FUNCTION_LEVEL 2

#include "simstruc.h"
#include "ethercat_ss_funcs.h"
#include "ss_analog_in_funcs.c"

#define MASTER       (mxGetScalar(ssGetSFcnParam(S, 0)))
#define SLAVE                    (ssGetSFcnParam(S, 1))
#define VENDOR       (mxGetScalar(ssGetSFcnParam(S, 2)))
#define PRODUCT      (mxGetScalar(ssGetSFcnParam(S, 3)))
#define INPUTS                    ssGetSFcnParam(S, 4)
#define OUTPUTS                   ssGetSFcnParam(S, 5)
#define CONFIG                    ssGetSFcnParam(S, 6)
#define TSAMPLE      (mxGetScalar(ssGetSFcnParam(S, 7)))
#define PARAM_COUNT                                 8

struct PdoType {
    const int_T matlabType;
    const char *typeStr;
};

static const struct PdoType typeTable[] = {
    {SS_INT8,   "int8_T"},
    {SS_UINT8,  "uint8_T"},
    {SS_INT16,  "int16_T"},
    {SS_UINT16, "uint16_T"},
    {SS_INT32,  "int32_T"},
    {SS_UINT32, "uint32_T"},
    {SS_SINGLE, "single_T"},
    {SS_DOUBLE, "double_T"},
    {}
};

struct PdoList {
    uint_T count;
    uint16_T *indices;
    uint8_T *subIndices;
    int_T *matlabTypes;
    const char **typeStrings;
};

struct Slave {
    struct PdoList inputs;
    struct PdoList outputs;
};

/****************************************************************************/

/**
 * PdoList constructor.
 */

static void initPdoList(struct PdoList *p)
{
    p->count = 0;
    p->indices = NULL;
    p->subIndices = NULL;
    p->matlabTypes = NULL;
    p->typeStrings = NULL;
}

/****************************************************************************/

/**
 * Clears a Pdo list and frees all allocated memory.
 */

static void clearPdoList(struct PdoList *p)
{
    if (p->indices)
        free(p->indices);
    if (p->subIndices)
        free(p->subIndices);
    if (p->matlabTypes)
        free(p->matlabTypes);
    if (p->typeStrings)
        free(p->typeStrings);
}

/****************************************************************************/

/**
 * Looks up a data type, given a type string.
 */

static const struct PdoType *lookupPdoType(
        SimStruct *S,
        const mxArray *typeInfo)
{
    const struct PdoType *t;
    const char *str;

    if (!(str = getString(S, typeInfo)))
        return NULL;

    for (t = typeTable; t->typeStr; t++) {
        if (!(strcmp(str, t->typeStr))) {
            return t;
        }
    }

    ssSetErrorStatus(S, "Unknown type string");
    return NULL;
}

/****************************************************************************/

/**
 * Fills a Pdo list from a matlab struct.
 */

static int fillPdoList(
        SimStruct *S,
        struct PdoList *p,
        const mxArray *pdoInfo)
{
    uint_T fieldCount, i;
    int_T indexField = -1, subIndexField = -1, typeField = -1;
    const char *name;
    const mxArray *tmp;
    const struct PdoType *t;

    if (!mxIsStruct(pdoInfo)) {
        ssSetErrorStatus(S,
                "Parameters 'inputs' and 'outputs' must be structs");
        return -1;
    }

    if (!(p->count = mxGetNumberOfElements(pdoInfo)))
        return 0;

    fieldCount = mxGetNumberOfFields(pdoInfo);
    for (i = 0; i < fieldCount; i++) {
        name = mxGetFieldNameByNumber(pdoInfo, i);
        if (!strcmp(name, "index")) {
            indexField = i;
        } else if (!strcmp(name, "subindex")) {
            subIndexField = i;
        } else if (!strcmp(name, "type")) {
            typeField = i;
        } else {
            ssSetErrorStatus(S,
                    "Unknown field in inputs or outputs parameter.\n");
            return -1;
        }
    }

    if (indexField == -1) {
        ssSetErrorStatus(S,
                "Parameters 'inputs' and 'outputs'"
                "must contain an 'index' field");
        return -1;
    }
    if (subIndexField == -1) {
        ssSetErrorStatus(S,
                "Parameters 'inputs' and 'outputs'"
                "must contain a 'subindex' field");
        return -1;
    }
    if (typeField == -1) {
        ssSetErrorStatus(S,
                "Parameters 'inputs' and 'outputs'"
                "must contain a 'type' field");
        return -1;
    }

    if (!(p->indices = (uint16_T *)
                calloc(p->count, sizeof(uint16_T)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return -1;
    }

    if (!(p->subIndices = (uint8_T *)
                calloc(p->count, sizeof(uint8_T)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return -1;
    }

    if (!(p->matlabTypes = (int_T *)
                calloc(p->count, sizeof(int_T)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return -1;
    }

    if (!(p->typeStrings = (const char **)
                calloc(p->count, sizeof(const char *)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return -1;
    }

    for (i = 0; i < p->count; i++) {
        tmp = mxGetFieldByNumber(pdoInfo, i, indexField);
        p->indices[i] = (uint16_T) mxGetScalar(tmp);
        tmp = mxGetFieldByNumber(pdoInfo, i, subIndexField);
        p->subIndices[i] = (uint8_T) mxGetScalar(tmp);
        tmp = mxGetFieldByNumber(pdoInfo, i, typeField);
        if (!(t = lookupPdoType(S, tmp)))
            return -1;
        p->matlabTypes[i] = t->matlabType;
        p->typeStrings[i] = t->typeStr; 
    }

    return 0;
}

/****************************************************************************/

/**
 * Slave constructor.
 */

static void initSlave(struct Slave *slave)
{
    initPdoList(&slave->outputs);
    initPdoList(&slave->inputs);
}

/****************************************************************************/

/**
 * Slave destructor.
 */

static void clearSlave(struct Slave *slave)
{
    clearPdoList(&slave->outputs);
    clearPdoList(&slave->inputs);
}

/****************************************************************************/

/**
 * Create a string suitable for ssWriteRTWStrVectParam().
 */

char *createStrVect(SimStruct *S, const char **strings, unsigned int count)
{
    unsigned int i, sum = 3, off = 0;
    char *str;

    for (i = 0; i < count; i++) {
        sum += strlen(strings[i]) + 3;
    }

    if (!(str = calloc(sum, sizeof(char)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return NULL;
    }

    strcpy(str + off, "[");
    off++;
    for (i = 0; i < count; i++) {
        strcpy(str + off, "\"");
        off++;
        strcpy(str + off, strings[i]);
        off += strlen(strings[i]);
        strcpy(str + off, "\"");
        off++;
        if (i < count - 1) {
            strcpy(str + off, ",");
            off++;
        }
    }
    strcpy(str + off, "]");
    off++;

    return str;
}

/* Function: mdlInitializeSizes ===============================================
 * Abstract:
 *    The sizes information is used by Simulink to determine the S-function
 *    block's characteristics (number of inputs, outputs, states, etc.).
 */

static void mdlInitializeSizes(SimStruct *S)
{
    int_T i;
    struct Slave *slave;
    
    ssSetNumSFcnParams(S, PARAM_COUNT);  /* Number of expected parameters */
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        /* Return if number of expected != number of actual parameters */
        return;
    }
    for (i = 0; i < PARAM_COUNT; i++) 
        ssSetSFcnParamTunable(S, i, SS_PRM_NOT_TUNABLE);

    /* Allocate slave struct */

    if (!(slave = (struct Slave *) calloc(1, sizeof(struct Slave)))) {
        ssSetErrorStatus(S, "Could not allocate memory");
        return;
    }
    initSlave(slave);
    ssSetUserData(S, slave);

    /* Evaluate outputs */

    if (fillPdoList(S, &slave->outputs, OUTPUTS))
        goto clear_slave;

    if (!ssSetNumOutputPorts(S, slave->outputs.count))
        goto clear_slave;

    for (i = 0; i < slave->outputs.count; i++) {
        ssSetOutputPortWidth(S, i, 1);
        ssSetOutputPortDataType(S, i, slave->outputs.matlabTypes[i]);
    }

    /* Evaluate inputs */

    if (fillPdoList(S, &slave->inputs, INPUTS))
        goto clear_slave;

    if (!ssSetNumInputPorts(S, slave->inputs.count))
        goto clear_slave;

    for (i = 0; i < slave->inputs.count; i++) {
        ssSetInputPortWidth(S, i, 1);
        ssSetInputPortDataType(S, i, slave->inputs.matlabTypes[i]);
    }

    ssSetNumSampleTimes(S, 1);
    ssSetNumContStates(S, 0);
    ssSetNumDiscStates(S, 0);

    ssSetNumRWork(S, 0);
    ssSetNumIWork(S, 0);
    ssSetNumPWork(S, slave->outputs.count + slave->inputs.count);
    ssSetNumModes(S, 0);
    ssSetNumNonsampledZCs(S, 0);

    ssSetOptions(S, 
            SS_OPTION_WORKS_WITH_CODE_REUSE | 
            SS_OPTION_RUNTIME_EXCEPTION_FREE_CODE);
    return;

clear_slave:
    clearSlave(slave);
    free(slave);
    ssSetUserData(S, NULL);
}

/* Function: mdlInitializeSampleTimes =========================================
 * Abstract:
 *    This function is used to specify the sample time(s) for your
 *    S-function. You must register the same number of sample times as
 *    specified in ssSetNumSampleTimes.
 */

static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, TSAMPLE);
    ssSetOffsetTime(S, 0, 0.0);
}

/* Function: mdlOutputs =======================================================
 * Abstract:
 *    In this function, you compute the outputs of your S-function
 *    block. Generally outputs are placed in the output vector, ssGetY(S).
 */
static void mdlOutputs(SimStruct *S, int_T tid)
{
    /* all output work is done inline via TLC */
}

/* Function: mdlTerminate =====================================================
 * Abstract:
 *    In this function, you should perform any actions that are necessary
 *    at the termination of a simulation.  For example, if memory was
 *    allocated in mdlStart, this is the place to free it.
 */
static void mdlTerminate(SimStruct *S)
{
    struct Slave *slave = (struct Slave *) ssGetUserData(S);

    if (!slave)
        return;

    clearSlave(slave);
    free(slave);
}

#define MDL_RTW
static void mdlRTW(SimStruct *S)
{
    struct Slave *slave = (struct Slave *) ssGetUserData(S);
    uint32_T master = MASTER, vendor = VENDOR, product = PRODUCT;
    const char *addr = getString(S, SLAVE);
    char *strVect;

    if (!ssWriteRTWScalarParam(S, "MasterIndex", &master, SS_UINT32))
        return;
    if (!ssWriteRTWStrParam(S, "SlaveAddress", addr))
        return;
    if (!ssWriteRTWScalarParam(S, "Vendor", &vendor, SS_UINT32))
        return;
    if (!ssWriteRTWScalarParam(S, "Product", &product, SS_UINT32))
        return;

    /* Write output information */

    if (!ssWriteRTWScalarParam(S, "NumberOfOutputs",
                &slave->outputs.count, SS_UINT32))
        return;
    if (!ssWriteRTWVectParam(S, "OutputIndices",
                slave->outputs.indices, SS_UINT16, slave->outputs.count))
        return;
    if (!ssWriteRTWVectParam(S, "OutputSubIndices",
                slave->outputs.subIndices, SS_UINT8, slave->outputs.count))
        return;
    if (!(strVect = createStrVect(S, slave->outputs.typeStrings, slave->outputs.count)))
        return;
    if (!ssWriteRTWStrVectParam(S, "OutputTypes",
                strVect, slave->outputs.count)) {
        free(strVect);
        return;
    }
    free(strVect);

    /* Write input information */

    if (!ssWriteRTWScalarParam(S, "NumberOfInputs",
                &slave->inputs.count, SS_UINT32))
        return;
    if (!ssWriteRTWVectParam(S, "InputIndices",
                slave->inputs.indices, SS_UINT16, slave->inputs.count))
        return;
    if (!ssWriteRTWVectParam(S, "InputSubIndices",
                slave->inputs.subIndices, SS_UINT8, slave->inputs.count))
        return;
    if (!(strVect = createStrVect(S, slave->inputs.typeStrings, slave->inputs.count)))
        return;
    if (!ssWriteRTWStrVectParam(S, "InputTypes",
                strVect, slave->inputs.count)) {
        free(strVect);
        return;
    }
    free(strVect);

    if (!ssWriteRTWWorkVect(S, "PWork", 2,
                "TxPdo", slave->outputs.count,
                "RxPdo", slave->inputs.count)) {
        return;
    }
}

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
