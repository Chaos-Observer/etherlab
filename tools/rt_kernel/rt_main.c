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

/* RTAI headers */
#include <rtai_sched.h>
#include <rtai_sem.h>

/* Local headers */
#include "rt_kernel.h"          /* Public functions exported by manager */
#include "rt_main.h"            /* Private to kernel */
#include "fio_ioctl.h"          /* MAX_MODEL_NAME_LEN */

struct rt_manager rt_manager;

unsigned long basetick = 0;        /**< Basic tick in microseconds; 
                                    * default 1000us
                                    * All RT Models must have a tick rate 
                                    * that is an integer multiple of this. */
module_param(basetick,ulong,S_IRUGO);

/**
 * Structure to pass world time (UTC) to Real-Time Tasks
 *
 * The current time (struct timeval) is placed in tv periodically. flag is then
 * set to all ones.  Each individual Real-Time Task then has the opportunity 
 * to read this value if its bit is set in flag, resetting this bit at 
 * the same time to mark that it was read. This eliminates the need to compare
 * the whole tv structure to see whether the time changed.
 */
static struct {
    struct timeval tv;          /** Current world time */
    unsigned long flag;         /** Every process can mark whether it read the
                                  * current time by using a bit out of this
                                  * register */

    /* Private data for the task */
    SEM time_sem;               /* Protection for this struct */
    struct timer_list task;
    unsigned int run;           /* True if this task is running */
    unsigned long period;       /* jiffy time delay between calls */

    unsigned int seq_count;

    wait_queue_head_t time_q;
} get_time = {
    .period = HZ/10,
};

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

/** Function: get_time_task
 * Arguments: data - private data for this task. Not needed here
 *
 * This separate task fetches the current World Time as known by the kernel
 * and passes it on to the model.
 */
static void get_time_task(unsigned long _unused_data)
{
    struct timeval current_time;

    /* Do the expensive operation in standard kernel time */
    do_gettimeofday(&current_time);

    /* Inform RTAI processes that there is a new time */
    rt_sem_wait(&get_time.time_sem);
    get_time.tv = current_time;
    get_time.flag = ~0UL;
    rt_sem_signal(&get_time.time_sem);

    get_time.seq_count++;
    wake_up_interruptible(&get_time.time_q);

    /* Put myself back onto the queue */
    get_time.task.expires += get_time.period;
    //pr_info("get_time\n");

    if (get_time.run)
        add_timer(&get_time.task);

    return;
}

/** 
 * Return the time placed in get_time.tv as a float. A semaphore guarantees
 * a valid read.
 */
inline double get_float_time(void) 
{
    double time;

    rt_sem_wait(&get_time.time_sem);
    time = (double)get_time.tv.tv_sec + get_time.tv.tv_usec*1.0e-6;
    rt_sem_signal(&get_time.time_sem);

    return time;
}

/** Function: mdl_main_thread
 * arguments: long model_id: RTW-Manager Task Id for this task
 *
 * This is the fastest thread of the model. It differs from mdl_sec_thread
 * in that it has to take care of parameter transfer and world time.
 */
static void mdl_main_thread(long priv_data)
{
    struct rt_task *rt_task = (struct rt_task *)priv_data;
    struct rt_mdl *rt_mdl = rt_task->rt_mdl;
    struct rtw_model *rtw_model = rt_mdl->rtw_model;
    unsigned int overrun = 0;
    unsigned long counter = 0;
    const char *errmsg;
    cycles_t rt_start = get_cycles(), rt_end, rt_prevstart;

    /* Set the world time for the first time. There is a bit of a race
     * condition here, since hopefully get_time has not been called
     * in the meantime */
    if (test_and_clear_bit(rt_mdl->model_id, &get_time.flag)) {
        rtw_model->init_time(get_float_time());
    } else {
        rt_printk("Initialise the clock first!\n");
        return;
    }

    while (1) {
        counter++;
        rt_prevstart = rt_start;
        rt_start = get_cycles();

        /* Check for new parameters */
        if (rtw_model->new_rtP) {
                rt_sem_wait(&rt_mdl->io_rtP_sem);
                rt_printk("Got new parameter set\n");
                memcpy(rtw_model->mdl_rtP, rtw_model->pend_rtP,
                                rtw_model->rtP_size);
                rtw_model->new_rtP = 0;
                rt_sem_signal(&rt_mdl->io_rtP_sem);
        }

        /* Check whether a new timestamp arrived */
        if ( test_and_clear_bit(rt_mdl->model_id, &get_time.flag)) {
            rtw_model->set_time(get_float_time());
        }

        /* Do one calculation step of the model */
        if ((errmsg = rtw_model->rt_OneStepMain()))
            break;

        /* Send a copy of rtB to the buddy process */
        if (rtw_model->take_photo()) {
            rtp_make_photo(rt_mdl);
        }

        /* Calculate and report execution statistics */
        rt_end = get_cycles();
        rtw_model->exec_stats(
                ((double)(rt_end-rt_start))/cpu_khz/1000,
                ((double)(rt_start-rt_prevstart))/cpu_khz/1000,
                overrun
                );

        /* Wait until next call */
        if (rt_task_wait_period()) {
            pr_info("Model overrun on main thread ... "
                    "tick %lu took %luus, allowed are %luus\n",
                    counter,
                    (unsigned long)(get_cycles() - rt_prevstart)/(cpu_khz/1000),
                    rt_task->period);
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
    rt_printk("Model %s main thread aborted. Error message:\n\t%s\n", 
            rtw_model->modelName, errmsg);
}

/** Function: mdl_sec_thread
 * arguments: long model_id: RTW-Manager Task Id for this task
 *
 * This calls secondary model threads running more slowly than the main
 * thread.
 */
static void mdl_sec_thread(long priv_data)
{
    struct rt_task *rt_task = (struct rt_task *)priv_data;
    struct rt_mdl *rt_mdl = rt_task->rt_mdl;
    unsigned int mdl_tid = rt_task->mdl_tid;
    struct rtw_model *rtw_model = rt_mdl->rtw_model;
    unsigned int overrun = 0;
    unsigned long counter = 0;
    const char *errmsg;
    cycles_t rt_start = get_cycles(), rt_end, rt_prevstart;

    while (1) {
        counter++;
        rt_prevstart = rt_start;
        rt_start = get_cycles();

        /* Do one calculation step of the model */
        if ((errmsg = rtw_model->rt_OneStepTid(mdl_tid)))
            break;

        /* Calculate and report execution statistics */
        rt_end = get_cycles();
/*
 * FIXME: make exec_stats for subtasks
        rtw_model->exec_stats(
                mdl_tid,
                ((double)(rt_end-rt_start))/cpu_khz/1000,
                ((double)(rt_start-rt_prevstart))/cpu_khz/1000,
                overrun
                );
*/


        /* Wait until next call */
        if (rt_task_wait_period()) {
            pr_info("Model overrun on tid %i ... "
                    "tick %lu took %luus, allowed are %luus\n",
                    mdl_tid,
                    counter,
                    (unsigned long)(get_cycles() - rt_prevstart)/(cpu_khz/1000),
                    rt_task->period);
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
    struct rt_mdl *rt_mdl = rt_manager.rt_mdl[model_id];
    unsigned int i;

    down(&rt_manager.sem);
    if (!test_and_set_bit(rt_mdl->model_id, &rt_manager.stopped_models)) {
        for (i = 0; i < rt_mdl->rtw_model->numst; i++) {
            rt_task_suspend(&rt_mdl->thread[i].rtai_thread);
            printk("Unused stack memory: %i\n", 
                rt_task_stack_check(&rt_mdl->thread[i].rtai_thread,STACK_MAGIC));

            rt_task_delete(&rt_mdl->thread[i].rtai_thread);
        }

        if (!(rt_manager.loaded_models ^ rt_manager.stopped_models)) {
            /* Last process to be removed */
            rt_manager.base_period = 0;
            stop_rt_timer();
            pr_info("Stopped RT Timer\n");
        }
        rtp_fio_clear_mdl(rt_mdl);
    }

    /* If the buddy is alive, give it a chance to clean up */
    if (rt_mdl->buddy_there && rt_mdl->wp) {
        rt_manager.notify_buddy = 1;
        
        up(&rt_manager.sem);

        wake_up_interruptible(&rt_manager.event_q);

        /* I hope it's ok to use data_q here. Signalling over this queue
         * was stopped above */
        do {
        } while (wait_event_interruptible(rt_mdl->data_q, 
                    !(rt_mdl->buddy_there && rt_mdl->wp)));

        down(&rt_manager.sem);
    }

    clear_bit(rt_mdl->model_id, &rt_manager.loaded_models);
    clear_bit(rt_mdl->model_id, &rt_manager.stopped_models);
    kfree(rt_mdl);
    up(&rt_manager.sem);

    return;
}

int register_rtw_model(struct rtw_model *rtw_model,
        size_t struct_len,
        char *revision_str)
{
    unsigned int i;
    unsigned int model_id;
    unsigned int multiplier;
    RTIME now;
    int err = -1;
    struct rt_mdl *rt_mdl;
    unsigned int get_time_seq;

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
    if (rt_manager.base_period && 
            rtw_model->base_period % rt_manager.base_period) {
        pr_info("Model's base rate (%uns) is not an integer multiple of "
                "RTAI baserate %luus\n", 
                rtw_model->base_period, rt_manager.base_period);
        err = -EINVAL;
        goto out;
    }

    /* Get some memory to manage RT model */
    rt_mdl = kmalloc(
            sizeof(struct rt_mdl) + rtw_model->numst*sizeof(struct rt_task), 
            GFP_KERNEL);
    if (!rt_mdl) {
        printk("Error: Could not get memory for Real-Time Task\n");
        err = -ENOMEM;
        goto out;
    }

    down(&rt_manager.sem);

    /* Make sure there is one free slot */
    if (rt_manager.loaded_models == ~0UL) {
        printk("Exceeded maximum number of tasks (%i)\n", MAX_MODELS);
        err = -ENOMEM;
        goto out_check_full;
    }
    model_id = ffz(rt_manager.loaded_models);
    rt_mdl->model_id = model_id;
    set_bit(model_id, &rt_manager.loaded_models);
    rt_manager.notify_buddy = 1;
    pr_debug("Found free RTW Task %i\n", model_id);

    rt_manager.rt_mdl[model_id] = rt_mdl;

    rt_mdl->rtw_model = rtw_model;

    /* Before firing off the Real-Time Task, first setup the external
     * communications - the character devices. This is race free (considering 
     * that the process is available to the user, i.e. live), because 
     * all the setup has been completed. 
     * Create character devices for this process */
    if ((err = rtp_fio_init_mdl(rt_mdl, rtw_model))) {
        printk("Could not initialise file io\n");
        goto out_fio_init;
    }

    for (i = 0; i < rtw_model->numst; i++) {
        rt_mdl->thread[i].mdl_tid = i;
        rt_mdl->thread[i].rt_mdl = rt_mdl;

        /* Initialise RTAI task structure */
        if ((err = rt_task_init(
                        &rt_mdl->thread[i].rtai_thread, /* RT_TASK 	*/
                        (i ? mdl_sec_thread 
                         : mdl_main_thread),      /* Task function 	*/
                        (long)&rt_mdl->thread[i], /* Private data passed to 
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
        rt_task_stack_init(&rt_mdl->thread[i].rtai_thread, STACK_MAGIC);
    }

    /* Check that the model's rate is a multiple of rt_manager's baserate */
    if (!rt_manager.base_period) {
        /* Use global baserate if it has been passed as a module parameter
         * otherwise take that from the model */
        rt_manager.base_period = basetick ? basetick : rtw_model->base_period;
        rt_manager.tick_period = 
            start_rt_timer_ns(rt_manager.base_period*1000);
        pr_info("Started RT timer at a rate of %luus\n", 
                rt_manager.base_period);
    }

    
    /* Syncronize with get_time task. This is important so that the RT-Task
     * starts with a very recent time sample */
    get_time_seq = get_time.seq_count;
    if ((err = wait_event_interruptible(get_time.time_q, 
                    get_time_seq != get_time.seq_count))) {
        pr_info("Interrupted while waiting for clock tick. Try again\n");
        goto out_sleep;
    }
    rt_sem_init(&rt_mdl->mdl_lock,1); /* Unlock model */

    now = rt_get_time();
    for (i = 0; i < rtw_model->numst; i++) {
        multiplier = rtw_model->get_sample_time_multiplier(i);
        rt_mdl->thread[i].period = multiplier*rt_manager.base_period;
        pr_info("RTW Model tid %i running at %luus\n", 
                i, rt_mdl->thread[i].period);
        if ((err = rt_task_make_periodic(
                        &rt_mdl->thread[i].rtai_thread, 
                        now + nano2count(1e6), //rt_manager.tick_period,
                        multiplier*rt_manager.tick_period
                        ))) {
                printk("ERROR: Could not start periodic RTAI rask.\n");
                goto out_make_periodic;
        }
    }


    up(&rt_manager.sem);

    wake_up_interruptible(&rt_manager.event_q);

    return model_id;

out_make_periodic:
    for (i--; i > rtw_model->numst; i--) {
        rt_task_suspend(&rt_mdl->thread[i].rtai_thread);
        rt_task_delete(&rt_mdl->thread[i].rtai_thread);
    }
out_sleep:
out_task_init:
    rtp_fio_clear_mdl(rt_mdl);
out_fio_init:
    clear_bit(model_id, &rt_manager.loaded_models);
out_check_full:
    up(&rt_manager.sem);
    kfree(rt_mdl);
out:
    return err;
}

#undef TEST_THREAD

#ifdef TEST_THREAD
RT_TASK test_thread[2];

/** Function: test_thread
 * arguments: long model_id: RTW-Manager Task Id for this task
 *
 * This is the main thread that is called by RTAI.
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
    if (get_time.run) {
        get_time.run = 0;
        del_timer_sync(&get_time.task);
    }
    pr_info("RTW-Manager stopped\n");
    return;
}

int __init mod_init(void)
{
    int err = -1;
#ifdef TEST_THREAD
    return register_test_model();
#endif //TEST_THREAD

    /* The only thing to do here is start a thread to fetch world time */

    /* First start a separate thread to fetch world time.
     * Do this first so that the model can initialise to the correct
     * time */
    init_waitqueue_head(&get_time.time_q);
    rt_sem_init(&get_time.time_sem,1);
    init_timer(&get_time.task);
    get_time.task.expires = jiffies;
    get_time.task.function = get_time_task;
    get_time.run = 1;
    get_time_task(0); /* Kickstart it. From now on it restarts itself. */

    /* Initialise management variables */
    init_MUTEX(&rt_manager.sem);
    init_MUTEX(&rt_manager.file_lock);
    init_waitqueue_head(&rt_manager.event_q);
    rt_manager.loaded_models = 0;
    rt_manager.stopped_models = 0;
    rt_manager.notify_buddy = 0;

    /* Initialise RTAI */
    rt_set_periodic_mode();

    /* Initialise file io to communicate with buddy */
    if ((err = rtp_fio_init())) {
        goto out_fio_init;
    }

    /* Now the module is ready to accept RT models, that register themselves
     * here by calling register_rtw_model() */

    pr_info("Successfully started RTW-Manager\n");

    return 0;

out_fio_init:
    get_time.run = 0;
    del_timer_sync(&get_time.task);
    return err;
}


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Real Time Workshop for RTAI module");
MODULE_AUTHOR("Richard Hacker");

EXPORT_SYMBOL_GPL(register_rtw_model);
EXPORT_SYMBOL_GPL(free_rtw_model);

module_init(mod_init);
module_exit(mod_cleanup);
