#include <stdio.h>
#include <pdserv.h>
#include <time.h>
#include <pthread.h>
#include <sys/mman.h>

#include "rtmodel.h"
#include "rtwtypes.h"
#include "rt_nonfinite.h"
#include "rt_sim.h"

#define MAX_SAFE_STACK (8 * 1024) /* The maximum stack size which is guranteed
                                     safe to access without faulting. */

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

#ifdef __cplusplus
}
#endif

static rtwCAPI_ModelMappingInfo* mmi;
static const rtwCAPI_DimensionMap* dimMap;
static const rtwCAPI_DataTypeMap* dTypeMap;
static const rtwCAPI_SampleTimeMap* sampleTimeMap;
static const uint_T* dimArray;
static void ** dataAddressMap;

const char* rt_OneStepMain(RT_MODEL *s);
const char* rt_OneStepTid(RT_MODEL *s, uint_T);

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

#undef timeradd
inline void timeradd(struct timespec *t, unsigned int dt)
{
    t->tv_nsec += dt;
    while (t->tv_nsec >= 1000000000U) {
        t->tv_nsec -= 1000000000U;
        t->tv_sec++;
    }
}

#if !defined(MULTITASKING)  /* SINGLETASKING */

#define NUMTASKS 1

/* Function: rtOneStep ========================================================
 *
 * Abstract:
 *      Perform one step of the model. This function is modeled such that
 *      it could be called from an interrupt service routine (ISR) with minor
 *      modifications.
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
} /* end rtOneStep */

#else /* MULTITASKING */

#define NUMTASKS (NUMST - FIRST_TID)

/* Function: rtOneStep ========================================================
 *
 * Abstract:
 *      Perform one step of the model. This function is modeled such that
 *      it could be called from an interrupt service routine (ISR) with minor
 *      modifications.
 *
 *      This routine is modeled for use in a multitasking environment and
 *	therefore needs to be fully re-entrant when it is called from an
 *	interrupt service routine.
 *
 * Note:
 *      Error checking is provided which will only be used if this routine
 *      is attached to an interrupt.
 *
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
        rt_SimUpdateDiscreteTaskTime(rtmGetTPtr(S), 
                                     rtmGetTimingData(S), 0);
    }

#if FIRST_TID == 1
    rt_SimUpdateDiscreteTaskTime(rtmGetTPtr(S), 
                                 rtmGetTimingData(S),1);
#endif

    return rtmGetErrorStatusFlag(S);
} /* end rtOneStepMain */

/* Function: rt_OneStepTid ====================================================
 *
 * Abstract:
 *      Perform one step of the model subrates
 *
 *      This routine is modeled for use in a multitasking environment and
 *	therefore needs to be fully re-entrant when it is called from an
 *	interrupt service routine.
 *
 * Note:
 *      Error checking is provided which will only be used if this routine
 *      is attached to an interrupt.
 *
 */
const char *
rt_OneStepTid(RT_MODEL *S, uint_T tid)
{
    MdlOutputs(tid);

    MdlUpdate(tid);

    rt_SimUpdateDiscreteTaskTime(rtmGetTPtr(S), 
                                 rtmGetTimingData(S),tid);

    return rtmGetErrorStatusFlag(S);

} /* end rtOneStepTid */

void *run_task(void *p)
{
    struct thread_task *thread = p;
    unsigned int dt = thread->sample_time * 1.0e9 + 0.5;

    do {
        thread->err = rt_OneStepTid(thread->S, thread->tid);
        pdserv_update(thread->pdtask, &thread->time);
        timeradd(&thread->time, dt);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &thread->time, 0);
    } while(!thread->err && *thread->running);

    *thread->running = 0;

    return 0;
}

#endif /* MULTITASKING */

/* Function: get_etl_data_type ===============================================
 * Abstract:
 *     return the data type as expected by EtherLab
 */
unsigned int get_etl_data_type(size_t dataTypeIdx) {
    switch (rtwCAPI_GetDataTypeSLId(dTypeMap, dataTypeIdx)) {
        case SS_DOUBLE:  return pd_double_T;
        case SS_SINGLE:  return pd_single_T;
        case SS_INT8:    return pd_sint8_T;
        case SS_UINT8:   return pd_uint8_T;
        case SS_INT16:   return pd_sint16_T;
        case SS_UINT16:  return pd_uint16_T;
        case SS_INT32:   return pd_sint32_T;
        case SS_UINT32:  return pd_uint32_T;
        case SS_BOOLEAN: return pd_boolean_T;
        default:         return 0;
    }
}

const char *register_signal(const struct thread_task *task,
        const rtwCAPI_Signals* signals, size_t idx)
{
    size_t addrMapIndex    = rtwCAPI_GetSignalAddrIdx(signals, idx);
    /* size_t sysNum = rtwCAPI_GetSignalSysNum(signals, idx); */
    const char *blockPath  = rtwCAPI_GetSignalBlockPath(signals, idx);
    const char *signalName = rtwCAPI_GetSignalName(signals, idx);
    uint_T portNumber      = rtwCAPI_GetSignalPortNumber(signals, idx);
    size_t dataTypeIndex   = rtwCAPI_GetSignalDataTypeIdx(signals, idx);
    size_t dimIndex        = rtwCAPI_GetSignalDimensionIdx(signals, idx);
    size_t sTimeIndex      = rtwCAPI_GetSignalSampleTimeIdx(signals, idx);

    const void *address =
        rtwCAPI_GetDataAddress(dataAddressMap, addrMapIndex);
    size_t dimArrayIndex = rtwCAPI_GetDimArrayIndex(dimMap, dimIndex);
    unsigned int data_type = get_etl_data_type(dataTypeIndex);
    size_t pathLen = strlen(blockPath) + strlen(signalName) + 9;
    size_t ndim = rtwCAPI_GetNumDims(dimMap, dimIndex);
    const real_T *sampleTime =
        rtwCAPI_GetSamplePeriodPtr(sampleTimeMap, sTimeIndex);
    int tid = rtwCAPI_GetSampleTimeTID(sampleTimeMap, sTimeIndex);
    uint_T decimation;

    struct pdvariable *signal;
    char *path = malloc(pathLen);
    size_t *dim = 0;

    size_t i;
    const char *err = 0;

    if (!path) {
        err = "No memory";
        goto out;
    }

    /* Check that the data type is compatable */
    if (rtwCAPI_GetDataIsComplex(dTypeMap, dataTypeIndex)) {
        err = "Cannot interact with complex data types yet.";
        goto out;
    }
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
    if (tid >= 0)
        task += tid - (tid && FIRST_TID);
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
        for (i = 0; i < ndim; ++i)
            dim[i] = dimArray[dimArrayIndex + i];
    }

#if 0
    printf("%s task[%u], decim=%u, dt=%u, %p, ndim=%u, %p\n",
            path, tid - (tid && FIRST_TID),
            decimation, data_type, address, ndim, dim);
#endif

    signal = pdserv_signal(task->pdtask, decimation,
            path, data_type, address, ndim, dim);

    if (signalName && *signalName)
        pdserv_set_alias(signal, signalName);
   
out:
    free(path);
    free(dim);
    if (err)
        printf("%s\n", err);
    return err;
}

const char *register_parameter( struct pdserv *pdserv,
        const rtwCAPI_BlockParameters* params, size_t idx)
{
    size_t addrMapIndex = rtwCAPI_GetBlockParameterAddrIdx(params, idx);
    const char *blockPath = rtwCAPI_GetBlockParameterBlockPath(params, idx);
    const char *paramName = rtwCAPI_GetBlockParameterName(params, idx);
    size_t dataTypeIndex = rtwCAPI_GetBlockParameterDataTypeIdx(params, idx);
    size_t dimIndex = rtwCAPI_GetBlockParameterDimensionIdx(params, idx);

    void *address = rtwCAPI_GetDataAddress(dataAddressMap, addrMapIndex);
    size_t dimArrayIndex = rtwCAPI_GetDimArrayIndex(dimMap, dimIndex);
    unsigned int data_type = get_etl_data_type(dataTypeIndex);
    size_t pathLen = strlen(blockPath) + strlen(paramName) + 9;
    size_t ndim = rtwCAPI_GetNumDims(dimMap, dimIndex);

    struct pdvariable *param;
    char *path = malloc(pathLen);
    size_t *dim = 0;

    size_t i;
    const char *err = 0;

    if (!path) {
        err = "No memory";
        goto out;
    }

    /* Check that the data type is compatable */
    if (rtwCAPI_GetDataIsComplex(dTypeMap, dataTypeIndex)) {
        err = "Cannot interact with complex data types yet.";
        goto out;
    }
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
        for (i = 0; i < ndim; ++i)
            dim[i] = dimArray[dimArrayIndex + i];
    }

    param = pdserv_parameter(pdserv, path, 0666,
            data_type, address, ndim, dim, 0, 0);

out:
    free(path);
    free(dim);
    if (err)
        printf("%s\n", err);
    return err;
}

/* Function: rtw_capi_init ====================================================
 *
 * Abstract:
 *      Initialize all the model variables
 *
 * Note:
 *
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
    for (i = 0; signals && i < rtwCAPI_GetNumSignals(mmi); ++i)
        register_signal(task, signals, i);

    params = rtwCAPI_GetBlockParameters(mmi);
    for (i = 0; params && i < rtwCAPI_GetNumBlockParameters(mmi); ++i)
        register_parameter(pdserv, params, i);

    return NULL;
}

/* Function: init_application ================================================
 *
 * Abstract:
 *      Execute model on a generic target such as a workstation.
 */
const char * init_application(RT_MODEL *S)
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
} /* end main */

/* Function: gettime ============================================================
 *
 * Abstract:
 *      Return the current system time. This is a callback needed by pdserv
 */
int gettime(struct timespec *time)
{
    return clock_gettime(CLOCK_REALTIME, time);
}

/****************************************************************************/

void stack_prefault(void)
{
    unsigned char dummy[MAX_SAFE_STACK];

    memset(dummy, 0, MAX_SAFE_STACK);
}

/* Function: main ============================================================
 *
 * Abstract:
 *      Execute model on a generic target such as a workstation.
 */
int main (int argc, char **argv)
{
    RT_MODEL  *S = MODEL();
    struct thread_task task[NUMTASKS];
    struct pdserv *pdserv;
    unsigned int dt;
    unsigned int running = 1;
    const char *err = NULL;

    size_t i;

    if (!(pdserv = pdserv_create(QUOTE(MODEL), MODEL_VERSION, gettime))) {
        err = "Failed to init pdserv.";
        goto out;
    }

    /* Create necessary pdserv tasks */
    for (i = 0; i < NUMTASKS; ++i) {
        task[i].S = S;
        task[i].tid = i + FIRST_TID;
        task[i].sample_time = rtmGetSampleTime(S, task[i].tid);
        task[i].pdtask = pdserv_create_task( pdserv, task[i].sample_time, 0);
    }

    /* Register signals and parameters */
    if ((err = rtw_capi_init(S, pdserv, task)))
        goto out;

    /* Prepare process-data interface, create threads, etc. */
    pdserv_prepare(pdserv);

    /* Lock all memory forever. */
    mlockall(MCL_CURRENT | MCL_FUTURE);

    /* Set task priority. */
    {
        struct sched_param param = {};
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        printf("Using priority %i\n", param.sched_priority);
        if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
            perror("sched_setscheduler failed");
        }
    }

    /* Provoke the first stack fault before cyclic operation. */
    stack_prefault();

    if ((err = init_application(S)))
        goto out;

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
        pdserv_get_parameters(pdserv, task[0].pdtask, &task[0].time);

        err = rt_OneStepMain(S);

        pdserv_update(task[0].pdtask, &task[0].time);

        timeradd(&task[0].time, dt);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &task[0].time, 0);
    } while(!err && running);

    running = 0;

    for (i = 1; i < NUMTASKS; ++i)
        pthread_join(task[i].thread, 0);

    pdserv_exit(pdserv);
    MdlTerminate();

out:
    if (err) {
        fprintf(stderr, "Fatal error: %s\n", err);
        return 1;
    }
    else
        return 0;
}
