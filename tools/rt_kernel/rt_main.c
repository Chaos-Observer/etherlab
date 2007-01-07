/******************************************************************************
 *
 *           $RCSfile: rtai_module.c,v $
 *           $Revision: 2.0 $
 *           $Author: rich $
 *           $Date: 2006/02/10 19:06:40 $
 *           $State: Exp $
 *
 *           Autor: Richard Hacker
 *
 *           (C) Copyright IgH 2005
 *           Ingenieurgemeinschaft IgH
 *           Heinz-Baecker Str. 34
 *           D-45356 Essen
 *           Tel.: +49 201/36014-0
 *           Fax.: +49 201/36014-14
 *           E-mail: info@igh-essen.com
 *
 * 	This file uses functions in rtai_wrapper.c to start the model.
 * 	This two layer concept is required because it is not (yet) possible
 * 	to compile the RTW generated C-code with kernel header files.
 *
 *           $Log: rtai_module.c,v $
 *           Revision 2.0  2006/02/10 19:06:40  rich
 *           Major revision, separating RTAI from the Model. Now there is a service
 *           called rtw_manager that runs as a separate module. RTW Models
 *           attach themselves to this manager. Now it is possible having more
 *           than one RTW Model running.
 *
 *           Revision 1.2  2006/02/04 11:07:15  rich
 *           Added report of RTP error message when RTP init fails
 *
 *           Revision 1.1  2006/01/23 14:49:53  rich
 *           Initial revision
 *
 *****************************************************************************/ 

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "Don't have config.h"
#endif

/* Kernel header files */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/delay.h>

/* RTAI headers */
#include <rtai_sched.h>
#include <rtai_sem.h>

/* Local headers */
#include "rt_kernel.h"          /* Public functions exported by manager */
#include "rt_main.h"            /* Private to kernel */

struct rt_kernel rt_kernel;

unsigned long basetick = 0;        /**< Basic tick in microseconds; 
                                    * default 1000us
                                    * All RT Models must have a tick rate 
                                    * that is an integer multiple of this. */
module_param(basetick,ulong,S_IRUGO);

/*
 *   Initialize the stack with a recognizable pattern, one that will
 *   be looked for in rt_task_stack_check() to see how much was actually
 *   used.
 */
static void rt_task_stack_init(RT_TASK * task, int pattern)
{
    int * ptr;
    /*
     *     Clobber the stack with a recognizable pattern. The bottom,
     *     task->stack_bottom, is writeable. The top, task->stack,
     *     points to some RTAI data and can't be written.
     **/
    for (ptr = (int *)task->stack_bottom; ptr < (int *)task->stack; ptr++) {
        *ptr = pattern;
    }
}

/*
 *   Check the stack and see how much was used by comparing what's left
 *   with the initialization pattern.
 **/
static int rt_task_stack_check(RT_TASK * task, int pattern)
{
    int * ptr;
    int unused = 0;
    /*
     *     Read up from the bottom and count the unused bytes.
     *       */
    for (ptr = (int *)task->stack_bottom; ptr < (int *)task->stack; ptr++) {
        if (*ptr != pattern) {
            break;
        }
        unused += sizeof(int);
    }
    return unused;
}

/** Function: rt_kernel_helper
 *
 * This separate task thread runs in parallel to the real time tasks and
 * has the following tasks:
 *   - periodically get the real world time
 *   - wake up the buddy if the real time processes have produced data for the 
 *     buddy
 */
int rt_kernel_helper(void *_unused_data)
{
    struct timeval current_time;

    do {
        /* Do the expensive operation in standard kernel time */
        do_gettimeofday(&current_time);

        /* Inform RTAI processes that there is a new time */
        rt_sem_wait(&rt_kernel.time.lock);
        rt_kernel.time.tv = current_time;
        rt_sem_signal(&rt_kernel.time.lock);
        set_bit(0,&rt_kernel.time.flag);

        /* Check the availability of data */
        rtp_data_avail_handler();

        /* Hang up for a while */
        msleep(1000/HELPER_CALL_RATE);

    } while(!kthread_should_stop());

    return 0;
}

/* Initialise the internal time keeper observer */
void init_time(void)
{
    clear_bit(0, &rt_kernel.time.flag);

    rt_kernel.time.i = 1.0;
    rt_kernel.time.base_step = rt_kernel.base_period/1.0e6;
    rt_kernel.time.u = rt_kernel.time.base_step;

    rt_kernel.time.value = 
        (double)rt_kernel.time.tv.tv_sec + rt_kernel.time.tv.tv_usec*1.0e-6;
}

/* This function gets called at the fastest sampling rate and is an 
 * observer interpolating the time between the samples of real world time
 * provided by rt_kernel_helper().
 * It is a second order feedback system:
 *
 *                            +-----+   p                          
 *                         +--> K_p +-----+                        
 *   T     +   +-----+  e  |  +-----+     |  u  +-----+  value     
 *  --------X--> ZOH +-----+              @-----> 1/s +----+--     
 *         -|  +-----+     | +-------+    |     +-----+    |       
 *          |              +-> K_i/s +----+                |       
 *          |                +-------+  i                  |       
 *          |                                              |       
 *          |                                              |       
 *          +----------------------------------------------+       
 */
void update_time(void)
{
/* Define PI controller for tracking the real world time. Arbitrarily
 * choose the controller to have a response of HELPER_CALL_RATE/50.
 * This makes for a reasonably smooth increase in time
 */
#define K_P (2*0.707*6.28*HELPER_CALL_RATE/50)
#define K_I (6.28*HELPER_CALL_RATE/50)*(6.28*HELPER_CALL_RATE/50)
    double e, p;

    /* Check whether a new time sample has arrived. This happens at a 
     * rate of HELPER_CALL_RATE.  */
    if (test_and_clear_bit(0, &rt_kernel.time.flag)) {
        /* Time error between Real World time and time observer */
        e = (double)rt_kernel.time.tv.tv_sec + 
            rt_kernel.time.tv.tv_usec*1.0e-6 - rt_kernel.time.value;

        /* Proportional part */
        p = K_P*e;

        /* Integration part */
        rt_kernel.time.i += K_I/HELPER_CALL_RATE*e;

        /* Input to the time integrator */
        rt_kernel.time.u = (p + rt_kernel.time.i)*rt_kernel.time.base_step;
    }

    /* Here is the time integrator, called once at the fastest sampling
     * rate */
    rt_sem_wait(&rt_kernel.time.lock);
    rt_kernel.time.value += rt_kernel.time.u;
    rt_sem_signal(&rt_kernel.time.lock);
}

/** Function: mdl_main_thread
 * arguments: long priv_data: Pointer to the mdl_task
 *
 * This is the fastest task of the model. It differs from mdl_sec_thread
 * in that it has to take care of parameter transfer and world time.
 */
static void mdl_main_thread(long priv_data)
{
    struct mdl_task *mdl_task = (struct mdl_task *)priv_data;
    struct model *model = mdl_task->model;
    const struct rtw_model *rtw_model = model->rtw_model;
    unsigned int overrun = 0;
    unsigned long counter = 0;
    const char *errmsg;
    cycles_t rt_start = get_cycles(), rt_end, rt_prevstart;
    unsigned int cpu_mhz;

    if (mdl_task->master) {
        /* This task is the master time keeper. Initialise the clock */
        init_time();
    }

    while (1) {
        counter++;
        rt_prevstart = rt_start;
        rt_start = get_cycles();

        /* Check for new parameters */
        if (model->new_rtP) {
                rt_sem_wait(&model->rtP_sem);
                rt_printk("Got new parameter set\n");
                memcpy(rtw_model->mdl_rtP, rtw_model->pend_rtP,
                                rtw_model->rtP_size);
                model->new_rtP = 0;
                rt_sem_signal(&model->rtP_sem);
        }

        /* Do one calculation step of the model */
        mdl_task->stats->time = rt_kernel.time.value;
        if ((errmsg = rtw_model->rt_OneStepMain(mdl_task->stats->time)))
            break;

        /* Send a copy of rtB to the buddy process */
        if (!--model->photo_sample) {
            rtp_make_photo(model);
            model->photo_sample = rtw_model->downsample;
        }

        /* Calculate and report execution statistics */
        rt_end = get_cycles();
        mdl_task->stats->time = rt_kernel.time.value;
        cpu_mhz = cpu_khz/1000;  // cpu_khz is not constant
        mdl_task->stats->exec_time = 
            ((unsigned int)(rt_end - rt_start))/cpu_mhz;
        mdl_task->stats->time_step = 
            ((unsigned int)(rt_start - rt_prevstart))/cpu_mhz;
        mdl_task->stats->overrun = overrun;

        /* Wait until next call */
        if (rt_task_wait_period()) {
            if (overrun++) {
                pr_info(".");
            } else {
                pr_info("Model overrun on main task ... "
                        "tick %lu took %luus, allowed are %luus ",
                        counter,
                        (unsigned long)(get_cycles() - 
                                        rt_start)/(cpu_khz/1000),
                        mdl_task->period);
            }

            if (overrun == rtw_model->max_overrun) {
                errmsg = "Too many overruns";
                rtw_model->set_error_msg("Abort");
                break;
            }
        } else {
            if (overrun)
                if (!--overrun)
                    pr_info("\n");
        }

        if (mdl_task->master) {
            update_time();
        }
    }
    rt_printk("Model %s main task aborted. Error message:\n\t%s\n", 
            rtw_model->modelName, errmsg);

    /* If this task is not the master, it can end here */
    if (!mdl_task->master)
        return;

    /* Master task has to continue to keep the time ticking... */
    while (1) {
        update_time();
        rt_task_wait_period();
    }
}

/** Function: mdl_sec_thread
 * arguments: long model_id: RT-Kernel Task Id for this task
 *
 * This calls secondary model threads running more slowly than the main
 * task.
 */
static void mdl_sec_thread(long priv_data)
{
    struct mdl_task *mdl_task = (struct mdl_task *)priv_data;
    struct model *model = mdl_task->model;
    unsigned int mdl_tid = mdl_task->mdl_tid;
    const struct rtw_model *rtw_model = model->rtw_model;
    unsigned int overrun = 0;
    unsigned long counter = 0;
    const char *errmsg;
    cycles_t rt_start = get_cycles(), rt_end, rt_prevstart;
    unsigned int cpu_mhz;

    while (1) {
        counter++;
        rt_prevstart = rt_start;
        rt_start = get_cycles();

        /* Do one calculation step of the model */
        mdl_task->stats->time = rt_kernel.time.value;
        if ((errmsg = rtw_model->rt_OneStepTid(mdl_tid, 
                        mdl_task->stats->time)))
            break;

        /* Calculate and report execution statistics */
        rt_end = get_cycles();
        cpu_mhz = cpu_khz/1000;  // cpu_khz is not constant
        mdl_task->stats->exec_time = 
            ((unsigned int)(rt_end - rt_start))/cpu_mhz;
        mdl_task->stats->time_step = 
            ((unsigned int)(rt_start - rt_prevstart))/cpu_mhz;
        mdl_task->stats->overrun = overrun;

        /* Wait until next call */
        if (rt_task_wait_period()) {
            pr_info("Model overrun on tid %i ... "
                    "tick %lu took %luus, allowed are %luus\n",
                    mdl_tid,
                    counter,
                    (unsigned long)(get_cycles() - rt_start)/(cpu_khz/1000),
                    mdl_task->period);
            if (++overrun == rtw_model->max_overrun) {
                errmsg = "Too many overruns";
                rtw_model->set_error_msg("Abort");
                break;
            }
        } else {
            if (overrun)
                overrun--;
        }
    }
    rt_printk("Model %s tid %u aborted. Error message:\n\t%s\n", 
            rtw_model->modelName, mdl_tid, errmsg);
}

void free_rtw_model(int model_id)
{
    struct model *model = rt_kernel.model[model_id];
    unsigned int i;


    clear_bit(model->id, &rt_kernel.loaded_models);
    rtp_fio_clear_mdl(model);

    down(&rt_kernel.lock);
    for (i = 0; i < model->rtw_model->numst; i++) {
        rt_task_suspend(&model->task[i].rtai_thread);
        printk("Unused stack memory: %i\n", 
            rt_task_stack_check(&model->task[i].rtai_thread,STACK_MAGIC));

        rt_task_delete(&model->task[i].rtai_thread);
    }

    if (!rt_kernel.loaded_models) {
        /* Last process to be removed */
        rt_kernel.base_period = 0;
        stop_rt_timer();
        pr_info("Stopped RT Timer\n");
    }

    kfree(model);

    up(&rt_kernel.lock);

    return;
}

int register_rtw_model(const struct rtw_model *rtw_model,
        size_t struct_len,
        const char *revision_str,
        struct module *owner)
{
    unsigned int i;
    unsigned int model_id;
    unsigned int multiplier;
    RTIME now;
    int err = -1;
    struct model *model;

    if ((struct_len != sizeof(struct rtw_model))
            || strcmp(revision_str, REVISION)) {
        pr_info("Error: The header file revisions and lengths do not match.\n"
                "You have probably updated and are using an old rt_kernel.\n"
                "Recompile and reinstall all parts of " PACKAGE_NAME ".\n");
        return -1;
    }

    pr_debug("Registering new RTW Model %s with rtw_manager\n", 
            rtw_model->modelName);

    /* Make sure the model name is within range */
    if (strlen(rtw_model->modelName) > MAX_MODEL_NAME_LEN-1) {
        pr_info("Error: model name exceeds %i bytes\n", MAX_MODEL_NAME_LEN-1);
        err = -E2BIG;
        goto out;
    }

    /* If the ticker is already running, check that the base rates are
     * compatible */
    if (rt_kernel.base_period && 
            rtw_model->base_period % rt_kernel.base_period) {
        pr_info("Model's base rate (%uns) is not an integer multiple of "
                "RTAI baserate %luus\n", 
                rtw_model->base_period, rt_kernel.base_period);
        err = -EINVAL;
        goto out;
    }

    /* Get some memory to manage RT model */
    model = kcalloc( 1, 
            ( sizeof(struct model) 
              + rtw_model->numst*sizeof(struct mdl_task)
              + rtw_model->numst*sizeof(struct task_stats)),
            GFP_KERNEL);
    if (!model) {
        printk("Error: Could not get memory for Real-Time Task\n");
        err = -ENOMEM;
        goto out;
    }
    model->task_stats = 
        (struct task_stats *)&model->task[rtw_model->numst];
    model->task_stats_len = rtw_model->numst*sizeof(struct task_stats);

    down(&rt_kernel.lock);

    /* Make sure there is one free slot */
    if (rt_kernel.loaded_models == ~0UL) {
        printk("Exceeded maximum number of tasks (%i)\n", MAX_MODELS);
        err = -ENOMEM;
        goto out_check_full;
    }
    model_id = ffz(rt_kernel.loaded_models);
    model->id = model_id;
    set_bit(model_id, &rt_kernel.loaded_models);
    pr_debug("Found free RTW Task %i\n", model_id);

    rt_kernel.model[model_id] = model;

    model->rtw_model = rtw_model;

    for (i = 0; i < rtw_model->numst; i++) {
        model->task[i].mdl_tid = i;
        model->task[i].model = model;
        model->task[i].stats = &model->task_stats[i];

        /* Initialise RTAI task structure */
        if ((err = rt_task_init(
                        &model->task[i].rtai_thread, /* RT_TASK 	*/
                        (i ? mdl_sec_thread 
                         : mdl_main_thread),      /* Task function 	*/
                        (long)&model->task[i], /* Private data passed to 
                                                   * task */
                        rtw_model->stack_size,   /* Stack size 	*/
                        i,                      /* priority 	*/
                        1,                      /* use fpu 		*/
                        NULL                    /* signal		*/
                        ))) {
                printk("ERROR: Could not initialize RTAI task.\n");
                goto out_task_init;
        }

        /* Fill stack with a recognisable pattern to enable stack size 
         * profiling */
        rt_task_stack_init(&model->task[i].rtai_thread, STACK_MAGIC);
    }

    /* Check that the model's rate is a multiple of rt_kernel's baserate */
    if (!rt_kernel.base_period) {
        /* Use global baserate if it has been passed as a module parameter
         * otherwise take that from the model */
        rt_kernel.base_period = basetick ? basetick : rtw_model->base_period;
        rt_kernel.tick_period = 
            start_rt_timer_ns(rt_kernel.base_period*1000);
        model->task[0].master = 1;
        pr_info("Started RT timer at a rate of %luus\n", 
                rt_kernel.base_period);
    }

    /* Now setup the external
     * communications - the character devices. This is race free (considering 
     * that the process is available to the user, i.e. live), because 
     * all the setup has been completed. 
     * Create character devices for this process */
    if ((err = rtp_fio_init_mdl(model, owner))) {
        printk("Could not initialise file io\n");
        goto out_fio_init;
    }

    now = rt_get_time();
    model->photo_sample = 1;           /* Take a photo of next calculation */
    for (i = 0; i < rtw_model->numst; i++) {
        multiplier = rtw_model->get_sample_time_multiplier(i);
        model->task[i].period = multiplier*rt_kernel.base_period;
        pr_info("RTW Model tid %i running at %luus\n", 
                i, model->task[i].period);
        if ((err = rt_task_make_periodic(
                        &model->task[i].rtai_thread, 
                        now + nano2count(1e7), //rt_kernel.tick_period,
                        multiplier*rt_kernel.tick_period
                        ))) {
                printk("ERROR: Could not start periodic RTAI rask.\n");
                goto out_make_periodic;
        }
    }

    up(&rt_kernel.lock);

    return model_id;

out_make_periodic:
    rtp_fio_clear_mdl(model);
out_fio_init:
    for (i--; i > rtw_model->numst; i--) {
        rt_task_suspend(&model->task[i].rtai_thread);
        rt_task_delete(&model->task[i].rtai_thread);
    }
out_task_init:
    clear_bit(model_id, &rt_kernel.loaded_models);
    if (!rt_kernel.loaded_models) {
        rt_kernel.base_period = 0;
        stop_rt_timer();
    }
out_check_full:
    up(&rt_kernel.lock);
    kfree(model);
out:
    return err;
}

#undef TEST_THREAD

#ifdef TEST_THREAD
RT_TASK test_thread[2];

/** Function: test_thread
 * arguments: long model_id: RT-Kernel Task Id for this task
 *
 * This is the main task that is called by RTAI.
 */
static void test_thread_func(long priv_data)
{
    unsigned long counter = 0;
    cycles_t rt_start = get_cycles(), rt_end, rt_prevstart;

    while (1) {
        rt_prevstart = rt_start;
        rt_start = get_cycles();

        rt_end = get_cycles();

        if (counter++ % 1000 == 0)
            printk("Model %li\n", priv_data);

        /* Wait until next call */
        if (rt_task_wait_period()) {
            pr_info("Model overrun ... tick %lu took %luns, model run %luns\n",
                    counter,
                    (unsigned long)((get_cycles() - rt_start)*1000)/(cpu_khz/1000),
                    (unsigned long)((rt_end - rt_start)*1000)/(cpu_khz/1000));
                break;
        }
    }
    rt_printk("Max overrun reached. Aborting...\n");
}

void free_test_model(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        rt_task_suspend(&test_thread[i]);
        rt_task_delete(&test_thread[i]);
    }
    stop_rt_timer();
    return;
}

int register_test_model(void)
{
    RTIME tick_period, now;
    int err, i;

    pr_debug("Registering new TEST Model with rtw_manager\n");

#if 1
    rt_set_periodic_mode();
#else
    rt_set_oneshot_mode();
#endif

    tick_period = start_rt_timer_ns(1e6);
    pr_info("Started RT timer at a rate of %uus\n", (unsigned)tick_period);

    for (i = 0; i < 3; i++) {
        if ((err = rt_task_init(
                        &test_thread[i], /* RT_TASK 	*/
                        test_thread_func,             /* Task function 	*/
                        (long)i, /* Private data passed to * task */
                        2000,   /* Stack size 	*/
                        i,                      /* priority 	*/
                        1,                      /* use fpu 		*/
                        NULL                    /* signal		*/
                        ))) {
                printk("ERROR: Could not initialize RTAI task.\n");
                goto out_test_task_init;
        }

        now = rt_get_time();
        if ((err = rt_task_make_periodic(
                        &test_thread[i],
                        now + tick_period,
                        tick_period*(i+1)
                        ))) {
                printk("ERROR: Could not start periodic RTAI rask.\n");
                goto out_test_make_periodic;
        }
    }


    return 0;

out_test_make_periodic:
    for (i--; i >= 0; i--) {
        rt_task_suspend(&test_thread[i]);
        rt_task_delete(&test_thread[i]);
    }
out_test_task_init:
    return -1;
}
#endif //TEST_THREAD

void __exit mod_cleanup(void)
{
#ifdef TEST_THREAD
    free_test_model();
    return;
#endif //TEST_THREAD

    rtp_fio_clear();

    /* Stop world time task */
    kthread_stop(rt_kernel.helper_thread);

    pr_info("RT-Kernel stopped\n");
    return;
}

int __init mod_init(void)
{
    int err = -1;

    pr_info("Starting RT-Kernel " PACKAGE_VERSION "\n");

#ifdef TEST_THREAD
    return register_test_model();
#endif //TEST_THREAD

    /* First start a separate thread to fetch world time.
     * Do this first so that the model can initialise to the correct
     * time */
    rt_sem_init(&rt_kernel.time.lock,1);

    /* Initialise management variables */
    init_MUTEX(&rt_kernel.lock);
    init_MUTEX(&rt_kernel.file_lock);
    init_waitqueue_head(&rt_kernel.event_q);
    rt_kernel.loaded_models = 0;

    /* Initialise RTAI */
    rt_set_periodic_mode();

    /* Initialise file io to communicate with buddy */
    if ((err = rtp_fio_init())) {
        goto out_fio_init;
    }

    /* The only thing to do here is start a thread to fetch world time */
    rt_kernel.helper_thread = 
        kthread_run(rt_kernel_helper, NULL, "rt_kernel_helper");
    if (rt_kernel.helper_thread == ERR_PTR(-ENOMEM)) {
        pr_info("Could not start timer thread - Out of memory\n");
        goto out_start_thread;
    }

    /* Now the module is ready to accept RT models, that register themselves
     * here by calling register_rtw_model() */

    pr_info("Successfully started RT-Kernel\n");

    return 0;

    kthread_stop(rt_kernel.helper_thread);
out_start_thread:
    rtp_fio_clear();
out_fio_init:
    return err;
}


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("RT-Kernel for " PACKAGE_NAME);
MODULE_AUTHOR("Richard Hacker");
MODULE_VERSION(PACKAGE_VERSION);

EXPORT_SYMBOL_GPL(register_rtw_model);
EXPORT_SYMBOL_GPL(free_rtw_model);

module_init(mod_init);
module_exit(mod_cleanup);
