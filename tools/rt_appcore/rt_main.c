/******************************************************************************
 *
 * $Id$
 *
 * Here is the main file for the RT-AppCore. It prepares the environment in
 * which Real-Time applications can run in.
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
 * Original copyright notice:
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
#include "rt_appcore.h"          /* Public functions exported by manager */
#include "rt_main.h"            /* Private to AppCore. */
#include "rtcom_chardev.h"

struct rt_appcore rt_appcore;

unsigned long basetick = 0;        /**< Basic tick in microseconds; 
                                    * default 1000us
									* All RT Applications must have a tick
									* rate that is an integer multiple of
									* this. */
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

/** Function: rt_appcore_helper
 *
 * This separate task thread runs in parallel to the real time tasks and
 * has the following tasks:
 *   - periodically get the real world time
 *   - wake up the buddy if the real time processes have produced data for the 
 *     buddy
 */
int rt_appcore_helper(void *_unused_data)
{
    struct timeval current_time;

    do {
        /* Do the expensive operation in standard kernel time */
        do_gettimeofday(&current_time);

        /* Inform RTAI processes that there is a new time */
        rt_sem_wait(&rt_appcore.time.lock);
        rt_appcore.time.tv = current_time;
        rt_sem_signal(&rt_appcore.time.lock);
        set_bit(0,&rt_appcore.time.flag);

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
    clear_bit(0, &rt_appcore.time.flag);

    rt_appcore.time.i = 1.0;
    rt_appcore.time.base_step = rt_appcore.base_period/1.0e6;
    rt_appcore.time.u = rt_appcore.time.base_step;

    rt_appcore.time.value = 
        (double)rt_appcore.time.tv.tv_sec + rt_appcore.time.tv.tv_usec*1.0e-6;
}

/* This function gets called at the fastest sampling rate and is an 
 * observer interpolating the time between the samples of real world time
 * provided by rt_appcore_helper().
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

    /* Check whether a new time sample has arrived. This happens at a 
     * rate of HELPER_CALL_RATE.  */
    if (test_and_clear_bit(0, &rt_appcore.time.flag)) {
        /* Time error between Real World time and time observer */
        double e, p;

#if 0
        struct timeval e_tv;
        e_tv.tv_sec = rt_appcore.time.tv.tv_sec - rt_appcore.time.tv_sec;
        if (rt_appcore.time.tc.tv_usec < rt_appcore.time.tv_usec) {
            e_tv.tv_sec--;
            e_tv.tv_usec = 
                rt_appcore.time.tv.tv_usec + 1000000 - rt_appcore.time.tv_usec;
        }
        else {
            e_tv.tv_usec = 
                rt_appcore.time.tv.tv_usec - rt_appcore.time.tv_usec;
        }

        e = e_tv.tv_sec + 1.0e-6 * e_tv.tv_usec*1.0e-6;
#else

        e = (double)rt_appcore.time.tv.tv_sec + 
            rt_appcore.time.tv.tv_usec*1.0e-6 - rt_appcore.time.value;
#endif

        /* Proportional part */
        p = K_P*e;

        /* Integration part */
        rt_appcore.time.i += K_I/HELPER_CALL_RATE*e;

        /* Input to the time integrator */
        rt_appcore.time.u = (p + rt_appcore.time.i)*rt_appcore.time.base_step;
    }

    /* Here is the time integrator, called once at the fastest sampling
     * rate */
    rt_sem_wait(&rt_appcore.time.lock);
    rt_appcore.time.value += rt_appcore.time.u;
    rt_appcore.time.tv.tv_sec = rt_appcore.time.value;
    rt_appcore.time.tv.tv_usec = 
        1.0e6 * (rt_appcore.time.value - rt_appcore.time.tv.tv_sec);
    rt_sem_signal(&rt_appcore.time.lock);
}

/** Function: app_main_thread
 * arguments: long priv_data: Pointer to the rt_task
 *
 * This is the fastest task of the app. It differs from app_sec_thread
 * in that it has to take care of parameter transfer and world time.
 */
static void app_main_thread(long priv_data)
{
    struct rt_task *rt_task = (struct rt_task *)priv_data;
    struct app *app = rt_task->app;
    const struct rt_app *rt_app = app->rt_app;
    unsigned int overrun = 0;
    unsigned long counter = 0;
    const char *errmsg;
    cycles_t rt_start = get_cycles(), rt_end, rt_prevstart;
    unsigned int cpu_mhz;

    if (rt_task->master) {
        /* This task is the master time keeper. Initialise the clock */
        init_time();
    }

    while (1) {
        counter++;
        rt_prevstart = rt_start;
        rt_start = get_cycles();

        /* Check for new parameters */
        if (app->new_rtP) {
                rt_sem_wait(&app->rtP_sem);
                pr_debug("Got new parameter set\n");
                memcpy(rt_app->app_rtP, rt_app->pend_rtP,
                                rt_app->rtP_size);
                app->new_rtP = 0;
                rt_sem_signal(&app->rtP_sem);
        }

        /* Do one calculation step of the app */
        rt_sem_wait(&rt_appcore.time.lock);
        rt_task->stats->time = rt_appcore.time.tv;
        rt_sem_signal(&rt_appcore.time.lock);
        if ((errmsg = rt_app->rt_OneStepMain()))
            break;

        /* Send a copy of rtB to the buddy process */
        if (!--app->photo_sample) {
            rtp_make_photo(app);
            app->photo_sample = rt_app->decimation;
        }

        /* Calculate and report execution statistics */
        rt_end = get_cycles();
        cpu_mhz = cpu_khz/1000;
        rt_task->stats->exec_time = 
            ((unsigned int)(rt_end - rt_start))/cpu_mhz;
        rt_task->stats->time_step = 
            ((unsigned int)(rt_start - rt_prevstart))/cpu_mhz;
        rt_task->stats->overrun = overrun;

        /* Wait until next call */
#ifdef RTE_TMROVRN
        if (RTE_TMROVRN == rt_task_wait_period()) {
#else
        if (rt_task_wait_period()) {
#endif
            if (overrun++) {
                printk(".");
            } else {
                pr_info(//"Application overrun on main task ... "
                        "tick %lu took %lu(%lu)us, allowed are %uus %llu %llu",
                        counter,
                        (unsigned long)(get_cycles() - rt_prevstart)/cpu_mhz,
                        (unsigned long)(get_cycles() - rt_start)/cpu_mhz,
                        rt_app->task_period[0],
                        get_cycles(), rt_start
                        );
            }

            if (overrun == rt_app->max_overrun) {
                printk("\n");
                errmsg = "Too many overruns";
                rt_app->set_error_msg("Abort");
                break;
            }
        } else {
            if (overrun)
                if (!--overrun)
                    printk("\n");
        }

        if (rt_task->master) {
            update_time();
        }
    }
    rt_printk("Application %s main task aborted. Error message:\n\t%s\n", 
            rt_app->name, errmsg);

    /* If this task is not the master, it can end here */
    if (!rt_task->master)
        return;

    /* Master task has to continue to keep the time ticking... */
    while (1) {
        update_time();
        rt_task_wait_period();
    }
}

/** Function: app_sec_thread
 * arguments: long app_id: RT-AppCore Task Id for this task
 *
 * This calls secondary app threads running more slowly than the main
 * task.
 */
static void app_sec_thread(long priv_data)
{
    struct rt_task *rt_task = (struct rt_task *)priv_data;
    struct app *app = rt_task->app;
    unsigned int app_tid = rt_task->app_tid;
    const struct rt_app *rt_app = app->rt_app;
    unsigned int overrun = 0;
    unsigned long counter = 0;
    const char *errmsg;
    cycles_t rt_start = get_cycles(), rt_end, rt_prevstart;
    unsigned int cpu_mhz;

    while (1) {
        counter++;
        rt_prevstart = rt_start;
        rt_start = get_cycles();

        /* Do one calculation step of the app */
        rt_sem_wait(&rt_appcore.time.lock);
        rt_task->stats->time = rt_appcore.time.tv;
        rt_sem_signal(&rt_appcore.time.lock);
        if ((errmsg = rt_app->rt_OneStepTid(app_tid))) 
            break;

        /* Calculate and report execution statistics */
        rt_end = get_cycles();
        cpu_mhz = cpu_khz/1000;  // cpu_khz is not constant
        rt_task->stats->exec_time = 
            ((unsigned int)(rt_end - rt_start))/cpu_mhz;
        rt_task->stats->time_step = 
            ((unsigned int)(rt_start - rt_prevstart))/cpu_mhz;
        rt_task->stats->overrun = overrun;

        /* Wait until next call */
        if (rt_task_wait_period()) {
            pr_info("Task overrun on tid %i ... "
                    "tick %lu took %luus, allowed are %uus\n",
                    app_tid,
                    counter,
                    (unsigned long)(get_cycles() - rt_start)/(cpu_khz/1000),
                    rt_app->task_period[app_tid]);
            if (++overrun == rt_app->max_overrun) {
                errmsg = "Too many overruns";
                rt_app->set_error_msg("Abort");
                break;
            }
        } else {
            if (overrun)
                overrun--;
        }
    }
    rt_printk("Application %s tid %u aborted. Error message:\n\t%s\n", 
            rt_app->name, app_tid, errmsg);
}

void stop_rt_app(int app_id)
{
    struct app *app = rt_appcore.application[app_id];
    unsigned int i;

    down(&rt_appcore.lock);

    for (i = 0; i < app->rt_app->num_tasks; i++) {
        rt_task_suspend(&app->task[i].rtai_thread);
        printk("Unused stack memory: %i\n", 
            rt_task_stack_check(&app->task[i].rtai_thread,STACK_MAGIC));

        rt_task_delete(&app->task[i].rtai_thread);
    }

    clear_bit(app_id, &rt_appcore.loaded_apps);
    rtp_fio_clear_app(app);
    rtcom_del_app(app);

    if (!rt_appcore.loaded_apps) {
        /* Last process to be removed */
        rt_appcore.base_period = 0;
        stop_rt_timer();
        pr_info("Stopped RT Timer\n");
    }

    kfree(app);

    up(&rt_appcore.lock);

    return;
}

int start_rt_app(const struct rt_app *rt_app,
        size_t struct_len,
        const char *revision_str,
        struct module *owner)
{
    unsigned int tid;
    unsigned int app_id;
    unsigned int decimation;
    RTIME now;
    int err = -1;
    struct app *app = NULL;

    if (strcmp(revision_str, REVISION)) {
        pr_info("Error: The header file revisions do not match.\n"
                "Application has: %s\n"
                "Expected: " REVISION "\n"
                "You have probably updated and are using an old RT-AppCore.\n"
                "Recompile and reinstall all parts of " PACKAGE_NAME ".\n",
                revision_str);
        return -1;
    }

    if (struct_len != sizeof(struct rt_app)) {
        pr_info("Error: The header file lengths do not match.\n"
                "Application has: %i\n"
                "Expected: %i\n"
                "You have probably updated and are using an old RT-AppCore.\n"
                "Recompile and reinstall all parts of " PACKAGE_NAME ".\n",
                struct_len, sizeof(struct rt_app));
        return -1;
    }

    pr_debug("Registering new application %s with rtw_manager\n", 
            rt_app->name);

    /* Make sure the app name is within range */
    if (strlen(rt_app->name) > MAX_APP_NAME_LEN-1) {
        pr_info("Error: app name exceeds %i bytes\n", MAX_APP_NAME_LEN-1);
        err = -E2BIG;
        goto out;
    }

    /* If the ticker is already running, check that the base rates are
     * compatible */
    if (rt_appcore.base_period && 
            rt_app->task_period[0] % rt_appcore.base_period) {
        pr_info("Application's base rate (%uns) is not an integer multiple of "
                "RTAI baserate %luus\n", 
                rt_app->task_period[0], rt_appcore.base_period);
        err = -EINVAL;
        goto out;
    }

    /* Get some memory to manage RT app */
    app = kcalloc( 1, (void*)&app->task[rt_app->num_tasks] - (void*)app,
            GFP_KERNEL);
    if (!app) {
        printk("Error: Could not get memory for Real-Time Task\n");
        err = -ENOMEM;
        goto out;
    }
    pr_info("Malloc'ed struct app *(%p) for rt_app *(%p)\n",
            app, rt_app);

    down(&rt_appcore.lock);

    /* Make sure there is one free slot */
    if (rt_appcore.loaded_apps == ~0UL) {
        printk("Exceeded maximum number of tasks (%i)\n", MAX_APPS);
        err = -ENOMEM;
        goto out_check_full;
    }
    app_id = ffz(rt_appcore.loaded_apps);
    app->id = app_id;
    set_bit(app_id, &rt_appcore.loaded_apps);
    pr_debug("Found free RTW Task %i\n", app_id);

    rt_appcore.application[app_id] = app;

    app->rt_app = rt_app;
    app->task_stats_len = rt_app->num_tasks * sizeof(struct task_stats);

    for (tid = 0; tid < rt_app->num_tasks; tid++) {
        struct rt_task *task = &app->task[tid];
        task->app_tid = tid;
        task->app = app;
        task->stats = &rt_app->task_stats[tid];

        /* Initialise RTAI task structure */
        if ((err = rt_task_init(
                        &task->rtai_thread, /* RT_TASK 	*/
                        (tid ? app_sec_thread 
                         : app_main_thread),      /* Task function 	*/
                        (long)task, /* Private data passed to 
                                                   * task */
                        rt_app->stack_size,   /* Stack size 	*/
                        tid,                      /* priority 	*/
                        1,                      /* use fpu 		*/
                        NULL                    /* signal		*/
                        ))) {
                printk("ERROR: Could not initialize RTAI task.\n");
                goto out_task_init;
        }

        /* Fill stack with a recognisable pattern to enable stack size 
         * profiling */
        rt_task_stack_init(&task->rtai_thread, STACK_MAGIC);
    }

    /* Check that the app's rate is a multiple of rt_appcore's baserate */
    if (!rt_appcore.base_period) {
        /* Use global baserate if it has been passed as a module parameter
         * otherwise take that from the app */
        rt_appcore.base_period = 
            basetick ? basetick : rt_app->task_period[0];
        rt_appcore.tick_period = 
            start_rt_timer_ns(rt_appcore.base_period*1000);
        app->task[0].master = 1;
        pr_info("Started RT timer at a rate of %luus\n", 
                rt_appcore.base_period);
    } else {
        if (rt_app->task_period[0] < rt_appcore.base_period) {
            printk("ERROR: Period of app faster than RT-AppCore's base "
                    "period. Load fastest app first.\n");
            goto out_incompatible_ticks;
        } 

        if (rt_app->task_period[0] % rt_appcore.base_period) {
            printk("ERROR: Period of app not an integral multiple "
                    "of RT-AppCore's base period.\n");
            goto out_incompatible_ticks;
        } 
    }

    /* Now setup the external
     * communications - the character devices. This is race free (considering 
     * that the process is available to the user, i.e. live), because 
     * all the setup has been completed. 
     * Create character devices for this process */
    if ((err = rtp_fio_init_app(app, owner))) {
        printk("Could not initialise file io\n");
        goto out_fio_init;
    }
    if ((err = rtcom_new_app(app))) {
        printk("Could not initialise rtcom file io\n");
        goto out_rtcom_init_fail;
    }

    now = rt_get_time();
    app->photo_sample = 1;           /* Take a photo of next calculation */
    for (tid = 0; tid < rt_app->num_tasks; tid++) {
        pr_info("Application tid %i running at %uus\n", 
                tid, rt_app->task_period[tid]);
        decimation = rt_app->task_period[tid] / rt_app->task_period[0];
        if ((err = rt_task_make_periodic(
                        &app->task[tid].rtai_thread, 
                        now + nano2count(1e7), //rt_appcore.tick_period,
                        decimation*rt_appcore.tick_period
                        ))) {
                printk("ERROR: Could not start periodic RTAI rask.\n");
                goto out_make_periodic;
        }
    }

    up(&rt_appcore.lock);

    return app_id;

out_make_periodic:
    rtcom_del_app(app);
out_rtcom_init_fail:
    rtp_fio_clear_app(app);
out_fio_init:
out_incompatible_ticks:
    while (tid--) {
        rt_task_suspend(&app->task[tid].rtai_thread);
        rt_task_delete(&app->task[tid].rtai_thread);
    }
out_task_init:
    clear_bit(app_id, &rt_appcore.loaded_apps);
    if (!rt_appcore.loaded_apps) {
        rt_appcore.base_period = 0;
        stop_rt_timer();
    }
out_check_full:
    up(&rt_appcore.lock);
    kfree(app);
out:
    return err;
}

#undef TEST_THREAD

#ifdef TEST_THREAD
RT_TASK test_thread[2];

/** Function: test_thread
 * arguments: long app_id: RT-AppCore Task Id for this task
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
            printk("Application %li\n", priv_data);

        /* Wait until next call */
        if (rt_task_wait_period()) {
            pr_info("Application overrun ... tick %lu took %luns, app run %luns\n",
                    counter,
                    (unsigned long)((get_cycles() - rt_start)*1000)/(cpu_khz/1000),
                    (unsigned long)((rt_end - rt_start)*1000)/(cpu_khz/1000));
                break;
        }
    }
    rt_printk("Max overrun reached. Aborting...\n");
}

void free_test_application(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        rt_task_suspend(&test_thread[i]);
        rt_task_delete(&test_thread[i]);
    }
    stop_rt_timer();
    return;
}

int register_test_application(void)
{
    RTIME tick_period, now;
    int err, i;

    pr_debug("Registering new TEST application with rtw_manager\n");

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
    free_test_application();
    return;
#endif //TEST_THREAD

    rtcom_fio_clear();
    rtp_fio_clear();

    /* Stop world time task */
    kthread_stop(rt_appcore.helper_thread);

    pr_info("RT-AppCore stopped\n");
    return;
}

int __init mod_init(void)
{
    int err = -1;

    pr_info("Starting RT-AppCore " PACKAGE_VERSION "\n");

#ifdef TEST_THREAD
    return register_test_application();
#endif //TEST_THREAD

    /* First start a separate thread to fetch world time.
     * Do this first so that the app can initialise to the correct
     * time */
    rt_sem_init(&rt_appcore.time.lock,1);

    /* Initialise management variables */
    init_MUTEX(&rt_appcore.lock);
    init_MUTEX(&rt_appcore.file_lock);
    init_waitqueue_head(&rt_appcore.event_q);
    rt_appcore.loaded_apps = 0;

    /* Initialise RTAI */
    rt_set_periodic_mode();

    /* Initialise file io to communicate with buddy */
    if ((err = rtp_fio_init())) {
        goto out_fio_init;
    }

    /* Initialise file io with rtcom buddy */
    if ((err = rtcom_fio_init())) {
        goto out_rtcom_fio_init;
    }


    /* The only thing to do here is start a thread to fetch world time */
    rt_appcore.helper_thread = 
        kthread_run(rt_appcore_helper, NULL, "rt_appcore_helper");
    if (rt_appcore.helper_thread == ERR_PTR(-ENOMEM)) {
        pr_info("Could not start timer thread - Out of memory\n");
        goto out_start_thread;
    }

    /* Now the module is ready to accept RT application, that register themselves
     * here by calling start_rt_app() */

    pr_info("Successfully started RT-AppCore\n");

    return 0;

    kthread_stop(rt_appcore.helper_thread);
out_start_thread:
    rtcom_fio_clear();
out_rtcom_fio_init:
    rtp_fio_clear();
out_fio_init:
    return err;
}


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("RT-AppCore for " PACKAGE_NAME);
MODULE_AUTHOR("Richard Hacker");
MODULE_VERSION(PACKAGE_VERSION);

EXPORT_SYMBOL_GPL(start_rt_app);
EXPORT_SYMBOL_GPL(stop_rt_app);

module_init(mod_init);
module_exit(mod_cleanup);
