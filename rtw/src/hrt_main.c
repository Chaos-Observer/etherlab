/*****************************************************************************
 *
 * $Id$
 *
 * vim: tw=78
 *
 * Note about creating new Simulink data types:
 * In Simulink, new data types can be created. These named data types are
 * available in pdserv. These data types are C structures.
 *
 * IMPORTANT: Use this interface carefully. If you mess up, your application
 * will crash!
 *
 * PdServ detects these data types and searches the symbol table for a symbol
 * called "<name>_description". This symbol must be a zero terminated
 * structure vector with the following elements:
 * struct compound_desc {
 *     const char   *fieldName;   // Name of field
 *     size_t        offset;      // Offset of field in the structure
 *                                // Use offsetof() to get this value
 *     uint8_T       slDataId;    // enumerated data type
 *                                // from simstruc_types.h
 *                                // Can also be SS_STRUCT, in which case
 *                                // ->next must be used
 *     size_t        dataSize;    // data size in Bytes
 *     uint8_T       isComplex;   // Used only for primary data types
 *     size_t        numDims;     // number of elements in dimMap that
 *                                // specifies how many dimensions
 *                                // If dimMap == NULL, this value directly
 *                                // specifies the number of elements
 *     const size_t  *dimMap;     // Dimension list. If NULL, numDims
 *                                // specifies number of elements
 *     const struct compound_desc *next; // Pointer to another zero terminated
 *                                       // field set when
 *                                       // slDataId == SS_STRUCT
 * };
 *
 * The symbol name must also have "C" linkage, so add 'extern "C" {}' around
 * this definition (see below)
 *
 * To explain the mechanism behind numDims and dimMap:
 * For:              numDims=   dimMap=
 *      Scalars         1       NULL              double d
 *      Vectors         n       NULL              double h[6]
 *      N-Dim array     N       {d1,d2,...,dN}    double v[2][3][4]
 *
 *
 * For example, suppose you have a new data type called PRIV_TYPE, defined
 * as:
 *
   // In file <privtype.h>
  
   struct priv_type {
          double dbl[4],
          int16_t i16[5][6];
          char c;
          struct inner {
              int32_t i32;
              char b[40];
          } inner[2];
   };
  
   
 * Include the following code in the executable:
 * =========================================================================
 *
   #include "privtype.h"
  
   // Definition for struct priv_type
  
   const size_t dimMap[] = {
       5, 6,    // for i16
   };
  
   #ifdef __cplusplus
   extern "C" {
   #endif
   struct compound_desc {
       const char   *fieldName;
       size_t        offset;
       uint8_T       slDataId;
       size_t        dataSize;
       uint8_T       isComplex;
       size_t        numDims;
       const size_t  *dimMap;
       const struct compound_desc *next;
   } PRIV_TYPE_description[] = {
       {"dbl",   offsetof(struct priv_type, dbl), SS_DOUBLE,
           sizeof(real_T),        0, 4, NULL},

       {"i16",   offsetof(struct priv_type, i16), SS_INT16,
           sizeof(int16_T),       0, 2, dimMap},

       {"c",     offsetof(struct priv_type, c), SS_INT8,
           sizeof(char_T),        0, 1, NULL},

       {"inner", offsetof(struct priv_type, inner), SS_STRUCT,
                sizeof(struct inner),  0, 2, NULL, PRIV_TYPE_description + 5},

       {0,}, // Zero terminator

       {"i32",   offsetof(struct inner, i32), SS_INT32,
           sizeof(int32_T),       0, 1,  NULL},

       {"b",     offsetof(struct inner, b), SS_INT32,
           sizeof(char_T),        0, 40, NULL},

       {0,}, // Zero terminator
   };
   #ifdef __cplusplus
   }
   #endif
 * =========================================================================
 *
 ****************************************************************************/

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>
#include <getopt.h>
#include <libgen.h> // basename()
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>  // daemon()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>     // malloc
#include <string.h>
#include <dlfcn.h>

#include <pdserv.h>

#include "rtmodel.h"
#include "rtwtypes.h"
#include "rt_nonfinite.h"
#include "rt_sim.h"

/****************************************************************************/

#define MAX_SAFE_STACK (8 * 1024) /** The maximum stack size which is
                                    guranteed safe to access without faulting.
                                   */

/* To quote a string */
#define STR(x) #x
#define QUOTE(x) STR(x)

#define EXPAND_CONCAT(name1, name2) name1 ## name2
#define CONCAT(name1, name2) EXPAND_CONCAT(name1, name2)
#define MODEL_VERSION        CONCAT(MODEL, _version)
#define RT_MODEL             CONCAT(MODEL, _rtModel)

#ifndef max
#define max(x1, x2) ((x1) > (x2) ? (x1) : (x2))
#endif

#ifndef min
#define min(x1, x2) ((x2) > (x1) ? (x1) : (x2))
#endif

#ifndef RT
# error "must define RT"
#endif

#ifndef MODEL
# error "must define MODEL"
#endif

#ifndef NUMST
# error "must define number of sample times, NUMST"
#endif

#ifndef NCSTATES
# error "must define NCSTATES"
#endif

/*====================*
 * External functions *
 *====================*/

#ifdef __cplusplus
extern "C" {
#endif

extern RT_MODEL *MODEL(void);

extern void MdlInitializeSizes(void);
extern void MdlInitializeSampleTimes(void);
extern void MdlStart(void);
extern void MdlOutputs(int_T tid);
extern void MdlUpdate(int_T tid);
extern void MdlTerminate(void);

extern const char *MODEL_VERSION;

#if NCSTATES > 0
extern void rt_ODECreateIntegrationData(RTWSolverInfo *si);
extern void rt_ODEUpdateContinuousStates(RTWSolverInfo *si);

# define rt_CreateIntegrationData(S) \
    rt_ODECreateIntegrationData(rtmGetRTWSolverInfo(S));
# define rt_UpdateContinuousStates(S) \
    rt_ODEUpdateContinuousStates(rtmGetRTWSolverInfo(S));
# else
# define rt_CreateIntegrationData(S)  \
      rtsiSetSolverName(rtmGetRTWSolverInfo(S),"FixedStepDiscrete");
# define rt_UpdateContinuousStates(S) \
      rtmSetT(S, rtsiGetSolverStopTime(rtmGetRTWSolverInfo(S)));
#endif


/* See comment at the top of the file for registering new data types */
struct compound_desc {
    const char   *fieldName;
    size_t        offset;
    uint8_T       slDataId;
    size_t        dataSize;
    uint8_T       isComplex;
    size_t        numDims;
    const size_t  *dimMap;
    const struct compound_desc *next;
};


#ifdef __cplusplus
}
#endif

/* Command-line option variables.  */

char *base_name = NULL; /**< basename of executable for usage() output. */
int priority = -1; /**< Task priority, -1 means RT (maximum). */
char *pdserv_config = NULL; /**< Path to PdServ configuration file. */
bool daemonize = false; /**< Become a daemon. */
char *pidPath = ""; /**< Path of PID file (empty for no PID file). */

static rtwCAPI_ModelMappingInfo* mmi;
static const rtwCAPI_DimensionMap* dimMap;
static const rtwCAPI_DataTypeMap* dTypeMap;
static const rtwCAPI_SampleTimeMap* sampleTimeMap;
static const uint_T* dimArray;
static void ** dataAddressMap;
static void *exe;      /* Pointer to this executable. */

const char* rt_OneStepMain(RT_MODEL *s);
const char* rt_OneStepTid(RT_MODEL *s, uint_T);
int get_etl_data_type (const char *mwName,
        uint8_T slDataId, size_t size, unsigned int isComplex);

struct thread_task {
    RT_MODEL *S;
    uint_T tid;
    unsigned int *running;
    const char *err;
    double sample_time;
    struct pdtask *pdtask;
    struct timespec time;
    pthread_t thread;
};

# if TID01EQ == 1
#  define FIRST_TID 1
# else
#  define FIRST_TID 0
# endif

#define NSEC_PER_SEC (1000000000U)

#undef timeradd
inline void timeradd(struct timespec *t, unsigned int dt)
{
    t->tv_nsec += dt;
    while (t->tv_nsec >= NSEC_PER_SEC) {
        t->tv_nsec -= NSEC_PER_SEC;
        t->tv_sec++;
    }
}

#define DIFF_NS(A, B) (((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC + \
        (B).tv_nsec - (A).tv_nsec)

/****************************************************************************/

#if !defined(MULTITASKING)  /* SINGLETASKING */

#define NUMTASKS 1

/** Perform one step of the model.
 *
 * This function is modeled such that it could be called from an interrupt
 * service routine (ISR) with minor modifications.
 */
const char *
rt_OneStepMain(RT_MODEL *S)
{
    real_T tnext;

    tnext = rt_SimGetNextSampleHit();
    rtsiSetSolverStopTime(rtmGetRTWSolverInfo(S),tnext);

    MdlOutputs(0);
    MdlUpdate(0);

    rt_SimUpdateDiscreteTaskSampleHits(rtmGetNumSampleTimes(S),
                                       rtmGetTimingData(S),
                                       rtmGetSampleHitPtr(S),
                                       rtmGetTPtr(S));

    if (rtmGetSampleTime(S,0) == CONTINUOUS_SAMPLE_TIME) {
        rt_UpdateContinuousStates(S);
    }

    return rtmGetErrorStatusFlag(S);
}

/****************************************************************************/

#else /* MULTITASKING */

#define NUMTASKS (NUMST - FIRST_TID)


/** Perform one step of the model.
 *
 * This function is modeled such that it could be called from an interrupt
 * service routine (ISR) with minor modifications.
 *
 * This routine is modeled for use in a multitasking environment and therefore
 * needs to be fully re-entrant when it is called from an interrupt service
 * routine.
 *
 * Note:
 *      Error checking is provided which will only be used if this routine
 *      is attached to an interrupt.
 */
const char *
rt_OneStepMain(RT_MODEL *S)
{
    real_T tnext;

    /*******************************************
     * Step the model for the base sample time *
     *******************************************/

    tnext = rt_SimUpdateDiscreteEvents(
                rtmGetNumSampleTimes(S),
                rtmGetTimingData(S),
                rtmGetSampleHitPtr(S),
                rtmGetPerTaskSampleHitsPtr(S));

    rtsiSetSolverStopTime(rtmGetRTWSolverInfo(S),tnext);

    MdlOutputs(FIRST_TID);
    MdlUpdate(FIRST_TID);

    if (rtmGetSampleTime(S,0) == CONTINUOUS_SAMPLE_TIME) {
        rt_UpdateContinuousStates(S);
    } else {
        rt_SimUpdateDiscreteTaskTime(rtmGetTPtr(S), rtmGetTimingData(S), 0);
    }

#if FIRST_TID == 1
    rt_SimUpdateDiscreteTaskTime(rtmGetTPtr(S), rtmGetTimingData(S), 1);
#endif

    return rtmGetErrorStatusFlag(S);
}

/****************************************************************************/

/** Perform one step of the model subrates
 *
 * This routine is modeled for use in a multitasking environment and therefore
 * needs to be fully re-entrant when it is called from an interrupt service
 * routine.
 *
 * Note:
 *      Error checking is provided which will only be used if this routine
 *      is attached to an interrupt.
 */
const char *
rt_OneStepTid(RT_MODEL *S, uint_T tid)
{
    MdlOutputs(tid);

    MdlUpdate(tid);

    rt_SimUpdateDiscreteTaskTime(rtmGetTPtr(S), rtmGetTimingData(S), tid);

    return rtmGetErrorStatusFlag(S);

}

/****************************************************************************/

/** Run the main task.
 */
void *run_task(void *p)
{
    struct thread_task *thread = p;
    unsigned int dt = thread->sample_time * 1.0e9 + 0.5;
    uint32_t exec_ns = 0, period_ns = 0;
    struct timespec start_time, last_start_time = {0, 0},
                    end_time = last_start_time, world_time;

    do {
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        clock_gettime(CLOCK_REALTIME, &world_time);

        thread->err = rt_OneStepTid(thread->S, thread->tid);

        pdserv_update(thread->pdtask, &world_time);

        period_ns = DIFF_NS(last_start_time, start_time);
        exec_ns = DIFF_NS(last_start_time, end_time);
        last_start_time = start_time;
        pdserv_update_statistics(thread->pdtask,
                exec_ns / 1e9, period_ns / 1e9, 0);

        timeradd(&thread->time, dt);

        clock_gettime(CLOCK_MONOTONIC, &end_time);

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &thread->time, 0);
    } while(!thread->err && *thread->running);

    *thread->running = 0;

    return 0;
}

#endif /* MULTITASKING */


/****************************************************************************/

/** Create a new compound data type
 */
size_t make_compound(int compound,
        const struct compound_desc* compound_desc, size_t offset)
{
    int dt;
    //printf("%s() compound=%i, offset=%zu\n", __func__, compound, offset);

    for (; compound_desc->fieldName; compound_desc++) {
        if (compound_desc->slDataId == SS_STRUCT) {
            dt = pdserv_create_compound(
                    compound_desc->fieldName, compound_desc->dataSize);
            make_compound(dt, compound_desc->next,
                    offset + compound_desc->offset);
        }
        else
            dt = get_etl_data_type(
                    compound_desc->fieldName,
                    compound_desc->slDataId,
                    compound_desc->dataSize,
                    compound_desc->isComplex);

        pdserv_compound_add_field( compound,
                compound_desc->fieldName, dt, offset + compound_desc->offset,
                compound_desc->numDims, compound_desc->dimMap);
    }

    return offset;
}

/** Get the compound data type as expected by EtherLab.
 */
int get_compound_data_type(const char *mwName, size_t size)
{
    struct compound_list {
        struct compound_list *next;
        const char *mwName;
        int dtype;
    };
    static struct compound_list compound_list_head = {
        &compound_list_head
    };
    struct compound_list *list;
    const void *compound_desc;
    char *compound_name;
    size_t n, offset;

    /* Try to find the compound in the list */
    for (list = &compound_list_head;
            list->next != &compound_list_head; list = list->next) {
        //printf("diff=, name=%s\n", list->mwName);
        if (!strcmp(mwName, list->mwName))
            return list->dtype;
    }

    /* Look for a symbol <mwName>_description */
    if (!exe) {
        exe = dlopen(NULL, RTLD_LAZY);
        if (!exe)
            return 0;
    }
    n = strlen(mwName);
    compound_name = malloc(n + 20);
    strcpy(compound_name, mwName);
    strcpy(compound_name+n, "_description");
    compound_desc = dlsym(exe, compound_name);
    free(compound_name);
    if (!compound_desc)
        return 0;

    /* Append to end of list */
    list->next = calloc(1, sizeof(struct compound_list));
    list->mwName = mwName;
    list->dtype = pdserv_create_compound(mwName, size);

    list->next->next = &compound_list_head;

    make_compound(list->dtype, compound_desc, 0);

    //printf("%s exe=%p desc=%p\n", compound_name, exe, compound_desc);
    return list->dtype;
}

/** Get the data type as expected by EtherLab.
 */
int get_etl_data_type (const char *mwName,
        uint8_T slDataId, size_t size, unsigned int isComplex)
{
    static int pd_complex[pd_datatype_end];

    int pd_type;
    switch (slDataId) {
        case SS_DOUBLE:  pd_type = pd_double_T;        break;
        case SS_SINGLE:  pd_type = pd_single_T;        break;
        case SS_INT8:    pd_type = pd_sint8_T;         break;
        case SS_UINT8:   pd_type = pd_uint8_T;         break;
        case SS_INT16:   pd_type = pd_sint16_T;        break;
        case SS_UINT16:  pd_type = pd_uint16_T;        break;
        case SS_INT32:   pd_type = pd_sint32_T;        break;
        case SS_UINT32:  pd_type = pd_uint32_T;        break;
        case SS_BOOLEAN: pd_type = pd_boolean_T;       break;
        case SS_STRUCT:
                         pd_type = get_compound_data_type(mwName, size); break;
        default:         pd_type = 0;                  break;
    }

    //printf("new datat type %i\n", pd_type);
    if (!isComplex || !pd_type || slDataId == SS_STRUCT)
        return pd_type;

//    printf("complex type %i %i %i\n",
//            dataTypeIndex, pd_type, pd_complex[pd_type]);
    if (pd_complex[pd_type])
        return pd_complex[pd_type];

    pd_complex[pd_type] = pdserv_create_compound( mwName, size);
    pdserv_compound_add_field( pd_complex[pd_type],
            "Re", pd_type, 0, 1, NULL);
    pdserv_compound_add_field( pd_complex[pd_type],
            "Im", pd_type, size / 2, 1, NULL);

    return pd_complex[pd_type];
}

/****************************************************************************/

/** Register a signal with PdServ.
 */
const char *register_signal(const struct thread_task *task,
        const rtwCAPI_Signals* signals, size_t idx)
{
    uint_T addrMapIndex    = rtwCAPI_GetSignalAddrIdx(signals, idx);
    /* size_t sysNum = rtwCAPI_GetSignalSysNum(signals, idx); */
    const char *blockPath  = rtwCAPI_GetSignalBlockPath(signals, idx);
    const char *signalName = rtwCAPI_GetSignalName(signals, idx);
    uint16_T portNumber      = rtwCAPI_GetSignalPortNumber(signals, idx);
    uint16_T dataTypeIndex   = rtwCAPI_GetSignalDataTypeIdx(signals, idx);
    uint16_T dimIndex        = rtwCAPI_GetSignalDimensionIdx(signals, idx);
    uint8_T  sTimeIndex      = rtwCAPI_GetSignalSampleTimeIdx(signals, idx);

    const void *address =
        rtwCAPI_GetDataAddress(dataAddressMap, addrMapIndex);
    uint_T dimArrayIndex = rtwCAPI_GetDimArrayIndex(dimMap, dimIndex);
    int data_type = get_etl_data_type(
            rtwCAPI_GetDataTypeMWName(dTypeMap, dataTypeIndex),
            rtwCAPI_GetDataTypeSLId(dTypeMap, dataTypeIndex),
            rtwCAPI_GetDataTypeSize(dTypeMap, dataTypeIndex), 
            rtwCAPI_GetDataIsComplex(dTypeMap, dataTypeIndex));
    size_t pathLen = strlen(blockPath) + strlen(signalName) + 9;
    uint8_T ndim = rtwCAPI_GetNumDims(dimMap, dimIndex);
    const real_T *sampleTime =
        rtwCAPI_GetSamplePeriodPtr(sampleTimeMap, sTimeIndex);
    int8_T tid = rtwCAPI_GetSampleTimeTID(sampleTimeMap, sTimeIndex);
    uint_T decimation;

    struct pdvariable *signal;
    char *path;
    size_t *dim = 0;

    size_t i;
    const char *err = 0;

    /* Only allow built-in data types */
    if (!data_type)
        return NULL;

    path = malloc(pathLen);
    if (!path) {
        err = "No memory";
        goto out;
    }

    /* Check that the data type is compatible */
    if (rtwCAPI_GetDataIsPointer(dTypeMap, dataTypeIndex)) {
        err = "Cannot interact with pointer data types.";
        goto out;
    }

    blockPath = strchr(blockPath, '/');
    if (!blockPath) {
        err = "No '/' in path";
        goto out;
    }

    /* If portNumber or the port number of the next signal is set, it means
     * that these signals are part of a single block. Try to use the
     * signalName to extend the path, otherwise attach the portNumber.
     *
     * Note: * idx + 1 is valid, since the list is null terminated
     */
    if (portNumber || rtwCAPI_GetSignalPortNumber(signals, idx + 1)) {
        if (*signalName) {
            snprintf(path, pathLen, "%s/%s", blockPath, signalName);
            signalName = NULL;
        }
        else {
            snprintf(path, pathLen, "%s/%u", blockPath, portNumber);
        }
    }
    else {
        snprintf(path, pathLen, "%s", blockPath);
    }

#if !defined(MULTITASKING)
    decimation =
        tid >= 0 && *sampleTime ? *sampleTime / task[0].sample_time : 1;
#else
    decimation = 1;
    if (tid >= 0) {
        task += tid - (tid && FIRST_TID);
    }
#endif

    if (ndim == 1) {
        ndim = dimArray[dimArrayIndex];
    }
    else if (ndim == 2
            && min(dimArray[dimArrayIndex],
                dimArray[dimArrayIndex + 1]) == 1) {
        ndim = max(dimArray[dimArrayIndex], dimArray[dimArrayIndex + 1]);
    }
    else {
        dim = calloc(ndim, sizeof(size_t));
        if (!dim) {
            err = "No memory";
            goto out;
        }
        for (i = 0; i < ndim; ++i) {
            dim[i] = dimArray[dimArrayIndex + i];
        }
    }

#if 0
    printf("%s task[%u], decim=%u, dt=%u, %p, ndim=%u, %p\n",
            path, tid - (tid && FIRST_TID),
            decimation, data_type, address, ndim, dim);
#endif

    //printf("Reg with dt=%i\n", data_type);
    signal = pdserv_signal(task->pdtask, decimation,
            path, data_type, address, ndim, dim);

    if (signalName && *signalName)
        pdserv_set_alias(signal, signalName);

out:
    free(path);
    free(dim);
    if (err) {
        printf("%s\n", err);
    }
    return err;
}

/****************************************************************************/

/** Register a parameter with PdServ.
 */
const char *register_parameter( struct pdserv *pdserv,
        const rtwCAPI_BlockParameters* params, size_t idx)
{
    uint_T addrMapIndex = rtwCAPI_GetBlockParameterAddrIdx(params, idx);
    const char *blockPath = rtwCAPI_GetBlockParameterBlockPath(params, idx);
    const char *paramName = rtwCAPI_GetBlockParameterName(params, idx);
    uint16_T dataTypeIndex = rtwCAPI_GetBlockParameterDataTypeIdx(params, idx);
    uint16_T dimIndex = rtwCAPI_GetBlockParameterDimensionIdx(params, idx);

    void *address = rtwCAPI_GetDataAddress(dataAddressMap, addrMapIndex);
    uint_T dimArrayIndex = rtwCAPI_GetDimArrayIndex(dimMap, dimIndex);
    size_t pathLen = strlen(blockPath) + strlen(paramName) + 9;
    uint8_T ndim = rtwCAPI_GetNumDims(dimMap, dimIndex);

    int data_type = get_etl_data_type(
            rtwCAPI_GetDataTypeMWName(dTypeMap, dataTypeIndex),
            rtwCAPI_GetDataTypeSLId(dTypeMap, dataTypeIndex),
            rtwCAPI_GetDataTypeSize(dTypeMap, dataTypeIndex), 
            rtwCAPI_GetDataIsComplex(dTypeMap, dataTypeIndex));

    struct pdvariable *param;
    char *path;
    size_t *dim = 0;

    size_t i;
    const char *err = 0;

    /* Only allow built-in data types */
    if (!data_type)
        return NULL;

    path = malloc(pathLen);
    if (!path) {
        err = "No memory";
        goto out;
    }

    /* Check that the data type is compatible */
    if (rtwCAPI_GetDataIsPointer(dTypeMap, dataTypeIndex)) {
        err = "Cannot interact with pointer data types.";
        goto out;
    }

    blockPath = strchr(blockPath, '/');
    if (!blockPath) {
        err = "No '/' in path";
        goto out;
    }

    snprintf(path, pathLen, "%s/%s", blockPath, paramName);

    if (ndim == 1) {
        ndim = dimArray[dimArrayIndex];
    }
    else if (ndim == 2
            && min(dimArray[dimArrayIndex], dimArray[dimArrayIndex+1]) == 1) {
        ndim = max(dimArray[dimArrayIndex], dimArray[dimArrayIndex+1]);
    }
    else {
        dim = calloc(ndim, sizeof(size_t));
        if (!dim) {
            err = "No memory";
            goto out;
        }
        for (i = 0; i < ndim; ++i) {
            dim[i] = dimArray[dimArrayIndex + i];
        }
    }

    param = pdserv_parameter(pdserv, path, 0666,
            data_type, address, ndim, dim, 0, 0);

out:
    free(path);
    free(dim);
    if (err) {
        printf("%s\n", err);
    }
    return err;
}

/****************************************************************************/

/** Initialize all model variables.
 */
const char *
rtw_capi_init(RT_MODEL *S,
        struct pdserv *pdserv, const struct thread_task *task)
{
    const rtwCAPI_Signals* signals;
    const rtwCAPI_BlockParameters* params;
    const char *err;
    size_t i;

    mmi = &(rtmGetDataMapInfo(S).mmi);
    dimMap = rtwCAPI_GetDimensionMap(mmi);
    dTypeMap = rtwCAPI_GetDataTypeMap(mmi);
    dimArray = rtwCAPI_GetDimensionArray(mmi);
    sampleTimeMap = rtwCAPI_GetSampleTimeMap(mmi);
    dataAddressMap = rtwCAPI_GetDataAddressMap(mmi);

    signals = rtwCAPI_GetSignals(mmi);
    for (i = 0; signals && i < rtwCAPI_GetNumSignals(mmi); ++i) {
        register_signal(task, signals, i);
    }

    params = rtwCAPI_GetBlockParameters(mmi);
    for (i = 0; params && i < rtwCAPI_GetNumBlockParameters(mmi); ++i) {
        register_parameter(pdserv, params, i);
    }

    return NULL;
}

/****************************************************************************/

/** Execute model.
 */
const char *init_application(RT_MODEL *S)
{
    const char *errmsg;

    /************************
     * Initialize the model *
     ************************/
    MdlInitializeSizes();
    MdlInitializeSampleTimes();

    if ((errmsg = rt_SimInitTimingEngine(
                    rtmGetNumSampleTimes(S),
                    rtmGetStepSize(S),
                    rtmGetSampleTimePtr(S),
                    rtmGetOffsetTimePtr(S),
                    rtmGetSampleHitPtr(S),
                    rtmGetSampleTimeTaskIDPtr(S),
                    rtmGetTStart(S),
                    &rtmGetSimTimeStep(S),
                    &rtmGetTimingData(S)))) {
        return errmsg;
    }
    rt_CreateIntegrationData(S);

    MdlStart();
    if ((errmsg = rtmGetErrorStatus(S))) {
        MdlTerminate();
        return errmsg;
    }

    return NULL;
}

/****************************************************************************/

/** Return the current system time.
 *
 * This is a callback needed by pdserv.
 */
int gettime(struct timespec *time)
{
    return clock_gettime(CLOCK_REALTIME, time);
}

/****************************************************************************/

/** Cause a stack fault before entering cyclic operation.
 */
void stack_prefault(void)
{
    unsigned char dummy[MAX_SAFE_STACK];

    memset(dummy, 0, MAX_SAFE_STACK);
}

/*****************************************************************************/

/** Remove the PID file.
 */
void remove_pid_file()
{
    int ret;

    ret = unlink(pidPath);
    if (ret == -1) {
        fprintf(stderr, "Failed to remove PID file \"%s\": %s\n",
                pidPath, strerror(errno));
    }
}

/****************************************************************************/

/** Create the PID file.
 */
void create_pid_file()
{
    int fd, ret, len;
    char str[32];

    fd = open(pidPath, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd == -1) {
        fprintf(stderr, "Failed to create PID file \"%s\": %s\n",
                pidPath, strerror(errno));
        return;
    }

    len = snprintf(str, sizeof(str), "%i\n", getpid());

    ret = write(fd, str, len);
    if (ret == -1) {
        fprintf(stderr, "Failed to write to PID file \"%s\": %s\n",
                pidPath, strerror(errno));
        goto out_unlink;
    }

    if (ret != len) {
        fprintf(stderr, "Failed to write to PID file \"%s\"."
                " Written %i of %i bytes.", pidPath, ret, len);
        goto out_unlink;
    }

out_unlink:
    close(fd);
    remove_pid_file();
}

/****************************************************************************/

/** Output the usage.
 */
void usage(FILE *f)
{
    fprintf(f,
            "Usage: %s [OPTIONS]\n"
            "Options:\n"
            "  --priority       -p <PRIO>  Set task priority. Default: RT.\n"
            "  --pdserv-config  -c <PATH>  PdServ configuration file.\n"
            "                              Default: None (use defaults).\n"
            "  --pid-path       -i <PATH>  Write PID file. Default:\n"
            "                              No PID file.\n"
            "  --daemon         -d         Become a daemon before cyclic\n"
            "                              operation.\n"
            "  --help           -h         Show this help.\n",
            base_name);
}

/*****************************************************************************/

/** Get the command-line options.
 */
void get_options(int argc, char **argv)
{
    int c, arg_count;

    static struct option longOptions[] = {
        //name,           has_arg,           flag, val
        {"priority",      required_argument, NULL, 'p'},
        {"pdserv-config", required_argument, NULL, 'c'},
        {"pid-file",      required_argument, NULL, 'i'},
        {"daemon",        no_argument,       NULL, 'd'},
        {"help",          no_argument,       NULL, 'h'},
        {}
    };

    do {
        c = getopt_long(argc, argv, "p:c:i:dh", longOptions, NULL);

        switch (c) {
            case 'p':
                if (!strcmp(optarg, "RT")) {
                    priority = -1;
                } else {
                    char *end;
                    priority = strtoul(optarg, &end, 10);
                    if (!*optarg || *end) {
                        fprintf(stderr, "Invalid priority: %s\n", optarg);
                        exit(1);
                    }
                }
                break;

            case 'c':
                pdserv_config = optarg;
                break;

            case 'i':
                pidPath = optarg;
                break;

            case 'd':
                daemonize = true;
                break;

            case 'h':
                usage(stdout);
                exit(0);

            case '?':
                usage(stderr);
                exit(1);

            default:
                break;
        }
    }
    while (c != -1);

    arg_count = argc - optind;

    if (arg_count) {
        fprintf(stderr, "%s takes no arguments!\n", base_name);
        usage(stderr);
        exit(1);
    }
}

/****************************************************************************/

/** Process main function.
 */
int main(int argc, char **argv)
{
    RT_MODEL *S;
    struct thread_task task[NUMTASKS];
    struct pdserv *pdserv = NULL;
    unsigned int dt;
    unsigned int running = 1;
    const char *err = NULL;
    size_t i;
    uint32_t exec_ns = 0, period_ns = 0;
    struct timespec start_time, last_start_time = {0, 0},
                    end_time = last_start_time, world_time;

    /* Set defaults for command-line options. */
    base_name = basename(argv[0]);

    get_options(argc, argv);

    if (!(pdserv = pdserv_create(QUOTE(MODEL), MODEL_VERSION, gettime))) {
        err = "Failed to init pdserv.";
        goto out;
    }

    if (pdserv_config) {
        pdserv_config_file(pdserv, pdserv_config);
    }

    /* Initialize model */
    S = MODEL();

    /* Create necessary pdserv tasks */
    for (i = 0; i < NUMTASKS; ++i) {
        task[i].S = S;
        task[i].tid = i + FIRST_TID;
        task[i].sample_time = rtmGetSampleTime(S, task[i].tid);
        task[i].pdtask = pdserv_create_task(pdserv, task[i].sample_time, 0);
    }

    /* Register signals and parameters */
    if ((err = rtw_capi_init(S, pdserv, task))) {
        pdserv_exit(pdserv);
        goto out;
    }

    /* Prepare process-data interface, create threads, etc. */
    pdserv_prepare(pdserv);

    /* Lock all memory forever. */
    if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
        fprintf(stderr, "mlockall() failed: %s\n", strerror(errno));
    }

    /* Set task priority. */
    {
        struct sched_param param = {};
        if (priority == -1) {
            param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        } else {
            param.sched_priority = priority;
        }
        if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
            fprintf(stderr, "Setting SCHED_FIFO"
                    " with priority %i failed: %s\n",
                    param.sched_priority, strerror(errno));
        }
    }

    /* Provoke the first stack fault before cyclic operation. */
    stack_prefault();

    if ((err = init_application(S))) {
        pdserv_exit(pdserv);
        goto out;
    }

    if (daemonize) {
        int ret;
        fprintf(stderr, "Now becoming a daemon.\n");
        ret = daemon(0, 0);
        if (ret != 0) {
            fprintf(stderr, "Failed to become daemon: %s\n", strerror(errno));
            pdserv_exit(pdserv);
            err = "daemon() failed.";
            goto out;
        }
    }

    if (pidPath[0]) {
        create_pid_file();
    }

    clock_gettime(CLOCK_MONOTONIC, &task[0].time);

#if defined(MULTITASKING)
    /* Start subtask threads */
    for (i = 1; i < NUMTASKS; ++i) {
        task[i].time = task[0].time;
        task[i].running = &running;
        task[i].err = 0;
        pthread_create(&task[i].thread, 0, run_task, &task[i]);
        /* FIXME: set subtask priorities. */
    }
#endif

    dt = task[0].sample_time * 1.0e9 + 0.5;

    /* Main thread running here */
    do {
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        clock_gettime(CLOCK_REALTIME, &world_time);

        pdserv_get_parameters(pdserv, task[0].pdtask, &task[0].time);

        err = rt_OneStepMain(S);

        pdserv_update(task[0].pdtask, &world_time);

        period_ns = DIFF_NS(last_start_time, start_time);
        exec_ns = DIFF_NS(last_start_time, end_time);
        last_start_time = start_time;
        pdserv_update_statistics(task[0].pdtask,
                exec_ns / 1e9, period_ns / 1e9, 0);

        timeradd(&task[0].time, dt);

        clock_gettime(CLOCK_MONOTONIC, &end_time);

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &task[0].time, 0);
    } while(!err && running);

    running = 0;

    for (i = 1; i < NUMTASKS; ++i) {
        pthread_join(task[i].thread, 0);
    }

    pdserv_exit(pdserv);
    MdlTerminate();

    if (pidPath[0]) {
        remove_pid_file();
    }

out:
    if (err) {
        fprintf(stderr, "Fatal error: %s\n", err);
        return 1;
    }
    else {
        return 0;
    }
}

/****************************************************************************/
