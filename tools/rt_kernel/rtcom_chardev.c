/******************************************************************************
 *
 * $Id$
 *
 * This is the RTCom character device implementation for the buddy.
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
 *****************************************************************************/ 

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#error "Don't have config.h"
#endif

#include "rtcom_chardev.h"

#define DEBUG

/* Kernel header files */
#include <linux/kernel.h>       /* pr_debug() */
#include <linux/fs.h>           /* everything */
#include <linux/miscdevice.h>   /* that is what this section does ;-) */
#include <linux/sem.h>          /* semaphores */
#include <linux/poll.h>         /* poll_wait() */
#include <linux/mm.h>           /* mmap */
#include <asm/uaccess.h>        /* get_user(), put_user() */

/* RTAI headers */
#include <rtai_sem.h>

/* Local headers */
#include "rt_main.h"
#include "rt_model.h"

// 4MB IO memory
#define IO_MEM_LEN (1<<23)
#define EVENTQ_LEN 128

static int open_kernel(struct file *filp);
static int open_model(struct file *filp, const char* model_name);
static struct page * vma_nopage(struct vm_area_struct *vma, 
        unsigned long address, int *type);
static int fop_release_kernel( struct inode *inode, struct file *filp);
static int fop_release_model( struct inode *inode, struct file *filp);
static unsigned int fop_poll_kernel( struct file *filp, poll_table *wait);
static unsigned int fop_poll_model( struct file *filp, poll_table *wait);
static long fop_ioctl( struct file *filp, unsigned int command, 
        unsigned long data);
static long fop_ioctl_model( struct file *filp, unsigned int command, 
        unsigned long data);
static ssize_t fop_read_kernel( struct file * filp, char *buffer, 
        size_t bufLen, loff_t * offset);
static ssize_t fop_read_model( struct file * filp, char *buffer, 
        size_t bufLen, loff_t * offset);
static int fop_mmap_model(struct file *filp, struct vm_area_struct *vma);

// Wait queue for poll
static DECLARE_WAIT_QUEUE_HEAD(event_q);

// This semaphore ensures that there is only one file opened to the
// main kernel chardev
static DECLARE_MUTEX(rtcom_lock);

// This semaphore serialises access to the event buffer
static DECLARE_MUTEX(event_lock);

static struct rtcom_event *event_list, *event_list_end,
                          *event_rp, *event_wp = NULL;


/*#########################################################################*
 * The chardev that is provided by rtcom has very little capabilities.
 * Opening the chardev is allways possible. The only operation allowed
 * is ioctl, thereby either selecting the handle to the kernel or
 * model.
 *#########################################################################*/
static struct file_operations rtcom_fops = {
    .unlocked_ioctl = fop_ioctl,
    .owner          = THIS_MODULE,
};

static long 
fop_ioctl( struct file *filp, unsigned int command, unsigned long data) 
{
    long rv = 0;

    down(&rt_kernel.lock);
    switch (command) {
        // Try to gain access to main rtcom kernel
        case LOCK_KERNEL:
            {
                rv = open_kernel(filp);
            }
            break;

        // Try to gain access to a running model
        case SELECT_MODEL:
            {
                char model_name[MAX_MODEL_NAME_LEN];

                // data points to model's name
                rv = strncpy_from_user(model_name, (char*)data, 
                            MAX_MODEL_NAME_LEN);
                if (rv > 0) {
                    rv = open_model(filp, model_name);
                }
            }
            break;

        default:
            rv = -ENOTTY;
            break;
    }
    up(&rt_kernel.lock);

    return rv;
}

/*#########################################################################*
 * Here the file operations to control the Real-Time Kernel are defined.
 * The buddy needs this to be able to change things in the Kernel.
 *#########################################################################*/
static struct file_operations kernel_fops = {
    .poll           = fop_poll_kernel,
    .read           = fop_read_kernel,
    .release        = fop_release_kernel,
    .owner          = THIS_MODULE,
};

static int 
open_kernel(struct file *filp) 
{
    int err = 0;
    unsigned int model_id;

    // Make sure there is only one file handle to the rtcom kernel
    if (down_trylock(&rtcom_lock)) {
        err = -EBUSY;
        goto out_trylock;
    }

    event_list = kmalloc(sizeof(struct rtcom_event) * EVENTQ_LEN, GFP_KERNEL);
    if (!event_list) {
        err = -ENOMEM;
        goto out_kmalloc;
    }
    event_rp = event_list;
    event_list_end = &event_list[EVENTQ_LEN];

    /* Dont need to lock here */
    event_wp = event_list;

    /* Inform the buddy of currently running models */
    for ( model_id = 0; model_id < MAX_MODELS; model_id++ ) {
        if (!test_bit(model_id, &rt_kernel.loaded_models))
            continue;
        rtcom_new_model(rt_kernel.model[model_id]);
    }
    pr_debug("Opened rtcom file: %p\n", event_list);

    /* Put new file operations in place. These now operate on the kernel */
    filp->f_op = &kernel_fops;

    return 0;

    kfree(event_list);
out_kmalloc:
    up(&rtcom_lock);
out_trylock:
    return err;
}

static unsigned int 
fop_poll_kernel( struct file *filp, poll_table *wait) 
{
    unsigned int mask = 0;

    /* Wait for data */
    poll_wait(filp, &event_q, wait);

    /* Check whether there was a change in the states of a Real-Time Process */
    if (event_wp != event_rp) {
        mask = POLLIN | POLLRDNORM;
    }

    return mask;
}


/* RTCom buddy reads the current event list */
static ssize_t 
fop_read_kernel( struct file * filp, char *buffer, 
        size_t bufLen, loff_t * offset)
{
    struct rtcom_event *read_end = event_wp;
    ssize_t len;

    /* If event_wp is NULL, there was a buffer overflow */
    if (!read_end)
        return -ENOSPC;

    /* Check whether the buffer is empty */
    if (read_end == event_rp)
        return 0;

    /* Write pointer wrapped around, so only copy to the end of the buffer
     * The buddy will have to read again to get the buffer start
     */
    if (read_end < event_rp)
        read_end = event_list_end;

    /* The number of characters to be copied */
    len = (void*)read_end - (void*)event_rp;

    /* Check whether the buddy's buffer is long enough */
    if (len > bufLen) {

        /* truncate to an integer length of struct rtcom_event */
        read_end = event_rp + bufLen / sizeof(*read_end);
        len = (void*)read_end - (void*)event_rp;
        if (!len)
            return 0;
    }

    if (copy_to_user(buffer, event_rp, len))
        return -EFAULT;

    event_rp = (read_end == event_list_end) ? event_list : read_end;

    return len;
}

static int 
fop_release_kernel( struct inode *inode, struct file *filp) 
{
    kfree(event_list);
    up(&rtcom_lock);
    return 0;
}

/*#########################################################################*
 * Here the file operations to control the Real-Time Kernel are defined.
 * The buddy needs this to be able to change things in the Kernel.
 *#########################################################################*/
static struct file_operations model_fops = {
    .unlocked_ioctl = fop_ioctl_model,
    .poll           = fop_poll_model,
    .read           = fop_read_model,
    .mmap           = fop_mmap_model,
    .release        = fop_release_model,
    .owner          = THIS_MODULE,
};

static int 
open_model(struct file *filp, const char* model_name) 
{
    int err = 0;
    unsigned int model_id;
    struct model* model;

    /* Find model with name model_name */
    for ( model_id = 0; model_id < MAX_MODELS; model_id++ ) {
        if (!test_bit(model_id, &rt_kernel.loaded_models))
            continue;
        if (strcmp(model_name, 
                    rt_kernel.model[model_id]->rt_model->modelName)) {
            continue;
        }
    }
    if (model_id == MAX_MODELS) {
        /* There is no model named model_name */
        return -ENOENT;
        goto out_no_model;
    }
    model = rt_kernel.model[model_id];

    /* Make sure there is only one file handle opened to the model */
    if (down_trylock(&model->buddy_lock)) {
        err = -EBUSY;
        goto out_trylock;
    }

    /* Allocate memory for signal output. This memory will be mmap()ed 
     * later */
    model->rtb_buf = vmalloc(model->rtB_cnt * model->rtB_len);
    if (!model->rtb_buf) {
        err = -ENOMEM;
        goto out_vmalloc;
    }
    model->rtb_last = model->rtb_buf + model->rtB_cnt * model->rtB_len;

    /* Allocate memory for the event stream */
    model->event_list = 
        kmalloc(sizeof(*event_list) * model->rtB_cnt, GFP_KERNEL);
    if (!model->event_list) {
        err = -ENOMEM;
        goto out_kmalloc;
    }
    model->event_list_end = model->event_list + model->rtB_cnt;

    pr_debug("Opened rtcom model file to %s\n", model->rt_model->modelName);

    /* Put new file operations in place. These now operate on the kernel */
    filp->f_op = &model_fops;
    filp->private_data = model;
    init_waitqueue_head(&model->waitq);

    /* Setting the write pointers is an indication that the memory has
     * been set up and writing can begin. */
    model->rp = model->wp = model->rtb_buf;
    model->event_rp = model->event_wp = model->event_list;

    return 0;

    kfree(model->event_list);
out_kmalloc:
    vfree(model->rtb_buf);
out_vmalloc:
    up(&model->buddy_lock);
out_trylock:
out_no_model:
    return err;
}

static int 
fop_release_model( struct inode *inode, struct file *filp) 
{
    struct model *model = (struct model *) filp->private_data;

    down(&model->buddy_lock);
    model->event_wp = NULL;
    up(&model->buddy_lock);

    kfree(model->event_list);
    vfree(model->rtb_buf);
    return 0;
}

static unsigned int 
fop_poll_model( struct file *filp, poll_table *wait) 
{
    unsigned int mask = 0;
    struct model *model = (struct model *) filp->private_data;

    /* Wait for data */
    poll_wait(filp, &model->waitq, wait);

    /* Check whether there was a change in the states of a Real-Time Process */
    if (model->event_wp != model->event_rp) {
        mask = POLLIN | POLLRDNORM;
    }

    return mask;
}

static long 
fop_ioctl_model( struct file *filp, unsigned int command, unsigned long data) 
{
    long rv = 0;
    struct model* model = (struct model*) filp->private_data;
    const struct rt_model* rt_model = model->rt_model;

    pr_debug("rt_kernel ioctl command %#x, data %lu\n", command, data);

    down(&rt_kernel.lock);
    switch (command) {
        case GET_RTK_PROPERTIES:
            {
                struct rt_kernel_prop p;
                p.iomem_len = model->rtB_len * model->rtB_cnt;
                p.eventq_len = sizeof(struct model_event) * model->rtB_cnt;
                printk("In GET_RTK_PROPERTIES\n");

                if (copy_to_user((void*)data, &p, sizeof(p))) {
                    rv = -EFAULT;
                    break;
                }
            }
            break;

        case GET_MDL_SAMPLETIMES:
            {
                printk("In GET_MDL_SAMPLETIMES\n");

                if (copy_to_user((void*)data, rt_model->task_period, 
                            rt_model->num_st*sizeof(*rt_model->task_period))) {
                    rv = -EFAULT;
                    break;
                }
            }
            break;

        case GET_PARAM:
            {
                printk("In GET_PARAM\n");

                if (copy_to_user((void*)data, rt_model->mdl_rtP, 
                            rt_model->rtP_size)) {
                    rv = -EFAULT;
                    break;
                }
            }
            break;

        case GET_SIGNAL_INFO:
        case GET_PARAM_INFO:
            {
                struct signal_info si;
                const char *path;
                const char *err;
                size_t len;
                pr_debug("In GET_(SIGNAL|PARAM)_INFO\n");

                if (copy_from_user(&si, (void*)data, sizeof(si))) {
                    rv = -EFAULT;
                    break;
                }

                err = (command == GET_SIGNAL_INFO)
                    ? rt_model->get_signal_info(&si, &path)
                    : rt_model->get_param_info(&si, &path);
                if (err) {
                    pr_info("Error occurred in get_signal_info(): %s\n",
                            err);
                    rv = -EINVAL;
                    break;
                }
                len = strlen(path) + 1;

                if (copy_to_user((void*)data, &si, sizeof(si)) ||
                        copy_to_user(si.path, path, len)) {
                    rv = -EFAULT;
                    break;
                }
            }
            break;

        case GET_MDL_PROPERTIES:
            {
                struct mdl_properties p;
                printk("In GET_MDL_PROPERTIES\n");

                p.signal_count  = rt_model->signal_count;
                p.param_count   = rt_model->param_count;
                p.variable_path_len  = rt_model->variable_path_len;
                p.num_st        = rt_model->num_st;
                p.num_tasks     = rt_model->num_tasks;

                // No buffer overflow here, the array is defined as
                // name[MAX+1]
                strncpy(p.name, rt_model->modelName, MAX_MODEL_NAME_LEN);
                strncpy(p.version, rt_model->modelVersion, 
                        MAX_MODEL_VER_LEN);
                p.name[MAX_MODEL_NAME_LEN] = '\0';
                p.version[MAX_MODEL_VER_LEN] = '\0';

                if ( copy_to_user((void*)data, &p, sizeof(p))) {
                    rv = -EFAULT;
                    break;
                }
            }
            break;

        default:
            rv = -ENOTTY;
            break;
    }
    up(&rt_kernel.lock);

    return rv;
}

/* RTCom buddy reads the current event list */
static ssize_t 
fop_read_model( struct file * filp, char *buffer, 
        size_t bufLen, loff_t * offset)
{
    struct model* model = (struct model*) filp->private_data;
    struct model_event *read_end = model->event_wp;
    ssize_t len;

    /* If event_wp is NULL, there was a buffer overflow */
    if (!read_end)
        return -ENOSPC;

    /* Check whether the buffer is empty */
    if (read_end == model->event_rp)
        return 0;

    /* Write pointer wrapped around, so only copy to the end of the buffer
     * The buddy will have to read again to get the buffer start
     */
    if (read_end < model->event_rp)
        read_end = model->event_list_end;

    /* The number of characters to be copied */
    len = (void*)read_end - (void*)model->event_rp;

    /* Check whether the buddy's buffer is long enough */
    if (len > bufLen) {

        /* truncate to an integer length of struct model_event */
        read_end = model->event_rp + bufLen / sizeof(*read_end);
        len = (void*)read_end - (void*)model->event_rp;
        if (!len)
            return 0;
    }

    if (copy_to_user(buffer, model->event_rp, len))
        return -EFAULT;

    model->event_rp = (read_end == model->event_list_end)
        ? model->event_list : read_end;

    return len;
}

static struct page *
vma_nopage(struct vm_area_struct *vma, unsigned long address, int *type)
{
    unsigned long offset;
    struct page *page = NOPAGE_SIGBUS;
    struct model* model = (struct model*)vma->vm_private_data;

    offset = (address - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);

    /* Check that buddy did not want to access memory out of range */
    if (offset >= IO_MEM_LEN)
        return NOPAGE_SIGBUS;

    page = vmalloc_to_page(model->rtb_buf + offset);

//    pr_debug("Nopage fault vma, address = %#lx, offset = %#lx, page = %p\n", 
//            address, offset, page);

    /* got it, now increment the count */
    get_page(page);
    if (type)
        *type = VM_FAULT_MINOR;
    
    return page;
}

static struct vm_operations_struct vm_ops = {
    .nopage = vma_nopage,
};

static int
fop_mmap_model(struct file *filp, struct vm_area_struct *vma)
{
    vma->vm_ops = &vm_ops;
    vma->vm_flags |= VM_RESERVED;       /* Pages will not be swapped out */
    vma->vm_private_data = filp->private_data;
    return 0;
}


/*#########################################################################*
 * Here is general management code to initialise file handles for the 
 * Real-Time Kernel.
 *#########################################################################*/
static inline void update_wp(void)
{
    // Lock must be held here somewhere else
    if (++event_wp == event_list_end)
        event_wp = event_list;
    if (event_wp == event_rp)
        event_wp = NULL;
}

void rtcom_new_model(struct model *model)
{
    init_MUTEX(&model->buddy_lock);
    init_waitqueue_head(&model->waitq);
    model->event_wp = NULL;
    rt_sem_init(&model->buf_sem,0);
    rt_sem_init(&model->rtP_sem,0);
    rt_sem_init(&model->msg_sem,0);

    /* Append a new_model event to the end of the rtcom event list */
    down(&event_lock);
    if (event_wp) {
        event_wp->type = new_model;
        // No buffer overflow here!
        strncpy(event_wp->model_name, model->rt_model->modelName,
                MAX_MODEL_NAME_LEN);
        event_wp->model_name[MAX_MODEL_NAME_LEN] = '\0';
        update_wp();
    }
    up(&event_lock);
    wake_up_interruptible(&event_q);
}

void rtcom_del_model(struct model *model)
{
    down(&event_lock);
    if (event_wp) {
        event_wp->type = del_model;
        strncpy(event_wp->model_name, model->rt_model->modelName,
                MAX_MODEL_NAME_LEN);
        event_wp->model_name[MAX_MODEL_NAME_LEN] = '\0';
        update_wp();
    }
    up(&event_lock);
    wake_up_interruptible(&event_q);
}

struct miscdevice misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "etl",
    .fops = &rtcom_fops,
};

/* Clear the Real-Time Kernel file handles */
void __exit rtcom_fio_clear(void)
{
    pr_debug("Tearing down FIO for rt_kernel\n");

    misc_deregister(&misc_dev);
}

/* Set up the Real-Time Kernel file handles. This is called once when
 * the rt_kernel is loaded, and opens up the char device for 
 * communication between rtcom-buddy and rt_kernel */
int __init rtcom_fio_init(void)
{
    int err = -1;

    pr_debug("Initialising FIO for rt_kernel\n");

    if ((err = misc_register(&misc_dev))) {
        printk("Could not create misc device etl.\n");
        goto out_misc_register;
    }

    return 0;

    misc_deregister(&misc_dev);
out_misc_register:
    return err;
}

