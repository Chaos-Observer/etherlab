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

#include "rtcom_io.h"

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
#include "mdl_wrapper.h"

// 4MB IO memory
#define IO_MEM_LEN (1<<23)
#define EVENTQ_LEN (1<<14)

// Wait queue for poll
static DECLARE_WAIT_QUEUE_HEAD(event_q);
static DECLARE_MUTEX(rtcom_lock);
static DECLARE_MUTEX(dev_lock);

static struct rtcom_event *event_list, *event_list_end,
                          *event_rp, *event_wp = NULL;
static void *io_mem;

static struct page *
vma_nopage(struct vm_area_struct *vma, unsigned long address, int *type)
{
    unsigned long offset;
    struct page *page = NOPAGE_SIGBUS;

    offset = (address - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);

    /* Check that buddy did not want to access memory out of range */
    if (offset >= IO_MEM_LEN)
        return NOPAGE_SIGBUS;

    page = vmalloc_to_page(io_mem + offset);

//    pr_debug("Nopage fault vma, address = %#lx, offset = %#lx, page = %p\n", 
//            address, offset, page);

    /* got it, now increment the count */
    get_page(page);
    if (type)
        *type = VM_FAULT_MINOR;
    
    return page;
}

/*#########################################################################*
 * Here the file operations to control the Real-Time Kernel are defined.
 * The buddy needs this to be able to change things in the Kernel.
 *#########################################################################*/
static int 
fop_open( struct inode *inode, struct file *filp) 
{
    int err = 0;
    unsigned int model_id;

    if (down_trylock(&rtcom_lock)) {
        err = -EBUSY;
        goto out_trylock;
    }

    io_mem = vmalloc(IO_MEM_LEN);
    if (!io_mem) {
        err = -ENOMEM;
        goto out_vmalloc;
    }

    event_list = kmalloc(sizeof(struct rtcom_event) * EVENTQ_LEN, GFP_KERNEL);
    if (!io_mem) {
        err = -ENOMEM;
        goto out_kmalloc;
    }
    event_rp = event_list;
    event_list_end = &event_list[EVENTQ_LEN];

    /* Dont need to lock here */
    event_wp = event_list;

    down(&rt_kernel.lock);
    for ( model_id = 0; model_id < MAX_MODELS; model_id++ ) {
        if (!test_bit(model_id, &rt_kernel.loaded_models))
            continue;
        rtcom_new_model(rt_kernel.model[model_id]);
    }
    up(&rt_kernel.lock);
    pr_debug("Opened rtcom file: %p %p\n", io_mem, event_list);
    return 0;

    kfree(event_list);
out_kmalloc:
    vfree(io_mem);
out_vmalloc:
    up(&rtcom_lock);
out_trylock:
    return err;
}

static int 
fop_release( struct inode *inode, struct file *filp) 
{
    down(&dev_lock);
    event_wp = NULL;
    up(&dev_lock);

    vfree(io_mem);
    kfree(event_list);
    up(&rtcom_lock);
    return 0;
}

static unsigned int 
fop_poll( struct file *filp, poll_table *wait) 
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

static long 
fop_ioctl( struct file *filp, unsigned int command, unsigned long data) 
{
    long rv = 0;

    pr_debug("rt_kernel ioctl command %#x, data %lu\n", command, data);

    down(&rt_kernel.lock);
    switch (command) {
        case GET_RTK_PROPERTIES:
            {
                struct rt_kernel_prop p;
                p.iomem_len = IO_MEM_LEN;
                p.eventq_len = EVENTQ_LEN;
                printk("In GET_RTK_PROPERTIES\n");

                if (copy_to_user((void*)data, &p, sizeof(p))) {
                    rv = -EFAULT;
                    break;
                }
            }
            break;

        case SET_PRIV_DATA:
            {
                struct rtcom_ioctldata id;
                struct model* model;
                printk("In SET_PRIV_DATA\n");

                if (copy_from_user(&id, (void*)data, sizeof(id))) {
                    rv = -EFAULT;
                    break;
                }
                printk("setting private data %p\n", id.data);

                // Set the RTCom priv_data field
                model = id.model_id;
                model->rtcom_data.priv_data = id.data;
            }
            break;

        case GET_MDL_SAMPLETIMES:
            {
                struct rtcom_ioctldata id;
                const struct rtw_model* rtw_model;
                printk("In GET_MDL_SAMPLETIMES\n");

                if (copy_from_user(&id, (void*)data, sizeof(id))) {
                    rv = -EFAULT;
                    break;
                }
                rtw_model = id.model_id->rtw_model;

                if (sizeof(*rtw_model->task_period) * rtw_model->num_st
                        != id.data_len) {
                    pr_info("Error: data area too small.");
                    rv = -ENOSPC;
                    break;
                }

                if (copy_to_user(id.data, rtw_model->task_period, 
                            id.data_len)) {
                    rv = -EFAULT;
                    break;
                }
            }
            break;

        case GET_PARAM:
            {
                struct rtcom_ioctldata id;
                const struct rtw_model* rtw_model;
                printk("In GET_PARAM\n");

                if (copy_from_user(&id, (void*)data, sizeof(id))) {
                    rv = -EFAULT;
                    break;
                }
                rtw_model = id.model_id->rtw_model;

                if (copy_to_user(id.data, rtw_model->mdl_rtP, 
                            rtw_model->rtP_size)) {
                    rv = -EFAULT;
                    break;
                }
            }
            break;

        case GET_SIGNAL_INFO:
        case GET_PARAM_INFO:
            {
                struct rtcom_ioctldata id;
                const struct rtw_model* rtw_model;
                struct signal_info si;
                const char *path;
                const char *err;
                size_t len;
                pr_debug("In GET_(SIGNAL|PARAM)_INFO\n");

                if (copy_from_user(&id, (void*)data, sizeof(id))) {
                    rv = -EFAULT;
                    break;
                }
                rtw_model = id.model_id->rtw_model;
                if (copy_from_user(&si, id.data, sizeof(si))) {
                    rv = -EFAULT;
                    break;
                }

                err = (command == GET_SIGNAL_INFO)
                    ? rtw_model->get_signal_info(&si, &path)
                    : rtw_model->get_param_info(&si, &path);
                if (err) {
                    pr_info("Error occurred in get_signal_info(): %s\n",
                            err);
                    rv = -EINVAL;
                    break;
                }
                len = strlen(path) + 1;

                if (sizeof(si) + len > id.data_len) {
                    pr_info("Error: data area too small.");
                    rv = -ENOSPC;
                    break;
                }

                if (copy_to_user(id.data, &si, sizeof(si)) ||
                        copy_to_user(((struct signal_info*)id.data)->path,
                            path, len)) {
                    rv = -EFAULT;
                    break;
                }
            }
            break;

        case GET_MDL_PROPERTIES:
            {
                struct rtcom_ioctldata id;
                const struct rtw_model *rtw_model;
                struct mdl_properties p;
                printk("In GET_MDL_PROPERTIES\n");

                if (copy_from_user(&id, (void*)data, sizeof(id))) {
                    rv = -EFAULT;
                    break;
                }
                rtw_model = id.model_id->rtw_model;

                p.signal_count  = rtw_model->signal_count;
                p.param_count   = rtw_model->param_count;
                p.variable_path_len  = rtw_model->variable_path_len;
                p.num_st        = rtw_model->num_st;
                p.num_tasks     = rtw_model->num_tasks;

                // No buffer overflow here, the array is defined as
                // name[MAX+1]
                strncpy(p.name, rtw_model->modelName, MAX_MODEL_NAME_LEN);
                strncpy(p.version, rtw_model->modelVersion, 
                        MAX_MODEL_VER_LEN);
                p.name[MAX_MODEL_NAME_LEN] = '\0';
                p.version[MAX_MODEL_VER_LEN] = '\0';

                if (sizeof(p) != id.data_len ||
                        copy_to_user(id.data, &p, sizeof(p))) {
                    rv = -EFAULT;
                    break;
                }
                break;
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
fop_read( struct file * filp, char *buffer, size_t bufLen, loff_t * offset)
{
    struct rtcom_event *wp = event_wp;
    ssize_t len;

    /* If event_wp is NULL, there was a buffer overflow */
    if (!wp)
        return -ENOSPC;

    /* Check whether the buffer is empty */
    if (wp == event_rp)
        return 0;

    /* Write pointer wrapped around, so only copy to the end of the buffer
     * The buddy will have to read again to get the buffer start
     */
    if (wp < event_rp)
        wp = event_list_end;

    /* The number of characters to be copied */
    len = (void*)wp - (void*)event_rp;

    /* Check whether the buddy's buffer is long enough */
    if (len > bufLen) {

        /* truncate to an integer length of struct rtcom_event */
        len -= bufLen % sizeof(*wp);
        if (!len)
            return 0;
    }

    if (copy_to_user(buffer, event_rp, len))
        return -EFAULT;

    event_rp = wp;

    return len;
}

static struct vm_operations_struct vm_ops = {
    .nopage = vma_nopage,
};

static int
fop_mmap(struct file *filp, struct vm_area_struct *vma)
{
    vma->vm_ops = &vm_ops;
    vma->vm_flags |= VM_RESERVED;       /* Pages will not be swapped out */
    return 0;
}

static struct file_operations kernel_fops = {
    .unlocked_ioctl = fop_ioctl,
    .poll           = fop_poll,
    .open           = fop_open,
    .read           = fop_read,
    .mmap           = fop_mmap,
    .release        = fop_release,
    .owner          = THIS_MODULE,
};

struct miscdevice misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "etl",
    .fops = &kernel_fops,
};


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
    down(&dev_lock);
    if (event_wp) {
        event_wp->type = new_model;
        event_wp->data.model_ref = model;
        update_wp();
    }
    up(&dev_lock);
    wake_up_interruptible(&event_q);
}

void rtcom_del_model(struct model *model)
{
    down(&dev_lock);
    if (event_wp) {
        event_wp->type = del_model;
        event_wp->priv_data = model->rtcom_data.priv_data;
        printk("Generating delete event: %p %p\n", model, model->rtcom_data.priv_data);
        update_wp();
    }
    up(&dev_lock);
    wake_up_interruptible(&event_q);
}

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

