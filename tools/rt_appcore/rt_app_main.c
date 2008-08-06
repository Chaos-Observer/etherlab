/***********************************************************************
 *
 * $Id$
 *
 * module_init() and module_exit() are defined here. This file will be
 * compiled together with the Real-Time application to form a kernel module,
 * registering it with the RT-AppCore.
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
 *      Autor: Richard Hacker
 *
 *      (C) Copyright IgH 2005
 *      Ingenieurgemeinschaft IgH
 *      Heinz-Baecker Str. 34
 *      D-45356 Essen
 *      Tel.: +49 201/36014-0
 *      Fax.: +49 201/36014-14
 *      E-mail: info@igh-essen.com
 *
 *****************************************************************************/ 

#define DEBUG

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/autoconf.h>
#include <rt_appcore.h>

extern struct rt_app rt_app;

char *app_name;
module_param_named(application_name, app_name, charp, S_IRUGO);
MODULE_PARM_DESC(application_name,"Assign alternative application name.");

unsigned long stack_size = 0;
module_param(stack_size, ulong, S_IRUGO);
MODULE_PARM_DESC(stack_size,"Override the stack size specified in the application.");

unsigned long decimation = 0;
module_param(decimation, ulong, S_IRUGO);
MODULE_PARM_DESC(decimation, "Override the photo decimation rate in the application.");

unsigned long overrun = 0;
module_param(overrun, ulong, S_IRUGO);
MODULE_PARM_DESC(overrun,"Override the application's overrun count.");

unsigned long buffer_time = 0;
module_param(buffer_time, ulong, S_IRUGO);
MODULE_PARM_DESC(buffer_time,"Override BlockIO buffer time specified in the application.");

unsigned long tick = 0;
module_param(tick, ulong, S_IRUGO);
MODULE_PARM_DESC(tick,"Override tick rate specified in the application.");

static void __exit 
mod_cleanup(void)
{
    stop_rt_app(rt_app.app_id);
    kfree(rt_app.pend_rtP);
    app_stop();
    pr_info("Removed application \"%s\".\n", rt_app.name);
}


int __init 
mod_init(void)
{
    int err = -1;
    const char *errmsg;
    unsigned int i;

    pr_debug("Inserting application.\n");

    /* Initialise the real time application */
    if ((errmsg = app_start())) {
            printk("ERROR: Could not initialize application: %s\n", errmsg);
            err = -1;
            goto out;
    }

    if (app_name && strlen(app_name)) {
        rt_app.name = app_name;
    }

    // If a new tick is supplied, use it instead of the application's
    for (i = rt_app.num_st; tick && i--; ) {
        rt_app.task_period[i] = 
            rt_app.task_period[i] / rt_app.task_period[0] * tick;
    }
    rt_app.decimation = decimation ? decimation : rt_app.decimation;
    rt_app.max_overrun = overrun ? overrun : rt_app.max_overrun;
    rt_app.buffer_time = 
        buffer_time ? buffer_time*1000000 : rt_app.buffer_time;
    rt_app.stack_size = stack_size ? stack_size : rt_app.stack_size;

    /* Work out how fast the application is sampled by test_manager */
    rt_app.sample_period = rt_app.decimation 
        ? rt_app.task_period[0]*rt_app.decimation
        : rt_app.task_period[0];

    rt_app.rtB_count = rt_app.buffer_time/rt_app.sample_period;
    rt_app.rtB_count = rt_app.rtB_count ? rt_app.rtB_count : 1;

    /* Get area where pending parameters are stored */
    rt_app.pend_rtP = kmalloc(rt_app.rtP_size, GFP_KERNEL);
    if (!rt_app.pend_rtP) {
            printk("Could not allocate memory for rtP exchange area\n");
            err = -ENOMEM;
            goto out_kmalloc;
    }

    /* Initialise the pending parameter area with the current parameters */
    memcpy(rt_app.pend_rtP, rt_app.app_rtP, rt_app.rtP_size);

    /* Having finished all the application initialisation, it is now time to 
     * register this RTW Model with the Real-Time Kernel to be scheduled */
    if ((rt_app.app_id = start_rt_app(&rt_app,
                    sizeof(rt_app), REVISION, THIS_MODULE)) < 0) {
        printk("Could not register application with RT-AppCore; rc = %i\n",
                rt_app.app_id);
        err = rt_app.app_id;
        goto out_register_mdl;
    }

    pr_info("Successfully registered application \"%s\" with RT-AppCore.\n",
            rt_app.name);

    return 0;

out_register_mdl:
    kfree(rt_app.pend_rtP);
out_kmalloc:
    app_stop();
out:
    return err;
}



MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Real Time Workshop kernel module");
MODULE_AUTHOR("Richard Hacker");

module_init(mod_init);
module_exit(mod_cleanup);
