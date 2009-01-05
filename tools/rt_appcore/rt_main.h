/******************************************************************************
 *
 * $Id$
 *
 * Here structures are defined that are used within rt_appcore and associated
 * files.
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

#ifndef RT_MAIN_H
#define RT_MAIN_H

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <rtai_sem.h>

#include "fio_ioctl.h"          /* MAX_APPS */

/* The calling frequency of the appcore helper */
#define HELPER_CALL_RATE 10

/* Every application can have more than one task - one of this structure is 
 * allocated for each task */
struct rt_task {
    RT_TASK rtai_thread;        /** Structure for RTAI data */

    int master;                 /** This is true for the first application 
                                 * task's fastest timing rate. Used by the
                                 * helper_thread */

    struct app *app;      /** Reference to the app */
    unsigned int app_tid;       /** The app's task id */

    struct task_stats *stats;
};

struct app {
    int id;                     /** The app index in the array 
                                 * \ref rt_appcore.rt_app assigned by 
                                 * rt_appcore for this app. */
    const struct rt_app *rt_app; /** Data structure for the RTW app. 
                                  * This gets passed to us when the app
                                  * is registered */
    struct class_device *sysfs_dev;

    struct rt_vars *rt_vars;

    struct buddy_fio *buddy_fio; 

    struct cdev rtp_dev;        /* Device for Process IO */
    SEM buf_sem;                /* RT Sem to protect data path to buddy */
    SEM rtP_sem;                /* RT Sem for parameters */

    int new_rtP;   /* True if a new parameter set needs to be loaded */

    struct semaphore buddy_lock;
    wait_queue_head_t waitq;
    unsigned int rtB_cnt;
    size_t rtB_len;

    /* Management for Process IO */
    void *rtb_buf;
    void *rp;                   /* Read pointers */
    void *wp;                   /* Write pointers */
    void *rtb_last;             /* Pointer to last block of rtb_buf */
    unsigned int photo_sample;  /* Decrease this by one every tick;
                                 * when ==1, make a photo */

    SEM msg_sem;
    char *msg_buf;
    char *msg_ptr;
    size_t msg_buf_len;

    size_t task_stats_len;

    struct rt_task task[];    /** Array of length equal to the number
                                 * of sample times the app has. */
};

struct rt_appcore {
    dev_t dev;                  /**< Char dev range registered for rt_appcore */
    struct cdev buddy_dev;      /**< Char dev struct for rt_appcore 
                                  * communication with the buddy */
    unsigned int chrdev_cnt;    /**< Count of character devices reserved */
    struct class *sysfs_class;  /**< Pointer to SysFS class */
    struct class_device *sysfs_dev; /**< Base device of rt_appcore */

    unsigned long loaded_apps;  /**< The bits represent whether a RT app 
                                   * is loaded in this slot. Max no of RT 
                                   * apps is thus 32 */
    unsigned long app_state_changed;
    unsigned long data_mask;    /**< The bits indicate to the service request
                                 * handler that the specific RT task had data
                                 * to be processed */

    struct semaphore lock;      /* Protect manipulation of above vars */

    struct app *application[MAX_APPS];

    /* The following variables are used to manage the communication between
     * the main buddy process and the rt_appcore */
    wait_queue_head_t event_q;  /* Event queue for poll of task load/unload
                                 * events */

    struct semaphore file_lock; /* Only one is allowed to open Management
                                 * channel */

    unsigned long base_period;  /* The tick period in microsec */
    RTIME tick_period;          /* RTAI tick counts for base_period */

    struct task_struct *helper_thread;

    struct time {
        double u;
        double value;

        struct timeval tv;          /** Current world time */
        SEM lock;                   /* Protection for this struct */
        unsigned int tv_update_period;  /* Time in microsec in which tv
                                         * is updated */

        unsigned long flag;         /** Lowest bit is 1 if there is a new
                                     * time sample */

        double i;
        double base_step;
    } time;
};

extern struct rt_appcore rt_appcore;

/* Copies the current Process Image to the internal buffer */
void rtp_make_photo(struct app *);

int rtp_fio_init_app(struct app *, struct module *owner);
void rtp_fio_clear_app(struct app *);
int rtp_fio_init(void);
void rtp_fio_clear(void);
void rtp_data_avail_handler(void);

#endif // RT_MAIN_H
