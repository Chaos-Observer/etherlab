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
#include <linux/err.h>          /* ERR_PTR(), etc */
#include <linux/list.h>         /* Kernel lists */
#include <linux/random.h>       /* random32() */
#include <asm/uaccess.h>        /* get_user(), put_user() */

/* RTAI headers */
#include <rtai_sem.h>

/* Local headers */
#include "rt_main.h"
#include "rt_model.h"

struct model_dev {
    struct list_head list;
    SEM rt_lock;
    struct model *model;
    unsigned int id;

    unsigned int buddy_lock;
    size_t io_len;
    void *rtb_buf, *rtb_last, *rp, *wp;

    size_t eventq_len;
    struct model_event *event_list, *event_list_end, *event_wp, *event_rp;
    wait_queue_head_t waitq;
};

static int open_kernel(void);
struct model_dev* open_model(unsigned int id);
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
static unsigned int rtcom_lock = 0;

static LIST_HEAD(model_list);

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

/* File operations used for kernel */
static struct file_operations kernel_fops = {
    .poll           = fop_poll_kernel,
    .read           = fop_read_kernel,
    .release        = fop_release_kernel,
    .owner          = THIS_MODULE,
};

/* File operations used for model */
static struct file_operations model_fops = {
    .unlocked_ioctl = fop_ioctl_model,
    .poll           = fop_poll_model,
    .read           = fop_read_model,
    .mmap           = fop_mmap_model,
    .release        = fop_release_model,
    .owner          = THIS_MODULE,
};

static long 
fop_ioctl( struct file *filp, unsigned int command, unsigned long data) 
{
    long rv = 0;

    down(&rt_kernel.lock);
    switch (command) {
        // Try to gain access to main rtcom kernel
        case SELECT_KERNEL:
            {
                rv = open_kernel();
                if (!rv) {
                    /* Put new file operations in place. 
                     * These now operate on the kernel */
                    filp->f_op = &kernel_fops;
                }
            }
            break;

        // Try to gain access to a running model
        case SELECT_MODEL:
            {
                struct model_dev* md;
                unsigned int id = data;

                md = open_model(id);
                if (IS_ERR(md)) {
                    rv = PTR_ERR(md);
                    printk("Could not open model with id %u\n", id);
                    break;
                }
                rv = 0;

                /* Exchange the file operations with that of model_fops */
                filp->f_op = &model_fops;
                filp->private_data = md;
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
void signal_model_change(struct model_dev *md, char insert)
{
    if (!event_wp)
        return;

    if (insert) {
        event_wp->type = new_model;
    }
    else {
        event_wp->type = del_model;
    }
    event_wp->id = md->id;

    /* Update write pointer */
    if (++event_wp == event_list_end)
        event_wp = event_list;
    if (event_wp == event_rp)
        event_wp = NULL;

    wake_up_interruptible(&event_q);
}

static int 
open_kernel(void)
{
    int err = 0;
    struct model_dev *md;

    // Make sure there is only one file handle to the rtcom kernel
    if (rtcom_lock) {
        err = -EBUSY;
        goto out_trylock;
    }
    rtcom_lock = 1;

#define EVENTQ_LEN 128

    event_list = kmalloc(sizeof(*event_list) * EVENTQ_LEN, GFP_KERNEL);
    if (!event_list) {
        err = -ENOMEM;
        goto out_kmalloc;
    }
    event_wp = event_rp = event_list;
    event_list_end = &event_list[EVENTQ_LEN];

    /* Inform the buddy of currently running models */
    list_for_each_entry(md, &model_list, list) {
        if (md->model)
            signal_model_change(md, 1);
    }

    pr_debug("Opened rtcom file: %p\n", event_list);

    return 0;

    kfree(event_list);
out_kmalloc:
    rtcom_lock = 0;
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
    struct rtcom_event *curr_wp = event_wp;
    size_t len;

    /* If event_wp is NULL, there was a buffer overflow */
    if (!curr_wp)
        return -ENOSPC;

    /* Check whether the buffer is empty */
    if (curr_wp == event_rp)
        return 0;

    /* Write pointer wrapped around, so only copy to the end of the buffer
     * The buddy will have to read again to get the buffer start
     */
    if (curr_wp < event_rp)
        curr_wp = event_list_end;

    /* The number of characters to be copied */
    len = (caddr_t)curr_wp - (caddr_t)event_rp;

    /* Check whether the buddy's buffer is long enough */
    if (len > bufLen) {

        /* truncate to an integer length of struct rtcom_event */
        curr_wp = event_rp + bufLen / sizeof(*curr_wp);
        len = (caddr_t)curr_wp - (caddr_t)event_rp;
        if (!len)
            return 0;
    }

    if (copy_to_user(buffer, event_rp, len))
        return -EFAULT;

    event_rp = (curr_wp == event_list_end) ? event_list : curr_wp;

    return len;
}

static int 
fop_release_kernel( struct inode *inode, struct file *filp) 
{
    kfree(event_list);
    rtcom_lock = 0;
    return 0;
}

/*#########################################################################*
 * Here the file operations to control interact with the model are defined.
 * The buddy needs this to be able to change things in the Kernel.
 *#########################################################################*/
struct model_dev*
open_model(unsigned int id) 
{
    int err = 0;
    struct model* model = NULL;
    struct model_dev *md;
    const struct rt_model *rt_model;

    /* Find model with name model_name. Note that since the rt_kernel is
     * locked, no models can be added or removed, nor opening and closing
     * of file handles. This means that no structures will change here. */
    list_for_each_entry(md, &model_list, list) {
        if (md->model && md->id == id) {
            model = md->model;
            break;
        }
    }
    if (!model) {
        /* There is no model named model_name */
        err = -ENOENT;
        goto out_no_model;
    }
    rt_model = model->rt_model;

    /* Make sure there is only one file handle opened to the model */
    if (md->buddy_lock) {
        err = -EBUSY;
        goto out_trylock;
    }
    md->buddy_lock = 1;

    /* Allocate memory for signal output. This memory will be mmap()ed 
     * later */
    md->io_len = 
        rt_model->rtB_count * (rt_model->rtB_size + model->task_stats_len);
    md->rtb_buf = vmalloc(md->io_len);
    if (!md->rtb_buf) {
        err = -ENOMEM;
        goto out_vmalloc;
    }
    md->rtb_last = md->rtb_buf + model->rtB_cnt * model->rtB_len;

    /* Allocate memory for the event stream */
    md->event_list = 
        kmalloc(sizeof(*event_list) * model->rtB_cnt, GFP_KERNEL);
    if (!md->event_list) {
        err = -ENOMEM;
        goto out_kmalloc;
    }
    md->event_list_end = md->event_list + model->rtB_cnt;

    pr_debug("Opened rtcom model file to %s\n", model->rt_model->modelName);

    /* Put new file operations in place. These now operate on the kernel */
    init_waitqueue_head(&md->waitq);

    /* Setting the write pointers is an indication that the memory has
     * been set up and writing can begin. */
    md->rp = md->wp = md->rtb_buf;
    md->event_rp = md->event_wp = md->event_list;

    return md;

    kfree(md->event_list);
out_kmalloc:
    vfree(md->rtb_buf);
    printk("Could not kmalloc %i bytes\n", 
            sizeof(*event_list) * model->rtB_cnt);
out_vmalloc:
    md->buddy_lock = 0;
    printk("Could not vmalloc %i segments of %i bytes\n", 
            rt_model->rtB_count, rt_model->rtB_size);
out_trylock:
    printk("Buddy already active for model %s\n", rt_model->modelName);
out_no_model:
    printk("Model with id %u not found\n", id);
    return ERR_PTR(err);
}

static int 
fop_release_model( struct inode *inode, struct file *filp) 
{
    struct model_dev *md = filp->private_data;

    /* Make sure that the kernel does not write to the buffer any more */
    rt_sem_wait(&md->rt_lock);
    md->wp = NULL;
    rt_sem_signal(&md->rt_lock);

    /* Clean up memory */
    kfree(md->event_list);
    vfree(md->rtb_buf);

    md->buddy_lock = 0;

    /* If model does not exist any more, remove this item from the list */
    down(&rt_kernel.lock);
    if (!md->model) {
        list_del(&md->list);
        kfree(md);
    }
    up(&rt_kernel.lock);

    return 0;
}

static unsigned int 
fop_poll_model( struct file *filp, poll_table *wait) 
{
    unsigned int mask = 0;
    struct model_dev *md = filp->private_data;

    /* Wait for data */
    poll_wait(filp, &md->waitq, wait);

    /* Check whether there was a change in the states of a Real-Time Process */
    if (md->event_wp != md->event_rp) {
        mask = POLLIN | POLLRDNORM;
    }

    return mask;
}

static long 
fop_ioctl_model( struct file *filp, unsigned int command, unsigned long data) 
{
    long rv = 0;
    struct model_dev *md = filp->private_data;
    struct model* model;
    const struct rt_model* rt_model;

    pr_debug("rt_kernel ioctl command %#x, data %lu\n", command, data);

    down(&rt_kernel.lock);

    model = md->model;
    if (!model) {
        up(&rt_kernel.lock);
        return -ENODEV;
    }

    rt_model = model->rt_model;
    switch (command) {
        case GET_RTK_PROPERTIES:
            {
                struct rt_kernel_prop p;
                p.iomem_len = md->io_len;
                p.eventq_len = 
                    (caddr_t)md->event_list_end - (caddr_t)md->event_list;
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
                char *user_path;
                char *local_path;
                const char *err;
                pr_debug("In GET_(SIGNAL|PARAM)_INFO\n");

                // Get the index the user is interested in
                if (copy_from_user(&si, (void*)data, sizeof(si))) {
                    rv = -EFAULT;
                    break;
                }

                /* Save the pointer to the user's path string, and make
                 * si.path point to our own memory */
                user_path = si.path;
                si.path = local_path = kmalloc(si.path_buf_len, GFP_KERNEL);
                if (!local_path) {
                    rv = -ENOMEM;
                    break;
                }

                /* Get the information from the model */
                err = (command == GET_SIGNAL_INFO)
                    ? rt_model->get_signal_info(&si)
                    : rt_model->get_param_info(&si);
                if (err) {
                    printk("Error: %s\n", err);
                    rv = -ERANGE;
                }
                else if (copy_to_user(user_path, local_path, si.path_buf_len)) {
                        rv = -EFAULT;
                }
                else {
                    /* Replace user's path, otherwise he will be surprised */
                    si.path = user_path;
                    if (copy_to_user((void*)data, &si, sizeof(si))) {
                        rv = -EFAULT;
                    }
                }
                kfree(local_path);
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
    struct model_dev* md = filp->private_data;
    struct model_event *curr_wp = md->event_wp;
    size_t len;

    /* If event_wp is NULL, there was a buffer overflow */
    if (!curr_wp)
        return -ENOSPC;

    /* Check whether the buffer is empty */
    if (curr_wp == md->event_rp)
        return 0;

    /* Write pointer wrapped around, so only copy to the end of the buffer
     * The buddy will have to read again to get the buffer start
     */
    if (curr_wp < md->event_rp)
        curr_wp = md->event_list_end;

    /* The number of characters to be copied */
    len = (caddr_t)curr_wp - (caddr_t)md->event_rp;

    /* Check whether the buddy's buffer is long enough */
    if (len > bufLen) {

        /* truncate to an integer length of struct model_event */
        curr_wp = md->event_rp + bufLen / sizeof(*curr_wp);
        len = (caddr_t)curr_wp - (caddr_t)md->event_rp;
        if (!len)
            return 0;
    }

    if (copy_to_user(buffer, md->event_rp, len))
        return -EFAULT;

    md->event_rp = (curr_wp == md->event_list_end)
        ? md->event_list : curr_wp;

    return len;
}

static struct page *
vma_nopage(struct vm_area_struct *vma, unsigned long address, int *type)
{
    unsigned long offset;
    struct page *page = NOPAGE_SIGBUS;
    struct model_dev* md = (struct model_dev*)vma->vm_private_data;

    offset = (address - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);

    /* Check that buddy did not want to access memory out of range */
    if (offset >= md->model->rtB_cnt * md->model->rtB_len)
        return NOPAGE_SIGBUS;

    page = vmalloc_to_page(md->rtb_buf + offset);

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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14)
void * kzalloc(size_t len, int type)
{
    void *p;
    if ((p = kmalloc(len,type))) {
        memset(p, 0, len);
    }
    return p;
}
#endif

/*#########################################################################*
 * Here is general management code to initialise file handles for the 
 * Real-Time Kernel.
 *#########################################################################*/
int 
rtcom_new_model(struct model *model)
{
    int rv;

    struct model_dev *md;
    md = kzalloc(sizeof(*md), GFP_KERNEL);
    if (!md) {
        rv = -ENOMEM;
        goto out_kmalloc;
    }

    list_add_tail(&md->list, &model_list);
    md->model = model;
    get_random_bytes(&md->id, sizeof(md->id));
#endif
    rt_sem_init(&md->rt_lock, 1);
    init_waitqueue_head(&md->waitq);
    rt_sem_init(&model->rtP_sem,0);

    /* Append a new_model event to the end of the rtcom event list */
    signal_model_change(md, 1);

    return 0;

    kfree(md);
out_kmalloc:
    return rv;
}

void rtcom_del_model(struct model *model)
{
    struct model_dev *md;

    list_for_each_entry(md, &model_list, list) {
        if (md->model == model)
            break;
    }
    signal_model_change(md, 0);

    /* If the buddy does not not exist any more, free the memory */
    if (md->buddy_lock) {
        md->model = NULL;
    }
    else {
        list_del(&md->list);
        kfree(md);
    }
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

