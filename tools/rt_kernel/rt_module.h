/******************************************************************************
 *
 *           $RCSfile: rtai_module.c,v $
 *           $Revision: 13 $
 *           $Author: rich $
 *           $Date: 2006/02/04 11:07:15 $
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
 *
 *
 *****************************************************************************/ 

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <rtai_sem.h>

/* The following are the bits of rt_mdl->buddy_there to indicate
 * whether the specific channel has been opened by the buddy
 */
#define RTB_BIT 0
#define CTL_BIT 1

struct rt_task {
    unsigned int mdl_tid;
    unsigned long period;
    struct rt_mdl *rt_mdl;
    RT_TASK rtai_thread;
};

struct rt_mdl {
    int model_id;               /** The model index in the array 
                                 * \ref rt_manager.rt_mdl assigned by 
                                 * rt_kernel for this model. */
    struct rtw_model *rtw_model; /** Data structure for the RTW model */
    SEM mdl_lock;               /* If locked, model should not be executed */

    unsigned long buddy_there;   /* The bits indicate whether one of the IO
                                  * channels has been opened by the buddy.
                                  */

    struct cdev rtp_dev;        /* Device for Process IO */
    wait_queue_head_t data_q;
    SEM buf_sem;

    struct cdev ctl_dev;        /* Device for Control Interface */
    wait_queue_head_t ctl_q;
    SEM io_rtP_sem;

    /* Management for Process IO */
    void *rtb_buf;
    void *rp;                   /* Read pointers */
    void *wp;                   /* Write pointers */
    void *rtb_last;             /* Pointer to last block of rtb_buf */

    struct rt_task thread[];    /** Array of length equal to the number
                                 * of sample times the model has. */
};

#define MAX_MODELS sizeof(unsigned long)*8

struct rt_manager {
    dev_t dev;                  /**< Char dev range registered for rt_manager */
    struct cdev main_ctrl;      /**< Char dev struct for rt_manager */

    unsigned long loaded_models;  /**< The bits represent whether a RT model 
                                   * is loaded in this slot. Max no of RT 
                                   * models is thus 32 */
    unsigned long stopped_models; /**< The bits represent whether the model 
                                   * in this slot is stopped, but has not yet 
                                   * been released bu the buddy */

    int srq;                    /**< Service Request handle */
    unsigned long srq_mask;     /**< The bits indicate to the service request
                                 * handler that the specific RT task had data
                                 * to be processed */

    struct semaphore sem;       /* Protect manipulation of above vars */

    struct rt_mdl *rt_mdl[MAX_MODELS];

    /* The following variables are used to manage the communication between
     * the main buddy process and the rt_manager */
    unsigned int notify_buddy;  /* True wakes up buddy about a change in
                                 * one of the Real-Time Tasks */
    wait_queue_head_t event_q;  /* Event queue for poll of task load/unload
                                 * events */

    struct semaphore file_lock; /* Only one is allowed to open Management
                                 * channel */

    unsigned long base_period;  /* The tick period in microsec */
    RTIME tick_period;          /* RTAI tick counts for base_period */
};

extern struct rt_manager rt_manager;

/* Copies the current Process Image to the internal buffer */
void rtp_make_photo(struct rt_mdl *);

int rtp_fio_init_mdl(struct rt_mdl *, struct rtw_model *rtw_model);
void rtp_fio_clear_mdl(struct rt_mdl *);
int rtp_fio_init(void);
void rtp_fio_clear(void);
void rtp_data_avail_handler(void);
