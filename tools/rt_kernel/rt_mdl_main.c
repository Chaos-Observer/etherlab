/***********************************************************************
 *
 * $Id$
 *
 * module_init() and module_exit() are defined here. This file will be 
 * compiled together with the Real-Time Model to form a kernel module,
 * registering it with the rt_kernel.
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
#include "include/rt_kernel.h"

extern struct rt_model rt_model;

unsigned long stack_size = 0;
module_param(stack_size, ulong, S_IRUGO);
MODULE_PARM_DESC(stack_size,"Override the stack size specified in the model.");

unsigned long decimation = 0;
module_param(decimation, ulong, S_IRUGO);
MODULE_PARM_DESC(decimation, "Override the photo decimation rate in the model.");

unsigned long overrun = 0;
module_param(overrun, ulong, S_IRUGO);
MODULE_PARM_DESC(overrun,"Override the model's overrun count.");

unsigned long buffer_time = 0;
module_param(buffer_time, ulong, S_IRUGO);
MODULE_PARM_DESC(buffer_time,"Override BlockIO buffer time specified in the model.");

unsigned long tick = 0;
module_param(tick, ulong, S_IRUGO);
MODULE_PARM_DESC(tick,"Override tick rate specified in the model.");

static void __exit 
mod_cleanup(void)
{
    stop_rt_model(rt_model.model_id);
    kfree(rt_model.pend_rtP);
    mdl_stop();
    pr_info("Removed RTW Model \"%s\" from RT-Kernel\n", 
            rt_model.modelName);
}


int __init 
mod_init(void)
{
    int err = -1;
    const char *errmsg;
    unsigned int i;

    pr_debug("Inserting RTW model\n");

    /* Initialise the real time model */
    if ((errmsg = mdl_start())) {
            printk("ERROR: Could not initialize model: %s\n", errmsg);
            err = -1;
            goto out;
    }

    // If a new tick is supplied, use it instead of the model's
    for (i = rt_model.num_st; tick && i--; ) {
        rt_model.task_period[i] = 
            rt_model.task_period[i] / rt_model.task_period[0] * tick;
    }
    rt_model.decimation = decimation ? decimation : rt_model.decimation;
    rt_model.max_overrun = overrun ? overrun : rt_model.max_overrun;
    rt_model.buffer_time = 
        buffer_time ? buffer_time*1000000 : rt_model.buffer_time;
    rt_model.stack_size = stack_size ? stack_size : rt_model.stack_size;

    /* Work out how fast the model is sampled by test_manager */
    rt_model.sample_period = rt_model.decimation 
        ? rt_model.task_period[0]*rt_model.decimation
        : rt_model.task_period[0];

    rt_model.rtB_count = rt_model.buffer_time/rt_model.sample_period;
    rt_model.rtB_count = rt_model.rtB_count ? rt_model.rtB_count : 1;

    /* Get area where pending parameters are stored */
    rt_model.pend_rtP = kmalloc(rt_model.rtP_size, GFP_KERNEL);
    if (!rt_model.pend_rtP) {
            printk("Could not allocate memory for rtP exchange area\n");
            err = -ENOMEM;
            goto out_kmalloc;
    }

    /* Initialise the pending parameter area with the current parameters */
    memcpy(rt_model.pend_rtP, rt_model.mdl_rtP, rt_model.rtP_size);

    /* Having finished all the model initialisation, it is now time to 
     * register this RTW Model with the Real-Time Kernel to be scheduled */
    if ((rt_model.model_id = start_rt_model(&rt_model,
                    sizeof(rt_model), REVISION, THIS_MODULE)) < 0) {
        printk("Could not register model with rtw_manager; rc = %i\n",
                rt_model.model_id);
        err = rt_model.model_id;
        goto out_register_mdl;
    }

    pr_info("Successfully registered RTW Model \"%s\" with RT-Kernel\n",
            rt_model.modelName);

    return 0;

out_register_mdl:
    kfree(rt_model.pend_rtP);
out_kmalloc:
    mdl_stop();
out:
    return err;
}



MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Real Time Workshop kernel module");
MODULE_AUTHOR("Richard Hacker");

module_init(mod_init);
module_exit(mod_cleanup);
