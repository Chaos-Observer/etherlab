/******************************************************************************
 *
 *      $RCSfile: rtai_module.c,v $
 *      $Revision: 1.2 $
 *      $Author: rich $
 *      $Date: 2006/02/04 11:07:15 $
 *      $State: Exp $
 *
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
 *  These functions register a model generated with RTW with the Real-Time
 *  RTAI Kernel. It is statically linked to the model binary.
 *
 *      $Log: rtai_module.c,v $
 *
 *
 *****************************************************************************/ 

#define DEBUG

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/config.h>
#include "rt_kernel.h"

struct rtw_model rtw_model;

unsigned long stack_size = 0;
module_param(stack_size, ulong, S_IRUGO);
MODULE_PARM_DESC(stack_size,"Override the stack size specified in the model.");

unsigned long downsample = 0;
module_param(downsample, ulong, S_IRUGO);
MODULE_PARM_DESC(downsample,"Override the downsampling rate in the model.");

unsigned long overrun = 0;
module_param(overrun, ulong, S_IRUGO);
MODULE_PARM_DESC(overrun,"Override the model's overrun count.");

unsigned long buffer_time = 0;
module_param(buffer_time, ulong, S_IRUGO);
MODULE_PARM_DESC(buffer_time,"Specify time in usec you want to buffer the BlockIO sent to user space from the model.");

unsigned long tick = 0;
module_param(tick, ulong, S_IRUGO);
MODULE_PARM_DESC(tick,"Specify tick time in usec if you want to override the rate specified in the model.");

module_param_string(so_path, rtw_model.so_path, 
        sizeof(rtw_model.so_path), S_IRUGO);
MODULE_PARM_DESC(so_path,"Specify path to the shared object (<model>.so) containing the variable descriptions.");

static void __exit 
mod_cleanup(void)
{
    free_rtw_model(rtw_model.model_id);
    kfree(rtw_model.pend_rtP);
    mdl_stop();
    pr_info("Removed RTW Model \"%s\" from RTW_Manager\n", 
            rtw_model.modelName);
}


int __init 
mod_init(void)
{
    int err = -1;
    const char *errmsg;

    pr_debug("Inserting RTW model\n");

    /* Initialise the real time model */
    if ((errmsg = mdl_start())) {
            printk("ERROR: Could not initialize model: %s\n", errmsg);
            err = -1;
            goto out;
    }

    rtw_model.base_period = tick ? tick : rtw_model.base_period;
    rtw_model.downsample = downsample ? downsample : rtw_model.downsample;
    rtw_model.max_overrun = overrun ? overrun : rtw_model.max_overrun;
    rtw_model.buffer_time = buffer_time ? buffer_time : rtw_model.buffer_time;
    rtw_model.stack_size = stack_size ? stack_size : rtw_model.stack_size;

    /* Work out how fast the model is sampled by test_manager */
    rtw_model.sample_period = rtw_model.base_period*rtw_model.downsample;
    rtw_model.sample_period = 
        rtw_model.sample_period ? rtw_model.sample_period : 1;

    rtw_model.rtB_count = rtw_model.buffer_time/rtw_model.sample_period;
    rtw_model.rtB_count = rtw_model.rtB_count ? rtw_model.rtB_count : 1;

    /* Get area where pending parameters are stored */
    rtw_model.pend_rtP = kmalloc(rtw_model.rtP_size, GFP_KERNEL);
    if (!rtw_model.pend_rtP) {
            printk("Could not allocate memory for rtP exchange area\n");
            err = -ENOMEM;
            goto out_kmalloc;
    }

    /* Initialise the pending parameter area with the current parameters */
    memcpy(rtw_model.pend_rtP, rtw_model.mdl_rtP, rtw_model.rtP_size);

    /* Having finished all the model initialisation, it is now time to 
     * register this RTW Model with the Real-Time Kernel to be scheduled */
    if ((rtw_model.model_id = register_rtw_model(&rtw_model,
                    sizeof(rtw_model), REVISION)) < 0) {
        printk("Could not register model with rtw_manager; rc = %i\n",
                rtw_model.model_id);
        err = rtw_model.model_id;
        goto out_register_mdl;
    }

    pr_info("Successfully registered RTW Model \"%s\" with RTW-Manager\n",
            rtw_model.modelName);

    return 0;

out_register_mdl:
    kfree(rtw_model.pend_rtP);
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
